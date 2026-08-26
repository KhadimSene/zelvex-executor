# Zelvex Executor — Capabilities & Technical Reference

## What Is This?

A real **Lua 5.1 executor platform** for Mini World: CREATA. Not a remote-thread `luaL_loadstring` stub — a full VM compiled into a 32-bit DLL that lives inside `MiniGameApp.exe`, plus a Qt6 GUI that streams scripts to it via shared memory and draws an in-game Dear ImGui overlay through the game's own DirectX swapchain.

> **Quick idea:** `Zelvex.exe` (64-bit, Qt GUI + editor) ←→ shared memory (`ZelvexLuaSharedMem`) ←→ `lua_dll.dll` (32-bit, Lua VM + game natives + ImGui) injected into the game — all scripts run *inside* the game process, so they can call the engine directly.

---

## How It Works (Execution Flow)

```
1. ATTACH
   Zelvex finds MiniGameApp.exe, opens a handle.
   (Auto-attach polls every 500ms; manual ATTACH in the GUI)

2. INJECT
   Zelvex drops lua_dll.dll into the game via CreateRemoteThread(LoadLibrary).
   DLL entry:
     - creates shared memory "ZelvexLuaSharedMem"
       { command, done, error, cancel, outLen, code[16KB], output[32KB] }
     - starts a worker thread (CommandLoop) that watches `command`

3. RUN A SCRIPT
   GUI writes code[16KB], sets command=1, starts a 100ms poll timer.
   Worker picks it up (compare-exchange to 0), runs ExecuteLuaScript:
     - persistent lua_State* g_L (survives between runs)
     - instruction-count hook (100k) aborts tight loops on STOP
     - wait(sec) sleeps in 25ms slices, pumping gui callbacks + checking cancel
     - native.* / Player:/Actor:/Item: etc. dispatch via RunNativeCmd()
     - print() and auto-echo [cmd] lines stream to output[32KB] via outLen

4. OVERLAY (on demand)
   Script calls gui.start() -> Overlay::Start():
     - finds the game's largest visible window
     - creates a dummy D3D11 swapchain to read the shared Present vtable
     - swaps vtable[8] (Present) — copy-on-write makes it process-local
     - hooks WndProc on that window; swaps back on gui.reset() / Stop()

5. STOP / CANCEL
   GUI STOP button sets shared->cancel=1.
   PollCancel() mirrors it to g_cancelRequested; hook + LuaWait slices abort within 25ms.
```

**No `liblua.dll` is used from the game.** Lua 5.1.5 is compiled into the DLL (`dll/lua-5.1.5/`, ~30 .c files, ~100KB). All 21k+ engine exports are resolved live at attach.

---

## Capabilities

### What It CAN Do

#### Scripting — Full Lua 5.1

Functions, closures, tables, metatables, `string`/`math`/`table` libs, `loadstring`, `pcall`, coroutines. 16 KB code / 32 KB output buffers (streaming, not just on finish). Persistent VM — globals survive between runs; `native.reset()` wipes it.

#### Game Natives (~70)

| Area | Examples | How |
|------|----------|-----|
| **Player** | `Player:getPos()->x,y,z`, `setHealth(9999)`, `getHunger`, `setWalkSpeed`, `setJump`, `setScale`, `revive`, `addStar`, `fly`, `sprint`, `noclip`, `noDrop`, `jumpFly`, `slowFall`, `teleportTo(uid,dx,dy,dz)`, `bringPlayer`, `gmChangeSkin` | Direct calls into `ClientPlayer`/`MpPlayerControl` via resolved exports + AOB patches |
| **Players** | `Players.list()->{uid,x,y,z,team}`, `count()`, `nearest()` | Live scan `[[libMiniBaseGame+0xB36C]+78]+68`, 40 slots, `uid@+0 pos@+14/18/1C team@+B0`, filtered `uid!=0, team 1..64, |coord|<30M` |
| **World** | `World:setTime/setHours/getTime/setTimespeed`, `roomOwner/roomMap` | `WorldManager` exports |
| **Chat** | `Chat:send("hi")`, `sendSystemMsg` | `libiworld` chat func (`+0x185060`) + `[[MiniBase+0xB36C]+78]+4` |
| **Actor** | `Actor:killAura(0/1/2)`, `aimbot(on,range)`, `mountAll`, `mineAll`, `killAllHost` | Byte patches / caves + aimbot thread scanning the player table |
| **Item** | `Item:give(id,qty)`, `giveBatch`, `sort`, `repairAll`, `discardAll`, `unlockLocked` | `sendToHost` RPC (`GiveItemThread`) + caves for unlock (`66 0F 6E 40 2C`) |
| **Vision** | `Vision:hitWalls/groundSee/airSee` | Noclip-style gates (`intersectRay+0x62`, `getEyeHeight+0x29`, `8B 41 2C` cave) |
| **Room** | `Room:allDie/allDance`, `Player:kickRoom(uid)` | `ClientPlayer::onDie` iter + `doJump` iter + `RoomManager::requestRoomKickPlayer` |
| **Net** | `Net.send(msg,json)`, `Net.sniff(true/false)` | Generic `sendToHost` (`SandBoxManager::sendToHost`) + 5-byte JMP hook that logs `msg|json` to console |

`native.*` flat aliases exist for every command (`native.aimbot`, `native.giveItem`, etc.). `zout` is an alias for `print`.

#### In-Game UI (Full ImGui)

Declarative windows built from Lua, rendered inside the game's present path. **INSERT** toggles, coexistence input (messages over your panels go to ImGui, everything else passes to the game, `WM_INPUT` frozen only while hovering).

```
gui.start() / show() / reset() / accent(r,g,b)   windows
gui.window(title) -> handle
gui.label / textColored / paragraph (card) / separator / sameLine
gui.button / smallButton / buttonColored (full-width accent)
gui.toggle / toggleDesc (title+desc+right switch) / slider
gui.dropdown / combo / listbox  (8 options, live via gui.opts)
gui.input (text) / color (picker) / progress (bar)
gui.keybind (click then press any VK, INSERT reserved) / section (collapsing header)
gui.tab (sidebar page) / radar (HUD circle, dots/boxes/crosses)
gui.get / set / opts / config / clear
```

Limits: 5 windows × 96 widgets, 8 options per combo, 64 chars per input, 64 chars per label, 16 `Events.onTick` handlers, 64 pending overlay events.

#### ESP / Radar

- **Radar HUD** (`gui.radar`) — foreground circle, rotates with yaw (`[[pc+0x950]+4]`), range 3k/6k/12k, self arrow, team-colored dots. Always works.
- **W2S** — `Esp.w2s(uid) -> x,y,z` via `PlayerControl::getPointToScreen(&sx,&sy,&sz,actor,0)` and `Esp.screen()->W,H` (real swapchain size cached each frame). Needs live calibration: run `CALIBRATE.lua` with another player in world, hold them at center/left/right/top/bottom/behind and paste the SUMMARY block.

#### Background Work + Network

- `Events.onTick(fn)` / `Events.clear()` — 16 handlers stored as registry refs, pumped every ~30ms when no script is running; failing handlers auto-removed.
- `input.key(vk)` — `GetAsyncKeyState` edge detection for script hotkeys.
- `http.get(url)` / `http.post(url, body, ctype)` — WinINet, 10s timeout, HTTPS auto-flagged; enables remote hubs (`loadstring(http.get(...))()`).
- Global hotkeys `Ctrl+Alt+1/2/3` — system-wide, run Lua one-liners from the Misc tab even while the game is focused.

### What It CANNOT Do (Today)

| Limitation | Why |
|---|---|
| **2D boxes/nameplates through walls (without calibration)** | Needs a calibrated W2S or a ViewProj capture (`VSSetConstantBuffers` hook) — radar works, boxes need that step |
| **Server-side bypass** | `giveItem`/`teleport` RPCs are validated server-side; host-only cheats (`allDie`, `kick`) revert on official servers |
| **Survive a game update without a Pattern Doctor run** | AOBs (`39 39 74 11 40` teleport hook, `74 56` noclip, etc.) shift by bytes; re-scan or update `cheat_defs.h` |
| **Large scripts >16 KB in one go** | Shared-memory `code` buffer is 16 KB. Split or fetch remotely |
| **Non-ASCII labels** | Overlay uses the default ImGui ASCII atlas; use plain text |

---

## Known Issues & Caveats

1. **Join a map before you cheat.** `GetPlayer()` / `GetChatObj()` return null in the lobby; natives then return `ERR: not in world` instead of crashing (since the null-guard patch).
2. **Polling is streaming.** Console flushes incrementally via `outLen`; infinite loops now print live, but `STOP` still aborts via the instruction hook.
3. **Name color (`#R`, `#c{...}`) preview is local.** The confirmation preview rendering colored text does **not** mean the server will store `#` — replay the `sendToHost` packet with `Net.sniff` to test; `/api/health`-only captures in Fiddler mean the name change is binary RPC, not HTTP.
4. **Fiddler vs Wireshark for this game.** Fiddler sees only WinINet HTTP; the name-change RPC rides on `43.174.233.127:4018` binary TCP. Use `Net.sniff(true)`, change to `Test123`, watch `[sniff] msg | json`, then `Net.send` the same msg with `#RTest`.
5. **Code page** is UTF-8; `#b` blink etc. are renderer features.

---

## Testing Guide

### Prerequisites

- Zelvex built (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` with `C:\msys64\ucrt64\bin;C:\msys64\mingw32\bin` in `PATH`)
- Mini World Creata running, fully loaded into a world
- `ATTACH` shows `GAME ATTACHED` and `INJECT DLL` succeeded

### Test Steps

1. Launch Zelvex, **ATTACH**, **INJECT DLL**
2. Open the **Lua** tab -> **EXECUTE** a snippet -> check **Console** (right pane, auto-scrolls)
3. In-game press **INSERT** to show overlay; run a hub script and check sidebar pages

### Test Scripts (Progressive)

**1 — Basic (VM alive)**
```lua
print("hi " .. _VERSION)   -- should print  hi Lua 5.1
```

**2 — Read state**
```lua
print("uid", Player:getUid())
print("pos", Player:getPos())
print("players", Players.count())
```

**3 — Write state**
```lua
Player:fly(true); wait(2); Player:fly(false)
Player:setWalkSpeed(12)
```

**4 — UI + radar**
```lua
assert(gui.start()); gui.reset()
local w = gui.window("Test")
gui.label(w, "hello"); gui.button(w, "hi", function() print("click") end)
gui.radar(w, 110, 1, 6000)
```

**5 — RPC**
```lua
Net.send("TestMsg", '{"x":1}')
-- or: Chat:send("hello from Zelvex")
```

**6 — W2S calibration (needs a second player in world)**
```lua
-- load and run CALIBRATE.lua from the repo
-- follow center/left/right/top/bottom/behind prompts, paste SUMMARY
Esp.w2s(targetUid); print(Esp.screen())
```

**7 — Sniff the name change**
```lua
Net.sniff(true)   -- then change name to Test123 in game, watch [sniff] lines
Net.sniff(false)
Net.send("ChangeNick", '{"nick":"#RTest123"}')  -- msg name from sniff
```

---

## Architecture Notes

### DLL Dependencies

| DLL | What We Use |
|---|---|
| `libSandboxEngine.dll` | `g_pPlayerCtrl` (`0x27772F8`), `WorldManager`, `sendToHost` (`SandBoxManager::sendToHost`), `setDayTime` |
| `libMiniBaseGame.dll` | player table `[[+0xB36C]+78]+68`, 40 slots |
| `libiworld.dll` | `RoomManager::requestRoomKickPlayer`, chat func, `g_nHomeGardenSaveVersion` |
| `libEngine.dll` (ANGLE) | `d3d11`/`dxgi` — we hook `SwapChain::Present` (vtable[8]) |

Exports resolved at attach via `ResolveExport` + live `CollectExports` scans (for names absent from our snapshot, e.g. `requestRoomKickPlayer`).

### Memory Layout

```
Zelvex.exe (Qt6, 64-bit)                 lua_dll.dll (Lua+ImGui, 32-bit, inside MiniGameApp.exe)
  editor / console (streaming)    <---->   LuaSharedMemory "ZelvexLuaSharedMem"
  hotkeys Ctrl+Alt+1..3                    5*4 header + 16K code + 32K output = 49172 bytes
  memory_manager (attach/patches)          (both sides define it identically)
                                           |
                                           +-- persistent lua_State* g_L
                                           +-- gui.* -> game_overlay.cpp (ImGui/DX11/Win32)
                                           +-- native.* -> RunNativeCmd()
                                           |     export resolution (21k symbols)
                                           |     AOB patches + code caves (save/restore)
                                           |     direct thiscall into game classes
                                           +-- Events tick pump (16 refs, idle loop)
                                           +-- http (WinINet, 10s) + Net.sniff hook
```

Both sides define `LuaSharedMemory` with `MAX_CODE=16384, MAX_OUTPUT=32768, command/done/error/cancel/outLen` — keep them identical or the mapping breaks.

### Overlay Hook

- Game is ANGLE → D3D11, so `IDXGISwapChain::Present` is the present path.
- Hook: dummy `D3D11CreateDeviceAndSwapChain` → read shared vtable page → swap slot 8 → copy-on-write makes it process-local. No code patching, no disassembly.
- Input: coexistence — `ImGui::WantCaptureMouse/Keyboard` decides whether a message is consumed; otherwise it passes straight to the game. `WM_INPUT` frozen only while hovering your panels. `INSERT` toggles with anti-repeat.

### Cross-Bitness

- Zelvex `.exe` is 64-bit (UCRT64), `lua_dll.dll` is 32-bit (MINGW32, `-m32`, `i686-w64-mingw32-g++`). CMake pins the DLL compiler explicitly; the host app uses `CMAKE_POST_BUILD` to copy `bin/liblua_dll.dll` next to the exe as `lua_dll.dll`.

---

## Comparison with Existing Zelvex Features

| Feature | Old Cheat Cards (AOB patches) | Lua Platform |
|---|---|---|
| Mechanism | Per-cheat byte patches / shellcode caves | Persistent VM + direct engine calls + patches only where needed |
| Scope | Fixed set | Unlimited — write new scripts |
| Stability | AOB shifts break until Pattern Doctor | Exports + live scans, more resilient |
| UI | Prebuilt cards per tab | Free-form ImGui windows from scripts (redz-style sidebar pages) |
| Extensibility | Add a `CheatDef` entry | Just type Lua |

---

## What Could Be Added Next

1. **ViewProj capture** — hook `VSSetConstantBuffers` to steal the ViewProj matrix every frame; then C-side `worldToScreen(head)` for true chams/boxes without `Esp.w2s` calibration.
2. **Bone ESP** — offsets per actor type for skeletons (needs per-model dump).
3. **Trigger bot / TeleAura** — reuse the `pickActor+aim` path on a timer.
4. **Theme API** — `gui.theme(r,g,b, rounding)` per window.

---

## Disclaimer

Educational / research purposes. The executor calls documented engine exports and Lua C API from injected code. Use on accounts and servers you are allowed to. Not affiliated with Mini World.
