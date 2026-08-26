#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>

DWORD GetPid(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main() {
    DWORD pid = GetPid(L"MiniGameApp.exe");
    if (!pid) { printf("Process not found\n"); return 1; }
    printf("PID: %d\n", pid);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { printf("OpenProcess failed\n"); return 1; }

    const wchar_t* dllPath = L"C:\\Users\\voidcpp\\Documents\\MiniWorld MOD\\zelvex-miniworld-cheatmenu-main\\dll\\lua_dll.dll";
    SIZE_T pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID pRemote = VirtualAllocEx(hProc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, pRemote, dllPath, pathSize, nullptr);

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, pLoadLib, pRemote, 0, nullptr);
    if (!hThread) { printf("CreateRemoteThread failed\n"); return 1; }

    WaitForSingleObject(hThread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    printf("Exit code: %p\n", (void*)exitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;
}
