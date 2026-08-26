// test_dll.cpp - Minimal test: just write a file
#include <windows.h>
#include <cstdio>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        strncat_s(path, "test_dll_loaded.txt", MAX_PATH - strlen(path) - 1);
        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (f) {
            fputs("DLL loaded successfully!\n", f);
            fclose(f);
        }
    }
    return TRUE;
}
