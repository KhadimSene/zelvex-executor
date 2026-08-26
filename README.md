# zelvex-executor

**In-game Lua executor + cheat platform for Mini World: CREATA.**
Real Lua 5.1 VM + Dear ImGui overlay + ~70 game natives. Developed by nyxdev_.

![Version](https://img.shields.io/badge/version-4.0.0-blue) ![Qt](https://img.shields.io/badge/Qt-6.7-green) ![Lua](https://img.shields.io/badge/Lua-5.1.5-blue) ![License](https://img.shields.io/badge/license-private-lightgrey)

The executor injects a 32-bit DLL into the game, draws its UI inside the game's own DirectX swapchain, and lets you run Lua scripts that call the game directly. No second window, no code patching.

---

## What it can do (teaser)

```lua
Player:setHealth(9999)                 -- godmode
Player:teleportTo(targetUid, 0, 3, 0)  -- teleport onto a player
Actor:killAura(1)                      -- killaura (enemies only)
Actor:aimbot(true, 3000)               -- aimbot 3k range
Item:give(1105, 64)                     -- give any item by ID
Vision:hitWalls(true)                  -- see through walls

-- in-game UI from Lua (INSERT to show)
local w = gui.window("My Hub")
gui.toggle(w, "Fly", false, function(on) Player:fly(on) end)
gui.slider(w, "Speed", 4, 30, 8)
gui.radar(w, 110, 1, 6000)

-- remote hubs
loadstring(http.get("https://your.site/hub.lua"))()
```

See the full scripting reference in [`LUA_DOCS.md`](LUA_DOCS.md) and the engine internals in [`CAPABILITIES.md`](CAPABILITIES.md).

---

## Quick start (no compile needed)

1. Download `ZelvexSetup-4.0.0.exe` from Releases
2. Install and run `Zelvex.exe`
3. Launch Mini World, join a map, click **ATTACH** in Zelvex
4. Click **INJECT DLL**, paste a script, hit **EXECUTE**
5. In-game press **INSERT** to show the overlay

Hotkeys: `Ctrl+Alt+1/2/3` run your Lua one-liners even while the game is focused. `F1` panics (disables everything).

---

## Compiling from source — beginner friendly

You need Windows 10/11 (64-bit).

### 1. Install the tools

| Tool | What it is | Where to get it |
|------|------------|-----------------|
| **MSYS2** | Gives you the compilers + Qt | https://www.msys2.org/ -> download installer -> install to `C:\msys64` |
| **Git** | To clone the repo | https://git-scm.com/download/win |
| **Inno Setup 6** | Builds the installer `.exe` (optional) | https://jrsoftware.org/isdl.php |

After installing MSYS2, open **MSYS2 UCRT64** terminal (not the blue MSYS one) and run:

```bash
pacman -Syu
# close and reopen UCRT64 when it asks
pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools \
          mingw-w64-ucrt-x86_64-keystone mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-toolchain
pacman -S mingw-w64-i686-toolchain
```

Verify:

```bash
c++ --version
cmake --version
qmake --version
```

### 2. Clone and build

Open **PowerShell** (normal, not MSYS) and run:

```powershell
git clone https://github.com/KhadimSene/zelvex-executor.git
cd zelvex-executor

# IMPORTANT: PATH must contain both toolchains
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\mingw32\bin;C:\msys64\usr\bin;" + $env:PATH

# Quick dev build (produces Zelvex.exe + lua_dll.dll in build/)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If this succeeds you will have:
- `build/Zelvex.exe`
- `build/bin/liblua_dll.dll` (the 32-bit DLL that gets injected)

### 3. Build the portable folder + installer

```powershell
powershell -ExecutionPolicy Bypass -File build-dist.ps1
# -> build/dist/        portable folder (exe + all DLLs, run anywhere)
# -> build/installer/ZelvexSetup-4.0.0.exe   Inno Setup installer
```

`build-dist.ps1` does: configure -> build -> `windeployqt` (copies Qt DLLs) -> bundles 28 MinGW runtime DLLs -> copies `lua_dll.dll` -> runs `ISCC.exe` on `installer.iss`.

> **First-time note:** `build-dist.ps1` reconfigures CMake. If `cmake` complains about `CMAKE_CXX_COMPILER`, make sure `$env:PATH` still contains `C:\msys64\ucrt64\bin` in that same PowerShell window.

### Project layout

```
zelvex-executor/
  src/            Qt GUI, editor, memory_manager, item DB (2,628 items)
  dll/            Lua 5.1.5 (~30 .c files) + Dear ImGui 1.90.4 + overlay + game natives
  dll/game_overlay.*  Present hook, input router, widget store, radar
  ZELVEX_HUB.lua  flagship hub script (redz-style sidebar pages)
  CALIBRATE.lua   W2S calibration helper
  resources.qrc   1,347 embedded item icons + logo
  installer.iss   Inno Setup script
  build-dist.ps1  one-click dist + installer
  copy-runtime-deps.ps1  bundles MinGW DLLs windeployqt misses
```

Third-party sources compiled into the DLL:
- `dll/lua-5.1.5/` — Lua core (MIT)
- `dll/imgui/` — Dear ImGui 1.90.4 + DX11/Win32 backends (MIT)

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `cmake` says `CMAKE_CXX_COMPILER not set` | Your `PATH` is missing `C:\msys64\ucrt64\bin`. Re-run the `$env:PATH = ...` line in the same PowerShell |
| `moc.exe` crashes `0xc0000135` / `ninja: build stopped` | Same — `PATH` must include `C:\msys64\ucrt64\share\qt6\bin` (for `moc`) and `C:\msys64\mingw32\bin` (for 32-bit `g++`) |
| `unfinished string near '<eof>'` when executing Lua | Script exceeded 16 KB shared-memory buffer — split it and load via `loadstring(http.get(...))()` |
| `[cmd] ERR: ... unresolved` | Game updated, pattern stale — run **Pattern Doctor** (Misc tab) |
| Menu visible but game unclickable | Press **INSERT** to hide. If stuck, re-inject |
| Radar empty | Join a room — it reads the live actor table only |
| Item icons show gray tile with ID | Wiki coverage is ~51% (1,347/2,628). The ID is still giveable via the `Item ID` field |
| `Wireshark` shows only TLS garbage for name change | Name change is binary `sendToHost` RPC, not HTTP. Use `Net.sniff(true)` in Lua, change name, then `Net.sniff(false)` — the plain `msg\|json` is printed to console |
| `Esp.w2s` returns `ERR: actor not found` | You are alone — need another player in the same world for calibration |

Still stuck? Open an issue with your `Pattern Doctor` output and console log.

---

## Docs

- **Lua API** — every `Player:`, `Actor:`, `World:`, `Item:`, `gui.*`, `Events.*`, `http.*`, `Esp.*` call: [`LUA_DOCS.md`](LUA_DOCS.md)
- **Engine internals** — how the Present hook, shared memory, VM, and native patches work, plus the `W2S` / `VSSetConstantBuffers` ESP path: [`CAPABILITIES.md`](CAPABILITIES.md)

```lua
-- preview from LUA_DOCS.md
local w = gui.window("Demo")
gui.section(w, "COMBAT")
gui.toggle(w, "Kill Aura", false, function(on) Actor:killAura(on and 1 or 0) end)
gui.keybind(w, "Noclip", 0x58, function() Player:noclip(true) end)
gui.radar(w, 110, 1, 6000)
Esp.w2s(targetUid)  -- -> x, y, z  (calibrate with CALIBRATE.lua)
```

---

*Educational / research purposes. Use on accounts and servers you are allowed to. Not affiliated with Mini World.*
