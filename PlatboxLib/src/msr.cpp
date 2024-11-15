
#include "msr.h"

BOOL do_read_msr(UINT msr, UINT64 *res) {
	READ_MSR_CALL msr_call = { 0 };
	msr_call.msr = msr;

	UINT64 result = 0;

	#ifdef __linux__		
		msr_call.result = &result;

		int status = -1;		
		status = ioctl(g_hDevice, IOCTL_READ_MSR, &msr_call);
		if (status != 0) {
			debug_print("-> failed reading MSR:[%08x]\n", msr_call.msr);
			return FALSE;
		} else {
			debug_print("-> MSR:[%08x]: %lx\n", msr_call.msr, result);
		}
		

	#elif _WIN32
		DWORD bytesReturned = 0;
		NTSTATUS status = DeviceIoControl(g_hDevice, IOCTL_READ_MSR,
			&msr_call, sizeof(READ_MSR_CALL),
			&result, sizeof(UINT64),
			&bytesReturned, NULL);
		if (NT_SUCCESS(status)) {
			debug_print("-> MSR:[%08x]: %p\n", msr_call.msr, result);
		}
		else {
			debug_print("-> failed reading MSR:[%08x]\n", msr_call.msr);
			return FALSE;
		}
	#endif
	
	if (res != NULL) {
		*res = result;
	}

	return TRUE;
}

BOOL do_write_msr(UINT msr, UINT64 value) {
	WRITE_MSR_CALL msr_call = { 0 };
	msr_call.msr = msr;
	msr_call.value = value;

	#ifdef __linux__
		int status = -1;		
		status = ioctl(g_hDevice, IOCTL_WRITE_MSR, &msr_call);
		if (status != 0) {
			debug_print("-> failed writing to MSR:[%08x]\n", msr_call.msr);
			return FALSE;
		} else {
			debug_print("-> Attempted to write %016lx into MSR[%08x]\n", value, msr_call.msr);
		}
		
	#elif _WIN32
		DWORD bytesReturned = 0;
		NTSTATUS status = DeviceIoControl(g_hDevice, IOCTL_WRITE_MSR,
			&msr_call, sizeof(WRITE_MSR_CALL),
			NULL, NULL,
			&bytesReturned, NULL);
		if (NT_SUCCESS(status)) {
			debug_print("-> Attempted to write %016llx into MSR[%08x]\n", value, msr_call.msr);
		} else {
			debug_print("-> failed writing to MSR:[%08x]\n", msr_call.msr);
			return FALSE;
		}
	#endif
	
	return TRUE;
}

BOOL do_read_msr_for_core(UINT32 core, UINT msr, UINT64 *res) {
		
	READ_MSR_FOR_CORE_CALL msr_for_core = {
		.core_id = core,
		.msr     = msr,
		.result  = 0,
	};
		
	#ifdef __linux__
		int status = -1;		
		status = ioctl(g_hDevice, IOCTL_READ_MSR_FOR_CORE, &msr_for_core);
		if (status != 0) {
			debug_print("-> failed reading MSR:[%08x] for core-%d\n", 
				msr_for_core.msr, msr_for_core.core_id);
			return FALSE;
		} 
		debug_print("-> Core-%d | MSR:[%08x]: %016llx\n", msr_for_core.core_id,
			msr_for_core.msr, msr_for_core.result);

		*res = msr_for_core.result;

		return TRUE;
		
	#elif _WIN32
		UINT64 result = 0;
		DWORD bytesReturned = 0;
		NTSTATUS status = DeviceIoControl(g_hDevice, IOCTL_READ_MSR_FOR_CORE,
			&msr_for_core, sizeof(READ_MSR_FOR_CORE_CALL),
			&result, sizeof(UINT64),
			&bytesReturned, NULL);
		if (NT_SUCCESS(status)) {
			debug_print("-> Core-%d | MSR:[%08x]: %016llx\n", msr_for_core.core_id, msr_for_core.msr, result);
		}
		else {
			debug_print("-> failed reading MSR:[%08x] for core-%d\n", msr_for_core.msr, msr_for_core.core_id);
			return FALSE;
		}
		*res = result;

	#endif
}


BOOL do_write_msr_for_core(UINT32 core, UINT msr, UINT64 value) {

	WRITE_MSR_FOR_CORE_CALL msr_for_core = {
		.core_id = core,
		.msr     = msr,
		.value   = value,
	};

	#ifdef __linux__
		int status = -1;		
		status = ioctl(g_hDevice, IOCTL_WRITE_MSR_FOR_CORE, &msr_for_core);
		if (status != 0) {
			debug_print("-> failed writing to MSR:[%08x] for core-%d\n", 
				msr_for_core.msr, msr_for_core.core_id);
			return FALSE;
		} else {
			debug_print("-> Attempted to write %016lx into MSR[%08x] for core-%d\n", 
				value, msr_for_core.msr, msr_for_core.core_id);
		}

		return TRUE;
		
	#elif _WIN32
		DWORD bytesReturned = 0;
		NTSTATUS status = DeviceIoControl(g_hDevice, IOCTL_WRITE_MSR_FOR_CORE,
			&msr_for_core, sizeof(WRITE_MSR_FOR_CORE_CALL),
			NULL, 0,
			&bytesReturned, NULL);
		if (NT_SUCCESS(status)) {
			debug_print("-> Attempted to write %016lx into MSR[%08x] for core-%d\n", 
				value, msr_for_core.msr, msr_for_core.core_id);
		}
		else {
			debug_print("-> failed writing to MSR:[%08x] for core-%d\n", 
				msr_for_core.msr, msr_for_core.core_id);
			return FALSE;
		}

		return TRUE;
	#endif
}




