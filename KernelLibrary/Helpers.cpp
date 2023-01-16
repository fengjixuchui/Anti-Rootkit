#include "pch.h"
#include "Helpers.h"
#include "Logging.h"
#include <ntimage.h>
#include "PEParser.h"
#include "khook.h"

// AntiRootkit!khook::GetSystemServiceTable
ULONG_PTR Helpers::SearchSignature(ULONG_PTR address, PUCHAR signature, ULONG size,ULONG memSize) {
	int i = memSize;
    ULONG_PTR maxAddress = address + memSize - size;
    
	while (i--&&address < maxAddress) {
		if (memcmp(signature, (void*)address, size) == 0) {
			return address;
		}
		address++;
	}
	return 0;
}

NTSTATUS Helpers::OpenProcess(ACCESS_MASK accessMask, ULONG pid, PHANDLE phProcess) {
	CLIENT_ID cid;
	cid.UniqueProcess = ULongToHandle(pid);
	cid.UniqueThread = nullptr;

	OBJECT_ATTRIBUTES procAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, OBJ_KERNEL_HANDLE);
	return ZwOpenProcess(phProcess, accessMask, &procAttributes, &cid);
}

ULONG_PTR Helpers::KiDecodePointer(_In_ ULONG_PTR pointer, _In_ ULONG_PTR salt) {
	ULONG_PTR value = pointer;

#ifdef _WIN64
	value = RotateLeft64(value ^ KiWaitNever, KiWaitNever & 0xFF);
	value = RtlUlonglongByteSwap(value ^ salt)^ KiWaitAlways;
#else
	value = RotateLeft32(value ^ (ULONG)KiWaitNever, KiWaitNever & 0xFF);
	value = RtlUlongByteSwap(value ^ salt) ^ (ULONG)KiWaitAlways;
#endif // _WIN64
	return value;
}

// Check if fullName is ends with ShortName
bool Helpers::IsSuffixedUnicodeString(PCUNICODE_STRING fullName, PCUNICODE_STRING shortName, bool caseInsensitive) {
	if (fullName && shortName &&
		shortName->Length <= fullName->Length) {
		UNICODE_STRING ustr = {
			shortName->Length,
			ustr.Length,
			(PWSTR)RtlOffsetToPointer(fullName->Buffer,fullName->Length - ustr.Length)
		};
		return RtlEqualUnicodeString(&ustr, shortName, caseInsensitive);
	}
	return false;
}

bool Helpers::IsMappedByLdrLoadDll(PCUNICODE_STRING shortName) {
	// smss.exe can map kernel32.dll during creation of \\KnownDlls (in that  case ArbitrayUserPointer will be 0)
	// Wow64 processes map kernel32.dll serverals times (32 and 64-bit version) with WOW64_IMAGE_SECTION or
	// NOT_AN_IMAGE

	UNICODE_STRING name;
	__try {
		PNT_TIB Teb = (PNT_TIB)PsGetCurrentThreadTeb();
		if (!Teb || !Teb->ArbitraryUserPointer) {
			// This is not it
			return false;
		}
		name.Buffer = (PWSTR)Teb->ArbitraryUserPointer;

		// Check that we hava a valid user-mode address
		ProbeForRead(name.Buffer, sizeof(WCHAR), __alignof(WCHAR));

		// Check buffer length
		name.Length = (USHORT)wcsnlen_s(name.Buffer, MAXSHORT);

		if (name.Length == MAXSHORT) {
			// name is too long
			return false;
		}

		name.Length *= sizeof(WCHAR);
		name.MaximumLength = name.Length;

		// See if it's our needed module
		return IsSuffixedUnicodeString(&name, shortName);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		LogDebug("#EXCEPTION: (0x%X) IsMappedByLdrLoadDll", GetExceptionCode());
	}

	return false;
}

// Checks if process with pid is a specific process by its file name
bool Helpers::IsSpecificProcess(HANDLE pid,const WCHAR* imageName,bool isDebugged) {
	ASSERT(imageName);
	bool result = false;

	PEPROCESS Process;
	NTSTATUS status = PsLookupProcessByProcessId(pid, &Process);
	if (NT_SUCCESS(status)) {
		// Check for kernel debugger?
		if (!isDebugged ||
			PsIsProcessBeingDebugged(Process)) {
			// Get process handle
			HANDLE hProcess;
			status = ObOpenObjectByPointer(Process, OBJ_KERNEL_HANDLE, nullptr, 
				PROCESS_ALL_ACCESS, *PsProcessType, KernelMode, &hProcess);
			if (NT_SUCCESS(status)) {
				// Get process name
				// INFO: we need for file name, thus can't use PsGetProcessImageFileName which will truncate it past 14 chars
				UCHAR buffer[280] = { 0 };
				status = ZwQueryInformationProcess(hProcess, ProcessImageFileName, buffer, sizeof(buffer) - sizeof(WCHAR), nullptr);
				if (NT_SUCCESS(status)) {
					auto path = (UNICODE_STRING*)buffer;
					auto bs = wcsrchr(path->Buffer, L'\\');
					NT_ASSERT(bs);
					if (bs + 1 != nullptr && !_wcsicmp(bs + 1, imageName))
						result = true;
				}
				// Close handle
				ZwClose(hProcess);
			}
		}

		// Dereference process object back
		ObDereferenceObject(Process);
	}


	return result;
}

UINT Helpers::FindStringByGuid(PVOID baseAddress, UINT size, const GUID* guid) {
	union
	{
		const BYTE* pS;
		const GUID* pG;
	};

	pS = (const BYTE*)baseAddress;
	for (const BYTE* pE = pS + size - sizeof(GUID); pS <= pE; pS++)
	{
		if (memcmp(pG, guid, sizeof(GUID)) == 0)
		{
			//Matched!
			return (UINT)(pS + sizeof(GUID) - (const BYTE*)baseAddress);
		}
	}

	return (UINT)-1;
}

NTSTATUS Helpers::DumpSysModule(DumpSysData* pData) {
	// Get the process id of the "winlogon.exe" process
	bool success = khook::SearchSessionProcess();
	if (!success)
		return STATUS_UNSUCCESSFUL;

	PEPROCESS Process;
	NTSTATUS status = PsLookupProcessByProcessId(khook::_pid, &Process);
	if (!NT_SUCCESS(status))
		return status;

	PVOID pBuf = ExAllocatePool(NonPagedPool, pData->ImageSize);
	if (!pBuf) {
		ObDereferenceObject(Process);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	// attach a session process to prevent bugcheck when access session memory
	KAPC_STATE apcState;
	KeStackAttachProcess(Process, &apcState);

	// Dump pe
	RtlCopyMemory(pBuf, pData->ImageBase, pData->ImageSize);

	// Detach
	KeUnstackDetachProcess(&apcState);
	ObDereferenceObject(Process);

	// Parse pe format
	PEParser parser(pBuf);

	// fixup sections
	int count = parser.GetSectionCount();
	for (int i = 0; i < count; i++) {
		PIMAGE_SECTION_HEADER pSectionHeader = parser.GetSectionHeader(i);
		if (pSectionHeader) {
			pSectionHeader->PointerToRawData = pSectionHeader->VirtualAddress;
			pSectionHeader->SizeOfRawData = pSectionHeader->Misc.VirtualSize;
		}
	}

	void* buffer = nullptr;

	do
	{
		// Get caller's path
		UNICODE_STRING dumpDir{ 0 };
		HANDLE hProcess;
		status = ObOpenObjectByPointer(PsGetCurrentProcess(), OBJ_KERNEL_HANDLE, nullptr, 0, *PsProcessType, KernelMode, &hProcess);
		if (NT_SUCCESS(status)) {
			UCHAR buffer[280] = { 0 };
			status = ZwQueryInformationProcess(hProcess, ProcessImageFileName, buffer, sizeof(buffer) - sizeof(WCHAR), nullptr);
			ZwClose(hProcess);
			if (NT_SUCCESS(status)) {
				auto path = (UNICODE_STRING*)buffer;
				auto bs = wcsrchr(path->Buffer, L'\\');
				NT_ASSERT(bs);
				if (bs == nullptr) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				*(bs + 1) = L'\0';
				dumpDir.MaximumLength = path->Length;
				dumpDir.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, dumpDir.MaximumLength, 'pmud');
				if (dumpDir.Buffer == nullptr) {
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}
				RtlAppendUnicodeToString(&dumpDir, path->Buffer);
			}
		}

		FileManager mgr;
		
		IO_STATUS_BLOCK ioStatus;


		// set file size
		LARGE_INTEGER fileSize;
		fileSize.QuadPart = pData->ImageSize;

		// open target file
		UNICODE_STRING targetFileName;
		auto sysName = wcsrchr(pData->Name, L'\\') + 1;
		if (sysName == nullptr) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		USHORT len = wcslen(sysName);
		const WCHAR backupStream[] = L"_dump.sys";

		targetFileName.MaximumLength = dumpDir.Length + len * sizeof(WCHAR) + sizeof(backupStream);
		targetFileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetFileName.MaximumLength, 'pmud');
		if (targetFileName.Buffer == nullptr) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		RtlCopyUnicodeString(&targetFileName, &dumpDir);
		RtlAppendUnicodeToString(&targetFileName, sysName);
		RtlAppendUnicodeToString(&targetFileName, backupStream);

		status = mgr.Open(&targetFileName, FileAccessMask::Write | FileAccessMask::Synchronize);
		ExFreePool(targetFileName.Buffer);

		if (!NT_SUCCESS(status))
			break;

		// allocate buffer for copying purpose
		ULONG size = 1 << 21; // 2 MB
		buffer = ExAllocatePoolWithTag(PagedPool, size, 'kcab');
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// loop - read from source,write to target
		LARGE_INTEGER offset = { 0 }; // read
		LARGE_INTEGER writeOffset = { 0 }; // write

		ULONG bytes;
		auto saveSize = fileSize;
		while (fileSize.QuadPart > 0) {
			bytes = min(size, fileSize.QuadPart);

			RtlCopyMemory(buffer, pBuf, bytes);

			// write to target file
			status = mgr.WriteFile(buffer, bytes, &ioStatus, &writeOffset);

			if (!NT_SUCCESS(status))
				break;

			// update byte count and offsets
			offset.QuadPart += bytes;
			writeOffset.QuadPart += bytes;
			fileSize.QuadPart -= bytes;
		}

		FILE_END_OF_FILE_INFORMATION info;
		info.EndOfFile = saveSize;
		NT_VERIFY(NT_SUCCESS(mgr.SetInformationFile(&ioStatus, &info, sizeof(info), FileEndOfFileInformation)));

		
	} while (false);

	if (buffer)
		ExFreePool(buffer);
	ExFreePool(pBuf);

	return status;
}