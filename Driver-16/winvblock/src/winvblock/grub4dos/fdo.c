/**
 * Copyright (C) 2009-2012, Shao Miller <sha0.miller@gmail.com>.
 *
 * This file is part of WinVBlock, originally derived from WinAoE.
 *
 * WinVBlock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinVBlock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinVBlock.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * GRUB4DOS bus FDO
 */

#include <ntddk.h>

#include "portable.h"
#include "winvblock.h"
#include "wv_stdlib.h"
#include "driver.h"
#include "debug.h"
#include "x86.h"
#include "safehook.h"
#include "disk.h"
#include "grub4dos.h"

/** Function declarations */

/** IRP dispatchers */
DRIVER_DISPATCH WvG4dIrpDispatch;

static __drv_dispatchType(IRP_MJ_PNP) DRIVER_DISPATCH WvG4dDispatchPnpIrp;

/** PnP handlers */
static __drv_dispatchType(IRP_MN_QUERY_DEVICE_RELATIONS) DRIVER_DISPATCH
  WvG4dPnpQueryDeviceRelations;
static __drv_dispatchType(IRP_MN_REMOVE_DEVICE) DRIVER_DISPATCH
  WvG4dPnpRemoveDevice;

/** Function definitions */

/**
 * Handle an IRP for a GRUB4DOS bus
 *
 * @param DeviceObject
 *   The GRUB4DOS bus device
 *
 * @param Irp
 *   The IRP to process
 *
 * @return
 *   The status of the operation
 */
NTSTATUS STDCALL WvG4dIrpDispatch(
    IN DEVICE_OBJECT * dev_obj,
    IN IRP * irp
  ) {
    IO_STACK_LOCATION * io_stack_loc;
    NTSTATUS status;

    ASSERT(dev_obj);

    /* Process all IRPs in the device's thread */
    if (!WvlInDeviceThread(dev_obj))
      return WvlAddIrpToDeviceQueue(dev_obj, irp, TRUE);

    ASSERT(irp);
    io_stack_loc = IoGetCurrentIrpStackLocation(irp);
    ASSERT(io_stack_loc);

    /* Handle PnP IRPs, only */
    if (io_stack_loc->MajorFunction != IRP_MJ_PNP) {
        DBG("Unhandled major function for FDO %p\n", (VOID *) dev_obj);
        status = STATUS_NOT_SUPPORTED;
        irp->IoStatus.Information = 0;
        goto err_major;
      }

    return WvG4dDispatchPnpIrp(dev_obj, irp);

    err_major:

    irp->IoStatus.Status = status;
    WvlPassIrpUp(dev_obj, irp, IO_NO_INCREMENT);
    return status;
  }

/** PnP IRP dispatcher */
static NTSTATUS STDCALL WvG4dDispatchPnpIrp(
    IN DEVICE_OBJECT * dev_obj,
    IN IRP * irp
  ) {
    S_WV_G4D_BUS * bus;
    IO_STACK_LOCATION * io_stack_loc;
    UCHAR code;
    NTSTATUS status;

    ASSERT(dev_obj);
    bus = dev_obj->DeviceExtension;
    ASSERT(bus);
    ASSERT(irp);
    io_stack_loc = IoGetCurrentIrpStackLocation(irp);
    ASSERT(io_stack_loc);

    code = io_stack_loc->MinorFunction;
    switch (code) {
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        status = WvG4dPnpQueryDeviceRelations(dev_obj, irp);
        goto completed;

        case IRP_MN_REMOVE_DEVICE:
        status = WvG4dPnpRemoveDevice(dev_obj, irp);
        goto completed;

        default:
        DBG("Unhandled IRP_MN_*: %u\n", code);
      }

    return WvlPassIrpDown(dev_obj, irp);

    completed:
    return status;
  }

/**
 * IRP_MJ_PNP:IRP_MN_QUERY_DEVICE_RELATIONS handler
 *
 * IRQL == PASSIVE_LEVEL
 * Completed by PDO, must accumulate relations downwards (upwards optional)
 * BusRelations: system thread
 *   Yes: FDO
 *   Maybe: filter
 * TargetDeviceRelation: any thread
 *   Yes: PDO
 * RemovalRelations: system thread
 *   Maybe: FDO
 *   Maybe: filter
 * PowerRelations >= Windows 7: system thread
 *   Maybe: FDO
 *   Maybe: filter
 * EjectionRelations: system thread
 *   Maybe: PDO
 *
 * @param Parameters.QueryDeviceRelations.Type (I/O stack location)
 *
 * @param FileObject (I/O stack location)
 *
 * @return
 *   Success:
 *     Irp->IoStatus.Status == STATUS_SUCCESS
 *     Irp->IoStatus.Information populated from paged memory
 *   Error:
 *     Irp->IoStatus.Status == STATUS_INSUFFICIENT_RESOURCES
 */
static NTSTATUS STDCALL WvG4dPnpQueryDeviceRelations(
    IN DEVICE_OBJECT * dev_obj,
    IN IRP * irp
  ) {
    S_WV_G4D_BUS * bus;
    IO_STACK_LOCATION * io_stack_loc;
    NTSTATUS status;
    DEVICE_OBJECT * child;
    DEVICE_RELATIONS * dev_relations;
    DEVICE_RELATIONS * higher_dev_relations;
    DEVICE_RELATIONS * response;
    ULONG i;

    ASSERT(dev_obj);
    bus = dev_obj->DeviceExtension;
    ASSERT(bus);

    higher_dev_relations = (VOID *) irp->IoStatus.Information;

    ASSERT(irp);
    io_stack_loc = IoGetCurrentIrpStackLocation(irp);    

    switch (io_stack_loc->Parameters.QueryDeviceRelations.Type) {
        case BusRelations:

        /* Have we already created PDOs for the GRUB4DOS disks? */
        for (i = 0; i < M_G4D_SLOTS; ++i) {
            if (!(bus->NodesNeeded & (1 << i)))
              continue;

            child = WvG4dProcessSlot(dev_obj, bus->DriveMappings + i);
            if (child) {
                bus->BusRelations->Objects[bus->BusRelations->Count] = child;
                bus->BusRelations->Count++;
                /* TODO: Uncomment when disks are mini-driver */
                if (0)
                if (!WvlAssignDeviceToBus(child, dev_obj)) {
                    DBG("Couldn't add GRUB4DOS disk PDO\n");
                    bus->BusRelations->Count--;
                    bus->BusRelations->Objects[bus->BusRelations->Count] =
                      NULL;
                    WvlDeleteDevice(child);
                    continue;
                  }
                bus->NodesNeeded ^= 1 << i;
              }
          }

        /* Have we already created PDOs for the previous INT 0x13 handler? */
        if (bus->NodesNeeded & (1 << i)) {
            /* Try to create the previous safe hook in the chain */
            child = NULL;
            status = WvlCreateSafeHookDevice(
                bus->PreviousInt13hHandler,
                &child
              );
            if (child) {
                bus->BusRelations->Objects[bus->BusRelations->Count] = child;
                bus->BusRelations->Count++;
                if (!WvlAssignDeviceToBus(child, dev_obj)) {
                    DBG("Couldn't add safe hook PDO\n");
                    bus->BusRelations->Count--;
                    bus->BusRelations->Objects[bus->BusRelations->Count] =
                      NULL;
                    WvlDeleteDevice(child);
                  }
                bus->NodesNeeded ^= 1 << i;
              }
          }

        dev_relations = bus->BusRelations;
        break;

        case TargetDeviceRelation:
        case RemovalRelations:
        case EjectionRelations:
        default:
        DBG("Unsupported device relations query\n");
        return WvlPassIrpDown(dev_obj, irp);
      }

    /* We need to include any higher driver's PDOs */
    higher_dev_relations = (VOID *) irp->IoStatus.Information;
    response = WvlMergeDeviceRelations(dev_relations, higher_dev_relations);
    if (!response) {
        /*
         * It's not clear whether or not we should do the following,
         * but it can't hurt...  Right?
         */
        if (higher_dev_relations) {
            for (i = 0; i < higher_dev_relations->Count; ++i)
              ObDereferenceObject(higher_dev_relations->Objects[i]);
            wv_free(higher_dev_relations);
          }

        irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        WvlPassIrpUp(dev_obj, irp, IO_NO_INCREMENT);
        return status;
      }

    /* Reference our PDOs */
    for (i = 0; i < dev_relations->Count; ++i)
      ObReferenceObject(dev_relations->Objects[i]);

    /* Send the IRP down */
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = (ULONG_PTR) response;
    return WvlPassIrpDown(dev_obj, irp);
  }

/**
 * IRP_MJ_PNP:IRP_MN_REMOVE_DEVICE handler
 *
 * IRQL == PASSIVE_LEVEL, system thread
 * Completed by PDO
 * Do not send this IRP
 * Any child PDOs receive one of these IRPs first, except possibly
 * if they've been surprise-removed
 *
 * @return
 *   Success:
 *     Irp->IoStatus.Status == STATUS_SUCCESS
 */
static NTSTATUS STDCALL WvG4dPnpRemoveDevice(
    IN DEVICE_OBJECT * dev_obj,
    IN IRP * irp
  ) {
    S_WV_G4D_BUS * bus;
    UCHAR i;
    DEVICE_OBJECT * child;
    NTSTATUS status;
    S_X86_SEG16OFF16 * safe_hook;
    UCHAR * phys_mem;
    UINT32 hook_phys_addr;
    WV_S_PROBE_SAFE_MBR_HOOK * hook;
    LONG flags;

    ASSERT(dev_obj);
    bus = dev_obj->DeviceExtension;
    ASSERT(bus);
    ASSERT(irp);

    /* Unlink the child PDOs, if any */
    for (i = 0; i < bus->BusRelations->Count; ++i) {
        child = bus->BusRelations->Objects[i];
        bus->BusRelations->Objects[i] = NULL;
        /* Best effort */
        WvlAssignDeviceToBus(child, NULL);
        WvlDeleteDevice(child);
      }
    bus->BusRelations->Count -= i;

    /* Send the IRP down */
    status = WvlPassIrpDown(dev_obj, irp);

    /* Detach FDO from PDO */
    WvlDetachDevice(dev_obj);

    /* Schedule deletion of this device when the thread finishes */
    WvlDeleteDevice(dev_obj);

    /* Best effort to "unclaim" the safe hook */
    phys_mem = WvlMapUnmapLowMemory(NULL);
    if (phys_mem) {
        safe_hook = WvlGetSafeHook(bus->PhysicalDeviceObject);
        ASSERT(safe_hook);
        hook_phys_addr = M_X86_SEG16OFF16_ADDR(safe_hook);
        hook = (VOID *) (phys_mem + hook_phys_addr);
        flags = InterlockedAnd(&hook->Flags, ~1);
        hook->VendorId[6] = 'O';
        WvlMapUnmapLowMemory(phys_mem);
      }

    return status;
  }
