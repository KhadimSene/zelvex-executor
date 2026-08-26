// zelvex dll - script executor for Mini World
//
// Design:
//  - Single scripting mode: scripts are parsed and executed directly by this
//    DLL ("native mode"), calling game exports verified from the Cheat Engine
//    reference table. No game Lua VM involvement anywhere.
//  - Runs on the DLL's own worker thread; nothing needs the game's main
//    thread or its script context.
//  - The script language is a small Lua-like subset: native.* calls with
//    result capture, variables, string concatenation (..), if/elseif/else,
//    while, for, break, print and wait.
#include <windows.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdint>
#include <cmath>

// ---- Embedded real Lua 5.1 VM (compiled into this DLL) ----
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "game_overlay.h"

// Ã¢â€â‚¬Ã¢â€â‚¬ Shared Memory Ã¢â€â‚¬Ã¢â€â‚¬
struct LuaSharedMemory {
    static const int MAX_CODE = 16384;
    static const int MAX_OUTPUT = 32768;
    volatile LONG command;
    volatile LONG done;
    volatile LONG error;
    volatile LONG cancel;
    volatile LONG outLen;   // live write cursor - GUI streams partial output
    char code[MAX_CODE];
    char output[MAX_OUTPUT];
};

static HANDLE g_hMapFile = nullptr;
static LuaSharedMemory* g_pShared = nullptr;
static HMODULE g_hModule = nullptr;

static void Log(const char* msg) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\voidcpp\\Documents\\MiniWorld MOD\\dll_log.txt", "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

// Loaded-module snapshot
static const int kMaxModules = 256;
static MODULEENTRY32 g_modules[kMaxModules];
static int g_moduleCount = 0;

static void SnapshotModules() {
    g_moduleCount = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32 me = {};
    me.dwSize = sizeof(me);
    if (Module32First(snap, &me)) {
        do {
            if (g_moduleCount < kMaxModules) g_modules[g_moduleCount++] = me;
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
}

static const char* OwningModuleName(void* p) {
    static char s_buf[64];
    DWORD_PTR addr = (DWORD_PTR)p;
    for (int i = 0; i < g_moduleCount; i++) {
        MODULEENTRY32& m = g_modules[i];
        if (addr >= (DWORD_PTR)m.modBaseAddr && addr < (DWORD_PTR)m.modBaseAddr + m.modBaseSize) {
            snprintf(s_buf, sizeof(s_buf), "%s", m.szModule);
            return s_buf;
        }
    }
    return "?";
}

static void LogRelevantModules() {
    for (int i = 0; i < g_moduleCount; i++) {
        MODULEENTRY32& m = g_modules[i];
        char low[128] = {};
        for (int j = 0; j < 127 && m.szModule[j]; j++)
            low[j] = (char)tolower((unsigned char)m.szModule[j]);
        if (strstr(low, "lua") || strstr(low, "sandbox") || strstr(low, "iworld") ||
            strstr(low, "mini") || strstr(low, "block")) {
            char buf[256] = {};
            snprintf(buf, sizeof(buf), "Module: %s base=%p size=%#x",
                m.szModule, (void*)m.modBaseAddr, m.modBaseSize);
            Log(buf);
        }
    }
}

static int g_outputPos = 0;

static void AppendOutput(const char* s) {
    if (!g_pShared) return;
    int len = (int)strlen(s);
    int space = LuaSharedMemory::MAX_OUTPUT - 1 - g_outputPos;
    if (space <= 0) return;
    if (len > space) len = space;
    memcpy(g_pShared->output + g_outputPos, s, len);
    g_outputPos += len;
    g_pShared->output[g_outputPos] = '\0';
    InterlockedExchange(&g_pShared->outLen, g_outputPos);   // stream cursor
}

// Native API: direct calls into game exports (MW_CT_REFERENCE.CT recipes)
// Bypasses the sandboxed Lua API entirely. Every call replicates an
// Auto-Assembler recipe proven in the CE table / Zelvex cheat_defs.h.

static void* g_sendToHostAddr = nullptr;
static bool  g_sniffOn = false;
static BYTE  g_sniffOrig[5] = {};
static void* g_sniffTramp = nullptr;
static void* g_sniffLogAddr = nullptr;
static void* g_newRepairAddr  = nullptr;
static void* g_discardAddr    = nullptr;
static void* g_sortPackAddr   = nullptr;
static void* g_setItemAddr    = nullptr;
static BYTE* g_pPlayerCtrlVar = nullptr;   // address of exported g_pPlayerCtrl pointer variable
static BYTE* g_uidVar         = nullptr;   // address of exported g_nHomeGardenSaveVersion (libiworld)
static BYTE* g_doMoveStepAddr = nullptr;
static BYTE* g_canPermitAddr  = nullptr;
static void* g_teleportPosAddr  = nullptr; // ClientPlayer::teleportPos(int,int,int)
static BYTE* g_pWorldMgrVar     = nullptr; // address of exported g_WorldMgr pointer variable
static void* g_getTimeInDayAddr = nullptr; // WorldManager::getTimeInDay
static void* g_setDayTimeAddr   = nullptr; // WorldManager::setDayTime
static void* g_setDayTimeSpeedAddr = nullptr; // WorldManager::setDayTimeSpeed
static void* g_isDaytimeAddr    = nullptr; // WorldManager::isMainworldDaytime
static void* g_setSpectatorAddr = nullptr; // PlayerControl::setSpectatorMode
static void* g_getSpectatorAddr = nullptr; // ClientPlayer::getSpectatorMode
static BYTE  g_moveStepSaved[2];
static BYTE  g_canPermitSaved[5];
static bool  g_noclipOn = false;
static bool  g_unlockOn = false;
static BYTE* g_noDropTarget = nullptr;  // dropEquipItems gate (0x75 -> 0x74)
static BYTE  g_noDropSaved[1];
static bool  g_noDropOn = false;
static BYTE* g_jumpFlyTarget = nullptr; // jump-fly gate (0x74 -> 0x70)
static BYTE  g_jumpFlySaved[1];
static bool  g_jumpFlyOn = false;
static BYTE* g_slowFallTarget = nullptr; // slow-fall gate (0F 84 -> 0F 85)
static BYTE  g_slowFallSaved[2];
static bool  g_slowFallOn = false;
static BYTE* g_chatFuncAddr  = nullptr; // libiworld.dll+185060 (chat send)
static BYTE* g_chatCtrlChain = nullptr; // libMiniBaseGame.dll+B36C (chat object chain)
static void* g_reviveAddr    = nullptr; // MpPlayerControl::revive (instant respawn)
static void* g_addStarAddr   = nullptr; // ClientPlayer::AddStar (star currency)
static void* g_onDieAddr     = nullptr; // ClientPlayer::onDie (mass kill)
static void* g_doJumpAddr    = nullptr; // PlayerLocoMotion::doJump (mass dance)
static void* g_roomKickAddr  = nullptr; // libiworld RoomManager::requestRoomKickPlayer
static BYTE* g_hotfixAnchor  = nullptr; // libiworld HOTFIX anchor (+0x20C8 -> RoomManager*)
static void* g_gmChangeSkinAddr = nullptr; // ClientPlayer::GMChangeSkin(int,int,const char*)
static void* g_ptScreenAddr  = nullptr; // PlayerControl::getPointToScreen (W2S!)
static BYTE* g_unlockItemsTarget = nullptr; // lock-flag read site (movd xmm0,[eax+2C])
static BYTE  g_unlockItemsSaved[5];
static BYTE* g_unlockItemsCave = nullptr;
static bool  g_unlockItemsOn = false;
static BYTE* g_hitWallsAddr  = nullptr; // CollisionDetect::intersectRay+0x62
static BYTE  g_hitWallsSaved[1];
static bool  g_hitWallsOn = false;
static BYTE* g_groundSeeAddr = nullptr; // ClientPlayer::getEyeHeight+0x29
static BYTE  g_groundSeeSaved[1];
static bool  g_groundSeeOn = false;
static uintptr_t g_airWallAob  = 0;     // libiworld.dll: 8B 41 2C 89 45 EC (air-wall check)
static uintptr_t g_airWallCave = 0;
static BYTE  g_airWallSaved[5];
static bool  g_airWallOn = false;

// Batch 4: PvP / player control (CT recipes, export-relative + AOB hooks).
static void* g_pickActorAddr      = nullptr; // World::pickActor (kill aura byte at +0x200+1)
static BYTE* g_killAuraTarget     = nullptr; // pickActor+0x201
static BYTE  g_killAuraSaved[1]   = { 0 };
static int   g_killAuraMode       = 0;       // 0=off, 1=aura(85), 2=selfdestruct(8B)
static void* g_rightClickPickAddr = nullptr; // ActionIdleState::doPickActorForRightClickDown
static BYTE  g_mountAllSaved[12];            // 6 bytes @+24F + 6 bytes @tryMountActor+45
static bool  g_mountAllInstalled = false;
static void* g_tryMountAddr       = nullptr; // MpPlayerControl::tryMountActor
static void* g_setOperateAddr     = nullptr; // ClientPlayer::setOperate (mine-all cave)
static uintptr_t g_mineAllAob     = 0;       // setOperate+15 (89 86 8C 03 00 00)
static uintptr_t g_mineAllCave    = 0;
static BYTE  g_mineAllSaved[6];
static bool  g_mineAllOn = false;
static void* g_isDeadAddr         = nullptr; // ClientPlayer::isDead (KillAll host cave)
static uintptr_t g_isDeadTarget   = 0;       // isDead+24
static uintptr_t g_killAllCave    = 0;
static BYTE  g_killAllSaved[8];
static bool  g_killAllHostOn = false;
static uintptr_t g_teleHookAob    = 0;       // libiworld.dll: 39 39 74 11 40 (per-entity loop)
static uintptr_t g_teleHookCave   = 0;
static volatile int  g_teleUid    = 0;       // target uid to capture (0 = off)
static volatile int  g_bringUid   = 0;       // target uid to drag to you (0 = off)
static volatile LONG g_capX = 0, g_capY = 0, g_capZ = 0; // last captured target pos
static volatile int  g_brX = 0, g_brY = 0, g_brZ = 0;    // bring offset adjust
static volatile int  g_capFound = 0;                     // set by cave when capture happened

static bool g_cancelRequested = false; // executor abort flag (see CancelSleep / loops)
static bool g_nativeEcho = true;       // per-call "[cmd] result" console echo (native.echo toggles)

// Batch 5: remote kill + aimbot (CE "Automatic 100-meter killing" / "AutoAim").
static void* g_interactActorAddr = nullptr; // MpPlayerControl::interactActor (attack call)
static void* g_performDigAddr    = nullptr; // PlayerAnimation::performDig (dig/attack animation)
static uintptr_t g_killcallAob   = 0;       // CE "Killcall" internal attack fn (AOB, optional)
static int   g_camLockSaved      = 0;       // [[pc]+950]+58 camera-lock value (killPlayer restores)
static volatile bool g_aimbotOn  = false;
static HANDLE g_aimbotThread     = nullptr;
static float g_aimRange          = 3000.0f; // trigger distance (in coord units, ~30m)

static void* ResolveExport(HMODULE h, const char* const* names, int n) {
    for (int i = 0; i < n; i++) {
        void* p = (void*)GetProcAddress(h, names[i]);
        if (p) return p;
    }
    return nullptr;
}

// Ã¢â€â‚¬Ã¢â€â‚¬ Memory-safety helpers Ã¢â€â‚¬Ã¢â€â‚¬
// Every pointer we deref and every game function we call is range-validated so
// a stale/bad pointer or wrong-typed object yields an ERR result instead of
// crashing the game. Module ranges are resolved once via Toolhelp and cached.

struct ModRange { ULONG_PTR base; SIZE_T size; };

static bool GetModuleRangeCached(const char* name, ModRange& out) {
    static char cachedName[2][64];
    static ModRange cachedR[2];
    static bool loaded[2] = { false, false };
    for (int i = 0; i < 2; i++) {
        if (loaded[i] && strcmp(cachedName[i], name) == 0) { out = cachedR[i]; return cachedR[i].size != 0; }
    }
    ModRange r = { 0, 0 };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me = { sizeof(me) };
        if (Module32First(snap, &me)) {
            do {
                if (_stricmp(me.szModule, name) == 0) {
                    r.base = (ULONG_PTR)me.modBaseAddr;
                    r.size = (SIZE_T)me.modBaseSize;
                    break;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
    }
    for (int i = 0; i < 2; i++) {
        if (!loaded[i]) {
            strncpy(cachedName[i], name, 63);
            cachedName[i][63] = 0;
            cachedR[i] = r;
            loaded[i] = true;
            break;
        }
    }
    out = r;
    return r.size != 0;
}

static bool PtrInModule(const void* p, const char* mod) {
    ModRange r;
    if (!GetModuleRangeCached(mod, r)) return false;
    ULONG_PTR a = (ULONG_PTR)p;
    return a >= r.base && a < r.base + r.size;
}

static bool IsReadable(const void* p, size_t len) {
    if (!p || len == 0) return false;
    ULONG_PTR a = (ULONG_PTR)p;
    if (a + len < a) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & PAGE_GUARD) return false;
    DWORD prot = mbi.Protect & 0xFF;
    if (prot == PAGE_NOACCESS) return false;
    if (prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
        prot == PAGE_EXECUTE || prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE ||
        prot == PAGE_EXECUTE_WRITECOPY) {
        return ((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize) >= a + len;
    }
    return false;
}

// First-match scan inside a loaded module image. mask[i]==0 => wildcard byte.
// Attach-time only; one pass over the image, then parked.
static BYTE* FindBytesMasked(const char* mod, const BYTE* pat, const BYTE* mask, int plen) {
    ModRange r;
    if (!GetModuleRangeCached(mod, r)) return nullptr;
    if (!pat || plen < 1 || plen > 64) return nullptr;
    if (r.size < (ULONG_PTR)plen) return nullptr;
    for (ULONG_PTR a = r.base; a + (ULONG_PTR)plen <= r.base + r.size; a++) {
        const BYTE* q = (const BYTE*)a;
        bool ok = true;
        for (int i = 0; i < plen; i++) {
            if (mask && !mask[i]) continue;
            if (q[i] != pat[i]) { ok = false; break; }
        }
        if (ok) return (BYTE*)a;
    }
    return nullptr;
}

static BYTE* FindBytes(const char* mod, const BYTE* pat, int plen) {
    return FindBytesMasked(mod, pat, nullptr, plen);
}

// Object sanity: memory readable AND its vtable pointer lives inside the game
// engine module - i.e. it really is the class we think it is.
static bool ObjectValid(const void* obj, const char* mod) {
    if (!IsReadable(obj, 4)) return false;
    const void* vt = *(const void* const*)obj;
    return PtrInModule(vt, mod);
}

static bool FnOk(const void* fn) {
    return fn && PtrInModule(fn, "libSandboxEngine.dll");
}

// Defined further below; used here for libiworld live export scans.
static int CollectExports(HMODULE h, std::vector<std::string>& out);

// Persistent Lua VM (created lazily, survives between runs).
static void CloseLuaState();

// Overlay event pump (defined in the gui.* section).
void GuiPumpEvents();

static void ResolveNativeApi() {
    HMODULE se = GetModuleHandleA("libSandboxEngine.dll");
    if (!se) { Log("ResolveNativeApi: libSandboxEngine.dll not loaded"); return; }

    const char* sendToHostNames[]  = { "?sendToHost@SandBoxManager@@QAE_NPBDPAD@Z" };
    const char* newRepairNames[]   = { "?NewRepair@ClientPlayer@@UAEHHHHHHH@Z" };
    const char* discardNames[]     = {
        "?discardItem@MpPlayerControl@@MAEXHH@Z",
        "?discardItem@PlayerControl@@UAEXHH@Z",
        "?discardItem@ClientPlayer@@QAEXHH@Z"
    };
    const char* sortPackNames[]    = { "?sortPack@PlayerControl@@UAEXH@Z" };
    const char* setItemNames[]     = {
        "?setItemWithoutLimit@MpPlayerControl@@MAEXHHHPBD0@Z",
        "?setItem@MpPlayerControl@@MAEXHHHPBD0@Z"
    };
    const char* pcNames[]          = { "?g_pPlayerCtrl@@3PAVPlayerControl@@A" };
    const char* uidNames[]         = { "?g_nHomeGardenSaveVersion@@3GA", "g_nHomeGardenSaveVersion" };
    const char* moveStepNames[]    = { "?doMoveStep@ActorLocoMotion@@UAEHABVVector3f@Rainbow@@@Z" };
    const char* canPermitNames[]   = { "?canPermit@PermitsSubSystem@@QAE_NHHH@Z" };
    const char* teleportNames[]    = { "?teleportPos@ClientPlayer@@QAEXHHH@Z" };
    const char* wmgrNames[]        = { "?g_WorldMgr@@3PAVWorldManager@@A" };
    const char* getTimeNames[]     = { "?getTimeInDay@WorldManager@@QAEHXZ" };
    const char* setTimeNames[]     = { "?setDayTime@WorldManager@@QAEXH@Z" };
    const char* setTimeSpeedNames[] = { "?setDayTimeSpeed@WorldManager@@QAEXH@Z" };
    const char* isDayNames[]       = { "?isMainworldDaytime@WorldManager@@QAE_NXZ" };
    const char* setSpecNames[]     = { "?setSpectatorMode@PlayerControl@@UAEXW4PLAYER_SPECTATOR_MODE@@@Z" };
    const char* getSpecNames[]     = { "?getSpectatorMode@ClientPlayer@@QAE?AW4PLAYER_SPECTATOR_MODE@@XZ" };

    g_sendToHostAddr = ResolveExport(se, sendToHostNames, 1);
    g_newRepairAddr  = ResolveExport(se, newRepairNames, 1);
    g_discardAddr    = ResolveExport(se, discardNames, 3);
    g_sortPackAddr   = ResolveExport(se, sortPackNames, 1);
    g_setItemAddr    = ResolveExport(se, setItemNames, 2);
    g_pPlayerCtrlVar = (BYTE*)ResolveExport(se, pcNames, 1);
    g_uidVar         = (BYTE*)ResolveExport(GetModuleHandleA("libiworld.dll"), uidNames, 1);
    g_doMoveStepAddr = (BYTE*)ResolveExport(se, moveStepNames, 1);
    g_canPermitAddr  = (BYTE*)ResolveExport(se, canPermitNames, 1);
    g_teleportPosAddr  = ResolveExport(se, teleportNames, 1);
    g_pWorldMgrVar     = (BYTE*)ResolveExport(se, wmgrNames, 1);
    g_getTimeInDayAddr = ResolveExport(se, getTimeNames, 1);
    g_setDayTimeAddr   = ResolveExport(se, setTimeNames, 1);
    g_setDayTimeSpeedAddr = ResolveExport(se, setTimeSpeedNames, 1);
    g_isDaytimeAddr    = ResolveExport(se, isDayNames, 1);
    g_setSpectatorAddr = ResolveExport(se, setSpecNames, 1);
    g_getSpectatorAddr = ResolveExport(se, getSpecNames, 1);

    // Phase 2 (CT recipes): mass effects + room kick
    {
        const char* onDieNames[]  = { "?onDie@ClientPlayer@@UAEXXZ" };
        const char* doJumpNames[] = { "?doJump@PlayerLocoMotion@@UAEXXZ" };
        const char* gmSkinNames[] = { "?GMChangeSkin@ClientPlayer@@QAEXHHPBD@Z" };
        const char* ptScrNames[]  = { "?getPointToScreen@PlayerControl@@QAEXAAM00PAVClientActor@@H@Z",
                                      "?getPointToScreen@PlayerControl@@QAEXAAM0HHH@Z" };
        g_onDieAddr  = ResolveExport(se, onDieNames, 1);
        g_doJumpAddr = ResolveExport(se, doJumpNames, 1);
        g_gmChangeSkinAddr = ResolveExport(se, gmSkinNames, 1);
        g_ptScreenAddr     = ResolveExport(se, ptScrNames, 2);

        // libiworld names are absent from our export snapshot - scan live.
        HMODULE iwK = GetModuleHandleA("libiworld.dll");
        if (iwK) {
            std::vector<std::string> ex;
            if (CollectExports(iwK, ex) > 0) {
                for (size_t i = 0; i < ex.size(); i++) {
                    if (!g_roomKickAddr &&
                        ex[i].find("requestRoomKickPlayer") != std::string::npos)
                        g_roomKickAddr = (void*)GetProcAddress(iwK, ex[i].c_str());
                    else if (!g_hotfixAnchor &&
                        ex[i].find("HOTFIX_DOWNLOAD_CACHE_PATH") != std::string::npos)
                        g_hotfixAnchor = (BYTE*)GetProcAddress(iwK, ex[i].c_str());
                }
            }
        }
        char kb[256];
        snprintf(kb, sizeof(kb),
            "ResolveNativeApi: onDie=%p doJump=%p roomKick=%p hotfixAnchor=%p",
            g_onDieAddr, g_doJumpAddr, g_roomKickAddr, g_hotfixAnchor);
        Log(kb);
    }

    // CE-recipe addresses (MW_CT_REFERENCE.CT): chat send lives at a fixed
    // offset in libiworld.dll; the chat object is a 3-level pointer chain.
    HMODULE iwH = GetModuleHandleA("libiworld.dll");
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (iwH) g_chatFuncAddr = (BYTE*)iwH + 0x185060;
    if (mbH) g_chatCtrlChain = (BYTE*)mbH + 0xB36C;
    // dropEquipItems gate: "jnz x" (0x75 0x1C) -> nop the branch = keep items on death
    static const BYTE noDropPat[9] = { 0x75, 0x1C, 0x8B, 0x4E, 0x50, 0x03, 0xCB, 0xE8, 0x73 };
    g_noDropTarget = FindBytes("libSandboxEngine.dll", noDropPat, 9);
    // jump fly gate (CE "Nháº£y Bay"): 74 ?? 8B 06 8B CE FF 90 ?? ?? ?? ?? 8B 45 ?? 5F
    static const BYTE jfPat[16] = { 0x74, 0x00, 0x8B, 0x06, 0x8B, 0xCE, 0xFF, 0x90,
                                    0x00, 0x00, 0x00, 0x00, 0x8B, 0x45, 0x00, 0x5F };
    static const BYTE jfMsk[16] = { 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF };
    g_jumpFlyTarget = FindBytesMasked("libSandboxEngine.dll", jfPat, jfMsk, 16);
    // slow fall gate (CE "Nháº£y Trá»ng Lá»±c"): 0F 84 4D 01 00 00 8B 93
    static const BYTE sfPat[8] = { 0x0F, 0x84, 0x4D, 0x01, 0x00, 0x00, 0x8B, 0x93 };
    g_slowFallTarget = FindBytes("libSandboxEngine.dll", sfPat, 8);

    // Batch 3 (named exports - version safe): instant respawn, star currency,
    // attack through walls, see through the ground, see through air walls.
    const char* reviveNames[]  = {
        "?revive@MpPlayerControl@@MAE_NHHHH@Z",
        "?revive@PlayerControl@@UAE_NHHHH@Z",
        "?revive@ClientPlayer@@UAE_NHHHH@Z"
    };
    const char* addStarNames[] = { "?AddStar@ClientPlayer@@UAEXH@Z" };
    const char* hitWallNames[] = { "?intersectRay@CollisionDetect@@QAEHABVVector3f@Rainbow@@0PAM@Z" };
    const char* eyeHNames[]    = { "?getEyeHeight@ClientPlayer@@UAEHXZ" };
    g_reviveAddr  = ResolveExport(se, reviveNames, 3);
    g_addStarAddr = ResolveExport(se, addStarNames, 1);
    {
        BYTE* ir = (BYTE*)ResolveExport(se, hitWallNames, 1);
        g_hitWallsAddr = ir ? ir + 0x62 : nullptr;
    }
    {
        BYTE* eh = (BYTE*)ResolveExport(se, eyeHNames, 1);
        g_groundSeeAddr = eh ? eh + 0x29 : nullptr;
    }
    // Air walls see-through (CE "TÆ°á»ng KhÃ´ng KhÃ­ CÃ³ Thá»ƒ NhÃ¬n XuyÃªn Tháº³ng"):
    // 8B 41 2C 89 45 EC in libiworld.dll; hook writes #1018 into [ecx+2C].
    static const BYTE airPat[5] = { 0x8B, 0x41, 0x2C, 0x89, 0x45 };
    g_airWallAob = (uintptr_t)FindBytes("libiworld.dll", airPat, 5);

    // Batch 4 (PvP / player control):
    // Kill aura: World::pickActor+0x201 byte swap (84=off, 85=aura, 8B=incl. self).
    const char* pickNames[]  = { "?pickActor@World@@QAEPAVClientActor@@ABVWorldRay@MINIW@@AAVActorExcludes@@PAM_N3@Z" };
    const char* rclickNames[] = { "?doPickActorForRightClickDown@ActionIdleState@@IAEPBDAA_N@Z" };
    const char* mountNames[] = { "?tryMountActor@MpPlayerControl@@MAEXPAVClientActor@@F@Z" };
    const char* operateNames[] = { "?setOperate@ClientPlayer@@QAEXHHH_J@Z" };
    const char* isDeadNames[] = { "?isDead@ClientPlayer@@UAE_NXZ" };
    g_pickActorAddr      = ResolveExport(se, pickNames, 1);
    g_killAuraTarget     = g_pickActorAddr ? (BYTE*)g_pickActorAddr + 0x201 : nullptr;
    g_rightClickPickAddr = ResolveExport(se, rclickNames, 1);
    g_tryMountAddr       = ResolveExport(se, mountNames, 1);
    g_setOperateAddr     = ResolveExport(se, operateNames, 1);
    g_isDeadAddr         = ResolveExport(se, isDeadNames, 1);
    if (g_setOperateAddr) g_mineAllAob = (uintptr_t)g_setOperateAddr + 0x15;
    if (g_isDeadAddr)     g_isDeadTarget = (uintptr_t)g_isDeadAddr + 0x24;
    // Teleport-to-player hook: per-entity loop in libiworld.dll (CE "Teleport PlayerID").
    static const BYTE telePat[5] = { 0x39, 0x39, 0x74, 0x11, 0x40 };
    g_teleHookAob = (uintptr_t)FindBytes("libiworld.dll", telePat, 5);

    // Batch 5: remote kill pieces (CE "Automatic 100-meter killing" / "Killcall").
    const char* interactNames[] = { "?interactActor@MpPlayerControl@@MAE_NPAVClientActor@@H_N@Z" };
    const char* digNames[]      = { "?performDig@PlayerAnimation@@QAEXW4DIG_METHOD_T@@@Z" };
    g_interactActorAddr = ResolveExport(se, interactNames, 1);
    g_performDigAddr    = ResolveExport(se, digNames, 1);
    // CE "Killcall" internal attack function prologue (Win32 SEH frame); optional mode 2.
    static const BYTE killPat[] = {
        0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x00, 0x00, 0x00, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
        0x50, 0x83, 0xEC, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x33, 0xC5, 0x89, 0x45, 0x00, 0x53, 0x56,
        0x57, 0x50, 0x8D, 0x45, 0x00, 0x64, 0xA3, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xF1, 0x8B, 0x5D, 0x00,
        0x8B, 0x7D, 0x00, 0x53
    };
    static const BYTE killMsk[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
        0xFF, 0xFF, 0x00, 0xFF
    };
    g_killcallAob = (uintptr_t)FindBytesMasked("libSandboxEngine.dll", killPat, killMsk,
        (int)sizeof(killPat));

    char buf[1000] = {};

    snprintf(buf, sizeof(buf),
        "ResolveNativeApi: sendToHost=%p newRepair=%p discard=%p sortPack=%p setItem=%p "
        "g_pPlayerCtrlVar=%p uidVar=%p doMoveStep=%p canPermit=%p",
        g_sendToHostAddr, g_newRepairAddr, g_discardAddr, g_sortPackAddr, g_setItemAddr,
        (void*)g_pPlayerCtrlVar, (void*)g_uidVar, (void*)g_doMoveStepAddr, (void*)g_canPermitAddr);
    Log(buf);
snprintf(buf, sizeof(buf),
        "ResolveNativeApi: teleportPos=%p g_WorldMgrVar=%p getTimeInDay=%p setDayTime=%p "
        "setDayTimeSpeed=%p isMainworldDaytime=%p setSpectatorMode=%p getSpectatorMode=%p",
        g_teleportPosAddr, (void*)g_pWorldMgrVar, g_getTimeInDayAddr, g_setDayTimeAddr,
        g_setDayTimeSpeedAddr, g_isDaytimeAddr, g_setSpectatorAddr, g_getSpectatorAddr);
    Log(buf);
    snprintf(buf, sizeof(buf),
        "ResolveNativeApi: chatFunc=%p chatCtrlChain=%p noDropTarget=%p jumpFlyTarget=%p slowFallTarget=%p",
        (void*)g_chatFuncAddr, (void*)g_chatCtrlChain, (void*)g_noDropTarget,
        (void*)g_jumpFlyTarget, (void*)g_slowFallTarget);
    Log(buf);
    snprintf(buf, sizeof(buf),
        "ResolveNativeApi: revive=%p addStar=%p hitWalls=%p groundSee=%p airWallAob=%p",
        g_reviveAddr, g_addStarAddr, (void*)g_hitWallsAddr, (void*)g_groundSeeAddr, (void*)g_airWallAob);
    Log(buf);
    snprintf(buf, sizeof(buf),
        "ResolveNativeApi: pickActor=%p mountAll=%p mineAllAob=%p isDeadTgt=%p teleHookAob=%p",
        g_pickActorAddr, g_tryMountAddr, (void*)g_mineAllAob, (void*)g_isDeadTarget, (void*)g_teleHookAob);
    Log(buf);
    snprintf(buf, sizeof(buf),
        "ResolveNativeApi: interactActor=%p performDig=%p killcallAob=%p",
        g_interactActorAddr, g_performDigAddr, (void*)g_killcallAob);
    Log(buf);

    struct { void* p; const char* n; const char* m; } chk[] = {
        { g_sendToHostAddr, "sendToHost", "libSandboxEngine.dll" },
        { g_newRepairAddr, "newRepair", "libSandboxEngine.dll" },
        { g_discardAddr, "discard", "libSandboxEngine.dll" },
        { g_sortPackAddr, "sortPack", "libSandboxEngine.dll" },
        { g_setItemAddr, "setItem", "libSandboxEngine.dll" },
        { (void*)g_doMoveStepAddr, "doMoveStep", "libSandboxEngine.dll" },
        { (void*)g_canPermitAddr, "canPermit", "libSandboxEngine.dll" },
        { g_teleportPosAddr, "teleportPos", "libSandboxEngine.dll" },
        { (void*)g_pWorldMgrVar, "g_WorldMgrVar", "libSandboxEngine.dll" },
        { g_getTimeInDayAddr, "getTimeInDay", "libSandboxEngine.dll" },
        { g_setDayTimeAddr, "setDayTime", "libSandboxEngine.dll" },
        { g_setDayTimeSpeedAddr, "setDayTimeSpeed", "libSandboxEngine.dll" },
        { g_isDaytimeAddr, "isMainworldDaytime", "libSandboxEngine.dll" },
        { g_setSpectatorAddr, "setSpectatorMode", "libSandboxEngine.dll" },
        { g_getSpectatorAddr, "getSpectatorMode", "libSandboxEngine.dll" },
{ (void*)g_pPlayerCtrlVar, "g_pPlayerCtrlVar", "libSandboxEngine.dll" },
        { (void*)g_uidVar, "uidVar", "libiworld.dll" },
        { (void*)g_chatFuncAddr, "chatFunc", "libiworld.dll" },
        { (void*)g_chatCtrlChain, "chatCtrlChain", "libMiniBaseGame.dll" },
        { (void*)g_noDropTarget, "noDropTarget", "libSandboxEngine.dll" },
        { (void*)g_jumpFlyTarget, "jumpFlyTarget", "libSandboxEngine.dll" },
        { (void*)g_slowFallTarget, "slowFallTarget", "libSandboxEngine.dll" },
        { g_reviveAddr, "revive", "libSandboxEngine.dll" },
        { g_addStarAddr, "addStar", "libSandboxEngine.dll" },
        { (void*)g_hitWallsAddr, "hitWallsAddr", "libSandboxEngine.dll" },
        { (void*)g_groundSeeAddr, "groundSeeAddr", "libSandboxEngine.dll" },
        { (void*)g_airWallAob, "airWallAob", "libiworld.dll" },
    };
    for (int i = 0; i < (int)(sizeof(chk) / sizeof(chk[0])); i++) {
        if (chk[i].p && !PtrInModule(chk[i].p, chk[i].m)) {
            char w[192] = {};
            snprintf(w, sizeof(w), "ResolveNativeApi: WARNING %s=%p NOT in %s", chk[i].n, chk[i].p, chk[i].m);
            Log(w);
        }
    }
}

static void* GetPlayer() {
    if (!g_pPlayerCtrlVar || !IsReadable(g_pPlayerCtrlVar, 4)) return nullptr;
    void* p = *(void**)g_pPlayerCtrlVar;
    if (!ObjectValid(p, "libSandboxEngine.dll")) return nullptr;
    return p;
}

static void* GetWorldMgr() {
    if (!g_pWorldMgrVar || !IsReadable(g_pWorldMgrVar, 4)) return nullptr;
    void* wm = *(void**)g_pWorldMgrVar;
    if (!wm) return nullptr;
    if (ObjectValid(wm, "libSandboxEngine.dll")) return wm;
    // WorldManager appears to be non-polymorphic (no vtable at offset 0),
    // so ObjectValid rejects a perfectly good instance. Accept any readable
    // non-null pointer that is not pointing into a code-only region.
    if (IsReadable(wm, 8)) return wm;
    return nullptr;
}

static int* BlockCoord(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x270, 4)) return nullptr;
    void* pos = *(void**)((BYTE*)p + 0x270);
    if (!IsReadable((BYTE*)pos + off, 4)) return nullptr;
    return (int*)((BYTE*)pos + off);
}

static float* AimFloat(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x950, 4)) return nullptr;
    void* aim = *(void**)((BYTE*)p + 0x950);
    if (!IsReadable((BYTE*)aim + off, 4)) return nullptr;
    return (float*)((BYTE*)aim + off);
}

static float* AttrField(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x280, 4)) return nullptr;
    void* a = *(void**)((BYTE*)p + 0x280);
    if (!IsReadable((BYTE*)a + off, 4)) return nullptr;
    return (float*)((BYTE*)a + off);
}

// [g_pPlayerCtrl]+0x56C alternate attr object: jump height (+DC), held slot (+248).
static float* AltAttrFloat(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x56C, 4)) return nullptr;
    void* a = *(void**)((BYTE*)p + 0x56C);
    if (!IsReadable((BYTE*)a + off, 4)) return nullptr;
    return (float*)((BYTE*)a + off);
}

static int* AltAttrInt(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x56C, 4)) return nullptr;
    void* a = *(void**)((BYTE*)p + 0x56C);
    if (!IsReadable((BYTE*)a + off, 4)) return nullptr;
    return (int*)((BYTE*)a + off);
}

// [g_pPlayerCtrl]+0x194 chain: player-visible state (emotion at +35C).
static int* EmoteInt(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x194, 4)) return nullptr;
    void* a = *(void**)((BYTE*)p + 0x194);
    if (!IsReadable((BYTE*)a + off, 4)) return nullptr;
    return (int*)((BYTE*)a + off);
}

// Plain int at [g_pPlayerCtrl]+off (held item id +9D0, scale +338).
static int* PlayerInt(int off) {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + off, 4)) return nullptr;
    return (int*)((BYTE*)p + off);
}

static int* WorldInt(int off) {
    void* wm = GetWorldMgr();
    if (!wm || !IsReadable((BYTE*)wm + off, 4)) return nullptr;
    return (int*)((BYTE*)wm + off);
}

static int* CondPtr() {
    void* p = GetPlayer();
    if (!p || !IsReadable((BYTE*)p + 0x23C, 4)) return nullptr;
    return (int*)((BYTE*)p + 0x23C);
}

// thiscall: args pushed right-to-left, ecx=self, callee cleans the stack.
// GCC inline asm replicates the CE scripts exactly (no esp adjust after call).
static int CallRaw2(void* fn, const char* a1, const char* a2) {
    int ret = 0;
    __asm__ __volatile__(
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        "movl %%eax, %0\n\t"
        : "=r"(ret), "+a"(fn)
        : "g"(a1), "g"(a2)
        : "ecx", "edx", "memory");
    return ret;
}

// ---- sniff: log every sendToHost msg+payload (for name-change probe) ----
static void SniffLog(const char* msg, const char* json) {
    if (!g_sniffOn || !msg || !msg[0]) return;
    char jbuf[420] = {};
    if (json) strncpy(jbuf, json, sizeof(jbuf)-1);
    // single line, truncated
    char out[600];
    snprintf(out, sizeof(out), "[sniff] %s | %.400s\n", msg, jbuf);
    AppendOutput(out);
}

__attribute__((naked)) static void HookSendToHost() {
    __asm__ volatile(
        "pushal\n\t"
        "movl 36(%%esp), %%eax\n\t"  // msg  (orig [esp+4] -> [esp+36] after pushal)
        "movl 40(%%esp), %%edx\n\t"  // json (orig [esp+8] -> [esp+40])
        "pushl %%edx\n\t"
        "pushl %%eax\n\t"
        "call *%1\n\t"
        "addl $8, %%esp\n\t"
        "popal\n\t"
        "jmp *%0\n\t"
        : : "m"(g_sniffTramp), "m"(g_sniffLogAddr) : "memory"
    );
}

static bool InstallSniff() {
    if (g_sniffTramp) return true;
    g_sniffLogAddr = (void*)SniffLog;
    if (!FnOk(g_sendToHostAddr)) return false;
    DWORD old = 0;
    if (!VirtualProtect(g_sendToHostAddr, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(g_sniffOrig, g_sendToHostAddr, 5);
    g_sniffTramp = VirtualAlloc(NULL, 16, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_sniffTramp) { VirtualProtect(g_sendToHostAddr, 5, old, &old); return false; }
    memcpy(g_sniffTramp, g_sniffOrig, 5);
    BYTE* p = (BYTE*)g_sniffTramp + 5;
    p[0] = 0xE9;
    *(DWORD*)(p+1) = (DWORD)((BYTE*)g_sendToHostAddr + 5 - (p+5));
    BYTE* src = (BYTE*)g_sendToHostAddr;
    src[0] = 0xE9;
    *(DWORD*)(src+1) = (DWORD)((BYTE*)HookSendToHost - src - 5);
    DWORD tmp; VirtualProtect(g_sendToHostAddr, 5, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), g_sendToHostAddr, 5);
    FlushInstructionCache(GetCurrentProcess(), g_sniffTramp, 10);
    return true;
}

static void CallP0(void* fn, void* self) {
    __asm__ __volatile__(
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        :
        : "edx", "memory");
}

static void CallP1(void* fn, void* self, int a1) {
    __asm__ __volatile__(
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1)
        : "edx", "memory");
}

static void CallP2(void* fn, void* self, int a1, int a2) {
    __asm__ __volatile__(
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2)
        : "edx", "memory");
}

static void CallP3(void* fn, void* self, int a1, int a2, int a3) {
    __asm__ __volatile__(
        "pushl %4\n\t"
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2), "g"(a3)
        : "edx", "memory");
}

static void CallP5(void* fn, void* self, int a1, int a2, int a3, int a4, int a5) {
    __asm__ __volatile__(
        "pushl %6\n\t"
        "pushl %5\n\t"
        "pushl %4\n\t"
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5)
        : "edx", "memory");
}

static void CallP4(void* fn, void* self, int a1, int a2, int a3, int a4) {
    __asm__ __volatile__(
        "pushl %5\n\t"
        "pushl %4\n\t"
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2), "g"(a3), "g"(a4)
        : "edx", "memory");
}

static void CallP6(void* fn, void* self, int a1, int a2, int a3, int a4, int a5, int a6) {
    __asm__ __volatile__(
        "pushl %7\n\t"
        "pushl %6\n\t"
        "pushl %5\n\t"
        "pushl %4\n\t"
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5), "g"(a6)
        : "edx", "memory");
}

// thiscall with a string first arg (chat send): (str, showname, 0, 1, 0).
static void CallP5S(void* fn, void* self, const char* a1, int a2, int a3, int a4, int a5) {
    __asm__ __volatile__(
        "pushl %6\n\t"
        "pushl %5\n\t"
        "pushl %4\n\t"
        "pushl %3\n\t"
        "pushl %2\n\t"
        "call *%%eax\n\t"
        : "+a"(fn), "+c"(self)
        : "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5)
        : "edx", "memory");
}

// Chat object: [[[libMiniBaseGame.dll+B36C]+78]+4] per the CE recipe.
static void* GetChatObj() {
    if (!g_chatCtrlChain || !IsReadable(g_chatCtrlChain, 4)) return nullptr;
    BYTE* c1 = *(BYTE**)g_chatCtrlChain;
    if (!IsReadable(c1, 0x80)) return nullptr;
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!IsReadable(c2, 0x10)) return nullptr;
    return c2 + 4;
}

// Generic thiscall: pushes args[0..n-1] right-to-left, ecx=self, result in EAX.
// Handles 0..8 args; pads nothing (callee pops exactly what it declared).
static int CallN(void* fn, void* self, const int* args, int n) {
    if (n < 0) n = 0;
    if (n > 8) n = 8;
    int ret = 0;
    if (n == 0) {
        __asm__ __volatile__(
            "call *%%eax\n\t"
            "movl %%eax, %0\n\t"
            : "=r"(ret)
            : "a"(fn), "c"(self)
            : "edx", "memory");
        return ret;
    }
    __asm__ __volatile__(
        "movl %3, %%esi\n\t"
        "movl %4, %%ebx\n\t"
        "1:\n\t"
        "  decl %%ebx\n\t"
        "  pushl (%%esi,%%ebx,4)\n\t"
        "  jnz 1b\n\t"
        "  call *%%eax\n\t"
        "  movl %%eax, %0\n\t"
        : "=r"(ret)
        : "a"(fn), "c"(self), "S"(args), "b"(n)
        : "edx", "memory");
    return ret;
}

static void PatchMem(BYTE* addr, const BYTE* bytes, int len) {
    DWORD oldProt = 0;
    VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(addr, bytes, len);
    VirtualProtect(addr, len, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), addr, len);
}

// â”€â”€ Batch 4 helpers: rel32 trampoline emitters (caves) â”€â”€
static void EmitJmp(BYTE*& c, uintptr_t dest) {
    int64_t disp = (int64_t)dest - ((int64_t)(uintptr_t)c + 5);
    *c++ = 0xE9;
    *(int32_t*)c = (int32_t)disp;
    c += 4;
}

static void EmitJcc(BYTE*& c, int cc8, uintptr_t dest) {
    int64_t disp = (int64_t)dest - ((int64_t)(uintptr_t)c + 6);
    *c++ = 0x0F;
    *c++ = 0x80 | cc8;
    *(int32_t*)c = (int32_t)disp;
    c += 4;
}

// Teleport-to-player hook (CE "Teleport PlayerID"): libiworld.dll `39 39 74 11 40`
// per-entity loop. When the entity uid matches g_teleUid its pos (int32 coords,
// same units as our teleport command) is captured; when it matches g_bringUid the
// target is dragged next to the local player (+offset). pushad/popad preserves the
// loop's register state; the original 5 bytes are replayed at the end.
static bool InstallTeleHook() {
    if (!g_teleHookAob || g_teleHookCave) return g_teleHookCave != 0;
    g_teleHookCave = (uintptr_t)VirtualAlloc(nullptr, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_teleHookCave) return false;
    uintptr_t inj = g_teleHookAob;
    BYTE* c = (BYTE*)g_teleHookCave;

    BYTE* f_jne1 = nullptr; // capture skip   (jne nottarget)
    BYTE* f_je2  = nullptr; // bring branch    (je doBring)
    BYTE* f_jmp3 = nullptr; // skip bring      (jmp doneBring)
    BYTE* f_je4  = nullptr; // ctrl null       (je doneBring)
    BYTE* f_je5  = nullptr; // entity null     (je doneBring)
    BYTE* f_je6  = nullptr; // original je     (je inj+0x13)
    BYTE* f_jmp7 = nullptr; // trampoline back (jmp inj+5)
    uintptr_t lab_notTarget = 0, lab_doBring = 0, lab_doneBring = 0, lab_skipInc = 0;

    auto putdd = [&](intptr_t v) { *(int32_t*)c = (int32_t)v; c += 4; };
    auto apJcc = [&](BYTE*& slot, int cc8) {
        slot = c;
        c += 6; // 0F 8x + rel32 (patched at the end)
        (void)cc8;
    };
    auto apJmp = [&](BYTE*& slot) { slot = c; c += 5; };

    *c++ = 0x60;                                       // pushad
    *c++ = 0x8B; *c++ = 0x35; putdd((intptr_t)&g_teleUid); // mov esi,[g_teleUid]
    *c++ = 0x39; *c++ = 0x31;                          // cmp [ecx],esi
    apJcc(f_jne1, 0x85);                               // jne nottarget
    *c++ = 0x8B; *c++ = 0x71; *c++ = 0x14;             // mov esi,[ecx+14]
    *c++ = 0x89; *c++ = 0x35; putdd((intptr_t)&g_capX);    // mov [g_capX],esi
    *c++ = 0x8B; *c++ = 0x71; *c++ = 0x18;             // mov esi,[ecx+18]
    *c++ = 0x89; *c++ = 0x35; putdd((intptr_t)&g_capY);    // mov [g_capY],esi
    *c++ = 0x8B; *c++ = 0x71; *c++ = 0x1C;             // mov esi,[ecx+1C]
    *c++ = 0x89; *c++ = 0x35; putdd((intptr_t)&g_capZ);    // mov [g_capZ],esi
    *c++ = 0xC7; *c++ = 0x05; putdd((intptr_t)&g_capFound); *c++ = 1; *c++ = 0; *c++ = 0; *c++ = 0; // mov [g_capFound],1
    lab_notTarget = (uintptr_t)c;                      // nottarget:
    *c++ = 0x8B; *c++ = 0x35; putdd((intptr_t)&g_bringUid); // mov esi,[g_bringUid]
    *c++ = 0x39; *c++ = 0x31;                          // cmp [ecx],esi
    apJcc(f_je2, 0x84);                                // je doBring
    apJmp(f_jmp3);                                     // jmp doneBring
    lab_doBring = (uintptr_t)c;                        // doBring:
    *c++ = 0x8B; *c++ = 0x35; putdd((intptr_t)&g_pPlayerCtrlVar); // mov esi,[&g_pPlayerCtrlVar]
    *c++ = 0x8B; *c++ = 0x36;                          // mov esi,[esi]   (player ctrl)
    *c++ = 0x85; *c++ = 0xF6;                          // test esi,esi
    apJcc(f_je4, 0x84);                                // je doneBring
    *c++ = 0x8B; *c++ = 0xB6; *c++ = 0x70; *c++ = 0x02; *c++ = 0x00; *c++ = 0x00; // mov esi,[esi+0x270]
    *c++ = 0x85; *c++ = 0xF6;                          // test esi,esi
    apJcc(f_je5, 0x84);                                // je doneBring
    *c++ = 0x8B; *c++ = 0xBE; *c++ = 0xDC; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; // mov edi,[esi+0xDC]
    *c++ = 0x03; *c++ = 0x3D; putdd((intptr_t)&g_brX);      // add edi,[g_brX]
    *c++ = 0x89; *c++ = 0x79; *c++ = 0x14;             // mov [ecx+14],edi
    *c++ = 0x8B; *c++ = 0xBE; *c++ = 0xE0; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; // mov edi,[esi+0xE0]
    *c++ = 0x03; *c++ = 0x3D; putdd((intptr_t)&g_brY);      // add edi,[g_brY]
    *c++ = 0x89; *c++ = 0x79; *c++ = 0x18;             // mov [ecx+18],edi
    *c++ = 0x8B; *c++ = 0xBE; *c++ = 0xE4; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; // mov edi,[esi+0xE4]
    *c++ = 0x03; *c++ = 0x3D; putdd((intptr_t)&g_brZ);      // add edi,[g_brZ]
    *c++ = 0x89; *c++ = 0x79; *c++ = 0x1C;             // mov [ecx+1C],edi
    lab_doneBring = (uintptr_t)c;                      // doneBring:
    *c++ = 0x61;                                       // popad
    *c++ = 0x39; *c++ = 0x39;                          // cmp [ecx],edi   (original bytes)
    apJcc(f_je6, 0x84);                                // je inj+0x13
    *c++ = 0x40;                                       // inc eax         (original)
    lab_skipInc = (uintptr_t)c;                        // skipInc:
    apJmp(f_jmp7);                                     // jmp inj+5

    // Resolve all forward references now that labels are known.
    auto relJcc = [](BYTE* at, int cc8, uintptr_t dest) {
        at[0] = 0x0F; at[1] = 0x80 | cc8;
        *(int32_t*)(at + 2) = (int32_t)((int64_t)dest - ((int64_t)(uintptr_t)at + 6));
    };
    auto relJmp = [](BYTE* at, uintptr_t dest) {
        at[0] = 0xE9;
        *(int32_t*)(at + 1) = (int32_t)((int64_t)dest - ((int64_t)(uintptr_t)at + 5));
    };
    relJcc(f_jne1, 0x85, lab_notTarget);
    relJcc(f_je2,  0x84, lab_doBring);
    relJmp(f_jmp3, lab_doneBring);
    relJcc(f_je4,  0x84, lab_doneBring);
    relJcc(f_je5,  0x84, lab_doneBring);
    relJcc(f_je6,  0x84, inj + 0x13);
    relJmp(f_jmp7, inj + 5);

    BYTE jmpCave[5] = { 0xE9, 0, 0, 0, 0 };
    *(int32_t*)(jmpCave + 1) = (int32_t)((int64_t)g_teleHookCave - ((int64_t)inj + 5));
    PatchMem((BYTE*)inj, jmpCave, 5);
    return true;
}

// KillAll (Host) cave (CE "KillAll (Host)"): patch ClientPlayer::isDead+24 so that
// every isDead() check compares against a forced 1 in [eax+94] instead of the real
// value - exactly what the CT script does.
static bool InstallKillAllCave() {
    if (!g_isDeadTarget || g_killAllCave) return g_killAllCave != 0;
    g_killAllCave = (uintptr_t)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_killAllCave) return false;
    if (!g_killAllSaved[0]) memcpy(g_killAllSaved, (void*)g_isDeadTarget, 8);
    BYTE* c = (BYTE*)g_killAllCave;
    static const BYTE setOne[] = { 0xC7, 0x40, 0x94, 0x01, 0x00, 0x00, 0x00 }; // mov dword[eax+94],1
    memcpy(c, setOne, sizeof(setOne)); c += sizeof(setOne);
    static const BYTE orig8[] = { 0xF3, 0x0F, 0x10, 0x88, 0x94, 0x00, 0x00, 0x00 }; // movss xmm1,[eax+94]
    memcpy(c, orig8, sizeof(orig8)); c += sizeof(orig8);
    EmitJmp(c, g_isDeadTarget + 8);
    BYTE jmpCave[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 }; // jmp + 3 nops = 8 bytes
    *(int32_t*)(jmpCave + 1) = (int32_t)((int64_t)g_killAllCave - ((int64_t)g_isDeadTarget + 5));
    PatchMem((BYTE*)g_isDeadTarget, jmpCave, 8);
    return true;
}

// Mine-all cave (CE "ÄÃ o Táº¥t Cáº£ Khá»‘i"): setOperate+15, mov [esi+38C],eax -> eax=#100.
static bool InstallMineAllCave() {
    if (!g_mineAllAob || g_mineAllCave) return g_mineAllCave != 0;
    g_mineAllCave = (uintptr_t)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_mineAllCave) return false;
    if (!g_mineAllSaved[0]) memcpy(g_mineAllSaved, (void*)g_mineAllAob, 6);
    BYTE* c = (BYTE*)g_mineAllCave;
    static const BYTE set100[] = { 0xB8, 0x64, 0x00, 0x00, 0x00 }; // mov eax,#100
    memcpy(c, set100, sizeof(set100)); c += sizeof(set100);
    static const BYTE orig6[] = { 0x89, 0x86, 0x8C, 0x03, 0x00, 0x00 }; // mov [esi+38C],eax
    memcpy(c, orig6, sizeof(orig6)); c += sizeof(orig6);
    EmitJmp(c, g_mineAllAob + 6);
    BYTE jmpCave[6] = { 0xE9, 0, 0, 0, 0, 0x90 }; // jmp + nop = 6 bytes
    *(int32_t*)(jmpCave + 1) = (int32_t)((int64_t)g_mineAllCave - ((int64_t)g_mineAllAob + 5));
    PatchMem((BYTE*)g_mineAllAob, jmpCave, 6);
    return true;
}

// UID read: libiworld.g_nHomeGardenSaveVersion + 0x5754 -> ptr -> +8
static uint32_t ReadRoleId() {
    if (!g_uidVar || !IsReadable(g_uidVar + 0x5754, 4)) return 0;
    uint32_t ptr = *(uint32_t*)(g_uidVar + 0x5754);
    if (!ptr || !IsReadable((void*)(uintptr_t)ptr + 8, 4)) return 0;
    return *(uint32_t*)((uintptr_t)ptr + 8);
}

struct GiveItemJob {
    std::string msg;
    std::string json;
};

// Aim bot worker (CE "AutoAim to Closest Player"): scans the in-memory player
// list [[libMiniBaseGame.dll+B36C]+78]+68 every ~100ms and aims at the nearest
// non-referee player by writing yaw/pitch floats into [[g_pPlayerCtrl]+950]+4/+8.
static DWORD WINAPI AimbotThread(LPVOID) {
    Log("AimbotThread: start");
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    while (g_aimbotOn) {
        void* pc = GetPlayer();
        if (pc && mbH && IsReadable((BYTE*)mbH + 0xB36C, 4)) {
            BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
            if (IsReadable(c1, 0x80) && IsReadable(c1 + 0x78, 4)) {
                BYTE* c2 = *(BYTE**)(c1 + 0x78);
                if (IsReadable(c2, 0x70) && IsReadable(c2 + 0x68, 4)) {
                    BYTE* list = *(BYTE**)(c2 + 0x68);
                    int mx = 0, my = 0, mz = 0;
                    int* bx = BlockCoord(0xDC); if (bx) mx = *bx;
                    int* by = BlockCoord(0xE0); if (by) my = *by;
                    int* bz = BlockCoord(0xE4); if (bz) mz = *bz;
                    uint32_t myUid = ReadRoleId();
                    BYTE* aim = nullptr;
                    if (IsReadable((BYTE*)pc + 0x950, 4)) aim = *(BYTE**)((BYTE*)pc + 0x950);
                    double bestD = (double)g_aimRange;
                    int tx = 0, ty = 0, tz = 0;
                    for (int i = 0; i < 40; i++) {
                        BYTE* p = nullptr;
                        if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
                        p = *(BYTE**)((BYTE*)list + i * 4);
                        if (!p || !IsReadable(p, 0xC0)) continue;
                        uint32_t uid = *(uint32_t*)p;
                        if (uid == 0 || uid == myUid) continue;
                        int fac = *(int*)(p + 0xB0);
                        if (fac < 1 || fac > 3) continue; // skip referees/empty (CE filter)
                        int px = *(int*)(p + 0x14), py = *(int*)(p + 0x18), pz = *(int*)(p + 0x1C);
                        double dx = px - mx, dy = py - my, dz = pz - mz;
                        double d = dx * dx + dy * dy + dz * dz;
                        if (d < bestD) {
                            bestD = d;
                            tx = px; ty = py; tz = pz;
                        }
                    }
                    if (aim && bestD < (double)g_aimRange && IsReadable(aim + 4, 8)) {
                        double dx = tx - mx;
                        double dy = (double)my - ty + 75; // CE formula (eye-level nudge)
                        double dz = tz - mz;
                        double dist2 = sqrt(dx * dx + dz * dz);
                        float yaw = (float)(atan2(dx, dz) * 180.0 / 3.14159265358979323846);
                        float pitch = (float)(atan2(dy, dist2) * 180.0 / 3.14159265358979323846);
                        *(float*)(aim + 4) = yaw;
                        *(float*)(aim + 8) = pitch;
                    }
                }
            }
        }
        for (int i = 0; i < 4 && g_aimbotOn; i++) Sleep(25);
    }
    Log("AimbotThread: stop");
    return 0;
}

// Remote kill (CE "Automatic 100-meter killing" / "Killcall"): server-synced
// teleport onto the target, fire the attack call, play the dig animation,
// teleport back and restore the camera lock. Fully export/AOB based.
static int KillPlayerByUid(int uid, int mode, char* buf, int bsz) {
    if (!g_teleHookAob) { snprintf(buf, bsz, "ERR: teleport hook unresolved"); return 0; }
    if (!g_teleportPosAddr) { snprintf(buf, bsz, "ERR: teleportPos unresolved"); return 0; }
    void* pc = GetPlayer();
    if (!pc) { snprintf(buf, bsz, "ERR: no player"); return 0; }
    void* attackFn = (mode == 2 && g_killcallAob) ? (void*)g_killcallAob : (void*)g_interactActorAddr;
    if (!attackFn) { snprintf(buf, bsz, "ERR: attack function unresolved (interactActor)"); return 0; }
    if (!g_performDigAddr) { snprintf(buf, bsz, "ERR: performDig unresolved"); return 0; }
    if (!InstallTeleHook()) { snprintf(buf, bsz, "ERR: teleport hook install failed"); return 0; }

    g_capFound = 0;
    g_teleUid = uid;
    for (int i = 0; i < 40 && !g_capFound; i++) {
        if (g_pShared && g_pShared->cancel) break;
        Sleep(25);
    }
    g_teleUid = 0;
    if (!g_capFound) { snprintf(buf, bsz, "ERR: player not found (wrong id / not in world?)"); return 0; }

    int* ox = BlockCoord(0xDC); int* oy = BlockCoord(0xE0); int* oz = BlockCoord(0xE4);
    int myX = ox ? *ox : 0, myY = oy ? *oy : 0, myZ = oz ? *oz : 0;

    BYTE* camLock = nullptr;
    if (IsReadable((BYTE*)pc + 0x950, 4)) {
        BYTE* cam = *(BYTE**)((BYTE*)pc + 0x950);
        if (cam && IsReadable(cam + 0x58, 4)) {
            camLock = cam + 0x58;
            g_camLockSaved = *(int*)camLock;
            *(int*)camLock = 8;
        }
    }

    int tx = (int)g_capX, ty = (int)g_capY - 64, tz = (int)g_capZ; // CE: target Y-64
    int callArgs[3] = { tx, ty, tz };
    CallN(g_teleportPosAddr, pc, callArgs, 3);

    int entId = 0;
    if (IsReadable((BYTE*)pc + 0x4D0, 4)) entId = *(int*)((BYTE*)pc + 0x4D0);
    int atkArgs[3] = { entId, 0, 0 };
    CallN(attackFn, pc, atkArgs, 3);

    if (IsReadable((BYTE*)pc + 0x9C4, 4)) {
        void* anim = *(void**)((BYTE*)pc + 0x9C4);
        if (anim) CallP1(g_performDigAddr, anim, 0);
    }

    int backArgs[3] = { myX, myY, myZ };
    CallN(g_teleportPosAddr, pc, backArgs, 3);

    if (camLock) *(int*)camLock = g_camLockSaved;
    snprintf(buf, bsz, "OK attack sent to %d at %d,%d,%d (mode %d)", uid - 1000000000, tx, ty + 64, tz, mode);
    return 1;
}

// Parse an address-ish Lua arg: "nil"/""/"0" -> 0, "player" -> current player,
// "0x..." hex or decimal number -> raw value.
static uintptr_t ParseAddrArg(const char* s, size_t len) {
    if (!s || len == 0) return 0;
    if (len >= 6 && strncmp(s, "player", 6) == 0) return (uintptr_t)GetPlayer();
    if (len >= 4 && strncmp(s, "world", 5) == 0) return 0; // world not wired yet
    if ((len > 2) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return (uintptr_t)strtoull(s + 2, nullptr, 16);
    return (uintptr_t)strtoull(s, nullptr, 10);
}

// Collect all export names of a module (names are the mangled C++ names).
static int CollectExports(HMODULE h, std::vector<std::string>& out) {
    if (!h) return 0;
    const BYTE* base = (const BYTE*)h;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dd.VirtualAddress == 0 || dd.Size == 0) return 0;
    IMAGE_EXPORT_DIRECTORY* ed = (IMAGE_EXPORT_DIRECTORY*)(base + dd.VirtualAddress);
    DWORD* names = (DWORD*)(base + ed->AddressOfNames);
    if (names == (DWORD*)(base + dd.VirtualAddress)) names = nullptr;
    int count = 0;
    for (DWORD i = 0; i < ed->NumberOfNames && names; i++) {
        const char* name = (const char*)(base + names[i]);
        if ((uintptr_t)name < (uintptr_t)base) continue;
        out.push_back(name);
        count++;
    }
    return count;
}

static DWORD WINAPI GiveItemThread(LPVOID p) {
    GiveItemJob* job = (GiveItemJob*)p;
    CallRaw2(g_sendToHostAddr, job->msg.c_str(), job->json.c_str());
    delete job;
    return 0;
}

// C-only native command dispatch: name + args (strings), writes result into out.
// Shared by the Lua bridge (zelvex_native) and the Lua-free executor
// (ExecuteNativeScript), so scripts run even when ScriptVM::callString refuses
// (e.g. script context not ready / not in a world).
// Sanity gate for player-table entries. Real players have small faction
// ids and plausible block coords; stale/garbage slots show huge random
// ints (e.g. team=1080000361, pos=2001490019).
static bool CoordSane(int v) { return v > -30000000 && v < 30000000; }

static bool PlayerEntryValid(BYTE* p, uint32_t myUid, bool includeSelf,
                             int minFac, int maxFac) {
    if (!p || !IsReadable(p, 0xC0)) return false;
    uint32_t uid = *(uint32_t*)p;
    if (uid == 0) return false;
    if (!includeSelf && uid == myUid) return false;
    int fac = *(int*)(p + 0xB0);
    if (fac < minFac || fac > maxFac || fac > 64) return false;
    if (!CoordSane(*(int*)(p + 0x14))) return false;
    if (!CoordSane(*(int*)(p + 0x18))) return false;
    if (!CoordSane(*(int*)(p + 0x1C))) return false;
    return true;
}

// Walks the live in-memory player table ([[libMiniBaseGame.dll+B36C]+78]+68,
// 40 slots) and invokes visit(playerPtr, ctx) for each valid entry.
// Layout per entry: uid @+0, pos ints @+14/+18/+1C, faction @+B0.
// Returns the number of visits performed.
typedef void (*PlayerVisitFn)(BYTE* p, void* ctx);
static int IteratePlayers(bool includeSelf, int minFac, int maxFac,
                          PlayerVisitFn visit, void* ctx) {
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (!mbH || !IsReadable((BYTE*)mbH + 0xB36C, 4)) return 0;
    BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
    if (!IsReadable(c1, 0x80) || !IsReadable(c1 + 0x78, 4)) return 0;
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!IsReadable(c2, 0x70) || !IsReadable(c2 + 0x68, 4)) return 0;
    BYTE* list = *(BYTE**)(c2 + 0x68);
    uint32_t myUid = ReadRoleId();
    int n = 0;
    for (int i = 0; i < 40; i++) {
        if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
        BYTE* p = *(BYTE**)((BYTE*)list + i * 4);
        if (!PlayerEntryValid(p, myUid, includeSelf, minFac, maxFac)) continue;
        visit(p, ctx);
        n++;
    }
    return n;
}

// Find a raw actor pointer by player-facing uid (player table scan).
static BYTE* FindActorByUid(uint32_t wantUid) {
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (!mbH || !IsReadable((BYTE*)mbH + 0xB36C, 4)) return nullptr;
    BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
    if (!IsReadable(c1, 0x80) || !IsReadable(c1 + 0x78, 4)) return nullptr;
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!IsReadable(c2, 0x70) || !IsReadable(c2 + 0x68, 4)) return nullptr;
    BYTE* list = *(BYTE**)(c2 + 0x68);
    for (int i = 0; i < 40; i++) {
        if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
        BYTE* p = *(BYTE**)(list + i * 4);
        if (!p || !IsReadable(p, 4)) continue;
        if (*(uint32_t*)p == wantUid) return p;
    }
    return nullptr;
}

static bool RunNativeCmd(const char* name, const char* const* args, int nargs, char* out, int outsz) {
    auto argi = [&](int idx, int def) -> int {
        const char* s = (idx < nargs) ? args[idx] : nullptr;
        if (!s || s[0] == 0) return def;
        return atoi(s);
    };
    auto argf = [&](int idx, float def) -> float {
        const char* s = (idx < nargs) ? args[idx] : nullptr;
        if (!s || s[0] == 0) return def;
        return (float)atof(s);
    };
    auto flagOn = [&](int idx) -> bool {
        const char* s = (idx < nargs) ? args[idx] : nullptr;
        if (!s || s[0] == 0) return true;
        return s[0] == '1' || s[0] == 't' || s[0] == 'T' || s[0] == 'y' || s[0] == 'Y' || s[0] == 'o' || s[0] == 'O';
    };
    char* buf = out;
    int bsz = outsz;
    auto result = [&](const char* s) { snprintf(buf, bsz, "%s", s); return true; };

    if (strcmp(name, "echo") == 0) {
        // native.echo(0) silences the per-call [cmd] console echo.
        g_nativeEcho = !(nargs > 0 && args[0] && args[0][0] == '0');
        return result(g_nativeEcho ? "ON" : "OFF");
    }
    if (strcmp(name, "reset") == 0) {
        // Tear down the persistent VM; a fresh one is created next run.
        CloseLuaState();
        return result("OK (fresh VM on next run)");
    }
    if (strcmp(name, "state") == 0) {
        void* p = GetPlayer();
        snprintf(buf, bsz,
            "player=%p sendToHost=%s noclip=%d unlock=%d noDrop=%d jumpFly=%d slowFall=%d chat=%s "
            "revive=%s hitWalls=%d groundSee=%d airSee=%d killAura=%d mountAll=%d mineAll=%d killAllHost=%d teleHook=%s interact=%s aimbot=%d "
            "roomKick=%s allDie=%d allDance=%d",
            p, g_sendToHostAddr ? "ok" : "missing", g_noclipOn, g_unlockOn,
            g_noDropOn, g_jumpFlyOn, g_slowFallOn, g_chatFuncAddr ? "ok" : "missing",
            g_reviveAddr ? "ok" : "missing", g_hitWallsOn, g_groundSeeOn, g_airWallOn,
            g_killAuraMode, g_mountAllInstalled ? 1 : 0, g_mineAllOn ? 1 : 0,
            g_killAllHostOn ? 1 : 0, g_teleHookCave ? "ok" : "no",
            g_interactActorAddr ? "ok" : "missing", g_aimbotOn ? 1 : 0,
            g_roomKickAddr ? "ok" : "missing", g_onDieAddr ? 1 : 0, g_doJumpAddr ? 1 : 0);
        return true;
    }
    if (strcmp(name, "giveItem") == 0) {
        if (!FnOk(g_sendToHostAddr)) return result("ERR: sendToHost unresolved");
        const char* iid = (nargs > 0) ? args[0] : nullptr;
        const char* num = (nargs > 1) ? args[1] : nullptr;
        uint32_t uid = 0;
        if (nargs > 2 && args[2] && args[2][0]) uid = (uint32_t)strtoul(args[2], nullptr, 10);
        if (uid == 0) uid = ReadRoleId();
        if (uid == 0) return result("ERR: role id not found (in-game?)");
        char json[256] = {};
        snprintf(json, sizeof(json), "{\"role_id\":%u,\"itemid\":%s,\"itemnum\":%s}",
            uid, iid ? iid : "0", num ? num : "1");
        GiveItemJob* job = new GiveItemJob();
        job->msg = "DEVELOPERSTORE_EXTRASTOREITEM_TOHOST";
        job->json = json;
        HANDLE h = CreateThread(nullptr, 0, GiveItemThread, job, 0, nullptr);
        if (!h) { delete job; return result("ERR: thread failed"); }
        WaitForSingleObject(h, 10000);
        CloseHandle(h);
        return result("OK");
    }
    if (strcmp(name, "giveItemBatch") == 0) {
        // Spam-grant an item into a backpack: repeat giveItem(itemid, num)
        // `count` times to the target uid (default = own role). No Lua VM needed.
        if (!FnOk(g_sendToHostAddr)) return result("ERR: sendToHost unresolved");
        const char* iid = (nargs > 0) ? args[0] : nullptr;
        int num = argi(1, 1);
        int count = argi(2, 1);
        if (num < 1) num = 1;
        if (count < 1) count = 1;
        if (count > 500) count = 500;
        uint32_t uid = 0;
        if (nargs > 3 && args[3] && args[3][0]) uid = (uint32_t)strtoul(args[3], nullptr, 10);
        if (uid == 0) uid = ReadRoleId();
        if (uid == 0) return result("ERR: role id not found (in-game?)");
        char json[256] = {};
        int sent = 0;
        for (int k = 0; k < count; k++) {
            snprintf(json, sizeof(json), "{\"role_id\":%u,\"itemid\":%s,\"itemnum\":%d}",
                uid, iid ? iid : "0", num);
            GiveItemJob* job = new GiveItemJob();
            job->msg = "DEVELOPERSTORE_EXTRASTOREITEM_TOHOST";
            job->json = json;
            HANDLE h = CreateThread(nullptr, 0, GiveItemThread, job, 0, nullptr);
            if (!h) { delete job; break; }
            WaitForSingleObject(h, 5000);
            CloseHandle(h);
            sent++;
            Sleep(50);
        }
        char rb[64] = {};
        snprintf(rb, sizeof(rb), "OK sent=%d", sent);
        return result(rb);
    }
    if (strcmp(name, "getUid") == 0) {
        uint32_t uid = ReadRoleId();
        if (uid) snprintf(buf, bsz, "%u", uid);
        else snprintf(buf, bsz, "ERR: no role id");
        return true;
    }
    if (strcmp(name, "getPos") == 0) {
        int* px = BlockCoord(0xDC);
        int* py = BlockCoord(0xE0);
        int* pz = BlockCoord(0xE4);
        if (!px || !py || !pz) return result("ERR: no player/pos struct");
        snprintf(buf, bsz, "%d,%d,%d", *px, *py, *pz);
        return true;
    }
    if (strcmp(name, "setPos") == 0) {
        int x = argi(0, 0), y = argi(1, 0), z = argi(2, 0);
        int* px = BlockCoord(0xDC);
        int* py = BlockCoord(0xE0);
        int* pz = BlockCoord(0xE4);
        if (!px || !py || !pz) return result("ERR: no player/pos struct");
        memcpy(px, &x, 4);
        memcpy(py, &y, 4);
        memcpy(pz, &z, 4);
        return result("OK");
    }
    if (strcmp(name, "getAim") == 0) {
        float* ax = AimFloat(4);
        float* ay = AimFloat(8);
        if (!ax || !ay) return result("ERR: no aim struct");
        snprintf(buf, bsz, "%g,%g", (double)*ax, (double)*ay);
        return true;
    }
    if (strcmp(name, "getHealth") == 0) {
        float* h = AttrField(0x94);
        float* m = AttrField(0x98);
        if (!h || !m) return result("ERR: no player/attr struct");
        snprintf(buf, bsz, "%g/%g", (double)*h, (double)*m);
        return true;
    }
    if (strcmp(name, "getHp") == 0) {
        float* h = AttrField(0x94);
        if (!h) return result("ERR: no player/attr struct");
        snprintf(buf, bsz, "%g", (double)*h);
        return true;
    }
    if (strcmp(name, "getMaxHp") == 0) {
        float* m = AttrField(0x98);
        if (!m) return result("ERR: no player/attr struct");
        snprintf(buf, bsz, "%g", (double)*m);
        return true;
    }
    if (strcmp(name, "setHealth") == 0) {
        float v = argf(0, 20);
        float* h = AttrField(0x94);
        if (!h) return result("ERR: no player/attr struct");
        memcpy(h, &v, 4);
        return result("OK");
    }
    {
        // CE-derived attr fields (all floats in the [player+0x280] struct):
        // walk/run/crouch/swim speed at +0x94+0x38/3C/40/44,
        // hunger +0x94+0x18C, maxhunger +0x94+0x198, stamina +0x94+0x1EC,
        // maxstamina +0x94+0x1F4, maxhealth +0x98.
        struct { const char* get; const char* set; int off; float def; } A[] = {
            { "walkspeed",   "setWalkspeed",   0xCC,  4.0f },
            { "runspeed",    "setRunspeed",    0xD0,  7.0f },
            { "crouchspeed", "setCrouchspeed", 0xD4,  2.0f },
            { "swimspeed",   "setSwimspeed",   0xD8,  3.5f },
            { "hunger",      "setHunger",      0x220, 20.0f },
            { "stamina",     "setStamina",     0x280, 100.0f },
        };
        for (int i = 0; i < (int)(sizeof(A) / sizeof(A[0])); i++) {
            if (strcmp(name, A[i].get) == 0 || strcmp(name, A[i].set) == 0) {
                float* f = AttrField(A[i].off);
                if (!f) return result("ERR: no player/attr struct");
                if (strcmp(name, A[i].set) == 0) {
                    float v = argf(0, A[i].def);
                    memcpy(f, &v, 4);
                    return result("OK");
                }
                snprintf(buf, bsz, "%g", (double)*f);
                return true;
            }
        }
        if (strcmp(name, "setMaxHealth") == 0) {
            float* m = AttrField(0x98);
            if (!m) return result("ERR: no player/attr struct");
            float v = argf(0, 20);
            memcpy(m, &v, 4);
            return result("OK");
        }
        if (strcmp(name, "getAttrs") == 0) {
            float* h = AttrField(0x94), * mx = AttrField(0x98);
            float* hn = AttrField(0x220), * mxhn = AttrField(0x22C);
            float* st = AttrField(0x280), * mxst = AttrField(0x288);
            float* w = AttrField(0xCC), * r = AttrField(0xD0);
            float* cr = AttrField(0xD4), * sw = AttrField(0xD8);
            if (!h && !hn && !w) return result("ERR: no player/attr struct");
            snprintf(buf, bsz,
                "hp=%g/%g hunger=%g/%g stamina=%g/%g walk=%g run=%g crouch=%g swim=%g",
                h ? (double)*h : 0.0, mx ? (double)*mx : 0.0,
                hn ? (double)*hn : 0.0, mxhn ? (double)*mxhn : 0.0,
                st ? (double)*st : 0.0, mxst ? (double)*mxst : 0.0,
                w ? (double)*w : 0.0, r ? (double)*r : 0.0,
                cr ? (double)*cr : 0.0, sw ? (double)*sw : 0.0);
            return true;
        }
    }
    if (strcmp(name, "fly") == 0 || strcmp(name, "sprint") == 0) {
        int* c = CondPtr();
        if (!c) return result("ERR: no condition ptr");
        bool en = flagOn(0);
        int bit = strcmp(name, "fly") == 0 ? 8 : 64;
        if (en) *c |= bit; else *c &= ~bit;
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "discard") == 0) {
        if (!FnOk(g_discardAddr)) return result("ERR: discardItem unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        int slot = argi(0, 1000), count = argi(1, 200);
        CallP2(g_discardAddr, p, slot, count);
        return result("OK");
    }
    if (strcmp(name, "sortPack") == 0) {
        if (!FnOk(g_sortPackAddr)) return result("ERR: sortPack unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        CallP1(g_sortPackAddr, p, 0);
        return result("OK");
    }
    if (strcmp(name, "repair") == 0) {
        if (!FnOk(g_newRepairAddr)) return result("ERR: NewRepair unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        int slot = argi(0, 1000);
        CallP6(g_newRepairAddr, p, slot, 114514, -1, 0, 1, -1);
        return result("OK");
    }
    if (strcmp(name, "setItem") == 0) {
        if (!FnOk(g_setItemAddr)) return result("ERR: setItem unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        int item = argi(0, 12239), slot = argi(1, 1000), count = argi(2, 1);
        CallP4(g_setItemAddr, p, item, slot, count, (int)(intptr_t)"");
        return result("OK");
    }
    if (strcmp(name, "noclip") == 0) {
        if (!FnOk(g_doMoveStepAddr)) return result("ERR: doMoveStep unresolved");
        BYTE* t = g_doMoveStepAddr + 0x22F;
        if (t[0] != 0x74 || t[1] != 0x56) return result("ERR: pattern mismatch at doMoveStep+22F");
        bool en = flagOn(0);
        static bool saved = false;
        if (en && !g_noclipOn) {
            if (!saved) { g_moveStepSaved[0] = t[0]; g_moveStepSaved[1] = t[1]; saved = true; }
            BYTE nop[2] = { 0x90, 0x90 };
            PatchMem(t, nop, 2);
            g_noclipOn = true;
        } else if (!en && g_noclipOn) {
            PatchMem(t, g_moveStepSaved, 2);
            g_noclipOn = false;
        }
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "unlock") == 0) {
        if (!FnOk(g_canPermitAddr)) return result("ERR: canPermit unresolved");
        BYTE* t = g_canPermitAddr + 0x78;
        if (t[0] != 0x83 || t[1] != 0xE0 || t[2] != 0x01 || t[3] != 0x83 || t[4] != 0xF8)
            return result("ERR: pattern mismatch at canPermit+78");
        bool en = flagOn(0);
        static bool saved = false;
        if (en && !g_unlockOn) {
            if (!saved) { memcpy(g_canPermitSaved, t, 5); saved = true; }
            BYTE pat[5] = { 0x83, 0xE0, 0x00, 0x90, 0x90 };
            PatchMem(t, pat, 5);
            g_unlockOn = true;
        } else if (!en && g_unlockOn) {
            PatchMem(t, g_canPermitSaved, 5);
            g_unlockOn = false;
        }
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "dump") == 0) {
        void* p = GetPlayer();
        int* px = BlockCoord(0xDC), * py = BlockCoord(0xE0), * pz = BlockCoord(0xE4);
        float* ax = AimFloat(4), * ay = AimFloat(8);
        float* h = AttrField(0x94), * m = AttrField(0x98);
        int* c = CondPtr();
        uint32_t uid = ReadRoleId();
        char line[512] = {};
        snprintf(line, sizeof(line),
            "dump: player=%p uid=%u block=%d,%d,%d aim=%g,%g hp=%g/%g cond=%s "
            "noclip=%d unlock=%d noDrop=%d jumpFly=%d slowFall=%d jump=%g scale=%g hand=%d slot=%d",
            p, uid, px ? *px : 0, py ? *py : 0, pz ? *pz : 0,
            ax && ay ? (double)*ax : 0.0, ax && ay ? (double)*ay : 0.0,
            h ? (double)*h : 0.0, m ? (double)*m : 0.0,
            c ? std::to_string(*c).c_str() : "?", g_noclipOn, g_unlockOn,
            g_noDropOn, g_jumpFlyOn, g_slowFallOn,
            AltAttrFloat(0xDC) ? (double)*AltAttrFloat(0xDC) : 0.0,
            IsReadable((BYTE*)p + 0x338, 4) ? (double)*(float*)((BYTE*)p + 0x338) : 0.0,
            PlayerInt(0x9D0) ? *PlayerInt(0x9D0) : 0,
            AltAttrInt(0x248) ? *AltAttrInt(0x248) : 0);
        Log(line);
        snprintf(buf, bsz, "OK: %s", line);
        return true;
    }
    if (strcmp(name, "teleport") == 0) {
        if (!FnOk(g_teleportPosAddr)) return result("ERR: teleportPos unresolved");
        void* p = GetPlayer();
        if (!p) return result("ERR: no player");
        int x = argi(0, 0), y = argi(1, 0), z = argi(2, 0);
        int a[3] = { x, y, z };
        CallN(g_teleportPosAddr, p, a, 3);
        return result("OK");
    }
    if (strcmp(name, "setTimespeed") == 0) {
        if (!FnOk(g_setDayTimeSpeedAddr)) return result("ERR: setDayTimeSpeed unresolved");
        void* wm = GetWorldMgr();
        if (!wm) return result("ERR: worldmgr null (must be in a world)");
        int v = argi(0, 1);
        int a[1] = { v };
        CallN(g_setDayTimeSpeedAddr, wm, a, 1);
        return result("OK");
    }
    if (strcmp(name, "setTime") == 0) {
        if (!FnOk(g_setDayTimeAddr)) return result("ERR: setDayTime unresolved");
        void* wm = GetWorldMgr();
        if (!wm) return result("ERR: worldmgr null (must be in a world)");
        int v = argi(0, 0);
        int a[1] = { v };
        CallN(g_setDayTimeAddr, wm, a, 1);
        return result("OK");
    }
    if (strcmp(name, "time") == 0) {
        void* wm = GetWorldMgr();
        if (!wm) return result("ERR: worldmgr null (must be in a world)");
        if (!FnOk(g_getTimeInDayAddr)) return result("ERR: getTimeInDay unresolved");
        int t = CallN(g_getTimeInDayAddr, wm, nullptr, 0);
        int day = 0;
        if (g_isDaytimeAddr) day = CallN(g_isDaytimeAddr, wm, nullptr, 0);
        snprintf(buf, bsz, "T=%d day=%d", t, day);
        return true;
    }
    if (strcmp(name, "getSpectate") == 0) {
        if (!FnOk(g_getSpectatorAddr)) return result("ERR: getSpectatorMode unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        int m = CallN(g_getSpectatorAddr, p, nullptr, 0);
        snprintf(buf, bsz, "spectator_mode=%d", m);
        return true;
    }
    if (strcmp(name, "spectate") == 0) {
        if (!FnOk(g_setSpectatorAddr)) return result("ERR: setSpectatorMode unresolved");
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        int m = argi(0, 1);
        CallP1(g_setSpectatorAddr, p, m);
        return result("OK");
    }
    // ---- CE-derived additions (MW_CT_REFERENCE.CT) ----
    if (strcmp(name, "chat") == 0) {
        const char* msg = (nargs > 0 && args[0]) ? args[0] : "";
        if (!msg[0]) return result("ERR: empty message");
        if (!g_chatFuncAddr || !PtrInModule(g_chatFuncAddr, "libiworld.dll"))
            return result("ERR: chat func unresolved");
        void* obj = GetChatObj();
        if (!obj) return result("ERR: chat object not found (in-game?)");
        int showname = flagOn(1) ? 1 : 0;
        CallP5S(g_chatFuncAddr, obj, msg, showname, 0, 1, 0);
        return result("OK");
    }
    if (strcmp(name, "getJump") == 0) {
        float* j = AltAttrFloat(0xDC);
        if (!j) return result("ERR: no player/attr struct");
        snprintf(buf, bsz, "%g", (double)*j);
        return true;
    }
    if (strcmp(name, "setJump") == 0) {
        float v = argf(0, 1.0f);
        float* j = AltAttrFloat(0xDC);
        if (!j) return result("ERR: no player/attr struct");
        memcpy(j, &v, 4);
        return result("OK");
    }
    if (strcmp(name, "getScale") == 0) {
        void* p = GetPlayer();
        if (!p || !IsReadable((BYTE*)p + 0x338, 4)) return result("ERR: no player");
        float* s = (float*)((BYTE*)p + 0x338);
        snprintf(buf, bsz, "%g", (double)*s);
        return true;
    }
    if (strcmp(name, "setScale") == 0) {
        float v = argf(0, 1.0f);
        void* p = GetPlayer();
        if (!p || !IsReadable((BYTE*)p + 0x338, 4)) return result("ERR: no player");
        memcpy((BYTE*)p + 0x338, &v, 4);
        return result("OK");
    }
    if (strcmp(name, "handItem") == 0) {
        int* h = PlayerInt(0x9D0);
        if (!h) return result("ERR: no player");
        snprintf(buf, bsz, "%d", *h);
        return true;
    }
    if (strcmp(name, "handSlot") == 0) {
        int* s = AltAttrInt(0x248);
        if (!s) return result("ERR: no player/attr struct");
        snprintf(buf, bsz, "%d", *s);
        return true;
    }
    if (strcmp(name, "getEmote") == 0) {
        int* e = EmoteInt(0x35C);
        if (!e) return result("ERR: no player");
        snprintf(buf, bsz, "%d", *e);
        return true;
    }
    if (strcmp(name, "getYaw") == 0) {
        void* p = GetPlayer(); if (!p) return result("ERR: not in world");
        if (!IsReadable((BYTE*)p + 0x950, 4)) return result("ERR: yaw ptr unreadable");
        BYTE* aim = *(BYTE**)((BYTE*)p + 0x950);
        if (!aim || !IsReadable(aim + 4, 4)) return result("ERR: yaw struct unreadable");
        float yaw = *(float*)(aim + 4);
        snprintf(buf, bsz, "%g", (double)yaw);
        return true;
    }
    if (strcmp(name, "setEmote") == 0) {
        int v = argi(0, 0);
        int* e = EmoteInt(0x35C);
        if (!e) return result("ERR: no player");
        *e = v;
        return result("OK");
    }
    if (strcmp(name, "roomOwner") == 0) {
        int* o = WorldInt(0x8C);
        if (!o) return result("ERR: worldmgr null (must be in a world)");
        snprintf(buf, bsz, "%d", *o);
        return true;
    }
    if (strcmp(name, "roomMap") == 0) {
        int* o = WorldInt(0x90);
        if (!o) return result("ERR: worldmgr null (must be in a world)");
        snprintf(buf, bsz, "%d", *o);
        return true;
    }
    if (strcmp(name, "noDrop") == 0) {
        if (!g_noDropTarget) return result("ERR: drop gate not found (game updated?)");
        if (g_noDropTarget[0] != 0x75) return result("ERR: pattern mismatch at drop gate");
        bool en = flagOn(0);
        static bool saved = false;
        if (en && !g_noDropOn) {
            if (!saved) { g_noDropSaved[0] = g_noDropTarget[0]; saved = true; }
            BYTE b = 0x74;
            PatchMem(g_noDropTarget, &b, 1);
            g_noDropOn = true;
        } else if (!en && g_noDropOn) {
            PatchMem(g_noDropTarget, g_noDropSaved, 1);
            g_noDropOn = false;
        }
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "jumpFly") == 0) {
        if (!g_jumpFlyTarget) return result("ERR: jump-fly gate not found (game updated?)");
        if (g_jumpFlyTarget[0] != 0x74) return result("ERR: pattern mismatch at jump-fly gate");
        bool en = flagOn(0);
        static bool saved = false;
        if (en && !g_jumpFlyOn) {
            if (!saved) { g_jumpFlySaved[0] = g_jumpFlyTarget[0]; saved = true; }
            BYTE b = 0x70;
            PatchMem(g_jumpFlyTarget, &b, 1);
            g_jumpFlyOn = true;
        } else if (!en && g_jumpFlyOn) {
            PatchMem(g_jumpFlyTarget, g_jumpFlySaved, 1);
            g_jumpFlyOn = false;
        }
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "slowFall") == 0) {
        if (!g_slowFallTarget) return result("ERR: slow-fall gate not found (game updated?)");
        if (g_slowFallTarget[0] != 0x0F || g_slowFallTarget[1] != 0x84)
            return result("ERR: pattern mismatch at slow-fall gate");
        bool en = flagOn(0);
        static bool saved = false;
        if (en && !g_slowFallOn) {
            if (!saved) { memcpy(g_slowFallSaved, g_slowFallTarget, 2); saved = true; }
            BYTE pat[2] = { 0x0F, 0x85 };
            PatchMem(g_slowFallTarget, pat, 2);
            g_slowFallOn = true;
        } else if (!en && g_slowFallOn) {
            PatchMem(g_slowFallTarget, g_slowFallSaved, 2);
            g_slowFallOn = false;
        }
        return result(en ? "ON" : "OFF");
    }
    if (strcmp(name, "repairAll") == 0) {
        if (!FnOk(g_newRepairAddr)) return result("ERR: NewRepair unresolved");
        void* p = GetPlayer();
        if (!p) return result("ERR: no player");
        int done = 0;
        for (int slot = 0; slot < 40; slot++) {
            CallP6(g_newRepairAddr, p, slot, 114514, -1, 0, 1, -1); done++;
        }
        for (int slot = 1000; slot < 1040; slot++) {
            CallP6(g_newRepairAddr, p, slot, 114514, -1, 0, 1, -1); done++;
        }
        snprintf(buf, bsz, "OK (%d slots)", done);
        return true;
    }
    if (strcmp(name, "discardAll") == 0) {
        if (!FnOk(g_discardAddr)) return result("ERR: discardItem unresolved");
        void* p = GetPlayer();
        if (!p) return result("ERR: no player");
        int done = 0;
        for (int slot = 0; slot < 40; slot++) {
            CallP2(g_discardAddr, p, slot, 200); done++;
        }
        for (int slot = 1000; slot < 1040; slot++) {
            CallP2(g_discardAddr, p, slot, 200); done++;
        }
        snprintf(buf, bsz, "OK (%d slots)", done);
        return true;
    }
    if (strcmp(name, "revive") == 0) {
        void* p = GetPlayer();
        if (!p) return result("ERR: no player/attr struct");
        if (!g_reviveAddr) return result("ERR: revive unresolved");
        int mode = (nargs > 0) ? atoi(args[0]) : 2;
        if (mode < 0) mode = 0;
        if (mode > 2) mode = 2;
        int callArgs[4] = { mode, 0, -1, 0 };
        bool ok = CallN(g_reviveAddr, p, callArgs, 4) != 0;
        snprintf(buf, bsz, "OK mode=%d%s", mode, ok ? "" : " (returned false)");
        return true;
    }
    if (strcmp(name, "addStar") == 0) {
        void* p = GetPlayer();
        if (!p) return result("ERR: no player/attr struct");
        if (!g_addStarAddr) return result("ERR: addStar unresolved");
        int n = (nargs > 0) ? atoi(args[0]) : 1;
        CallP1(g_addStarAddr, p, n);
        snprintf(buf, bsz, "OK +%d star(s)", n);
        return true;
    }
    if (strcmp(name, "hitWalls") == 0) {
        if (!g_hitWallsAddr) return result("ERR: hitWalls unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on) {
            if (!g_hitWallsSaved[0]) g_hitWallsSaved[0] = *(BYTE*)g_hitWallsAddr;
            PatchMem(g_hitWallsAddr, (const BYTE*)"\x90", 1);
            g_hitWallsOn = true;
        } else if (g_hitWallsSaved[0]) {
            PatchMem(g_hitWallsAddr, g_hitWallsSaved, 1);
            g_hitWallsOn = false;
        }
        snprintf(buf, bsz, "hitWalls=%d", g_hitWallsOn ? 1 : 0);
        return true;
    }
    if (strcmp(name, "groundSee") == 0) {
        if (!g_groundSeeAddr) return result("ERR: groundSee unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on) {
            if (!g_groundSeeSaved[0]) g_groundSeeSaved[0] = *(BYTE*)g_groundSeeAddr;
            const BYTE jz = 0x74;
            PatchMem(g_groundSeeAddr, &jz, 1);
            g_groundSeeOn = true;
        } else if (g_groundSeeSaved[0]) {
            PatchMem(g_groundSeeAddr, g_groundSeeSaved, 1);
            g_groundSeeOn = false;
        }
        snprintf(buf, bsz, "groundSee=%d", g_groundSeeOn ? 1 : 0);
        return true;
    }
    if (strcmp(name, "airSee") == 0) {
        if (!g_airWallAob) return result("ERR: airSee unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on && !g_airWallOn) {
            if (!g_airWallCave) {
                g_airWallCave = (uintptr_t)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (!g_airWallCave) return result("ERR: airSee alloc failed");
            }
            if (!g_airWallSaved[0]) memcpy(g_airWallSaved, (void*)g_airWallAob, 5);
            BYTE* c = (BYTE*)g_airWallCave;
            memcpy(c, g_airWallSaved, 5); c += 5;
            static const BYTE set1018[] = { 0xC7, 0x41, 0x2C, 0xFA, 0x03, 0x00, 0x00 }; // mov [ecx+2C],#1018
            memcpy(c, set1018, sizeof(set1018)); c += sizeof(set1018);
            BYTE jmpBack[5] = { 0xE9, 0, 0, 0, 0 };
            *(int*)(jmpBack + 1) = (int)((g_airWallAob + 5) - (g_airWallCave + 5 + sizeof(set1018) + 5));
            memcpy(c, jmpBack, 5);
            BYTE jmpCave[5] = { 0xE9, 0, 0, 0, 0 };
            *(int*)(jmpCave + 1) = (int)(g_airWallCave - (g_airWallAob + 5));
            PatchMem((BYTE*)g_airWallAob, jmpCave, 5);
            g_airWallOn = true;
        } else if (!on && g_airWallOn) {
            PatchMem((BYTE*)g_airWallAob, g_airWallSaved, 5);
            g_airWallOn = false;
        }
        snprintf(buf, bsz, "airSee=%d", g_airWallOn ? 1 : 0);
        return true;
    }
    if (strcmp(name, "killAura") == 0) {
        if (!g_killAuraTarget || !FnOk(g_pickActorAddr)) return result("ERR: killAura unresolved");
        if (!PtrInModule(g_killAuraTarget, "libSandboxEngine.dll")) return result("ERR: killAura target outside engine module");
        int mode = (nargs > 0) ? atoi(args[0]) : 1;
        if (mode < 0 || mode > 2) return result("ERR: mode 0|1|2");
        BYTE orig = *(BYTE*)g_killAuraTarget;
        if (mode == 0) {
            if (g_killAuraMode) {
                PatchMem(g_killAuraTarget, g_killAuraSaved, 1);
                g_killAuraMode = 0;
            }
        } else {
            if (!g_killAuraSaved[0]) g_killAuraSaved[0] = orig;
            if (g_killAuraSaved[0] != 0x84 && g_killAuraSaved[0] != 0x85) {
                return result("ERR: original byte unexpected (game updated?)");
            }
            BYTE b = (mode == 1) ? 0x85 : 0x8B;
            PatchMem(g_killAuraTarget, &b, 1);
            g_killAuraMode = mode;
        }
        snprintf(buf, bsz, "killAura=%d (byte %02X)", g_killAuraMode,
            g_killAuraMode ? *(BYTE*)g_killAuraTarget : g_killAuraSaved[0]);
        return true;
    }
    if (strcmp(name, "mountAll") == 0) {
        if (!g_rightClickPickAddr || !g_tryMountAddr) return result("ERR: mountAll unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on && !g_mountAllInstalled) {
            BYTE* rp = (BYTE*)g_rightClickPickAddr + 0x24F;
            BYTE* tm = (BYTE*)g_tryMountAddr + 0x45;
            if (!g_mountAllSaved[0]) {
                memcpy(g_mountAllSaved, rp, 6);
                memcpy(g_mountAllSaved + 6, tm, 6);
            }
            static const BYTE patA[6] = { 0x90, 0x90, 0x85, 0xF6, 0x74, 0x10 }; // NOP canBeRided je
            static const BYTE patB[6] = { 0x0F, 0x85, 0xA5, 0x04, 0x00, 0x00 }; // je -> jne
            PatchMem(rp, patA, 6);
            PatchMem(tm, patB, 6);
            g_mountAllInstalled = true;
        } else if (!on && g_mountAllInstalled) {
            PatchMem((BYTE*)g_rightClickPickAddr + 0x24F, g_mountAllSaved, 6);
            PatchMem((BYTE*)g_tryMountAddr + 0x45, g_mountAllSaved + 6, 6);
            g_mountAllInstalled = false;
        }
        snprintf(buf, bsz, "mountAll=%d", g_mountAllInstalled ? 1 : 0);
        return true;
    }
    if (strcmp(name, "mineAll") == 0) {
        if (!g_mineAllAob) return result("ERR: mineAll unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on && !g_mineAllOn) {
            if (!InstallMineAllCave()) return result("ERR: mineAll install failed");
            g_mineAllOn = true;
        } else if (!on && g_mineAllOn) {
            PatchMem((BYTE*)g_mineAllAob, g_mineAllSaved, 6);
            g_mineAllOn = false;
        }
        snprintf(buf, bsz, "mineAll=%d", g_mineAllOn ? 1 : 0);
        return true;
    }
    if (strcmp(name, "killAllHost") == 0) {
        if (!g_isDeadTarget) return result("ERR: killAllHost unresolved");
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on && !g_killAllHostOn) {
            if (!InstallKillAllCave()) return result("ERR: killAllHost install failed");
            g_killAllHostOn = true;
        } else if (!on && g_killAllHostOn) {
            PatchMem((BYTE*)g_isDeadTarget, g_killAllSaved, 8);
            g_killAllHostOn = false;
        }
        snprintf(buf, bsz, "killAllHost=%d", g_killAllHostOn ? 1 : 0);
        return true;
    }
    if (strcmp(name, "teleportTo") == 0 || strcmp(name, "playerPos") == 0 ||
        strcmp(name, "bringPlayer") == 0) {
        bool isTP = strcmp(name, "teleportTo") == 0;
        bool isPos = strcmp(name, "playerPos") == 0;
        if (!g_teleHookAob) return result("ERR: teleport hook unresolved");
        if (isTP && !g_teleportPosAddr) return result("ERR: teleportPos unresolved");
        if (isPos || isTP) {
            if (nargs < 1) return result("ERR: usage id (min 1000) [,dx,dy,dz]");
            int uid = atoi(args[0]);
            if (uid < 1000) return result("ERR: id looks wrong (min 1000)");
            uid += 1000000000; // entity uids are player-facing id + 1e9 (CE recipe)
            int dx = (nargs > 1) ? atoi(args[1]) : 0;
            int dy = (nargs > 2) ? atoi(args[2]) : 0;
            int dz = (nargs > 3) ? atoi(args[3]) : 0;
            if (!InstallTeleHook()) return result("ERR: teleport hook install failed");
            g_capFound = 0;
            g_teleUid = uid;
            for (int i = 0; i < 40 && !g_capFound; i++) {
                if (g_pShared && g_pShared->cancel) break;
                Sleep(25);
            }
            g_teleUid = 0;
            if (!g_capFound) return result("ERR: player not found (wrong id / not in world?)");
            int tx = (int)g_capX + dx, ty = (int)g_capY + dy, tz = (int)g_capZ + dz;
            if (isTP) {
                void* p = GetPlayer();
                if (!p) return result("ERR: no player");
                int callArgs[3] = { tx, ty, tz };
                CallN(g_teleportPosAddr, p, callArgs, 3);
                snprintf(buf, bsz, "OK teleported to %d at %d,%d,%d", uid - 1000000000, tx, ty, tz);
            } else {
                snprintf(buf, bsz, "OK %d at %d,%d,%d", uid - 1000000000, tx, ty, tz);
            }
        } else {
            // bringPlayer
            if (nargs >= 1 && atoi(args[0]) == 0) {
                g_bringUid = 0;
                return result("OK bringPlayer off");
            }
            if (nargs < 1) return result("ERR: usage bringPlayer(id [,dx,dy,dz]) or bringPlayer(0)");
            int uid = atoi(args[0]);
            if (uid < 1000) return result("ERR: id looks wrong (min 1000)");
            uid += 1000000000;
            int dx = (nargs > 1) ? atoi(args[1]) : 0;
            int dy = (nargs > 2) ? atoi(args[2]) : 0;
            int dz = (nargs > 3) ? atoi(args[3]) : 0;
            if (!InstallTeleHook()) return result("ERR: teleport hook install failed");
            g_bringUid = uid;
            g_brX = dx; g_brY = dy; g_brZ = dz;
            snprintf(buf, bsz, "OK dragging %d to you +(%d,%d,%d) every tick; bringPlayer(0) to stop",
                uid - 1000000000, g_brX, g_brY, g_brZ);
        }
        return true;
    }
    // ---- Phase 2 (CT recipes): room kick + mass room effects ----
    if (strcmp(name, "roomKick") == 0) {
        if (!g_roomKickAddr || !PtrInModule(g_roomKickAddr, "libiworld.dll"))
            return result("ERR: requestRoomKickPlayer unresolved");
        if (!g_hotfixAnchor || !IsReadable(g_hotfixAnchor + 0x20C8, 4))
            return result("ERR: room manager anchor unreadable");
        void* rm = *(void**)(g_hotfixAnchor + 0x20C8);
        if (!rm || !IsReadable(rm, 4)) return result("ERR: room manager null (join a room first)");
        int uid = argi(0, 0);
        if (uid < 1000) return result("ERR: id looks wrong (min 1000)");
        CallP1(g_roomKickAddr, rm, uid);
        return result("OK");
    }
    if (strcmp(name, "allDie") == 0) {
        if (!FnOk(g_onDieAddr)) return result("ERR: ClientPlayer::onDie unresolved");
        bool incSelf = (nargs > 0 && args[0] && args[0][0] == '1');
        struct DieCtx { void* fn; int n; } ctx = { g_onDieAddr, 0 };
        IteratePlayers(incSelf, 1, 0x7FFFFFFF, [](BYTE* p, void* c) {
            DieCtx* x = (DieCtx*)c;
            if (ObjectValid(p, "libSandboxEngine.dll")) CallP0(x->fn, p);
            x->n++;
        }, &ctx);
        snprintf(buf, bsz, "OK (%d players)", ctx.n);
        return true;
    }
    if (strcmp(name, "allDance") == 0) {
        if (!FnOk(g_doJumpAddr)) return result("ERR: PlayerLocoMotion::doJump unresolved");
        bool incSelf = (nargs > 0 && args[0] && args[0][0] == '1');
        struct JumpCtx { void* fn; int n; } ctx = { g_doJumpAddr, 0 };
        IteratePlayers(incSelf, 1, 0x7FFFFFFF, [](BYTE* p, void* c) {
            JumpCtx* x = (JumpCtx*)c;
            if (!IsReadable(p + 0x270, 4)) return;
            BYTE* lm = *(BYTE**)(p + 0x270);
            if (!lm || !ObjectValid(lm, "libSandboxEngine.dll")) return;
            CallP0(x->fn, lm);
            x->n++;
        }, &ctx);
        snprintf(buf, bsz, "OK (%d jumps)", ctx.n);
        return true;
    }
    // ---- Phase 3: GM skin changer + unlock locked items ----
    if (strcmp(name, "gmSkin") == 0) {
        if (!FnOk(g_gmChangeSkinAddr)) return result("ERR: GMChangeSkin unresolved");
        void* p = GetPlayer();
        if (!p) return result("ERR: no player");
        int a = argi(0, 0), b = argi(1, 0);
        const char* s = (nargs > 2 && args[2]) ? args[2] : "";
        CallP3(g_gmChangeSkinAddr, p, a, b, (int)(intptr_t)s);
        return result("OK");
    }
    if (strcmp(name, "unlockItems") == 0) {
        if (!g_unlockItemsTarget) {
            BYTE pat[6] = { 0x66, 0x0F, 0x6E, 0x40, 0x2C, 0x83 }; // movd xmm0,[eax+2C]; 83..
            g_unlockItemsTarget = FindBytes("libSandboxEngine.dll", pat, 6);
            if (g_unlockItemsTarget) {
                memcpy(g_unlockItemsSaved, g_unlockItemsTarget, 5);
                g_unlockItemsCave = (BYTE*)VirtualAlloc(nullptr, 32,
                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (g_unlockItemsCave) {
                    BYTE* c = g_unlockItemsCave;
                    *c++ = 0xC7; *c++ = 0x40; *c++ = 0x2C;          // mov dword [eax+2C],0
                    *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;
                    memcpy(c, g_unlockItemsSaved, 5); c += 5;       // movd xmm0,[eax+2C]
                    DWORD rel = (DWORD)((g_unlockItemsTarget + 5) - (c + 5));
                    *c++ = 0xE9; memcpy(c, &rel, 4);                // jmp back
                }
            }
        }
        if (!g_unlockItemsTarget || !g_unlockItemsCave)
            return result("ERR: unlock pattern not found (game updated?)");
        bool en = flagOn(0);
        if (en && !g_unlockItemsOn) {
            BYTE jmp[5] = { 0xE9 };
            DWORD rel = (DWORD)(g_unlockItemsCave - (g_unlockItemsTarget + 5));
            memcpy(jmp + 1, &rel, 4);
            PatchMem(g_unlockItemsTarget, jmp, 5);
            g_unlockItemsOn = true;
        } else if (!en && g_unlockItemsOn) {
            PatchMem(g_unlockItemsTarget, g_unlockItemsSaved, 5);
            g_unlockItemsOn = false;
        }
        snprintf(buf, bsz, "unlockItems=%d", g_unlockItemsOn ? 1 : 0);
        return true;
    }
    // ---- Stage: W2S (getPointToScreen) ----
    if (strcmp(name, "w2s") == 0) {
        if (!FnOk(g_ptScreenAddr)) return result("ERR: getPointToScreen unresolved");
        int uid = argi(0, 0);
        if (uid < 1000) return result("ERR: uid looks wrong");
        BYTE* actor = FindActorByUid((uint32_t)uid);
        if (!actor) {
            // verbose miss: list a few uids that ARE in the table so the
            // caller can see what is projectable right now
            char avail[160] = {};
            int n = 0;
            HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
            if (mbH && IsReadable((BYTE*)mbH + 0xB36C, 4)) {
                BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
                if (IsReadable(c1, 0x80) && IsReadable(c1 + 0x78, 4)) {
                    BYTE* c2 = *(BYTE**)(c1 + 0x78);
                    if (IsReadable(c2, 0x70) && IsReadable(c2 + 0x68, 4)) {
                        BYTE* list = *(BYTE**)(c2 + 0x68);
                        for (int i = 0; i < 40 && n < 3; i++) {
                            if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
                            BYTE* p = *(BYTE**)(list + i * 4);
                            if (!p || !IsReadable(p, 4)) continue;
                            uint32_t u2 = *(uint32_t*)p;
                            if (u2 == 0) continue;
                            char one[24];
                            snprintf(one, sizeof(one), "%s%u", n ? "," : "", u2);
                            strcat_s(avail, sizeof(avail), one);
                            n++;
                        }
                    }
                }
            }
            snprintf(buf, bsz, "ERR: actor %d not in table%s%s",
                     uid, n ? "; visible: " : " (empty)", avail);
            return true;
        }
        void* pc = GetPlayer();
        if (!pc) return result("ERR: no player");
        float sx = 0.f, sy = 0.f, sz = 0.f;
        CallP5(g_ptScreenAddr, pc, (int)(intptr_t)&sx, (int)(intptr_t)&sy,
               (int)(intptr_t)&sz, (int)(intptr_t)actor, 0);
        snprintf(buf, bsz, "%.4f,%.4f,%.4f", (double)sx, (double)sy, (double)sz);
        return true;
    }
    if (strcmp(name, "killPlayer") == 0) {
        // Server-synced remote kill (CE "Automatic 100-meter killing"):
        // TP onto target -> attack call -> dig animation -> TP back.
        if (nargs < 1) return result("ERR: usage killPlayer(id [,mode])");
        int uid = atoi(args[0]);
        if (uid < 1000) return result("ERR: id looks wrong (min 1000)");
        uid += 1000000000;
        int mode = (nargs > 1) ? atoi(args[1]) : 1; // 1=interactActor, 2=Killcall AOB
        KillPlayerByUid(uid, mode, buf, bsz);
        return true;
    }
    if (strcmp(name, "aimbot") == 0) {
        bool on = (nargs < 1) || atoi(args[0]) != 0;
        if (on) {
            g_aimRange = (nargs > 1) ? (float)atof(args[1]) : 3000.0f;
            if (g_aimRange < 100) g_aimRange = 100;
            g_aimbotOn = true;
            if (!g_aimbotThread) {
                g_aimbotThread = CreateThread(nullptr, 0, AimbotThread, nullptr, 0, nullptr);
                if (!g_aimbotThread) { g_aimbotOn = false; return result("ERR: aimbot thread failed"); }
            }
            snprintf(buf, bsz, "aimbot=1 range=%.0f", g_aimRange);
        } else {
            g_aimbotOn = false;
            if (g_aimbotThread) {
                WaitForSingleObject(g_aimbotThread, 1500);
                g_aimbotThread = nullptr;
            }
            return result("OK aimbot off");
        }
        return true;
    }
    return false;
}

/*
 * ===========================================================================
 * Executor v2 - single Lua-like scripting mode. No game Lua VM involved.
 * Statements: native.cmd(args), x = expr (result capture), if/elseif/else/end,
 * while/end, for i = a, b[, step]/end, break, print(...), wait(seconds).
 * Expressions: numbers, "quoted strings", variables, native.getX() inline
 * calls, + - * / .. (concat) == != < > <= >= and or ().
 * '--' and '#' start a comment.
 * ===========================================================================
 */
static std::map<std::string, std::string> g_nativeVars;

static const char* SkipWs(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static double StrToD(const std::string& s) {
    const char* c = s.c_str();
    while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') c++;
    return strtod(c, nullptr);
}

static bool IsNumeric(const std::string& s) {
    const char* c = s.c_str();
    while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') c++;
    if (!*c) return false;
    char* endp = nullptr;
    strtod(c, &endp);
    if (!endp || endp == c) return false;
    while (*endp == ' ' || *endp == '\t' || *endp == '\r' || *endp == '\n') endp++;
    return *endp == '\0';
}

static std::string NumToStr(double d) {
    long long ll = (long long)d;
    if ((double)ll == d && d > -9e15 && d < 9e15) {
        char b[40] = {};
        snprintf(b, sizeof(b), "%lld", ll);
        return std::string(b);
    }
    char b[40] = {};
    snprintf(b, sizeof(b), "%g", d);
    return std::string(b);
}

static bool Truthy(const std::string& s) {
    if (s.empty()) return false;
    return StrToD(s) != 0.0;
}

static void SplitArgs(const char* a, const char* cl, std::vector<std::string>& out) {
    if (!a || !cl || a >= cl) return;
    std::string cur;
    bool inQ = false;
    char qc = 0;
    for (const char* p = a; p < cl; p++) {
        char c = *p;
        if (inQ) {
            if (c == qc) inQ = false;
            cur += c;
        } else if (c == '"' || c == '\'') {
            inQ = true;
            qc = c;
            cur += c;
        } else if (c == ',') {
            size_t b = 0;
            while (b < cur.size() && (cur[b] == ' ' || cur[b] == '\t')) b++;
            size_t e2 = cur.size();
            while (e2 > b && (cur[e2 - 1] == ' ' || cur[e2 - 1] == '\t')) e2--;
            out.push_back(cur.substr(b, e2 - b));
            cur.clear();
        } else {
            cur += c;
        }
    }
    size_t b = 0;
    while (b < cur.size() && (cur[b] == ' ' || cur[b] == '\t')) b++;
    size_t e2 = cur.size();
    while (e2 > b && (cur[e2 - 1] == ' ' || cur[e2 - 1] == '\t')) e2--;
    out.push_back(cur.substr(b, e2 - b));
}

static std::string RunNativeCmdInline(const std::string& name, const std::vector<std::string>& a) {
    const char* av[8] = {};
    int n = 0;
    for (const auto& s : a) {
        if (n >= 8) break;
        av[n++] = s.c_str();
    }
    char buf[768] = {};
    if (RunNativeCmd(name.c_str(), av, n, buf, sizeof(buf))) return std::string(buf);
    return "ERR";
}

enum TkType {
    TK_END, TK_NUM, TK_FLOAT, TK_STR, TK_VAR, TK_NATIVE,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_CONCAT,
    TK_LPAREN, TK_RPAREN, TK_LT, TK_LE, TK_GT, TK_GE, TK_EQ, TK_NE
};

struct Tk {
    TkType t;
    int v;
    std::string s;
    std::vector<std::string> a;
};

static std::string ParseExpr(const std::vector<Tk>& t, int& i);

static std::vector<Tk> LexExpr(const char* src) {
    std::vector<Tk> out;
    const char* p = src;
    while (true) {
        p = SkipWs(p);
        if (*p == '\0') {
            out.push_back({TK_END, 0, "", {}});
            break;
        }
        char c = *p;
        if (c == '"' || c == '\'') {
            char q = c;
            p++;
            std::string s;
            while (*p && *p != q) s += *p++;
            if (*p == q) p++;
            out.push_back({TK_STR, 0, s, {}});
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)p[1]))) {
            char* np = nullptr;
            double d = strtod(p, &np);
            if (np && np != p) {
                std::string text(p, (size_t)(np - p));
                bool hasDot = text.find('.') != std::string::npos;
                if (hasDot) out.push_back({TK_FLOAT, 0, text, {}});
                else out.push_back({TK_NUM, (int)d, "", {}});
                p = np;
                continue;
            }
        }
        if (isalpha((unsigned char)c) || c == '_') {
            std::string n;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) n += *p++;
            if (n == "true") {
                out.push_back({TK_NUM, 1, "", {}});
                continue;
            }
            if (n == "false") {
                out.push_back({TK_NUM, 0, "", {}});
                continue;
            }
            if (n == "native" && *p == '.') {
                const char* save = p;
                p++;
                std::string nm;
                while (*p && (isalnum((unsigned char)*p) || *p == '_')) nm += *p++;
                if (!nm.empty() && *p == '(') {
                    std::vector<std::string> args;
                    std::string cur;
                    bool inQ = false;
                    char qc = 0;
                    const char* q = p + 1;
                    bool closed = false;
                    while (*q) {
                        char cc = *q;
                        if (inQ) {
                            if (cc == qc) inQ = false;
                            cur += cc;
                        } else if (cc == '"' || cc == '\'') {
                            inQ = true;
                            qc = cc;
                            cur += cc;
                        } else if (cc == ',') {
                            size_t b = 0;
                            while (b < cur.size() && (cur[b] == ' ' || cur[b] == '\t')) b++;
                            size_t e2 = cur.size();
                            while (e2 > b && (cur[e2 - 1] == ' ' || cur[e2 - 1] == '\t')) e2--;
                            args.push_back(cur.substr(b, e2 - b));
                            cur.clear();
                        } else if (cc == ')') {
                            size_t b = 0;
                            while (b < cur.size() && (cur[b] == ' ' || cur[b] == '\t')) b++;
                            size_t e2 = cur.size();
                            while (e2 > b && (cur[e2 - 1] == ' ' || cur[e2 - 1] == '\t')) e2--;
                            args.push_back(cur.substr(b, e2 - b));
                            p = q + 1;
                            closed = true;
                            break;
                        } else {
                            cur += cc;
                        }
                        q++;
                    }
                    if (closed) {
                        out.push_back({TK_NATIVE, 0, nm, args});
                        continue;
                    }
                    p = save;
                } else {
                    p = save;
                }
            }
            out.push_back({TK_VAR, 0, n, {}});
            continue;
        }
        if (c == '.' && p[1] == '.') {
            out.push_back({TK_CONCAT, 0, "..", {}});
            p += 2;
            continue;
        }
        if (c == '<' && p[1] == '=') {
            out.push_back({TK_LE, 0, "<=", {}});
            p += 2;
            continue;
        }
        if (c == '>' && p[1] == '=') {
            out.push_back({TK_GE, 0, ">=", {}});
            p += 2;
            continue;
        }
        if (c == '=' && p[1] == '=') {
            out.push_back({TK_EQ, 0, "==", {}});
            p += 2;
            continue;
        }
        if (c == '!' && p[1] == '=') {
            out.push_back({TK_NE, 0, "!=", {}});
            p += 2;
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == '<' || c == '>') {
            TkType m[256] = {};
            m['+'] = TK_PLUS;
            m['-'] = TK_MINUS;
            m['*'] = TK_STAR;
            m['/'] = TK_SLASH;
            m['('] = TK_LPAREN;
            m[')'] = TK_RPAREN;
            m['<'] = TK_LT;
            m['>'] = TK_GT;
            out.push_back({m[(unsigned char)c], 0, "", {}});
            p++;
            continue;
        }
        out.push_back({TK_END, 0, "", {}});
        break;
    }
    return out;
}

static std::string ParseFactor(const std::vector<Tk>& t, int& i) {
    if (i >= (int)t.size()) return "0";
    const Tk& tk = t[i];
    if (tk.t == TK_NUM) {
        i++;
        return NumToStr(tk.v);
    }
    if (tk.t == TK_FLOAT) {
        i++;
        return tk.s;
    }
    if (tk.t == TK_STR) {
        i++;
        return tk.s;
    }
    if (tk.t == TK_VAR) {
        i++;
        auto it = g_nativeVars.find(tk.s);
        return it == g_nativeVars.end() ? "0" : it->second;
    }
    if (tk.t == TK_MINUS) {
        i++;
        return NumToStr(-StrToD(ParseFactor(t, i)));
    }
    if (tk.t == TK_LPAREN) {
        i++;
        std::string s = ParseExpr(t, i);
        if (i < (int)t.size() && t[i].t == TK_RPAREN) i++;
        return s;
    }
    if (tk.t == TK_NATIVE) {
        i++;
        return RunNativeCmdInline(tk.s, tk.a);
    }
    i++;
    return "0";
}

static std::string ParseTerm(const std::vector<Tk>& t, int& i) {
    std::string v = ParseFactor(t, i);
    while (i < (int)t.size()) {
        if (t[i].t == TK_STAR) {
            i++;
            v = NumToStr(StrToD(v) * StrToD(ParseTerm(t, i)));
        } else if (t[i].t == TK_SLASH) {
            i++;
            double d = StrToD(ParseTerm(t, i));
            v = NumToStr(d != 0 ? StrToD(v) / d : 0);
        } else {
            break;
        }
    }
    return v;
}

static std::string ParseCmp(const std::vector<Tk>& t, int& i) {
    std::string v = ParseTerm(t, i);
    while (i < (int)t.size()) {
        if (t[i].t == TK_PLUS) {
            i++;
            v = NumToStr(StrToD(v) + StrToD(ParseTerm(t, i)));
        } else if (t[i].t == TK_MINUS) {
            i++;
            v = NumToStr(StrToD(v) - StrToD(ParseTerm(t, i)));
        } else if (t[i].t == TK_CONCAT) {
            i++;
            v = v + ParseTerm(t, i);
        } else {
            break;
        }
    }
    if (i < (int)t.size()) {
        TkType op = t[i].t;
        if (op == TK_LT || op == TK_LE || op == TK_GT || op == TK_GE || op == TK_EQ || op == TK_NE) {
            i++;
            std::string r = ParseCmp(t, i);
            bool aN = IsNumeric(v), bN = IsNumeric(r);
            bool res = false;
            if (op == TK_EQ) res = (aN && bN) ? (StrToD(v) == StrToD(r)) : (v == r);
            else if (op == TK_NE) res = (aN && bN) ? (StrToD(v) != StrToD(r)) : (v != r);
            else if (aN && bN) {
                double a = StrToD(v), b = StrToD(r);
                if (op == TK_LT) res = a < b;
                else if (op == TK_LE) res = a <= b;
                else if (op == TK_GT) res = a > b;
                else if (op == TK_GE) res = a >= b;
            } else {
                int cc = v.compare(r);
                if (op == TK_LT) res = cc < 0;
                else if (op == TK_LE) res = cc <= 0;
                else if (op == TK_GT) res = cc > 0;
                else if (op == TK_GE) res = cc >= 0;
            }
            return res ? "1" : "0";
        }
    }
    return v;
}

static std::string ParseAnd(const std::vector<Tk>& t, int& i) {
    std::string v = ParseCmp(t, i);
    while (i < (int)t.size() && t[i].t == TK_VAR && t[i].s == "and") {
        i++;
        std::string r = ParseCmp(t, i);
        v = (Truthy(v) && Truthy(r)) ? "1" : "0";
    }
    return v;
}

static std::string ParseOr(const std::vector<Tk>& t, int& i) {
    std::string v = ParseAnd(t, i);
    while (i < (int)t.size() && t[i].t == TK_VAR && t[i].s == "or") {
        i++;
        std::string r = ParseAnd(t, i);
        v = (Truthy(v) || Truthy(r)) ? "1" : "0";
    }
    return v;
}

static std::string ParseExpr(const std::vector<Tk>& t, int& i) {
    return ParseOr(t, i);
}

static std::string EvalExpr(const char* src) {
    std::vector<Tk> t = LexExpr(src);
    int i = 0;
    return ParseExpr(t, i);
}

static std::string EvalArgText(const std::string& raw) {
    std::string t = raw;
    size_t b = 0, e = t.size();
    while (b < e && (t[b] == ' ' || t[b] == '\t')) b++;
    while (e > b && (t[e - 1] == ' ' || t[e - 1] == '\t')) e--;
    t = t.substr(b, e - b);
    if (t.empty()) return "";
    bool ident = true;
    for (size_t i = 0; i < t.size(); i++) {
        if (!(isalnum((unsigned char)t[i]) || t[i] == '_')) {
            ident = false;
            break;
        }
    }
    if (ident) {
        auto it = g_nativeVars.find(t);
        if (it != g_nativeVars.end()) return it->second;
        return t;
    }
    bool exprLike = false;
    for (size_t i = 0; i < t.size(); i++) {
        char c = t[i];
        if (c == '"' || c == '\'' || c == '+' || c == '*' || c == '/' || c == '(' || c == ')' || c == '-') {
            exprLike = true;
            break;
        }
        if (c == '.' && i + 1 < t.size() && t[i + 1] == '.') {
            exprLike = true;
            break;
        }
    }
    if (!exprLike) return t;
    return EvalExpr(t.c_str());
}

static int ParseIfHeader(char* line, char* condBuf, int condSz) {
    if (strncmp(line, "if", 2) != 0 || line[2] != ' ') return 0;
    char* p = line + 3;
    char* c = p;
    size_t clen = strlen(c);
    while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t' || c[clen - 1] == '\r')) c[--clen] = '\0';
    if (clen >= 4 && strncmp(c + clen - 4, "then", 4) == 0) {
        c[clen - 4] = '\0';
        clen -= 4;
        while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t')) c[--clen] = '\0';
    }
    strncpy(condBuf, p, condSz - 1);
    condBuf[condSz - 1] = '\0';
    return 1;
}

static int ParseElseIfHeader(char* line, char* condBuf, int condSz) {
    if (strncmp(line, "elseif", 6) != 0) return 0;
    if (line[6] != ' ' && line[6] != '\t' && line[6] != '\0') return 0;
    char* p = line + 6;
    while (*p == ' ' || *p == '\t') p++;
    char* c = p;
    size_t clen = strlen(c);
    while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t' || c[clen - 1] == '\r')) c[--clen] = '\0';
    if (clen >= 4 && strncmp(c + clen - 4, "then", 4) == 0) {
        c[clen - 4] = '\0';
        clen -= 4;
        while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t')) c[--clen] = '\0';
    }
    strncpy(condBuf, p, condSz - 1);
    condBuf[condSz - 1] = '\0';
    return 1;
}

static int ParseWhileHeader(char* line, char* condBuf, int condSz) {
    if (strncmp(line, "while", 5) != 0 || line[5] != ' ') return 0;
    char* p = line + 6;
    char* c = p;
    size_t clen = strlen(c);
    while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t' || c[clen - 1] == '\r')) c[--clen] = '\0';
    if (clen >= 2 && c[clen - 2] == 'd' && c[clen - 1] == 'o') {
        c[clen - 2] = '\0';
    } else {
        return 0;
    }
    strncpy(condBuf, p, condSz - 1);
    condBuf[condSz - 1] = '\0';
    return 1;
}

static int ParseForHeader(char* line, char* vname, int vsz, char* condBuf, int condSz) {
    if (strncmp(line, "for", 3) != 0 || line[3] != ' ') return 0;
    char* p = line + 4;
    while (*p == ' ' || *p == '\t') p++;
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '=' && i < vsz - 1) vname[i++] = *p++;
    vname[i] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    char* c = p;
    size_t clen = strlen(c);
    while (clen > 0 && (c[clen - 1] == ' ' || c[clen - 1] == '\t' || c[clen - 1] == '\r')) c[--clen] = '\0';
    if (clen >= 2 && c[clen - 2] == 'd' && c[clen - 1] == 'o') {
        c[clen - 2] = '\0';
    } else {
        return 0;
    }
    strncpy(condBuf, p, condSz - 1);
    condBuf[condSz - 1] = '\0';
    return i > 0;
}

static int IsElseLine(char* line) {
    if (strncmp(line, "else", 4) != 0) return 0;
    char c = line[4];
    return c == '\0' || c == ' ' || c == '\t' || c == '\r';
}

static int IsLineEnd(const char* line) {
    if (!line) return 0;
    if (*line == '\0') return 1;
    const char* p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    return *p == '\0';
}

static int FindMatchingEnd(char* const* lines, int n, int start) {
    int depth = 0;
    for (int i = start; i < n; i++) {
        char buf[512] = {};
        size_t llen = strlen(lines[i]);
        memcpy(buf, lines[i], (llen > sizeof(buf) - 1) ? sizeof(buf) - 1 : llen);
        char* t = buf + strlen(buf);
        while (t > buf && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '\r')) *--t = '\0';
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if ((s[0] == 'i' && s[1] == 'f' && s[2] == ' ') ||
            (strncmp(s, "while", 5) == 0 && s[5] == ' ') ||
            (strncmp(s, "for", 3) == 0 && s[3] == ' ')) {
            depth++;
        } else if (s[0] == 'e' && s[1] == 'n' && s[2] == 'd' && (s[3] == '\0' || s[3] == ' ' || s[3] == '\t')) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

static int ParseAssign(char* line, char* vname, int vsz, char* ebuf, int esz) {
    if (!(isalpha((unsigned char)line[0]) || line[0] == '_')) return 0;
    if (strncmp(line, "if", 2) == 0 || strncmp(line, "while", 5) == 0 || strncmp(line, "for", 3) == 0 ||
        strncmp(line, "end", 3) == 0 || strncmp(line, "else", 4) == 0 || strncmp(line, "break", 5) == 0 ||
        strncmp(line, "print", 5) == 0 || strncmp(line, "wait", 4) == 0) return 0;
    char* p = line;
    int i = 0;
    while ((isalnum((unsigned char)*p) || *p == '_') && i < vsz - 1) vname[i++] = *p++;
    vname[i] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=' || p[1] == '=') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    strncpy(ebuf, p, esz - 1);
    ebuf[esz - 1] = '\0';
    return i > 0;
}

static void ExecNativeBlock(char* const* lines, int n, int start, int end, bool* breakFlag);

static void ExecNativeLine(char* line) {
    char buf[512] = {};
    size_t llen = strlen(line);
    memcpy(buf, line, (llen > sizeof(buf) - 1) ? sizeof(buf) - 1 : llen);
    char* t = buf + strlen(buf);
    while (t > buf && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '\r' || t[-1] == '\n')) *--t = '\0';
    char* s = buf;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return;
    if (s[0] == '-' && s[1] == '-') return;
    if (s[0] == '#') return;
    if (strncmp(s, "print", 5) == 0 && s[5] == '(') {
        const char* cl = strchr(s + 6, ')');
        if (cl) {
            std::vector<std::string> args;
            SplitArgs(s + 6, cl, args);
            bool first = true;
            for (auto& a : args) {
                if (!first) AppendOutput("\t");
                AppendOutput(EvalArgText(a).c_str());
                first = false;
            }
        }
        AppendOutput("\n");
        return;
    }
    if (strncmp(s, "wait", 4) == 0 && s[4] == '(') {
        const char* cl = strchr(s + 5, ')');
        if (cl) {
            std::vector<std::string> args;
            SplitArgs(s + 5, cl, args);
            if (!args.empty()) {
                double sec = StrToD(EvalArgText(args[0]));
                if (sec < 0) sec = 0;
                // Interruptible sleep: 50ms slices so the GUI Stop button
                // (shared->cancel) can abort long waits / infinite loops.
                DWORD ms = (DWORD)(sec * 1000.0);
                while (ms > 0) {
                    if (g_pShared && g_pShared->cancel) {
                        g_cancelRequested = true;
                        break;
                    }
                    DWORD chunk = (ms > 50) ? 50 : ms;
                    Sleep(chunk);
                    ms -= chunk;
                }
                if (ms == 0 && g_pShared && g_pShared->cancel) g_cancelRequested = true;
            }
        }
        return;
    }
    const char* npos = strstr(s, "native.");
    if (npos) s = (char*)npos + 7;
    char* par = strchr(s, '(');
    char nm[64] = {};
    const char* av[8] = {};
    int n = 0;
    if (par) {
        size_t nmLen = (size_t)(par - s);
        if (nmLen > 0 && nmLen < sizeof(nm)) {
            memcpy(nm, s, nmLen);
            nm[nmLen] = '\0';
        }
        const char* cl = strchr(par, ')');
        std::vector<std::string> raw;
        SplitArgs(par + 1, cl, raw);
        for (auto& a : raw) {
            if (n >= 8) break;
            std::string ev = EvalArgText(a);
            av[n++] = _strdup(ev.c_str());
        }
    } else {
        strncpy(nm, s, sizeof(nm) - 1);
    }
    char out[768] = {};
    if (RunNativeCmd(nm, av, n, out, sizeof(out))) {
        AppendOutput("native.");
        AppendOutput(nm);
        AppendOutput(" -> ");
        AppendOutput(out);
        AppendOutput("\n");
        char lb[256] = {};
        snprintf(lb, sizeof(lb), "native-exec: %s => %s", nm, out);
        Log(lb);
    } else {
        AppendOutput("ERR: unknown native cmd: ");
        AppendOutput(nm);
        AppendOutput("\n");
        char lb[256] = {};
        snprintf(lb, sizeof(lb), "native-exec: unknown/native unavailable cmd %s", nm);
        Log(lb);
    }
    for (int i = 0; i < n; i++) free((void*)av[i]);
}

typedef struct {
    int start;
    std::string cond;
} Clause;

static int ExecIfBlock(char* const* lines, int n, int start, bool* breakFlag) {
    char condBuf[256] = {};
    {
        char buf[512] = {};
        size_t llen = strlen(lines[start]);
        memcpy(buf, lines[start], (llen > sizeof(buf) - 1) ? sizeof(buf) - 1 : llen);
        char* t = buf + strlen(buf);
        while (t > buf && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '\r')) *--t = '\0';
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if (!ParseIfHeader(s, condBuf, sizeof(condBuf))) return -1;
    }
    int endIdx = FindMatchingEnd(lines, n, start);
    if (endIdx < 0) {
        AppendOutput("ERR: if without matching end\n");
        return -1;
    }
    std::vector<Clause> clauses;
    clauses.push_back({start, std::string(condBuf)});
    int elseStart = -1;
    for (int i = start + 1; i < endIdx; i++) {
        char buf[512] = {};
        size_t llen = strlen(lines[i]);
        memcpy(buf, lines[i], (llen > sizeof(buf) - 1) ? sizeof(buf) - 1 : llen);
        char* t = buf + strlen(buf);
        while (t > buf && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '\r')) *--t = '\0';
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        char cond2[256] = {};
        if (ParseElseIfHeader(s, cond2, sizeof(cond2))) {
            clauses.push_back({i, std::string(cond2)});
        } else if (IsElseLine(s)) {
            elseStart = i;
            break;
        }
    }
    int chosen = -1;
    for (auto& c : clauses) {
        if (Truthy(EvalExpr(c.cond.c_str()))) {
            chosen = c.start;
            break;
        }
    }
    if (chosen < 0 && elseStart >= 0) chosen = elseStart;
    if (chosen >= 0) {
        int to = endIdx;
        for (auto& c : clauses) {
            if (c.start > chosen && c.start < to) {
                to = c.start;
                break;
            }
        }
        if (elseStart > chosen && elseStart < to) to = elseStart;
        bool innerBrk = false;
        ExecNativeBlock(lines, n, chosen + 1, to, &innerBrk);
        if (innerBrk && breakFlag) *breakFlag = true;
    }
    return endIdx;
}

static void ExecNativeBlock(char* const* lines, int n, int start, int end, bool* breakFlag) {
    int i = start;
    while (i < end) {
        if (g_cancelRequested) return;   // GUI Stop -> abandon the rest of the script
        char buf[512] = {};
        size_t llen = strlen(lines[i]);
        memcpy(buf, lines[i], (llen > sizeof(buf) - 1) ? sizeof(buf) - 1 : llen);
        char* t = buf + strlen(buf);
        while (t > buf && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '\r' || t[-1] == '\n')) *--t = '\0';
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) {
            i++;
            continue;
        }
        if (s[0] == '-' && s[1] == '-') {
            i++;
            continue;
        }
        if (s[0] == '#') {
            i++;
            continue;
        }
        if (s[0] == 'i' && s[1] == 'f' && s[2] == ' ') {
            int skip = ExecIfBlock(lines, n, i, breakFlag);
            i = (skip > 0) ? skip + 1 : i + 1;
            continue;
        }
        char condBuf[256] = {};
        if (ParseWhileHeader(s, condBuf, sizeof(condBuf))) {
            int we = FindMatchingEnd(lines, n, i);
            if (we < 0) {
                AppendOutput("ERR: while without matching end\n");
                return;
            }
            while (Truthy(EvalExpr(condBuf))) {
                bool innerBrk = false;
                ExecNativeBlock(lines, n, i + 1, we, &innerBrk);
                if (innerBrk || g_cancelRequested) break;
            }
            i = we + 1;
            continue;
        }
        char varName[64] = {};
        if (ParseForHeader(s, varName, sizeof(varName), condBuf, sizeof(condBuf))) {
            int fe = FindMatchingEnd(lines, n, i);
            if (fe < 0) {
                AppendOutput("ERR: for without matching end\n");
                return;
            }
            char srcBuf[256] = {};
            strncpy(srcBuf, condBuf, sizeof(srcBuf) - 1);
            char* saveTok = nullptr;
            int part = 0;
            int from = 0, to = 0, step = 1;
            char* seg = strtok_s(srcBuf, ",", &saveTok);
            while (seg) {
                char tmp[64] = {};
                strncpy(tmp, seg, sizeof(tmp) - 1);
                char* tt = tmp + strlen(tmp);
                while (tt > tmp && (tt[-1] == ' ' || tt[-1] == '\t')) *--tt = '\0';
                char* ss = tmp;
                while (*ss == ' ' || *ss == '\t') ss++;
                double dv = StrToD(ss);
                if (part == 0) from = (int)dv;
                else if (part == 1) to = (int)dv;
                else if (part == 2) step = (int)dv;
                part++;
                seg = strtok_s(nullptr, ",", &saveTok);
            }
            if (step == 0) step = 1;
            bool innerBrk = false;
            bool stop = false;
            if (step > 0) {
                for (int v = from; v <= to; v += step) {
                    g_nativeVars[varName] = NumToStr((double)v);
                    ExecNativeBlock(lines, n, i + 1, fe, &innerBrk);
                    if (innerBrk || g_cancelRequested) stop = true;
                    if (stop) break;
                }
            } else {
                for (int v = from; v >= to; v += step) {
                    g_nativeVars[varName] = NumToStr((double)v);
                    ExecNativeBlock(lines, n, i + 1, fe, &innerBrk);
                    if (innerBrk || g_cancelRequested) stop = true;
                    if (stop) break;
                }
            }
            i = fe + 1;
            continue;
        }
        if (ParseElseIfHeader(s, condBuf, sizeof(condBuf))) {
            i++;
            continue;
        }
        if (IsElseLine(s)) {
            i++;
            continue;
        }
        if (s[0] == 'e' && s[1] == 'n' && s[2] == 'd' && IsLineEnd(s + 3)) {
            i++;
            continue;
        }
        if (strncmp(s, "break", 5) == 0 && IsLineEnd(s + 5)) {
            if (breakFlag) *breakFlag = true;
            return;
        }
        char vname[64] = {};
        char ebuf[256] = {};
        if (ParseAssign(s, vname, sizeof(vname), ebuf, sizeof(ebuf))) {
            g_nativeVars[vname] = EvalExpr(ebuf);
            i++;
            continue;
        }
        ExecNativeLine(s);
        i++;
    }
}

static void ExecuteNativeScript(const char* code) {
    if (!code) return;
    Log("ExecuteNativeScript: script execution start");
    g_nativeVars.clear();
    size_t len = strlen(code);
    char* copy = _strdup(code);
    int n = 0;
    for (const char* p = copy; *p; p++) if (*p == '\n') n++;
    char** lines = (char**)calloc((size_t)n + 1, sizeof(char*));
    int cnt = 0;
    {
        char* save = nullptr;
        char* tok = strtok_s(copy, "\n", &save);
        while (tok) {
            lines[cnt++] = tok;
            tok = strtok_s(nullptr, "\n", &save);
        }
    }
    ExecNativeBlock(lines, cnt, 0, cnt, nullptr);
    free(lines);
    free(copy);
}

/* ===========================================================================
 * Real Lua 5.1 runtime.
 * Scripts now run in an actual Lua VM (full syntax: functions, tables,
 * closures, metatables, string/math/table libs, pcall, loadstring).
 * The Mini World-style API below is a thin wrapper over RunNativeCmd so
 * scripts read like game API code:  Player:setHealth(1000)
 * ======================================================================== */

// Shared value formatter used by print() and script return values.
static void LuaFormatValue(lua_State* L, int i, std::string& out) {
    int t = lua_type(L, i);
    switch (t) {
    case LUA_TSTRING: {
        size_t len = 0;
        const char* s = lua_tolstring(L, i, &len);
        out.append(s ? s : "", len);
        break;
    }
    case LUA_TNUMBER: {
        char b[40];
        double v = lua_tonumber(L, i);
        if (v == (double)(long long)v && v > -9e15 && v < 9e15)
            snprintf(b, sizeof(b), "%lld", (long long)v);
        else
            snprintf(b, sizeof(b), "%g", v);
        out += b;
        break;
    }
    case LUA_TBOOLEAN: out += lua_toboolean(L, i) ? "true" : "false"; break;
    case LUA_TNIL:     out += "nil"; break;
    default: {
        const void* p = lua_topointer(L, i);
        char b[48];
        snprintf(b, sizeof(b), "%s: %p", lua_typename(L, t), (void*)(uintptr_t)p);
        out += b;
        break;
    }
    }
}

static int LuaPrint(lua_State* L) {
    int n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        if (i > 1) out += "  ";
        LuaFormatValue(L, i, out);
    }
    out += "\n";
    AppendOutput(out.c_str());
    return 0;
}

// GUI STOP writes shared->cancel; mirror it into the executor abort flag.
// Without this the Lua path never notices the STOP button.
static void PollCancel() {
    if (!g_cancelRequested && g_pShared && g_pShared->cancel)
        g_cancelRequested = true;
}

// Interruptible wait: sleeps in 25ms slices so STOP aborts quickly.
static int LuaWait(lua_State* L) {
    double secs = luaL_optnumber(L, 1, 0.05);
    if (secs < 0) secs = 0;
    if (secs > 3600) secs = 3600;
    DWORD remain = (DWORD)(secs * 1000.0);
    while (remain > 0) {
        PollCancel();
        if (g_cancelRequested) break;
        GuiPumpEvents();                       // dispatch overlay clicks
        DWORD slice = remain > 25 ? 25 : remain;
        Sleep(slice);
        remain -= slice;
    }
    return 0;
}

// Instruction-count hook: raises an error when the GUI pressed STOP so even
// tight infinite loops without wait() get aborted.
static void LuaCancelHook(lua_State* L, lua_Debug*) {
    PollCancel();
    if (g_cancelRequested) luaL_error(L, "cancelled");
}

static int LuaPanic(lua_State* L) {
    (void)L;
    AppendOutput("ERR: Lua panic (protected state)");
    return 0;
}

// Generic native-command dispatcher. The command name rides in an upvalue.
// Accepts both dot calls (Player.getPos()) and colon calls (Player:getPos())
// - a leading table argument (self) is skipped automatically.
// Every call is echoed to the console as "[cmd] result" (toggle via
// native.echo(false)) so bare statements are never silently ignored.
static int LuaNativeDispatch(lua_State* L) {
    const char* cmd = (const char*)lua_touserdata(L, lua_upvalueindex(1));
    if (!cmd) { lua_pushnil(L); lua_pushliteral(L, "bad binding"); return 2; }
    int top = lua_gettop(L);
    int argStart = 1;
    if (top >= 1 && lua_istable(L, 1)) argStart = 2;   // colon-call self
    int nargs = top - argStart + 1;
    if (nargs < 0) nargs = 0;
    if (nargs > 8) nargs = 8;
    const char* args[8] = {};
    char numbufs[8][40];
    for (int i = 0; i < nargs; i++) {
        int li = argStart + i;
        switch (lua_type(L, li)) {
        case LUA_TNUMBER: {
            double v = lua_tonumber(L, li);
            // Integral values must stay exact - uids (~1.3e9) were being
            // mangled by %.9g scientific notation (-> atoi read "1").
            double iv = (double)(long long)v;
            if (v == iv && v > -9e15 && v < 9e15)
                snprintf(numbufs[i], sizeof(numbufs[i]), "%lld", (long long)v);
            else
                snprintf(numbufs[i], sizeof(numbufs[i]), "%.9g", v);
            args[i] = numbufs[i];
            break;
        }
        case LUA_TBOOLEAN:
            args[i] = lua_toboolean(L, li) ? "1" : "0";
            break;
        case LUA_TSTRING:
            args[i] = lua_tostring(L, li);
            break;
        default:
            args[i] = nullptr;
            break;
        }
    }
    char out[512] = {};
    bool known = RunNativeCmd(cmd, args, nargs, out, sizeof(out));

    // Build a compact console summary regardless of return shape.
    char summary[160];
    if (!known) {
        snprintf(summary, sizeof(summary), "unknown command");
    } else if (strncmp(out, "ERR:", 4) == 0 || strncmp(out, "OK", 2) == 0 ||
               strcmp(out, "ON") == 0 || strcmp(out, "OFF") == 0) {
        snprintf(summary, sizeof(summary), "%s", out);
    } else {
        // numbers ("42" / "10,20,30") or plain string - copy trimmed
        char* nl = strchr(out, '\n');
        if (nl) *nl = '\0';
        snprintf(summary, sizeof(summary), "%s", out);
    }

    if (g_nativeEcho) {
        char line[224];
        snprintf(line, sizeof(line), "[%s] %s\n", cmd, summary);
        AppendOutput(line);
    }

    if (!known) { lua_pushnil(L); lua_pushfstring(L, "unknown command '%s'", cmd); return 2; }

    // Interpret the textual result into natural Lua values.
    if (strncmp(out, "ERR:", 4) == 0) {
        lua_pushnil(L);
        lua_pushstring(L, out);
        return 2;
    }
    if (strcmp(out, "ON") == 0 || strcmp(out, "OFF") == 0) {
        lua_pushboolean(L, out[1] == 'N');
        return 1;
    }
    if (strncmp(out, "OK", 2) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }
    // All-numeric output ("42" or "10,20,30") -> numbers (multi-return).
    double vals[8];
    int cnt = 0;
    bool allNum = out[0] != '\0';
    const char* p = out;
    while (*p) {
        char* endp = nullptr;
        double v = strtod(p, &endp);
        if (endp == p) { allNum = false; break; }
        if (cnt < 8) vals[cnt++] = v;
        while (*endp == ' ') endp++;
        if (*endp == ',') { p = endp + 1; while (*p == ' ') p++; continue; }
        if (*endp != '\0') allNum = false;
        break;
    }
    if (allNum && cnt > 0) {
        for (int i = 0; i < cnt; i++) lua_pushnumber(L, vals[i]);
        return cnt;
    }
    lua_pushstring(L, out);
    return 1;
}

// ---- gui.* : in-game ImGui overlay (declarative windows built from Lua) ----
// Handles are window index+1 so they are always truthy in Lua.
// Widget callbacks are invoked on the script thread via GuiPumpEvents(),
// which LuaWait calls during its 25ms slices (Lua is not thread-safe).

// Persistent VM handle - lives here so gui callbacks can run scripts' state.
static lua_State* g_L = nullptr;

void GuiPumpEvents() {
    if (!g_L) return;
    int wIdx, wgt;
    while (Overlay::PopEvent(&wIdx, &wgt)) {
        int cbRef = 0;
        Overlay::Widget::Type type = Overlay::Widget::LABEL;
        bool state = false; float f1 = 0, f2 = 0, f3 = 0; int vk = 0;
        int idx = 0; const char* textVal = nullptr;
        {
            int n;
            Overlay::Window* ws = Overlay::LockWindows(&n);
            if (wIdx >= 0 && wIdx < Overlay::MAX_WINDOWS &&
                wgt >= 0 && wgt < ws[wIdx].count) {
                Overlay::Widget& wd = ws[wIdx].widgets[wgt];
                cbRef = wd.cbRef;
                type  = wd.type;
                state = wd.bval;
                idx   = wd.valIdx + 1;
                f1    = wd.fval;
                vk    = (int)wd.fmin;
                f2    = wd.col[0]; f3 = wd.col[1];
                textVal = wd.text;
            }
            Overlay::UnlockWindows();
        }
        if (!cbRef) continue;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, cbRef);
        if (lua_isfunction(g_L, -1)) {
            int pushArgs = 0;
            switch (type) {
            case Overlay::Widget::TOGGLE:
                lua_pushboolean(g_L, state); pushArgs = 1; break;
            case Overlay::Widget::KEYBIND:
                lua_pushinteger(g_L, vk); pushArgs = 1; break;
            case Overlay::Widget::DROPDOWN:
            case Overlay::Widget::COMBO:
            case Overlay::Widget::LISTBOX:
                lua_pushinteger(g_L, (type == Overlay::Widget::DROPDOWN)
                                     ? idx : (int)f1 + 1); pushArgs = 1; break;
            case Overlay::Widget::INPUT:
                lua_pushstring(g_L, textVal ? textVal : ""); pushArgs = 1; break;
            case Overlay::Widget::COLOR: {
                // re-read full color under lock
                int n3;
                Overlay::Window* ws = Overlay::LockWindows(&n3);
                if (wIdx >= 0 && wIdx < Overlay::MAX_WINDOWS &&
                    wgt >= 0 && wgt < ws[wIdx].count) {
                    lua_pushnumber(g_L, ws[wIdx].widgets[wgt].col[0]);
                    lua_pushnumber(g_L, ws[wIdx].widgets[wgt].col[1]);
                    lua_pushnumber(g_L, ws[wIdx].widgets[wgt].col[2]);
                } else { lua_pushnumber(g_L, 0); lua_pushnumber(g_L, 0); lua_pushnumber(g_L, 0); }
                Overlay::UnlockWindows();
                pushArgs = 3; break;
            }
            default: break;   // buttons fire with no args
            }
            if (lua_pcall(g_L, pushArgs, 0, 0) != 0) {
                const char* e = lua_tostring(g_L, -1);
                AppendOutput((std::string("[gui] callback error: ") + (e ? e : "?") + "\n").c_str());
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
}

static int GuiStart(lua_State* L) {
    (void)L;
    if (Overlay::Running()) { lua_pushboolean(L, 1); return 1; }
    if (!Overlay::Start()) {
        lua_pushnil(L);
        lua_pushliteral(L, "overlay init failed (game window or d3d11 not found)");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int GuiShow(lua_State* L) {
    bool v = lua_isnoneornil(L, 1) ? !Overlay::Visible() : lua_toboolean(L, 1) != 0;
    Overlay::SetVisible(v);
    lua_pushboolean(L, Overlay::Visible());
    return 1;
}

static int GuiReset(lua_State* L) {
    Overlay::Window* ws = Overlay::LockWindows(nullptr);
    for (int w = 0; w < Overlay::MAX_WINDOWS; w++) {
        for (int i = 0; i < ws[w].count; i++) {
            if (ws[w].widgets[i].cbRef)
                luaL_unref(L, LUA_REGISTRYINDEX, ws[w].widgets[i].cbRef);
        }
        for (int i = 0; i < Overlay::MAX_WIDGETS; i++)
            ws[w].widgets[i].cbRef = 0;
        memset(&ws[w], 0, sizeof(ws[w]));
    }
    Overlay::SetWindowCount(0);
    Overlay::UnlockWindows();
    lua_pushboolean(L, 1);
    return 1;
}

static int GuiWindow(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    int n = 0;
    Overlay::Window* ws = Overlay::LockWindows(&n);
    int idx = -1;
    for (int i = 0; i < Overlay::MAX_WINDOWS; i++) {
        if (strncmp(ws[i].title, title, sizeof(ws[i].title)) == 0) { idx = i; break; }
        if (idx < 0 && !ws[i].title[0]) idx = i;
    }
    if (idx >= 0) {
        strncpy(ws[idx].title, title, sizeof(ws[idx].title) - 1);
        ws[idx].title[sizeof(ws[idx].title) - 1] = '\0';
        ws[idx].visible = true;
        if (idx + 1 > n) Overlay::SetWindowCount(idx + 1);
        lua_pushinteger(L, idx + 1);
    } else {
        lua_pushnil(L);
    }
    Overlay::UnlockWindows();
    return 1;
}

// Resolve a window handle. Returns a LOCKED Window* (caller MUST call
// Overlay::UnlockWindows) or nullptr after raising a Lua argument error.
static Overlay::Window* GuiGetWin(lua_State* L, int arg) {
    lua_Integer h = luaL_checkinteger(L, arg);
    if (h < 1 || h > Overlay::MAX_WINDOWS) {
        luaL_argerror(L, arg, "bad window handle");
        return nullptr;
    }
    Overlay::Window* ws = Overlay::LockWindows(nullptr);
    if (!ws[h - 1].title[0]) {
        Overlay::UnlockWindows();
        luaL_argerror(L, arg, "window not open");
        return nullptr;
    }
    return &ws[h - 1];
}

static int RefCallback(lua_State* L, int idx) {
    if (!lua_isfunction(L, idx)) return 0;
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

// Parse "a|b|c" into wd.opts (up to 8 entries). Shared by dropdown/combo/listbox.
static void ParseOpts(Overlay::Widget& wd, const char* opts) {
    const char* p = opts;
    while (*p && wd.optCount < 8) {
        const char* bar = strchr(p, '|');
        size_t n = bar ? (size_t)(bar - p) : strlen(p);
        if (n > 15) n = 15;
        memcpy(wd.opts[wd.optCount], p, n);
        wd.opts[wd.optCount][n] = 0;
        wd.optCount++;
        p = bar ? bar + 1 : p + strlen(p);
    }
    if (wd.optCount == 0) { wd.optCount = 1; strcpy(wd.opts[0], "-"); }
}

static int GuiLabel(lua_State* L) {
    const char* text = luaL_checkstring(L, 2);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::LABEL;
            strncpy(wd.label, text, sizeof(wd.label) - 1);
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

static int GuiButton(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    int cbRef = RefCallback(L, 3);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::BUTTON;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

static int GuiToggle(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    bool def = false;
    int fnIdx = 3;
    if (lua_isboolean(L, 3)) { def = lua_toboolean(L, 3) != 0; fnIdx = 4; }
    int cbRef = RefCallback(L, fnIdx);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::TOGGLE;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.bval = def;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

static int GuiSlider(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    float mn = (float)luaL_checknumber(L, 3);
    float mx = (float)luaL_checknumber(L, 4);
    float def = lua_isnumber(L, 5) ? (float)lua_tonumber(L, 5) : mn;
    if (def < mn) def = mn; if (def > mx) def = mx;
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::SLIDER;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.fmin = mn; wd.fmax = mx; wd.fval = def;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

static int GuiGet(lua_State* L) {
    lua_Integer h = luaL_checkinteger(L, 1);
    lua_Integer wid = luaL_checkinteger(L, 2) - 1;
    if (h < 1 || h > Overlay::MAX_WINDOWS) { lua_pushnil(L); return 1; }
    int n;
    Overlay::Window* ws = Overlay::LockWindows(&n);
    Overlay::Window& w = ws[(int)h - 1];
    if (wid < 0 || wid >= w.count) { Overlay::UnlockWindows(); lua_pushnil(L); return 1; }
    const Overlay::Widget& wd = w.widgets[(int)wid];
    switch (wd.type) {
    case Overlay::Widget::TOGGLE:   lua_pushboolean(L, wd.bval); break;
    case Overlay::Widget::SLIDER:   lua_pushnumber(L, wd.fval); break;
    case Overlay::Widget::DROPDOWN: lua_pushinteger(L, wd.valIdx + 1); break;
    case Overlay::Widget::KEYBIND:  lua_pushinteger(L, (lua_Integer)wd.fmin); break;
    case Overlay::Widget::INPUT:    lua_pushstring(L, wd.text); break;
    case Overlay::Widget::COLOR:    lua_pushnumber(L, wd.col[0]); lua_pushnumber(L, wd.col[1]); lua_pushnumber(L, wd.col[2]); Overlay::UnlockWindows(); return 3;
    case Overlay::Widget::PROGRESS: lua_pushnumber(L, wd.fval); break;
    case Overlay::Widget::COMBO:    lua_pushinteger(L, (lua_Integer)wd.fval + 1); break;
    case Overlay::Widget::LISTBOX:  lua_pushinteger(L, (lua_Integer)wd.fval + 1); break;
    default: lua_pushnil(L); break;
    }
    Overlay::UnlockWindows();
    return 1;
}

// ---- http.get(url) -> body string (script-hub loader) ----
// Runs on our worker thread; blocking with a 10s timeout. HTTPS supported.
static int LuaHttpGet(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    HINTERNET hInet = InternetOpenA("Zelvex/4.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                    nullptr, nullptr, 0);
    if (!hInet) { lua_pushnil(L); lua_pushliteral(L, "InternetOpen failed"); return 2; }
    DWORD timeout = 10000;
    InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    if (_strnicmp(url, "https:", 6) == 0) flags |= INTERNET_FLAG_SECURE;
    HINTERNET hUrl = InternetOpenUrlA(hInet, url, nullptr, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInet);
        lua_pushnil(L);
        lua_pushliteral(L, "request failed");
        return 2;
    }
    std::string body;
    char buf[8192];
    DWORD rd = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &rd) && rd > 0)
        body.append(buf, rd);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInet);
    lua_pushlstring(L, body.data(), (size_t)body.size());
    return 1;
}

// ---- input.key(vkCode) -> true while the key is held (script hotkeys) ----
static int LuaInputKey(lua_State* L) {
    lua_Integer vk = luaL_checkinteger(L, 1);
    short s = GetAsyncKeyState((int)vk);
    lua_pushboolean(L, (s & 0x8000) != 0);
    return 1;
}

static int GuiRadar(lua_State* L) {
    float radius = (float)luaL_optnumber(L, 2, 110);
    float style  = (float)luaL_optnumber(L, 3, 1);
    float range  = (float)luaL_optnumber(L, 4, 6000);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::RADAR;
            strncpy(wd.label, "radar", sizeof(wd.label) - 1);
            wd.fmin = radius; wd.fmax = style; wd.fval = range;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.config(h, id, radiusPx, style, range) - mutate a RADAR widget live
static int GuiConfig(lua_State* L) {
    lua_Integer h = luaL_checkinteger(L, 1);
    lua_Integer wid = luaL_checkinteger(L, 2) - 1;
    float radius = (float)luaL_checknumber(L, 3);
    float style  = (float)luaL_checknumber(L, 4);
    float range  = (float)luaL_checknumber(L, 5);
    if (h < 1 || h > Overlay::MAX_WINDOWS) { lua_pushboolean(L, 0); return 1; }
    int n;
    Overlay::Window* ws = Overlay::LockWindows(&n);
    Overlay::Window& w = ws[(int)h - 1];
    bool ok = false;
    if (wid >= 0 && wid < w.count && w.widgets[(int)wid].type == Overlay::Widget::RADAR) {
        w.widgets[(int)wid].fmin = radius;
        w.widgets[(int)wid].fmax = style;
        w.widgets[(int)wid].fval = range;
        ok = true;
    }
    Overlay::UnlockWindows();
    lua_pushboolean(L, ok);
    return 1;
}

// gui.clear(h) - wipe one window's widgets (dynamic lists)
static int GuiClear(lua_State* L) {
    lua_Integer h = luaL_checkinteger(L, 1);
    if (h < 1 || h > Overlay::MAX_WINDOWS) { lua_pushboolean(L, 0); return 1; }
    Overlay::Window* ws = Overlay::LockWindows(nullptr);
    Overlay::Window& w = ws[(int)h - 1];
    bool ok = w.title[0] != 0;
    if (ok) {
        for (int i = 0; i < w.count; i++) {
            if (w.widgets[i].cbRef)
                luaL_unref(L, LUA_REGISTRYINDEX, w.widgets[i].cbRef);
            w.widgets[i].cbRef = 0;
        }
        w.count = 0;
    }
    Overlay::UnlockWindows();
    lua_pushboolean(L, ok);
    return 1;
}

// gui.set(h, id, value) - live-update a widget from script:
//   SLIDER/PROGRESS -> number (fval), TOGGLE -> boolean, COMBO/LISTBOX -> index
static int GuiSet(lua_State* L) {
    lua_Integer h = luaL_checkinteger(L, 1);
    lua_Integer wid = luaL_checkinteger(L, 2) - 1;
    if (h < 1 || h > Overlay::MAX_WINDOWS) { lua_pushboolean(L, 0); return 1; }
    int n;
    Overlay::Window* ws = Overlay::LockWindows(&n);
    Overlay::Window& w = ws[(int)h - 1];
    bool ok = false;
    if (wid >= 0 && wid < w.count) {
        Overlay::Widget& wd = w.widgets[(int)wid];
        switch (wd.type) {
        case Overlay::Widget::SLIDER:
        case Overlay::Widget::PROGRESS: {
            float v = (float)luaL_checknumber(L, 3);
            if (wd.type == Overlay::Widget::SLIDER) {
                if (v < wd.fmin) v = wd.fmin;
                if (v > wd.fmax) v = wd.fmax;
            } else {
                if (v < 0) v = 0; if (v > 1) v = 1;
            }
            wd.fval = v; ok = true; break;
        }
        case Overlay::Widget::TOGGLE:
            wd.bval = lua_toboolean(L, 3) != 0; ok = true; break;
        case Overlay::Widget::COMBO:
        case Overlay::Widget::LISTBOX: {
            int v = (int)luaL_checkinteger(L, 3) - 1;
            if (v >= 0 && v < wd.optCount) { wd.fval = (float)v; ok = true; }
            break;
        }
        default: break;
        }
    }
    Overlay::UnlockWindows();
    lua_pushboolean(L, ok);
    return 1;
}

// gui.opts(h, id, "a|b|c") - replace dropdown/combo/listbox options live
// (player lists etc). Keeps current selection if still valid, else selects 1.
static int GuiOpts(lua_State* L) {
    lua_Integer h = luaL_checkinteger(L, 1);
    lua_Integer wid = luaL_checkinteger(L, 2) - 1;
    const char* opts = luaL_checkstring(L, 3);
    if (h < 1 || h > Overlay::MAX_WINDOWS) { lua_pushboolean(L, 0); return 1; }
    int n;
    Overlay::Window* ws = Overlay::LockWindows(&n);
    Overlay::Window& w = ws[(int)h - 1];
    bool ok = false;
    if (wid >= 0 && wid < w.count) {
        Overlay::Widget& wd = w.widgets[(int)wid];
        if (wd.type == Overlay::Widget::DROPDOWN || wd.type == Overlay::Widget::COMBO ||
            wd.type == Overlay::Widget::LISTBOX) {
            unsigned char keep = (wd.type == Overlay::Widget::DROPDOWN)
                                 ? (unsigned char)wd.valIdx : (unsigned char)(int)wd.fval;
            wd.optCount = 0;
            ParseOpts(wd, opts);
            if (keep >= wd.optCount) keep = 0;
            if (wd.type == Overlay::Widget::DROPDOWN) wd.valIdx = keep;
            else wd.fval = (float)keep;
            ok = true;
        }
    }
    Overlay::UnlockWindows();
    lua_pushboolean(L, ok);
    return 1;
}

// gui.accent(r,g,b in 0..1) - live theme recolor
static int GuiAccent(lua_State* L) {
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    Overlay::SetAccent(r, g, b);
    lua_pushboolean(L, 1);
    return 1;
}

// ---- http.post(url, body [,contentType]) -> response body ----
static int LuaHttpPost(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    size_t bodyLen = 0;
    const char* body = luaL_optlstring(L, 2, "", &bodyLen);
    const char* ctype = luaL_optstring(L, 3, "application/json");

    // crack url: [https://]host[:port]/path
    bool https = _strnicmp(url, "https:", 6) == 0;
    const char* p = strstr(const_cast<char*>(url), "://");
    p = p ? p + 3 : url;
    char host[256] = {}; const char* path = "/";
    const char* slash = strchr(p, '/');
    if (slash) { size_t n = slash - p; if (n >= sizeof(host)) n = sizeof(host) - 1;
                 memcpy(host, p, n); path = slash; }
    else { strncpy(host, p, sizeof(host) - 1); }
    int port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    char* colon = strchr(host, ':');
    if (colon) { *colon = 0; port = atoi(colon + 1); }

    HINTERNET hInet = InternetOpenA("Zelvex/4.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                    nullptr, nullptr, 0);
    if (!hInet) { lua_pushnil(L); lua_pushliteral(L, "InternetOpen failed"); return 2; }
    DWORD timeout = 10000;
    InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET hConn = InternetConnectA(hInet, host, port, nullptr, nullptr,
                                       INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hInet);
        lua_pushnil(L); lua_pushliteral(L, "connect failed"); return 2; }
    DWORD openFlags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    if (https) openFlags |= INTERNET_FLAG_SECURE;
    HINTERNET hReq = HttpOpenRequestA(hConn, "POST", path, nullptr, nullptr,
                                      nullptr, openFlags, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hInet);
        lua_pushnil(L); lua_pushliteral(L, "open failed"); return 2; }
    char hdrs[256];
    snprintf(hdrs, sizeof(hdrs), "Content-Type: %s\r\n", ctype);
    BOOL sent = HttpSendRequestA(hReq, hdrs, -1, (LPVOID)body, (DWORD)bodyLen);
    if (!sent) {
        InternetCloseHandle(hReq); InternetCloseHandle(hConn); InternetCloseHandle(hInet);
        lua_pushnil(L); lua_pushliteral(L, "send failed");
        return 2;
    }
    std::string resp;
    char rbuf[8192]; DWORD rd = 0;
    while (InternetReadFile(hReq, rbuf, sizeof(rbuf), &rd) && rd > 0)
        resp.append(rbuf, rd);
    InternetCloseHandle(hReq); InternetCloseHandle(hConn); InternetCloseHandle(hInet);
    lua_pushlstring(L, resp.data(), (size_t)resp.size());
    return 1;
}

// gui.section(w, title) -> collapsible header; following widgets hide when closed
static int GuiSection(lua_State* L) {
    const char* title = luaL_checkstring(L, 2);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::SECTION;
            strncpy(wd.label, title, sizeof(wd.label) - 1);
            wd.bval = true;   // open by default
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.dropdown(w, label, "a|b|c" [,defaultIdx] [,fn(newIdx)]) -> id
static int GuiDropdown(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    const char* opts = luaL_checkstring(L, 3);
    int def = lua_isnumber(L, 4) ? (int)lua_tointeger(L, 4) : 1;
    int cbRef = RefCallback(L, 5);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::DROPDOWN;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            ParseOpts(wd, opts);
            if (def < 1 || def > wd.optCount) def = 1;
            wd.valIdx = def - 1;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.keybind(w, label [,initialVk] [,fn]) -> id ; fires fn on each press
static int GuiKeybind(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    int vk = lua_isnumber(L, 3) ? (int)lua_tointeger(L, 3) : 0;
    int cbRef = RefCallback(L, 4);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::KEYBIND;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.fmin = (float)vk;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.input(w, label [,defaultText] [,fn(text)]) -> id
static int GuiInput(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    const char* def = luaL_optstring(L, 3, "");
    int cbRef = RefCallback(L, 4);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::INPUT;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            strncpy(wd.text, def, sizeof(wd.text) - 1);
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}
// gui.color(w, label [,r,g,b] [,fn(r,g,b)]) -> id
static int GuiColor(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    float r = (float)luaL_optnumber(L, 3, 0.24);
    float g = (float)luaL_optnumber(L, 4, 0.55);
    float b = (float)luaL_optnumber(L, 5, 1.0);
    int cbRef = RefCallback(L, 6);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::COLOR;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.col[0]=r; wd.col[1]=g; wd.col[2]=b; wd.col[3]=1.0f;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}
static int GuiSeparator(lua_State* L) {
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::SEPARATOR;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}
static int GuiSameLine(lua_State* L) {
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::SAMELINE;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}
static int GuiProgress(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    float frac = (float)luaL_optnumber(L, 3, 0.0);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::PROGRESS;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.fval = frac;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}
static int GuiCombo(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    const char* opts = luaL_checkstring(L, 3);
    int def = lua_isnumber(L, 4) ? (int)lua_tointeger(L, 4) : 1;
    int cbRef = RefCallback(L, 5);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::COMBO;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            ParseOpts(wd, opts);
            if (def < 1 || def > wd.optCount) def = 1;
            wd.fval = (float)(def - 1);
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.listbox(w, label, "a|b|c" [,defaultIdx] [,fn(newIdx)]) -> id
static int GuiListbox(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    const char* opts = luaL_checkstring(L, 3);
    int def = lua_isnumber(L, 4) ? (int)lua_tointeger(L, 4) : 1;
    int cbRef = RefCallback(L, 5);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::LISTBOX;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            ParseOpts(wd, opts);
            if (def < 1 || def > wd.optCount) def = 1;
            wd.fval = (float)(def - 1);
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.textColored(w, text, r, g, b [,a]) -> id
static int GuiTextColored(lua_State* L) {
    const char* text = luaL_checkstring(L, 2);
    float r = (float)luaL_optnumber(L, 3, 1.0);
    float g = (float)luaL_optnumber(L, 4, 1.0);
    float b = (float)luaL_optnumber(L, 5, 1.0);
    float a = (float)luaL_optnumber(L, 6, 1.0);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::TEXTCOLORED;
            strncpy(wd.label, text, sizeof(wd.label) - 1);
            wd.col[0]=r; wd.col[1]=g; wd.col[2]=b; wd.col[3]=a;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.smallButton(w, label [,fn]) -> id
static int GuiSmallButton(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    int cbRef = RefCallback(L, 3);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::SMALLBUTTON;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.tab(w, "PageName") -> starts a sidebar page; widgets after it belong
// to this page until the next tab. Widgets BEFORE any tab = header zone.
static int GuiTab(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::PAGETAB;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.toggleDesc(w, "Title", "description" [,default] [,fn(newstate)]) -> id
// Redz-style row: bold-ish title + gray subtitle, switch on the right.
static int GuiToggleDesc(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    const char* desc  = luaL_checkstring(L, 3);
    bool def = false;
    int fnIdx = 4;
    if (lua_isboolean(L, 4)) { def = lua_toboolean(L, 4) != 0; fnIdx = 5; }
    int cbRef = RefCallback(L, fnIdx);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::TOGGLE;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            strncpy(wd.desc, desc, sizeof(wd.desc) - 1);
            wd.bval = def;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.paragraph(w, "long text") -> rounded card with wrapped text
static int GuiParagraph(lua_State* L) {
    const char* text = luaL_checkstring(L, 2);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::LABEL;
            wd.flags |= 1;
            strncpy(wd.label, text, sizeof(wd.label) - 1);
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// gui.buttonColored(w, "Join", r, g, b [,fn]) -> full-width colored button
static int GuiButtonColored(lua_State* L) {
    const char* label = luaL_checkstring(L, 2);
    float r = (float)luaL_optnumber(L, 3, 0.18);   // green by default
    float g = (float)luaL_optnumber(L, 4, 0.80);
    float b = (float)luaL_optnumber(L, 5, 0.44);
    int cbRef = RefCallback(L, 6);
    int id = -1;
    if (Overlay::Window* w = GuiGetWin(L, 1)) {
        if (w->count < Overlay::MAX_WIDGETS) {
            Overlay::Widget& wd = w->widgets[w->count++];
            memset(&wd, 0, sizeof(wd));
            wd.type = Overlay::Widget::BUTTON;
            wd.flags |= 1;
            strncpy(wd.label, label, sizeof(wd.label) - 1);
            wd.col[0]=r; wd.col[1]=g; wd.col[2]=b; wd.col[3]=1.f;
            wd.cbRef = cbRef;
            id = w->count;
        }
        Overlay::UnlockWindows();
    }
    if (id < 0) return luaL_error(L, "gui: widget limit reached");
    lua_pushinteger(L, id);
    return 1;
}

// Esp.screen() -> swapchain width, height (cached by the render thread)
static int LuaEspScreen(lua_State* L) {
    lua_pushinteger(L, Overlay::DisplayWidth());
    lua_pushinteger(L, Overlay::DisplayHeight());
    return 2;
}

struct ApiMap { const char* name; const char* cmd; };

static void PushApiTable(lua_State* L, const ApiMap* m, int count) {
    lua_newtable(L);
    for (int i = 0; i < count; i++) {
        lua_pushlightuserdata(L, (void*)m[i].cmd);
        lua_pushcclosure(L, LuaNativeDispatch, 1);
        lua_setfield(L, -2, m[i].name);
    }
}

// ---- Net.send: raw packet crafter (generic sendToHost) ----
static int LuaNetSend(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    const char* json = luaL_optstring(L, 2, "{}");
    if (!FnOk(g_sendToHostAddr)) {
        lua_pushnil(L); lua_pushliteral(L, "sendToHost unresolved");
        return 2;
    }
    GiveItemJob* job = new GiveItemJob();
    job->msg = msg;
    job->json = json;
    HANDLE h = CreateThread(nullptr, 0, GiveItemThread, job, 0, nullptr);
    if (!h) { delete job; lua_pushnil(L); lua_pushliteral(L, "thread failed"); return 2; }
    WaitForSingleObject(h, 10000);
    CloseHandle(h);
    lua_pushboolean(L, 1);
    return 1;
}

static int LuaNetSniff(lua_State* L) {
    bool on = lua_toboolean(L, 1) != 0;
    if (on) {
        if (!InstallSniff()) { lua_pushnil(L); lua_pushliteral(L, "hook install failed"); return 2; }
        g_sniffOn = true;
        AppendOutput("[sniff] ON - change your name once, then Net.sniff(false)\n");
    } else {
        g_sniffOn = false;
        AppendOutput("[sniff] OFF\n");
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---- Players.list(): live scan of the in-memory player list ----
// Layout from CE reference: [[libMiniBaseGame.dll+B36C]+78]+68 -> 40 slots,
// each entry: uid @+0, pos ints @+14/+18/+1C, faction @+B0 (1..3 real).
static int LuaPlayersList(lua_State* L) {
    lua_newtable(L);
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (!mbH || !IsReadable((BYTE*)mbH + 0xB36C, 4)) return 1;
    BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
    if (!IsReadable(c1, 0x80) || !IsReadable(c1 + 0x78, 4)) return 1;
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!IsReadable(c2, 0x70) || !IsReadable(c2 + 0x68, 4)) return 1;
    BYTE* list = *(BYTE**)(c2 + 0x68);
    uint32_t myUid = ReadRoleId();
    int out = 1;
    for (int i = 0; i < 40; i++) {
        if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
        BYTE* p = *(BYTE**)((BYTE*)list + i * 4);
        if (!PlayerEntryValid(p, myUid, false, 1, 64)) continue;
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, (lua_Integer)*(uint32_t*)p); lua_setfield(L, -2, "uid");
        lua_pushinteger(L, *(int*)(p + 0x14));     lua_setfield(L, -2, "x");
        lua_pushinteger(L, *(int*)(p + 0x18));     lua_setfield(L, -2, "y");
        lua_pushinteger(L, *(int*)(p + 0x1C));     lua_setfield(L, -2, "z");
        lua_pushinteger(L, *(int*)(p + 0xB0));     lua_setfield(L, -2, "team");
        lua_rawseti(L, -2, out++);
    }
    return 1;
}

static int LuaPlayersCount(lua_State* L) {
    // Count entries directly from the live player list.
    int count = 0;
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (mbH && IsReadable((BYTE*)mbH + 0xB36C, 4)) {
        BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
        if (IsReadable(c1, 0x80) && IsReadable(c1 + 0x78, 4)) {
            BYTE* c2 = *(BYTE**)(c1 + 0x78);
            if (IsReadable(c2, 0x70) && IsReadable(c2 + 0x68, 4)) {
                BYTE* list = *(BYTE**)(c2 + 0x68);
                uint32_t myUid = ReadRoleId();
                for (int i = 0; i < 40; i++) {
                    if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
                    BYTE* p = *(BYTE**)((BYTE*)list + i * 4);
                    if (!PlayerEntryValid(p, myUid, false, 1, 64)) continue;
                    count++;
                }
            }
        }
    }
    lua_pushinteger(L, count);
    return 1;
}

// Players.nearest([maxDist]) -> nearest real player table or nil.
static int LuaPlayersNearest(lua_State* L) {
    double maxD = luaL_optnumber(L, 1, 1e18);
    int* bx = BlockCoord(0xDC); int* by = BlockCoord(0xE0); int* bz = BlockCoord(0xE4);
    if (!bx || !by || !bz) { lua_pushnil(L); return 1; }
    int mx = *bx, my = *by, mz = *bz;
    HMODULE mbH = GetModuleHandleA("libMiniBaseGame.dll");
    if (!mbH || !IsReadable((BYTE*)mbH + 0xB36C, 4)) { lua_pushnil(L); return 1; }
    BYTE* c1 = *(BYTE**)((BYTE*)mbH + 0xB36C);
    if (!IsReadable(c1, 0x80) || !IsReadable(c1 + 0x78, 4)) { lua_pushnil(L); return 1; }
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!IsReadable(c2, 0x70) || !IsReadable(c2 + 0x68, 4)) { lua_pushnil(L); return 1; }
    BYTE* list = *(BYTE**)(c2 + 0x68);
    uint32_t myUid = ReadRoleId();
    BYTE* best = nullptr;
    double bestD = maxD * maxD;
    for (int i = 0; i < 40; i++) {
        if (!IsReadable((BYTE*)list + i * 4, 4)) continue;
        BYTE* p = *(BYTE**)((BYTE*)list + i * 4);
        if (!PlayerEntryValid(p, myUid, false, 1, 64)) continue;
        double dx = *(int*)(p + 0x14) - mx;
        double dy = *(int*)(p + 0x18) - my;
        double dz = *(int*)(p + 0x1C) - mz;
        double d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) { bestD = d; best = p; }
    }
    if (!best) { lua_pushnil(L); return 1; }
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, (lua_Integer)*(uint32_t*)best); lua_setfield(L, -2, "uid");
    lua_pushinteger(L, *(int*)(best + 0x14));          lua_setfield(L, -2, "x");
    lua_pushinteger(L, *(int*)(best + 0x18));          lua_setfield(L, -2, "y");
    lua_pushinteger(L, *(int*)(best + 0x1C));          lua_setfield(L, -2, "z");
    lua_pushinteger(L, *(int*)(best + 0xB0));          lua_setfield(L, -2, "team");
    return 1;
}

// Every raw native command exposed flat under `native.*` (backward compat).
static const ApiMap kNativeFlat[] = {
    {"state","state"}, {"dump","dump"}, {"echo","echo"}, {"reset","reset"},
    {"giveItem","giveItem"}, {"giveItemBatch","giveItemBatch"},
    {"getUid","getUid"}, {"getPos","getPos"}, {"setPos","setPos"}, {"getAim","getAim"},
    {"getHealth","getHealth"}, {"setHealth","setHealth"},
    {"getHp","getHp"}, {"getMaxHp","getMaxHp"}, {"setMaxHealth","setMaxHealth"},
    {"walkspeed","walkspeed"}, {"setWalkspeed","setWalkspeed"},
    {"runspeed","runspeed"}, {"setRunspeed","setRunspeed"},
    {"crouchspeed","crouchspeed"}, {"setCrouchspeed","setCrouchspeed"},
    {"swimspeed","swimspeed"}, {"setSwimspeed","setSwimspeed"},
    {"hunger","hunger"}, {"setHunger","setHunger"},
    {"stamina","stamina"}, {"setStamina","setStamina"},
    {"getAttrs","getAttrs"},
    {"fly","fly"}, {"sprint","sprint"}, {"noclip","noclip"}, {"unlock","unlock"},
    {"noDrop","noDrop"}, {"jumpFly","jumpFly"}, {"slowFall","slowFall"},
    {"discard","discard"}, {"discardAll","discardAll"}, {"sortPack","sortPack"},
    {"repair","repair"}, {"repairAll","repairAll"}, {"setItem","setItem"},
    {"teleport","teleport"}, {"setTimespeed","setTimespeed"},
    {"setTime","setTime"}, {"time","time"},
    {"spectate","spectate"}, {"getSpectate","getSpectate"},
    {"chat","chat"}, {"getJump","getJump"}, {"setJump","setJump"},
    {"getScale","getScale"}, {"setScale","setScale"},
    {"handItem","handItem"}, {"handSlot","handSlot"},
    {"getEmote","getEmote"}, {"setEmote","setEmote"},
    {"roomOwner","roomOwner"}, {"roomMap","roomMap"},
    {"revive","revive"}, {"addStar","addStar"},
    {"hitWalls","hitWalls"}, {"groundSee","groundSee"}, {"airSee","airSee"},
    {"killAura","killAura"}, {"mountAll","mountAll"}, {"mineAll","mineAll"},
    {"killAllHost","killAllHost"}, {"killPlayer","killPlayer"}, {"aimbot","aimbot"},
    {"teleportTo","teleportTo"}, {"playerPos","playerPos"}, {"bringPlayer","bringPlayer"},
    {"roomKick","roomKick"}, {"allDie","allDie"}, {"allDance","allDance"},
    {"gmSkin","gmSkin"}, {"unlockItems","unlockItems"},
};

static const ApiMap kPlayerApi[] = {
    {"getUid","getUid"}, {"getMainPlayerUin","getUid"},
    {"getPos","getPos"}, {"setPos","setPos"}, {"getAim","getAim"},
    {"getYaw","getYaw"},
    {"getHealth","getHealth"}, {"getHp","getHp"}, {"getMaxHp","getMaxHp"},
    {"setHealth","setHealth"}, {"setMaxHealth","setMaxHealth"},
    {"getHunger","hunger"}, {"setHunger","setHunger"},
    {"getStamina","stamina"}, {"setStamina","setStamina"},
    {"getAttrs","getAttrs"},
    {"getWalkSpeed","walkspeed"}, {"setWalkSpeed","setWalkspeed"},
    {"getRunSpeed","runspeed"}, {"setRunSpeed","setRunspeed"},
    {"getCrouchSpeed","crouchspeed"}, {"setCrouchSpeed","setCrouchspeed"},
    {"getSwimSpeed","swimspeed"}, {"setSwimSpeed","setSwimspeed"},
    {"getJump","getJump"}, {"setJump","setJump"},
    {"getScale","getScale"}, {"setScale","setScale"},
    {"getEmote","getEmote"}, {"setEmote","setEmote"},
    {"handItem","handItem"}, {"handSlot","handSlot"},
    {"revive","revive"}, {"addStar","addStar"},
    {"fly","fly"}, {"sprint","sprint"}, {"noclip","noclip"}, {"unlock","unlock"},
    {"noDrop","noDrop"}, {"jumpFly","jumpFly"}, {"slowFall","slowFall"},
    {"spectate","spectate"}, {"getSpectate","getSpectate"},
    {"kill","killPlayer"}, {"kickRoom","roomKick"},
    {"gmChangeSkin","gmSkin"},
    {"teleportTo","teleportTo"},
    {"playerPos","playerPos"}, {"bringPlayer","bringPlayer"},
    {"dump","dump"},
};

static const ApiMap kRoomApi[] = {
    {"allDie","allDie"}, {"allDance","allDance"},
};

static const ApiMap kWorldApi[] = {
    {"getTime","time"}, {"time","time"},
    {"setTime","setTime"}, {"setHours","setTime"},
    {"setTimespeed","setTimespeed"},
    {"roomOwner","roomOwner"}, {"roomMap","roomMap"},
};

static const ApiMap kChatApi[] = {
    {"send","chat"}, {"sendSystemMsg","chat"}, {"say","chat"},
};

static const ApiMap kActorApi[] = {
    {"killAura","killAura"}, {"aimbot","aimbot"},
    {"mountAll","mountAll"}, {"mineAll","mineAll"}, {"killAllHost","killAllHost"},
};

static const ApiMap kItemApi[] = {
    {"give","giveItem"}, {"giveBatch","giveItemBatch"},
    {"setItem","setItem"},
    {"discard","discard"}, {"discardAll","discardAll"},
    {"sort","sortPack"},
    {"repair","repair"}, {"repairAll","repairAll"},
    {"unlockLocked","unlockItems"},
};

static const ApiMap kVisionApi[] = {
    {"hitWalls","hitWalls"}, {"groundSee","groundSee"}, {"airSee","airSee"},
};

static const ApiMap kEspApi[] = {
    {"w2s","w2s"},
};

// ---- Events: background tick handlers that keep running after the
// script that registered them returns (persistent VM + idle pump). ----
static int g_tickRefs[16];
static int g_tickCount = 0;

static int LuaEventsOnTick(lua_State* L) {
    if (!lua_isfunction(L, 1)) return luaL_argerror(L, 1, "expected function");
    if (g_tickCount >= 16) return luaL_error(L, "too many tick handlers (16 max)");
    lua_pushvalue(L, 1);
    g_tickRefs[g_tickCount++] = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, g_tickCount);
    return 1;
}

static int LuaEventsClear(lua_State* L) {
    for (int i = 0; i < g_tickCount; i++)
        luaL_unref(L, LUA_REGISTRYINDEX, g_tickRefs[i]);
    g_tickCount = 0;
    lua_pushboolean(L, 1);
    return 1;
}

// Called by CommandLoop while idle (~30ms cadence).
void PumpTicks() {
    if (!g_L || g_tickCount == 0) return;
    for (int i = 0; i < g_tickCount; i++) {
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_tickRefs[i]);
        if (lua_isfunction(g_L, -1)) {
            if (lua_pcall(g_L, 0, 0, 0) != 0) {
                const char* e = lua_tostring(g_L, -1);
                AppendOutput((std::string("[events] tick error: ") + (e ? e : "?") + "\n").c_str());
                lua_pop(g_L, 1);
                luaL_unref(g_L, LUA_REGISTRYINDEX, g_tickRefs[i]);   // drop failing handler
                g_tickRefs[i] = g_tickRefs[--g_tickCount];
                i--;
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
}

static void RegisterZelvexApi(lua_State* L) {
    // print / wait / sleep / zout (legacy alias from the old mini-parser)
    lua_register(L, "print", LuaPrint);
    lua_register(L, "wait", LuaWait);
    lua_register(L, "sleep", LuaWait);
    lua_register(L, "zout", LuaPrint);

    // native.* flat table (backward compatible with old scripts)
    PushApiTable(L, kNativeFlat, (int)(sizeof(kNativeFlat) / sizeof(kNativeFlat[0])));
    lua_setglobal(L, "native");

    // Mini World-style classes
    PushApiTable(L, kPlayerApi, (int)(sizeof(kPlayerApi) / sizeof(kPlayerApi[0])));
    lua_setglobal(L, "Player");

    PushApiTable(L, kWorldApi, (int)(sizeof(kWorldApi) / sizeof(kWorldApi[0])));
    lua_setglobal(L, "World");

    PushApiTable(L, kChatApi, (int)(sizeof(kChatApi) / sizeof(kChatApi[0])));
    lua_setglobal(L, "Chat");

    PushApiTable(L, kActorApi, (int)(sizeof(kActorApi) / sizeof(kActorApi[0])));
    lua_setglobal(L, "Actor");

    PushApiTable(L, kItemApi, (int)(sizeof(kItemApi) / sizeof(kItemApi[0])));
    lua_setglobal(L, "Item");

    PushApiTable(L, kVisionApi, (int)(sizeof(kVisionApi) / sizeof(kVisionApi[0])));
    lua_setglobal(L, "Vision");

    // Room.* (mass room effects)
    PushApiTable(L, kRoomApi, (int)(sizeof(kRoomApi) / sizeof(kRoomApi[0])));
    lua_setglobal(L, "Room");

    // Players.* (live scanning)
    lua_newtable(L);
    lua_pushcfunction(L, LuaPlayersList);   lua_setfield(L, -2, "list");
    lua_pushcfunction(L, LuaPlayersCount);  lua_setfield(L, -2, "count");
    lua_pushcfunction(L, LuaPlayersNearest); lua_setfield(L, -2, "nearest");
    lua_setglobal(L, "Players");

    // Net.send(msgName, json)
    lua_newtable(L);
    lua_pushcfunction(L, LuaNetSend);
    lua_setfield(L, -2, "send");
    lua_pushcfunction(L, LuaNetSniff);
    lua_setfield(L, -2, "sniff");
    lua_setglobal(L, "Net");

    // gui.* - in-game ImGui overlay
    lua_newtable(L);
    lua_pushcfunction(L, GuiStart);  lua_setfield(L, -2, "start");
    lua_pushcfunction(L, GuiShow);   lua_setfield(L, -2, "show");
    lua_pushcfunction(L, GuiReset);  lua_setfield(L, -2, "reset");
    lua_pushcfunction(L, GuiWindow); lua_setfield(L, -2, "window");
    lua_pushcfunction(L, GuiLabel);  lua_setfield(L, -2, "label");
    lua_pushcfunction(L, GuiButton); lua_setfield(L, -2, "button");
    lua_pushcfunction(L, GuiToggle); lua_setfield(L, -2, "toggle");
    lua_pushcfunction(L, GuiSlider); lua_setfield(L, -2, "slider");
    lua_pushcfunction(L, GuiGet);    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, GuiRadar);  lua_setfield(L, -2, "radar");
    lua_pushcfunction(L, GuiConfig); lua_setfield(L, -2, "config");
    lua_pushcfunction(L, GuiClear);  lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, GuiAccent); lua_setfield(L, -2, "accent");
    lua_pushcfunction(L, GuiSection);  lua_setfield(L, -2, "section");
    lua_pushcfunction(L, GuiDropdown); lua_setfield(L, -2, "dropdown");
    lua_pushcfunction(L, GuiKeybind);  lua_setfield(L, -2, "keybind");
    lua_pushcfunction(L, GuiInput);    lua_setfield(L, -2, "input");
    lua_pushcfunction(L, GuiColor);    lua_setfield(L, -2, "color");
    lua_pushcfunction(L, GuiSeparator);lua_setfield(L, -2, "separator");
    lua_pushcfunction(L, GuiSameLine); lua_setfield(L, -2, "sameLine");
    lua_pushcfunction(L, GuiProgress); lua_setfield(L, -2, "progress");
    lua_pushcfunction(L, GuiCombo);    lua_setfield(L, -2, "combo");
    lua_pushcfunction(L, GuiListbox);  lua_setfield(L, -2, "listbox");
    lua_pushcfunction(L, GuiTextColored); lua_setfield(L, -2, "textColored");
    lua_pushcfunction(L, GuiSmallButton); lua_setfield(L, -2, "smallButton");
    lua_pushcfunction(L, GuiSet);      lua_setfield(L, -2, "set");
    lua_pushcfunction(L, GuiOpts);     lua_setfield(L, -2, "opts");
    lua_pushcfunction(L, GuiTab);      lua_setfield(L, -2, "tab");
    lua_pushcfunction(L, GuiToggleDesc);  lua_setfield(L, -2, "toggleDesc");
    lua_pushcfunction(L, GuiParagraph);   lua_setfield(L, -2, "paragraph");
    lua_pushcfunction(L, GuiButtonColored);lua_setfield(L, -2, "buttonColored");
    lua_setglobal(L, "gui");

    // Events.* - background handlers
    lua_newtable(L);
    lua_pushcfunction(L, LuaEventsOnTick); lua_setfield(L, -2, "onTick");
    lua_pushcfunction(L, LuaEventsClear);  lua_setfield(L, -2, "clear");
    lua_setglobal(L, "Events");

    // input.* - raw key state for script-defined hotkeys
    lua_newtable(L);
    lua_pushcfunction(L, LuaInputKey); lua_setfield(L, -2, "key");
    lua_setglobal(L, "input");

    // http.* - remote hub transport
    lua_newtable(L);
    lua_pushcfunction(L, LuaHttpGet);  lua_setfield(L, -2, "get");
    lua_pushcfunction(L, LuaHttpPost); lua_setfield(L, -2, "post");
    lua_setglobal(L, "http");

    // Esp.* - projection helpers
    PushApiTable(L, kEspApi, (int)(sizeof(kEspApi) / sizeof(kEspApi[0])));
    lua_setglobal(L, "Esp");
    // Esp.screen() -> dispW, dispH (real swapchain size, for ESP math)
    lua_getglobal(L, "Esp");
    lua_pushcfunction(L, LuaEspScreen);
    lua_setfield(L, -2, "screen");
    lua_pop(L, 1);
}

// Run one script through the real Lua VM. Output goes to shared memory via
// print(); errors are reported as text; STOP aborts via instruction hook.
// The lua_State PERSISTS between runs - globals and functions survive,
// enabling incremental workflows. native.reset() recreates it fresh.
static void CloseLuaState() {
    if (g_L) { lua_close(g_L); g_L = nullptr; g_tickCount = 0; Log("Lua state closed"); }
}

static lua_State* EnsureLuaState() {
    if (g_L) return g_L;
    Log("ExecuteLuaScript: creating persistent Lua state");
    g_L = luaL_newstate();
    if (!g_L) return nullptr;
    lua_atpanic(g_L, LuaPanic);
    luaL_openlibs(g_L);
    RegisterZelvexApi(g_L);
    return g_L;
}

static void ExecuteLuaScript(const char* code) {
    if (!code) return;
    lua_State* L = EnsureLuaState();
    if (!L) { AppendOutput("ERR: cannot create Lua state"); return; }
    lua_sethook(L, LuaCancelHook, LUA_MASKCOUNT, 100000);

    int lr = luaL_loadbuffer(L, code, strlen(code), "=script");
    if (lr != 0) {
        const char* e = lua_tostring(L, -1);
        AppendOutput(e ? e : "ERR: syntax error");
        lua_pop(L, 1);
        return;
    }
    int pr = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (pr != 0) {
        const char* e = lua_tostring(L, -1);
        std::string msg = e ? e : "runtime error";
        if (msg != "cancelled")
            AppendOutput(("ERR: " + msg).c_str());
        lua_pop(L, 1);
    } else {
        int n = lua_gettop(L);
        if (n > 0) {
            std::string out;
            for (int i = 1; i <= n; i++) {
                if (i > 1) out += "  ";
                LuaFormatValue(L, i, out);
            }
            out += "\n";
            AppendOutput(out.c_str());
        }
        lua_settop(L, 0);
    }
    Log("ExecuteLuaScript: done");
}

static void ExecuteUserScript() {
    if (!g_pShared || !g_pShared->code) return;
    Log("ExecuteUserScript: start");
    g_cancelRequested = false;
    g_outputPos = 0;
    InterlockedExchange(&g_pShared->outLen, 0);
    ExecuteLuaScript(g_pShared->code);
    if (g_cancelRequested) {
        if (g_outputPos > 0) AppendOutput("\n");
        AppendOutput("<cancelled>");
        g_pShared->error = 1;
        Log("ExecuteUserScript: Cancelled");
    } else {
        if (g_outputPos == 0) AppendOutput("<no output>");
        g_pShared->error = 0;
        Log("ExecuteUserScript: Done");
    }
}

static DWORD WINAPI CommandLoop(LPVOID) {
    Log("CommandLoop: Starting");
    while (true) {
        if (g_pShared && InterlockedCompareExchange(&g_pShared->command, 0, 1) == 1) {
            Log("Command received, executing");
            InterlockedExchange(&g_pShared->cancel, 0);   // fresh run: clear any stale stop
            ExecuteUserScript();
            InterlockedExchange(&g_pShared->done, 1);
            Log("Execution complete");
        } else {
            PumpTicks();   // background event handlers between runs
        }
        Sleep(30);
    }
    return 0;
}

static volatile LONG g_initDone = 0;

extern "C" __declspec(dllexport) DWORD __cdecl Initialize(LPVOID) {
    if (InterlockedExchange(&g_initDone, 1) == 1) return 0;
    Log("Initialize: Starting");
    g_hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "ZelvexLuaSharedMem");
    if (!g_hMapFile) {
        Log("Initialize: OpenFileMappingA failed");
        return 1;
    }
    g_pShared = (LuaSharedMemory*)MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LuaSharedMemory));
    if (!g_pShared) {
        Log("Initialize: MapViewOfFile failed");
        return 1;
    }
    SnapshotModules();
    ResolveNativeApi();
    LogRelevantModules();
    CreateThread(nullptr, 0, CommandLoop, nullptr, 0, nullptr);
    Log("Initialize: CommandLoop started");
    g_pShared->done = 1;
    Log("Initialize: Ready");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        Log("DllMain: Entry");
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Initialize, nullptr, 0, nullptr);
    }
    return TRUE;
}


