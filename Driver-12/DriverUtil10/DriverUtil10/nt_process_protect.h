#pragma once
#include "Base.h"
#include "ntos.h"
#include "nt_process_callback.h"
namespace ddk
{
	namespace protect
	{ 
		class nt_process_protect :public Singleton<nt_process_protect>
		{
			//protect(processid)
		public:
			nt_process_protect() {
				protect_offset = ddk::special_data::getInstance().get_protect_offset();
				ddk::nt_process_callback::getInstance().reg_callback_ex(std::bind(
					&ddk::protect::nt_process_protect::callback_protect_process,
					this, 
					std::placeholders::_1,
					std::placeholders::_2,
					std::placeholders::_3));
			}
			~nt_process_protect()
			{

			}
		public:
			void callback_protect_process(
				PEPROCESS Process,
				HANDLE ProcessId,
				PPS_CREATE_NOTIFY_INFO CreateInfo)
			{
				if (!CreateInfo)
				{
					//̎��infomask���}
					auto object_header = reinterpret_cast<ddk::ntos_space::win10_14393_x64::POBJECT_HEADER>((ULONG_PTR)Process - 0x30);
					object_header->InfoMask &= ~0x10;
				}
			}
			bool protect(HANDLE ProcessId)
			{
				PEPROCESS Process = nullptr;
				auto ns = PsLookupProcessByProcessId(ProcessId, &Process);
				if (!NT_SUCCESS(ns))
				{
					return false;
				}
				//Protectλ��ʽ
				if (protect_offset)
				{
					auto ProcessPointer = reinterpret_cast<PBYTE>(Process);
					if (ddk::util::IsWindow8Point1OrGreater())
					{
						*(BYTE*)(ProcessPointer + protect_offset) |= 7;
					}
					else if (ddk::util::IsWindows80())
					{
						*(BYTE*)(ProcessPointer + protect_offset) |= 1;
					}
					else 
					{
						BitTestAndSet(reinterpret_cast<LONG*>(ProcessPointer + protect_offset), 11);
					}
				}

				//�۸� handle_entry�󷨣�����⫬�������
				//PspCidTable-->*(*(ULONG_PTR**)get_kdblock()->PspCidTable);

				//dispatch_header��Type�޸ģ�32λ�¿��ԣ�64λDeliverApcֱ���{��

				//�޸��M�̠�B̎춳�ʼ��ģʽ��PsLookupProcessByProcessId���Д��@��
				auto os = ddk::util::get_version();
				if (os==WIN7SP1)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN7SP1>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}
				if (os == WIN7)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN7>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}
				if (os == WIN8)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN8>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}
				if (os == WIN81)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN81>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}
				if (os == WIN10_10586)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN10_10586>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}
				if (os == WIN10_now)
				{
					auto process2 = reinterpret_cast<ddk::ntos<WIN10_now>::PEPROCESS>(Process);
					process2->ProcessInserted = 0;
				}

				//�޸�object���Խ�ֹ���Ì�
				auto object_header = reinterpret_cast<ddk::ntos_space::win10_14393_x64::POBJECT_HEADER>((ULONG_PTR)Process - 0x30);

				object_header->Flags |= 4;//��ֹӦ�ò�򿪶���
				LOG_DEBUG("infomask = %x\r\n", object_header->InfoMask);

				//���󪚁� �󷨣����@�Y��һЩ�ܲٵ������飡
				//Exclusive Object ���׌����
				//objectHeader = object-0x30
				//ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT &&
				//OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != NULL
				//_OBJECT_HEADER_PROCESS_INFO
				//Vista��ʼ object_header_to_exclusive_process��Ҫ���ӵĲ���
				/*
				char *__fastcall OBJECT_HEADER_TO_PROCESS_INFO(_OBJECT_HEADER *a1)
				{
				char *v1; // rcx@2

				if ( a1->InfoMask & 0x10 )
				v1 = (char *)a1 - ObpInfoMaskToOffset[a1->InfoMask & 0x1F];
				else
				v1 = 0i64;
				return v1;
				}
				__int64 ObpInitInfoBlockOffsets()
				{
				char *v0; // rdx@1
				int v1; // ecx@1
				char v2; // al@2
				__int64 result; // rax@18

				v0 = ObpInfoMaskToOffset;
				v1 = 0;
				do
				{
				v2 = 0;
				if ( v1 & 1 )
				v2 = 32;
				if ( v1 & 2 )
				v2 += 32;
				if ( v1 & 4 )
				v2 += 16;
				if ( v1 & 8 )
				v2 += 32;
				if ( v1 & 0x10 )
				v2 += 16;
				if ( v1 & 0x20 )
				v2 += 16;
				if ( v1 & 0x40 )
				v2 += 16;
				if ( (char)v1 < 0 )
				v2 += 4;
				++v1;
				*v0 = v2;
				result = v1;
				++v0;
				}
				while ( (unsigned __int64)v1 < 0x100 );
				return result;
				}
				*/
				object_header->Flags |= 8;
				if(object_header->InfoMask & 0x10)
				{
					auto pExclusiveInfo = reinterpret_cast<ddk::ntos_space::win10_14393_x64::POBJECT_HEADER_PROCESS_INFO>((ULONG_PTR)object_header - 48);
					pExclusiveInfo->ExclusiveProcess = (ddk::ntos_space::win10_14393_x64::_EPROCESS *)IoGetCurrentProcess();
				}
				else
				{
					//���܌�����������׌��ը�o�㿴
					//FileObject���Ԍ���������֪����ʲ��
					//��r����
				}
				object_header->InfoMask |= 0x10;
				//ʹ�ê��״󷨣�����ጷ�object��
				//ObDereferenceObject(Process);
				return true;
			}
			bool protect_process(std::wstring process_name)
			{
				UNICODE_STRING nsProcessName;
				RtlInitUnicodeString(&nsProcessName, process_name.c_str());
				auto pFin = ddk::util::GetSysInf(SystemProcessInformation);
				auto pInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(pFin);
				if (!pInfo)
				{
					return false;
				}
				auto fin = pFin;
				auto exit_1 = std::experimental::make_scope_exit([&]() {if (fin)free(fin); });
				bool b_ret = false;
				for (;;)
				{
					if (RtlEqualUnicodeString(&pInfo->ImageName,&nsProcessName,TRUE))
					{
						b_ret = protect(pInfo->UniqueProcessId);
					}

					if (pInfo->NextEntryOffset == 0)
					{
						break;
					}
					pInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>((ULONG_PTR)pInfo + pInfo->NextEntryOffset);
				}
				return b_ret;
			}
		private:
			ULONG protect_offset;
		};
	};
};