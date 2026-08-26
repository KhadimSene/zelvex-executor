// test_inject.cpp - Minimal DLL injector test
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>

DWORD GetProcessIdByName(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: test_inject <process_name> <dll_path>\n");
        return 1;
    }

    // Convert process name to wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, nullptr, 0);
    wchar_t* wname = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wname, wlen);

    DWORD pid = GetProcessIdByName(wname);
    delete[] wname;

    if (!pid) {
        printf("Process not found: %s\n", argv[1]);
        return 1;
    }
    printf("Found process PID: %d\n", pid);

    // Open process
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("OpenProcess failed: %lu\n", GetLastError());
        return 1;
    }

    // Convert DLL path to wide
    wlen = MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, nullptr, 0);
    wchar_t* wdll = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, wdll, wlen);

    // Allocate memory for DLL path
    SIZE_T pathSize = (wcslen(wdll) + 1) * sizeof(wchar_t);
    LPVOID pRemote = VirtualAllocEx(hProc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemote) {
        printf("VirtualAllocEx failed: %lu\n", GetLastError());
        return 1;
    }

    WriteProcessMemory(hProc, pRemote, wdll, pathSize, nullptr);

    // Get LoadLibraryW
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryW");

    // Create remote thread
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, pLoadLib, pRemote, 0, nullptr);
    if (!hThread) {
        printf("CreateRemoteThread failed: %lu\n", GetLastError());
        return 1;
    }

    printf("Waiting for thread...\n");
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    printf("Thread exit code (HMODULE): 0x%p\n", (void*)exitCode);

    if (exitCode == 0) {
        printf("LoadLibrary FAILED in remote process\n");
    } else {
        printf("DLL loaded successfully!\n");
    }

    CloseHandle(hThread);
    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);

    delete[] wdll;
    return 0;
}
