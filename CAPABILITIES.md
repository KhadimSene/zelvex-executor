# Zelvex Lua Injector — Capabilities & Technical Reference

## What Is This?

A Lua code injector that executes arbitrary Lua code inside Mini World Creata's game process by resolving the game's internal Lua VM and calling `luaL_loadstring` + `lua_pcall` through remote shellcode injection.

---

## How It Works (Execution Flow)

```
1. ATTACH
   Zelvex opens a handle to MiniGameApp.exe (the game process)

2. RESOLVE LUA VM
   ├─ Call SandboxCoreLuaDirector::GetCoreLuaDirector()  [static singleton]
   ├─ Call SandboxCoreLuaDirector::getLuaState()         [member function]
   └─ Returns: lua_State* (the game's Lua VM pointer)
   (Addresses resolved via PE export table parsing of libSandboxEngine.dll)

3. RESOLVE LUA API
   ├─ Resolve luaL_loadstring  from liblua.dll exports
   └─ Resolve lua_pcall        from liblua.dll exports

4. INJECT & EXECUTE
   ├─ Allocate memory in target process (VirtualAllocEx)
   ├─ Write Lua code string as UTF-8 to remote memory
   ├─ Write assembled x86 shellcode to remote memory:
   │   pushad
   │   push <code_ptr>          ; const char* code
   │   push <lua_State>         ; lua_State* L
   │   call luaL_loadstring     ; luaL_loadstring(L, code) → 0 on success
   │   test eax,eax
   │   jne fail                 ; skip pcall if load failed
   │   push 0                   ; errfunc = 0 (no error handler)
   │   push -1                  ; nresults = LUA_MULTRET (1 return value)
   │   push 0                   ; nargs = 0 (no arguments)
   │   push <lua_State>         ; lua_State* L
   │   call lua_pcall           ; lua_pcall(L, 0, -1, 0)
   │   fail:
   │   popad
   │   ret
   ├─ CreateRemoteThread to execute the shellcode
   ├─ WaitForSingleObject (5 second timeout)
   └─ Free remote memory
```

---

## Capabilities

### What It CAN Do

**Execute any Lua code** that the game's client-side VM supports. This includes:

#### Client-Side Game API Access
- **CustomUI** — Full UI manipulation (create/destroy/update UI elements)
  - `CustomUI:open()` / `CustomUI:close()` — Open/close UI windows
  - `CustomUI:setText()` / `CustomUI:setImage()` — Modify UI elements
  - `CustomUI:openUrl()` — Open URLs in the game's browser
  - 46+ CustomUI functions available
- **Player** — Read local player state
  - Position, health, hunger, stamina
  - Inventory access
- **World** — World manipulation
  - Block read/write (local)
  - Weather, time
- **RemoteFunction:FireServer()** — Send RPCs to the host
  - Request item spawning
  - Trigger server-side actions
  - (Server may validate/reject)
- **RemoteEvent:FireServer()** — Fire events to the host
- **ModuleScript** — Load shared modules

#### UI Generation
- Create dynamic in-game UI via CustomUI API
- Build custom HUDs, menus, overlays
- React to game events and update UI in real-time

#### Game State Reading
- Read player stats (health, hunger, stamina)
- Read player position and inventory
- Read world state (blocks, entities)
- Monitor game events

#### Local State Modification
- Modify local player variables
- Change local visual settings (already exists in Zelvex, but Lua can do more)
- Trigger local animations/effects

### What It CANNOT Do

| Limitation | Why |
|---|---|
| **Directly modify server state** | Server validates all state changes. Client code is untrusted. |
| **Execute before VM initializes** | `lua_State*` is only valid after the game's Lua VM is fully loaded. Wait until you're in-game. |
| **Survive infinite loops** | `CreateRemoteThread` has a 5-second timeout. The thread is killed if code doesn't return. |
| **Bypass server-side anticheat** | Server validates item/position changes. Client-only changes are fine. |
| **Access server-side Lua APIs** | `Script` (server-side scripts) are not accessible from the client VM. |
| **Modify other players** | Other players' state is managed server-side. |

---

## Known Issues & Caveats

### 1. VM Must Be Initialized
The game's Lua VM is created during startup. If you try to execute code before the VM exists, you'll see "VM not initialized." **Wait until you're fully loaded into a world** before executing.

### 2. Thread Timeout
Each execution gets a 5-second timeout. If your Lua code takes longer than 5 seconds (e.g., long loops, heavy computation), the thread will be terminated. The game won't crash, but your code won't complete.

### 3. Return Value Detection
After `lua_pcall` completes, the injector checks the thread exit code. If it's `0` or `1`, it reports "Success." This is a simplification — it doesn't capture error messages from the Lua stack. For debugging, you'll need to use `print()` or CustomUI to display output in-game.

### 4. Code Size Limit
Remote memory allocation has a practical limit. Very large scripts (>64KB) may fail to allocate. For large scripts, consider:
- Breaking them into smaller chunks
- Using `dofile()` / `loadfile()` to load from the game's filesystem

### 5. String Encoding
Code is sent as UTF-8. Lua source files in the game use UTF-8. Non-ASCII characters in your code should work fine.

---

## Testing Guide

### Prerequisites
- Zelvex built from source (`cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`)
- Mini World Creata running (English or any language)
- Game fully loaded into a world (not just the main menu)

### Test Steps

1. **Launch Zelvex**
2. **Click ATTACH** (or wait for auto-attach)
3. **Click the "Lua" tab** in the sidebar
4. **Enter test code** in the editor
5. **Click EXECUTE**
6. **Check the status label** — should say "Success!" or "Error"

### Test Scripts (Progressive Difficulty)

#### Test 1: Basic Execution (Verify VM Works)
```lua
print("Zelvex Lua injector works!")
```
This is the simplest test. If this doesn't work, the VM isn't initialized or the injector can't resolve functions.

#### Test 2: Read Player Info
```lua
local player = Player:getLocalPlayer()
if player then
    print("Player name: " .. tostring(player:getName()))
    local pos = player:getPosition()
    print("Position: " .. pos.x .. ", " .. pos.y .. ", " .. pos.z)
end
```

#### Test 3: Modify Local State
```lua
local player = Player:getLocalPlayer()
if player then
    player:setPosition(0, 100, 0)
end
```

#### Test 4: Create UI Element
```lua
CustomUI:open("test_ui", "Hello from Zelvex!")
```

#### Test 5: Send RPC to Host
```lua
RemoteFunction:FireServer("giveItem", 10001, 1)
```

#### Test 6: Complex Script
```lua
local count = 0
local timer = Timer:new(1000, function()
    count = count + 1
    print("Tick: " .. count)
    if count >= 5 then
        timer:stop()
    end
end)
timer:start()
```

---

## Architecture Notes

### DLL Dependencies
| DLL | What We Use |
|---|---|
| `liblua.dll` | Lua 5.1+rvm — `luaL_loadstring`, `lua_pcall` |
| `libSandboxEngine.dll` | `SandboxCoreLuaDirector::GetCoreLuaDirector()`, `getLuaState()` |
| `libSandboxEngineDriver.dll` | (Fallback for Lua state resolution) |

### Mangled Symbol Names (32-bit MSVC)
```
?GetCoreLuaDirector@SandboxCoreLuaDirector@MNSandbox@@SAPAV12@XZ
?getLuaState@SandboxCoreLuaDirector@MNSandbox@@QAEPAVlua_State@@XZ
?GetLuaState@SandboxCoreLuaDirector@MNSandbox@@QAEPAVlua_State@@XZ
```

### Memory Layout
```
Target Process (MiniGameApp.exe, 32-bit WoW64)
├── liblua.dll              ← Lua VM implementation
│   ├── luaL_loadstring()   ← Compiles Lua source string
│   └── lua_pcall()         ← Executes compiled chunk
├── libSandboxEngine.dll    ← Game engine + sandbox
│   ├── SandboxCoreLuaDirector (singleton)
│   │   └── lua_State*      ← The actual Lua VM pointer
│   └── SandBoxManager
│       └── sendToHost()    ← RPC function (already used by Zelvex)
└── Remote memory (allocated by Zelvex)
    ├── [code buffer]       ← Lua source as UTF-8
    └── [shell buffer]      ← x86 shellcode for injection
```

### Cross-Bitness Injection
- Zelvex is 64-bit
- MiniGameApp.exe is 32-bit (WoW64)
- Shellcode is assembled as x86 (32-bit) and injected into the 32-bit process
- All pointers in shellcode are 32-bit addresses within the target's address space

---

## Comparison with Existing Zelvex Features

| Feature | Existing Cheats | Lua Injector |
|---|---|---|
| **Mechanism** | AOB scan + byte patching / sendToHost shellcode | Lua VM function calls |
| **Scope** | Specific game functions (hardcoded patterns) | Any Lua code the VM supports |
| **Flexibility** | Fixed per-cheat | Unlimited — you write the code |
| **Stability** | Version-dependent (AOB patterns break) | More stable — uses exported functions |
| **UI** | Predefined cheat cards | Free-form code editor |
| **Extensibility** | Requires adding new CheatDef entries | Just type new Lua code |

---

## What Could Be Added Next

1. **Error message capture** — Read the Lua stack after `lua_pcall` to get error strings
2. **Async execution** — Don't block the UI thread during injection
3. **Code autocomplete** — Lua keyword/function completion in the editor
4. **Script files** — Load `.lua` files from disk instead of pasting code
5. **Persistent scripts** — Save frequently-used scripts to a list
6. **VM state inspection** — Read the Lua stack, global variables, loaded modules
7. **Direct memory access via Lua** — Expose Zelvex's memory read/write as Lua functions

---

## Disclaimer

This tool is for educational and research purposes. The Lua injector accesses the game's internal VM through documented Lua C API functions. Use at your own risk.
