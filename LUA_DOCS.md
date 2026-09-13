# Zelvex Scripting Guide

Welcome. Zelvex runs **real Lua 5.1** inside the game, with an in-game menu
you build yourself from simple building blocks. This guide teaches everything
by example - no reverse-engineering knowledge needed.

> **Golden rules**
> 1. Scripts run in a **persistent** VM: variables and functions you create
>    stay alive between runs until you inject fresh or call `native.reset()`.
> 2. Every cheat call is echoed to the console as `[command] result`, so you
>    always see what happened.
> 3. `print()` goes to the Zelvex console, not game chat. Use
>    `Chat:send("hi")` for real chat.
> 4. Keep overlay labels **ASCII only** (no fancy dashes/symbols) - the
>    in-game font renders plain characters only.

---

## 1. Your first script

```lua
print(_VERSION)                 --> Lua 5.1
print(Player:getUid())          -- your ID
local x, y, z = Player:getPos() -- functions can return many values
Player:setHealth(9999)
```

Run with Ctrl+Enter. Press **STOP** any time - even infinite loops abort.

---

## 2. Cheat API

All classes support both `Class.method(...)` and `Class:method(...)`.
Booleans accept `true/false`; results come back as natural values
(numbers, `true/false`, or `nil + error message`).

### Player - you
```lua
Player:getUid()  Player:getPos()  -- x,y,z   Player:getYaw()
Player:getHp() Player:getMaxHp() Player:getHealth()
Player:setHealth(n) Player:setMaxHealth(n)
Player:setHunger(n) Player:setStamina(n)
Player:setWalkSpeed(v) Player:setRunSpeed(v)     -- defaults ~4 / ~7
Player:setJump(h)      Player:setScale(s)        -- default 1
Player:fly(on)  Player:sprint(on)  Player:noclip(on)
Player:noDrop(on) Player:jumpFly(on) Player:slowFall(on)
Player:revive(mode)  Player:addStar(n)
Player:teleportTo(uid, dx, dy, dz)
Player:bringPlayer(uid | 0)      -- drag a player to you (host best)
Player:kill(uid)                 -- teleport-kill sequence
Player:kickRoom(uid)             -- request-kick via room packet
```

### Players - everyone else
```lua
Players.list()      -- { {uid=,x=,y=,z=,team=}, ... }
Players.count()
Players.nearest(range)             -- one table or nil
Esp.w2s(uid)                       -- screen x,y of a player (see ch.6)
```

### Actor / Item / World / Chat / Vision
```lua
Actor:killAura(0|1|2)              -- off / enemies / everything
Actor:aimbot(true, range)          -- locks aim at nearest enemy
Actor:mineAll(on)  Actor:mountAll(on)

Item:give(id, count)  Item:giveBatch(id, n, times)
Item:sort() Item:repairAll() Item:discardAll() Item:unlockLocked()

World:setHours(t) World:setTimespeed(v)
Chat:send("hello")                 -- visible in-game

Vision:hitWalls(on) Vision:groundSee(on) Vision:airSee(on)
```

Every command also exists flat: `native.setHealth(1000)` etc.

---

## 3. The GUI framework

Build menus that render **inside the game** (INSERT shows/hides panels;
clicks over panels belong to you, everything else stays with the game).

```lua
assert(gui.start())                -- boot once
gui.reset()                        -- clear previous layout
local w = gui.window("My Menu")    -- returns a handle
```

### Widgets
| Call | What it does |
|---|---|
| `gui.label(w, "text")` | plain text row |
| `gui.textColored(w, "text", r,g,b [,a])` | colored text (0..1 floats) |
| `gui.button(w, "name", fn)` | button; `fn` runs on click |
| `gui.smallButton(w, "name", fn)` | compact inline button |
| `gui.sameLine(w)` | next widget goes on the same row |
| `gui.separator(w)` | horizontal divider line |
| `gui.toggle(w, "name", default, fn)` | checkbox; `fn(newstate)` |
| `gui.slider(w, "name", min, max, default)` | slider; read with `gui.get` |
| `gui.dropdown(w, "name", "A\|B\|C", sel, fn)` | combo box; `fn(index)` |
| `gui.combo(w, "name", "A\|B\|C", sel, fn)` | same as dropdown |
| `gui.listbox(w, "name", "A\|B\|C", sel, fn)` | list box; up to 8 entries visible |
| `gui.input(w, "name", "default", fn)` | text field; `fn(text)` on Enter |
| `gui.color(w, "name", r,g,b, fn)` | color picker; `fn(r,g,b)` live |
| `gui.progress(w, "label", 0..1)` | progress bar (`gui.set` updates it) |
| `gui.keybind(w, "name", vk, fn)` | click, press a key; `fn` on press |
| `gui.section(w, "TITLE")` | collapsible header for rows below |
| `gui.radar(w, radiusPx, style, range)` | live radar HUD |
| `gui.get(w, id)` | read current value |
| `gui.set(w, id, value)` | write a value from script (slider/toggle/progress/listbox) |
| `gui.opts(w, id, "A\|B\|C")` | replace dropdown/combo/listbox options live - no flicker |
| `gui.config(w, id, r, style, range)` | retune a radar live |
| `gui.clear(w)` | empty one window (for dynamic lists) |
| `gui.accent(r, g, b)` | recolor the whole theme (0..1 floats) |
| `gui.show(false)` / `gui.show(true)` | hide/show all panels |

Limits: 5 windows x 96 widgets per window, 8 options per dropdown/combo/
listbox, 64 chars per input field, 48 chars per label. Callbacks fire on
the script thread inside `wait()` slices - keep them short.

Example section:

```lua
gui.section(w, "COMBAT")
gui.toggle(w, "Aimbot", false, function(on)
    Actor:aimbot(on, 3000)
end)
gui.dropdown(w, "Kill Aura", "Off|Enemies|All", 1, function(i)
    Actor:killAura(i - 1)
end)
gui.keybind(w, "Noclip key", 0x76, function()   -- 0x76 = F7
    noclipOn = not noclipOn
    Player:noclip(noclipOn)
end)
```

Live player list without flicker (update options in place):

```lua
local w = gui.window("Players")
local lb = gui.listbox(w, "players", "scanning...", 1)

while true do
    local names = {}
    for i, p in ipairs(Players.list()) do
        if i > 8 then break end
        names[i] = "#" .. p.uid .. " team " .. p.team
    end
    if #names == 0 then names[1] = "(none)" end
    gui.opts(w, lb, table.concat(names, "|"))
    wait(1)
end
```

Key codes are Windows VKs: letters/digits are their ASCII value (`X`=88),
F-keys are `0x70`=F1 ... `0x7B`=F12. INSERT is reserved for show/hide.

### The unload pattern (CLOSE button)

Always give users an exit:

```lua
local running = true
gui.button(w, "CLOSE", function() running = false end)

while running do
    -- apply sliders, godmode loops, refresh lists...
    wait(0.5)
end

-- restore everything you changed:
pcall(function()
    Actor:aimbot(false)  Actor:killAura(0)
    Player:noclip(false) Player:setWalkSpeed(4)
end)
gui.reset()
```

---

## 4. Background events

Handlers keep running even after your script ends:

```lua
Events.onTick(function()
    if godOn then Player:setHealth(9999) end
end)
Events.clear()                     -- stop them all
```

---

## 5. Input + HTTP

```lua
input.key(0x58)                    -- true while X is held
http.get("https://site/file.lua")  -- body string or nil,err
http.post("https://site/api", '{"a":1}', "application/json")
```

Remote loader pattern:

```lua
local src = http.get("https://mysite.vercel.app/script.lua")
if src then loadstring(src)() end
```

---

## 6. Game-VM bridge (official dev-wiki API)

The game runs its own Lua VM (`liblua.dll`) hosting the documented
Player/World/Actor/Chat/... tables
(https://dev-wiki.mini1.cn — Player, World, Actor, Block, Item, Chat...).
`G.*` calls into it from your scripts. Client-friendly: every failure
returns `nil + message`, never crashes. Writes still obey host authority
(as client you get reads + local effects; full power when you host).

```lua
local ok, detail = G.ready()
print(ok, detail)   -- false + "VM not ready (join a map first)" in lobby

G.call("Player", "getNickname", 0)     -- official API, any objid
G.call("Player", "getAttr", 0, 2)      -- host HP, etc.
G.call("Chat", "sendSystemMsg", "hi")  -- system message
G.exec("return Player:getMainPlayerUin()")
```

Rules: module/func are strings; args are number/string/boolean/nil
(max 8). One return value comes back. Use `G.ready()` first and
`pcall` around hot paths — the game VM is single-threaded.

---

## 7. ESP notes

- `Players.list()` + `gui.radar()` gives a rotating radar with team shapes.
- `Esp.w2s(uid)` projects a player onto your screen - the numbers map to
  screen space and power nameplates/boxes (see bundled ZELVEX_HUB.lua).

---

## 8. Limits & tips

- Script size cap: ~16 KB per run. Split big hubs into files loaded with
  `loadstring(http.get(...))()`.
- Overlay labels: ASCII only.
- Max 5 windows x 96 widgets per layout; `Events.onTick` holds 16 handlers.
- `Esp.screen()` returns real viewport size from the overlay.
- The console streams live while scripts run; output also keeps flowing
  between runs (that's how tick prints appear).

Happy scripting.
