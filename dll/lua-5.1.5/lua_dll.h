// lua_dll.h
#pragma once

#ifdef LUA_DLL_EXPORTS
#define LUA_DLL_API __declspec(dllexport)
#else
#define LUA_DLL_API __declspec(dllimport)
#endif

#include <windows.h>
#include <cstdint>

// Shared memory structure for communication
struct LuaSharedMemory {
    static const int MAX_CODE = 4096;
    static const int MAX_OUTPUT = 4096;

    volatile LONG command;      // 0=idle, 1=execute, 2=exit
    volatile LONG done;         // 0=pending, 1=complete
    volatile LONG error;        // 0=ok, 1=error
    volatile LONG cancel;       // 1 = abort the running script (GUI Stop button)
    char code[MAX_CODE];
    char output[MAX_OUTPUT];
};

// Function signatures for game exports
typedef void* (__fastcall *GetLuaInterfaceProxyFn)();
typedef void (__fastcall *CallLuaStringFn)(void* proxy, const char* code);
typedef int (__fastcall *CallLuaStringWithCallbackFn)(void* proxy, const char* code, const char* callback1, const char* callback2);

// Hook types
typedef BOOL (WINAPI *SwapBuffersFn)(HDC hdc);
typedef HRESULT (WINAPI *PresentFn)(void* swapChain, UINT syncInterval, UINT flags);
