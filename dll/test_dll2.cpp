// test_dll2.cpp - Test thread creation
#include <windows.h>
#include <cstdio>

static DWORD WINAPI TestThread(LPVOID) {
    Sleep(1000);
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strncat_s(path, "test_dll2_thread.txt", MAX_PATH - strlen(path) - 1);
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (f) {
        fputs("Thread executed successfully!\n", f);
        fclose(f);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Write from DllMain
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        strncat_s(path, "test_dll2_main.txt", MAX_PATH - strlen(path) - 1);
        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (f) {
            fputs("DllMain executed!\n", f);
            fclose(f);
        }

        // Create thread
        HANDLE hThread = CreateThread(nullptr, 0, TestThread, nullptr, 0, nullptr);
        if (!hThread) {
            FILE* f2 = nullptr;
            fopen_s(&f2, "C:\\Users\\voidcpp\\Documents\\MiniWorld MOD\\test_dll2_thread_failed.txt", "w");
            if (f2) { fputs("CreateThread failed\n", f2); fclose(f2); }
        }
    }
    return TRUE;
}
