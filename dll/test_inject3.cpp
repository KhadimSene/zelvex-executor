#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>

// Must match the DLL's shared memory structure
struct LuaSharedMemory {
    static const int MAX_CODE = 4096;
    static const int MAX_OUTPUT = 4096;
    volatile LONG command;
    volatile LONG done;
    volatile LONG error;
    char code[MAX_CODE];
    char output[MAX_OUTPUT];
};

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

// Must match the DLL's export signature
typedef DWORD (WINAPI *InitializeFn)(LPVOID);

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

    // Step 1: Load the DLL
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, pLoadLib, pRemote, 0, nullptr);
    WaitForSingleObject(hThread, 10000);
    DWORD mod = 0;
    GetExitCodeThread(hThread, &mod);
    CloseHandle(hThread);
    printf("LoadLibrary returned: %p\n", (void*)mod);

    if (!mod) { printf("LoadLibrary failed\n"); return 1; }

    // Step 2: Get address of exported function by ordinal
    HMODULE hLocalMod = LoadLibraryW(dllPath);
    InitializeFn pInitLocal = (InitializeFn)GetProcAddress(hLocalMod, MAKEINTRESOURCEA(1));
    printf("Local Initialize (ordinal 1) address: %p\n", (void*)pInitLocal);

    if (!pInitLocal) { printf("GetProcAddress failed\n"); return 1; }

    // Calculate offset from module base
    DWORD_PTR offset = (DWORD_PTR)pInitLocal - (DWORD_PTR)hLocalMod;
    printf("Offset: 0x%X\n", offset);

    // Calculate remote address
    DWORD_PTR remoteInit = (DWORD_PTR)mod + offset;
    printf("Remote Initialize address: %p\n", (void*)remoteInit);

    // Step 3: Create remote thread to call Initialize
    HANDLE hInitThread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)remoteInit, nullptr, 0, nullptr);
    if (!hInitThread) { printf("CreateRemoteThread for Initialize failed\n"); return 1; }

    WaitForSingleObject(hInitThread, 10000);
    DWORD initResult = 0;
    GetExitCodeThread(hInitThread, &initResult);
    CloseHandle(hInitThread);
    printf("Initialize returned: %lu\n", initResult);

    if (initResult != 0) { printf("Initialize failed\n"); return 1; }

    // Step 4: Write Lua code to shared memory and execute
    printf("\n--- Testing Lua Execution ---\n");

    // Create shared memory locally
    HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, "ZelvexLuaSharedMem");
    if (!hMapFile) { printf("CreateFileMapping failed\n"); return 1; }

    auto* shared = (LuaSharedMemory*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    if (!shared) { printf("MapViewOfFile failed\n"); return 1; }

    memset(shared, 0, sizeof(LuaSharedMemory));

    // Write Lua code
    const char* luaCode = "Chat:sendSystemMsg('Hello from DLL!', Player:getMainPlayerUin())";
    strncpy_s(shared->code, luaCode, sizeof(shared->code) - 1);
    shared->command = 0;
    shared->done = 0;

    printf("Press Enter to execute Lua code...\n");
    getchar();

    // Signal execution
    shared->command = 1;
    printf("Waiting for execution...\n");

    // Wait for completion
    int timeout = 100;
    while (timeout-- > 0 && !shared->done) {
        Sleep(100);
    }

    if (shared->done) {
        printf("Execution complete!\n");
        printf("Output: %s\n", shared->output);
        printf("Error: %ld\n", shared->error);
    } else {
        printf("Execution timed out\n");
    }

    UnmapViewOfFile(shared);
    CloseHandle(hMapFile);
    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;

    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;
}
