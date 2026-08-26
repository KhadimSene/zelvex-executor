#include <windows.h>
#include <cstdio>

int main() {
    const wchar_t* dllPath = L"C:\\Users\\voidcpp\\Documents\\MiniWorld MOD\\zelvex-miniworld-cheatmenu-main\\dll\\lua_dll_v3.dll";
    HMODULE hMod = LoadLibraryW(dllPath);
    if (!hMod) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    printf("Module: %p\n", hMod);

    void* p;

    p = (void*)GetProcAddress(hMod, "Initialize");
    printf("Initialize: %p\n", p);

    p = (void*)GetProcAddress(hMod, "_Initialize");
    printf("_Initialize: %p\n", p);

    p = (void*)GetProcAddress(hMod, "Initialize@4");
    printf("Initialize@4: %p\n", p);

    p = (void*)GetProcAddress(hMod, "_Initialize@4");
    printf("_Initialize@4: %p\n", p);

    printf("\nTrying ordinals...\n");
    for (int i = 0; i < 100; i++) {
        p = (void*)GetProcAddress(hMod, MAKEINTRESOURCEA(i));
        if (p) printf("Ordinal %d: %p\n", i, p);
    }

    FreeLibrary(hMod);
    return 0;
}
