#include "Base.h"
#include "HookX64.h"
#include "Utils.h"
#include "nt_patchguard.h"

EXTERN_C VOID AdjustStackCallPointer(
	IN ULONG_PTR NewStackPointer,
	IN PVOID StartAddress,
	IN PVOID Argument);

EXTERN_C CHAR GetCpuIndex();
EXTERN_C VOID HookKiRetireDpcList();
EXTERN_C VOID HookRtlCaptureContext();
EXTERN_C VOID BackTo1942();
extern "C"
{
	static ULONG g_ThreadContextRoutineOffset = 0;
	static UINT g_MaxCpu = 0;
	static ULONG64 g_KeBugCheckExAddress = 0;
	static ULONG64 g_KeBugCheck2Address = 0;
	KDPC  g_TempDpc[0x100];
	ULONG64 g_CpuContextAddress = 0;
	ULONG64 g_KiRetireDpcList = 0;
	typedef VOID(*TRtlCaptureContext)(PCONTEXT ContextRecord);
	typedef BOOLEAN(*TPoBugcheckEarlyCallback)(ULONG BugCode);
	TRtlCaptureContext OldRtlCaptureContext = nullptr;
	TPoBugcheckEarlyCallback OldPoBugcheckEarlyCallback = nullptr;
	typedef struct _HOOK_CTX
	{
		ULONG64 rax;
		ULONG64 rcx;
		ULONG64 rdx;
		ULONG64 rbx;
		ULONG64 rbp;
		ULONG64 rsi;
		ULONG64 rdi;
		ULONG64 r8;
		ULONG64 r9;
		ULONG64 r10;
		ULONG64 r11;
		ULONG64 r12;
		ULONG64 r13;
		ULONG64 r14;
		ULONG64 r15;
		ULONG64 Rflags;
		ULONG64 rsp;
	}HOOK_CTX, *PHOOK_CTX;

};
//#pragma LOCKEDCODE
//EXTERN_C BOOLEAN HookPoBugcheckEarlyCallback(ULONG BugCode)
//{
//	auto irql = ddk::special_data::getInstance().get_debug_irql();
//	if (BugCode==0x109)
//	{
//		DisablePG(irql);
//	}
//	return FALSE;
//}

VOID DisablePatchProtectionSystemThreadRoutine(
	IN PVOID Nothing)
{
	PUCHAR  CurrentThread = (PUCHAR)PsGetCurrentThread();
	for (g_ThreadContextRoutineOffset = 0;
	g_ThreadContextRoutineOffset < PAGE_SIZE;
		g_ThreadContextRoutineOffset += 4)
	{
		if (*(PVOID **)(CurrentThread +
			g_ThreadContextRoutineOffset) == (PVOID)DisablePatchProtectionSystemThreadRoutine)
			break;
	}

	if (g_ThreadContextRoutineOffset < PAGE_SIZE)
	{
		
		g_MaxCpu = (UINT)KeNumberProcessors;

		g_CpuContextAddress = (ULONG64)ExAllocatePool(NonPagedPool, 0x200 * g_MaxCpu + 0x1000);

		if (!g_CpuContextAddress)
		{
			return;
		}

		RtlZeroMemory(g_TempDpc, sizeof(KDPC) * 0x100);
		RtlZeroMemory((PVOID)g_CpuContextAddress, 0x200 * g_MaxCpu);
		//KeInitializeEvent(&g_BsodEvent, SynchronizationEvent, FALSE);

		{
			g_KeBugCheckExAddress = ULONG64(ddk::util::DynImport::Instance().get_proc_address("KeBugCheckEx"));
			auto KiRetireDpcListAddress = ddk::special_data::getInstance().GetKiRetireDpcList();
			auto _RtlCaptureContext = ddk::util::DynImport::Instance().get_proc_address("RtlCaptureContext");

			DBG_PRINT("KiRetireDpcList %p KeBugCheckEx %p RtlCaptureContext %p\r\n",
				KiRetireDpcListAddress,
				g_KeBugCheckExAddress,
				_RtlCaptureContext);

			{
				if (g_KeBugCheckExAddress &&KiRetireDpcListAddress && _RtlCaptureContext)
				{
					CHookX64::getInstance().inline_hook14(KiRetireDpcListAddress,
						reinterpret_cast<PVOID>(HookKiRetireDpcList),
						reinterpret_cast<PVOID*>(&g_KiRetireDpcList));
					CHookX64::getInstance().inline_hook14(_RtlCaptureContext,
						reinterpret_cast<PVOID>(HookRtlCaptureContext),
						reinterpret_cast<PVOID*>(&OldRtlCaptureContext));
				}
				else
				{
					//Contextģʽ
					ddk::nt_patchguard::getInstance().disable_pg_context();
				}
			}
		}
	}
}
VOID InitDisablePG()
{
	OBJECT_ATTRIBUTES Attributes;
	HANDLE            ThreadHandle = NULL;

	InitializeObjectAttributes(
		&Attributes,
		NULL,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);


	auto Status = PsCreateSystemThread(
		&ThreadHandle,
		THREAD_ALL_ACCESS,
		&Attributes,
		NULL,
		NULL,
		DisablePatchProtectionSystemThreadRoutine,
		NULL);

	if (ThreadHandle)
		ZwClose(ThreadHandle);
}
#pragma LOCKEDCODE
//���ĵ�
VOID
PgTempDpc(
	IN struct _KDPC *Dpc,
	IN PVOID DeferredContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	)
{
	return;
}

#pragma LOCKEDCODE
EXTERN_C VOID OnRtlCaptureContext(PHOOK_CTX hookCtx)
{
	ULONG64 Rcx;
	PCONTEXT pCtx = (PCONTEXT)(hookCtx->rcx);
	ULONG64 Rip = *(ULONG64 *)(hookCtx->rsp);
	
	OldRtlCaptureContext(pCtx);

	pCtx->Rsp = hookCtx->rsp + 0x08;
	pCtx->Rip = Rip;
	pCtx->Rax = hookCtx->rax;
	pCtx->Rbx = hookCtx->rbx;
	pCtx->Rcx = hookCtx->rcx;
	pCtx->Rdx = hookCtx->rdx;
	pCtx->Rsi = hookCtx->rsi;
	pCtx->Rdi = hookCtx->rdi;
	pCtx->Rbp = hookCtx->rbp;

	pCtx->R8 = hookCtx->r8;
	pCtx->R9 = hookCtx->r9;
	pCtx->R10 = hookCtx->r10;
	pCtx->R11 = hookCtx->r11;
	pCtx->R12 = hookCtx->r12;
	pCtx->R13 = hookCtx->r13;
	pCtx->R14 = hookCtx->r14;
	pCtx->R15 = hookCtx->r15;


	Rcx = *(ULONG64 *)(hookCtx->rsp + 0x48);
	//һ��ʼ�洢λ��rcx=[rsp+8+30]
	//call֮�����[rsp+8+30+8]

	if (Rcx == 0x109)
	{
		//PG��������
		if (Rip >= g_KeBugCheckExAddress && Rip <= g_KeBugCheckExAddress + 0x64)
		{

			//����KeBugCheckEx������
			// �Ȳ���һ��DPC
			//���IRQL�ļ��������DPC_LEVEL�ģ���˵�еĻص���ȥ�ļ�����
			//�������ͨ�ģ�������ThreadContext����
			PCHAR CurrentThread = (PCHAR)PsGetCurrentThread();
			PVOID StartRoutine = *(PVOID **)(CurrentThread + g_ThreadContextRoutineOffset);
			PVOID StackPointer = IoGetInitialStack();
			CHAR  Cpu = GetCpuIndex();
			KeInitializeDpc(&g_TempDpc[Cpu],
				PgTempDpc,
				NULL);
			KeSetTargetProcessorDpc(&g_TempDpc[Cpu], (CCHAR)Cpu);
			//KeSetImportanceDpc( &g_TempDpc[Cpu], HighImportance);
			KeInsertQueueDpc(&g_TempDpc[Cpu], NULL, NULL);
			if (1) {
				//Ӧ���жϰ汾��������¶���
				PCHAR StackPage = (PCHAR)IoGetInitialStack();

				*(ULONG64 *)StackPage = (((ULONG_PTR)StackPage + 0x1000) & 0x0FFFFFFFFFFFFF000);//stack��ʼ��MagicCode��
																								// ���û����win7�Ժ��ϵͳ�ϻ�50����
			}
			if (KeGetCurrentIrql() != PASSIVE_LEVEL)
			{
				//ʱ�⵹����
				BackTo1942();//�ص�call KiRetireDpcListȥ�ˣ�
			}
			//�߳�TIMER��ֱ��ִ���߳�ȥ��
			AdjustStackCallPointer(
				(ULONG_PTR)StackPointer - 0x8,
				StartRoutine,
				NULL);
		}
	}
	return;
}
//////////////////////////////////////////////////////////////////////////

