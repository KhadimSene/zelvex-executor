-- ================================================================
--   ZELVEX HUB v8  -  diagnostic + reliable cheats only
--   Every toggle prints [ok] or [fail: reason] so you know WHY.
--   INSERT hide/show | F1 panic | X CLOSE always visible
--   Only cheats that work as CLIENT are exposed. Host-only ones
--   are marked and will show "needs host" if you try.
-- ================================================================

assert(gui.start(), "overlay failed - inject the DLL first")
gui.reset()
gui.accent(0.24, 0.55, 1.00)

local running, panicNow = true, false
local S = {
    god=false, heal=false, food=false,
    fly=false, noclip=false, sprint=false, speedOn=false,
    walkSpd=8, runSpd=14, scale=1.0, jumpH=1.0,
    aura=0, aimbot=false, aimRange=3000,
    wallsee=false, ground=false, airsee=false,
    timeOn=false, timeSpd=10,
}

local function okPrint(tag, ok, err)
    if ok then print("["..tag.."] ok") else print("["..tag.."] FAIL: "..tostring(err)) end
    return ok
end

-- ================= WINDOW =================
local w = gui.window("Zelvex Hub")
gui.textColored(w, "ZELVEX HUB", 0.35, 0.62, 1.00)
gui.label(w, "v8  diagnostic  |  redz style")
gui.buttonColored(w, "X  CLOSE  [INSERT hides]", 0.95, 0.30, 0.28, function() running=false end)
gui.separator(w)
gui.paragraph(w, "If a toggle says FAIL, you are not in a map or the pattern is stale. Use DOCTOR below.")
gui.buttonColored(w, "JOIN DISCORD", 0.18, 0.80, 0.44, function() print("[zv] discord.gg/your-invite") end)
gui.separator(w)

-- ================= HOME =================
gui.tab(w, "Home")
gui.paragraph(w, "Welcome. Pick a page on the left. INSERT hides.")
gui.button(w, "Print status", function()
    local ok, hp = pcall(function() return Player:getHp() end)
    local ok2, mx = pcall(function() return Player:getMaxHp() end)
    local ok3, x, y, z = pcall(function() return Player:getPos() end)
    print(string.format("[status] hp=%s/%s pos=%s %s %s", tostring(hp), tostring(mx), tostring(x), tostring(y), tostring(z)))
    if not hp then print("[status] NOT IN WORLD - join a map first") end
end)
gui.button(w, "DOCTOR CHECK", function()
    print("--- doctor ---")
    local ok, a = pcall(function() return native.state and native.state() or "no state" end)
    print(tostring(a))
    local ok2, c = pcall(function() return Players.count() end)
    print("players: "..tostring(c))
    local ok3, s = pcall(function() return Player:getPos() end)
    if not ok3 then print("player: NOT IN WORLD") else print("player: in world") end
    -- test each native quickly
    local tests = {
        {"fly", function() return Player:fly(false) end},
        {"noclip", function() return Player:noclip(false) end},
        {"sprint", function() return Player:sprint(false) end},
        {"wall", function() return Vision:hitWalls(false) end},
        {"give", function() return Item:give(1,1) end},
    }
    for _, t in ipairs(tests) do
        local ok, res, err = pcall(t[2])
        if ok and res then print(t[1]..": ok") else print(t[1]..": FAIL "..tostring(err or res)) end
    end
    print("--- end doctor ---")
end)
gui.separator(w)
gui.keybind(w, "Panic", 0x70, function() panicNow=true end)

-- ================= PLAYER =================
gui.tab(w, "Player")
gui.toggleDesc(w, "God Mode", "health locked at max", false, function(on) S.god=on; print("[god] "..tostring(on)) end)
gui.toggleDesc(w, "Auto Heal", "heal when low", false, function(on) S.heal=on end)
gui.toggleDesc(w, "Full Hunger", "hunger+stamina full", false, function(on) S.food=on end)
gui.separator(w)
gui.label(w, "Scale")
local scaleSlide = gui.slider(w, "scale", 0.5, 3.0, 1.0)
gui.label(w, "Jump height")
local jumpSlide = gui.slider(w, "jump height", 0.5, 5.0, 1.0)
gui.separator(w)
gui.button(w, "REVIVE", function() okPrint("revive", Player:revive(2)) end)
gui.sameLine(w)
gui.button(w, "ADD STAR", function() okPrint("star", Player:addStar(1)) end)
gui.button(w, "MAX HEALTH", function() okPrint("hp", Player:setHealth(9999)) end)
gui.button(w, "HEAL TO FULL", function() local mx=Player:getMaxHp(); if mx then okPrint("heal", Player:setHealth(mx)) end end)

-- ================= MOVEMENT =================
gui.tab(w, "Movement")
local flyTog = gui.toggleDesc(w, "Fly", "native flight - CLIENT", false, function(on)
    local ok, err = Player:fly(on)
    okPrint("fly", ok, err); if ok then S.fly=on else S.fly=false; gui.set(w, flyTog, false) end
end)
local ncTog = gui.toggleDesc(w, "Noclip", "walk through walls - CLIENT", false, function(on)
    local ok, err = Player:noclip(on)
    okPrint("noclip", ok, err); if ok then S.noclip=on else gui.set(w, ncTog, false) end
end)
local spTog = gui.toggleDesc(w, "Sprint", "forced sprint - CLIENT", false, function(on)
    local ok, err = Player:sprint(on)
    okPrint("sprint", ok, err); if ok then S.sprint=on else gui.set(w, spTog, false) end
end)
local spdTog = gui.toggleDesc(w, "Speed Hack", "uses sliders below - CLIENT", false, function(on) S.speedOn=on; print("[speed] "..tostring(on)) end)
gui.keybind(w, "Fly bind", 0x46, function()
    local ok, err = Player:fly(not S.fly)
    if ok then S.fly=not S.fly; gui.set(w, flyTog, S.fly); print("[fly] "..tostring(S.fly)) else print("[fly] fail: "..tostring(err)) end
end)
gui.keybind(w, "Noclip bind", 0x58, function()
    local ok, err = Player:noclip(not S.noclip)
    if ok then S.noclip=not S.noclip; gui.set(w, ncTog, S.noclip) else print("[noclip] fail: "..tostring(err)) end
end)
gui.keybind(w, "Sprint bind", 0x43, function()
    local ok, err = Player:sprint(not S.sprint)
    if ok then S.sprint=not S.sprint; gui.set(w, spTog, S.sprint) else print("[sprint] fail: "..tostring(err)) end
end)
gui.separator(w)
gui.label(w, "Walk speed")
local walkSlide = gui.slider(w, "walk", 4, 30, 8)
gui.label(w, "Run speed")
local runSlide = gui.slider(w, "run", 7, 50, 14)

-- ================= COMBAT (CLIENT ONLY) =================
gui.tab(w, "Combat")
gui.paragraph(w, "CLIENT cheats - no host needed. Host-only ones hidden.")
local auraDrop = gui.dropdown(w, "Kill Aura", "Off|Enemies|All", 1, function(i)
    local ok, err = Actor:killAura(i-1)
    okPrint("aura", ok, err); if ok then S.aura=i-1 else gui.set(w, auraDrop, S.aura+1) end
end)
local aimTog = gui.toggleDesc(w, "Aimbot", "locks to nearest - CLIENT", false, function(on)
    local ok, err = Actor:aimbot(on, S.aimRange)
    okPrint("aim", ok, err); if ok then S.aimbot=on else gui.set(w, aimTog, false) end
end)
gui.label(w, "Aim range")
local aimSlide = gui.slider(w, "aim", 500, 9000, 3000)
gui.keybind(w, "Aim bind", 0x45, function()
    local ok, err = Actor:aimbot(not S.aimbot, S.aimRange)
    if ok then S.aimbot=not S.aimbot; gui.set(w, aimTog, S.aimbot) else print("[aim] fail: "..tostring(err)) end
end)
gui.separator(w)
gui.button(w, "Kill Look-At", function()
    local uid = Players:looking()
    if not uid then print("[kill] no target under crosshair") return end
    local ok, err = Player:kill(uid)
    okPrint("kill", ok, err)
end)
gui.keybind(w, "Kill bind", 0x4B, function()
    local uid = Players:looking(); if uid then Player:kill(uid) end
end)

-- ================= VISUAL (CLIENT) =================
gui.tab(w, "Visual")
gui.toggleDesc(w, "See Through Walls", "hitWalls - CLIENT", false, function(on)
    local ok, err = Vision:hitWalls(on)
    okPrint("wall", ok, err); if ok then S.wallsee=on else print("[wall] needs re-attach?") end
end)
gui.toggleDesc(w, "Ground See", "see through ground - CLIENT", false, function(on)
    local ok, err = Vision:groundSee(on); okPrint("ground", ok, err); if ok then S.ground=on end
end)
gui.toggleDesc(w, "See In Air", "air vision - CLIENT", false, function(on)
    local ok, err = Vision:airSee(on); okPrint("air", ok, err); if ok then S.airsee=on end
end)
gui.keybind(w, "Wallhack", 0x5A, function()
    local ok, err = Vision:hitWalls(not S.wallsee)
    if ok then S.wallsee=not S.wallsee; print("[wall] "..tostring(S.wallsee)) else print("[wall] fail: "..tostring(err)) end
end)

-- ================= PLAYERS =================
gui.tab(w, "Players")
gui.paragraph(w, "Live scan - CLIENT teleport, host kick is separate.")
local lb = gui.listbox(w, "players", "scanning...", 1)
gui.separator(w)
local yOff = gui.input(w, "y offset", "3")
gui.button(w, "TELEPORT TO SELECTED", function()
    local off = tonumber(gui.get(w, yOff)) or 3
    local sel = gui.get(w, lb)
    local ok, pl = pcall(Players.list)
    if ok and pl and sel and pl[sel] then
        local done, err = Player:teleportTo(pl[sel].uid, 0, off, 0)
        okPrint("tp", done, err)
    else print("[tp] no target - not in world?") end
end)
gui.button(w, "BRING SELECTED HERE", function()
    local sel = gui.get(w, lb)
    local ok, pl = pcall(Players.list)
    if ok and pl and sel and pl[sel] then
        local done, err = Player:bringPlayer(pl[sel].uid, 0, 2, 0)
        if not done then print("[bring] needs host: "..tostring(err)) else print("[bring] ok") end
    end
end)
gui.buttonColored(w, "KICK SELECTED (HOST ONLY)", 0.90, 0.30, 0.30, function()
    local sel = gui.get(w, lb)
    local ok, pl = pcall(Players.list)
    if ok and pl and sel and pl[sel] then
        local done, err = Player:kickRoom(pl[sel].uid)
        if not done then print("[kick] needs host: "..tostring(err)) else print("[kick] sent") end
    end
end)
gui.separator(w)
gui.toggle(w, "Radar", true, nil)
gui.radar(w, 110, 1, 6000)

-- ================= WORLD (CLIENT) =================
gui.tab(w, "World")
gui.toggleDesc(w, "Fast Time", "clock speed - CLIENT visual", false, function(on)
    S.timeOn=on; if not on then World:setTimespeed(1) end
end)
gui.label(w, "Time speed")
local tSpd = gui.slider(w, "time", 1, 30, 10)
gui.smallButton(w, "DAY", function() World:setHours(12) end)
gui.sameLine(w)
gui.smallButton(w, "NIGHT", function() World:setHours(0) end)
gui.separator(w)
gui.label(w, "Chat")
local chatIn = gui.input(w, "message", "hello")
gui.button(w, "SEND CHAT", function()
    local m = gui.get(w, chatIn)
    if m and m~="" then local ok, err=Chat:send(m); okPrint("chat", ok, err) end
end)
gui.separator(w)
gui.label(w, "Give item (CLIENT, may be validated)")
local gid = gui.input(w, "item id", "1105")
local gqty = gui.input(w, "qty", "1")
gui.buttonColored(w, "GIVE ITEMS", 0.24, 0.55, 1.00, function()
    local id=tonumber(gui.get(w,gid)); local q=tonumber(gui.get(w,gqty)) or 1
    if id then local ok, err=Item:give(id,q); okPrint("give", ok, err) end
end)
gui.button(w, "SORT PACK", function() okPrint("sort", Item:sort()) end)
gui.sameLine(w)
gui.button(w, "REPAIR ALL", function() okPrint("repair", Item:repairAll()) end)
gui.buttonColored(w, "GIVE TERRAIN EDITOR", 0.55, 0.45, 1.00, function()
    local a, e1 = Item:give(12239, 1)
    local b, e2 = Item:give(10500, 1)
    print((a and b) and "[terrain] given 12239+10500" or ("[terrain] fail: "..tostring(e1).."/"..tostring(e2)))
end)

-- ================= SYSTEM =================
gui.tab(w, "System")
gui.color(w, "Accent", 0.24, 0.55, 1.00, function(r,g,b) gui.accent(r,g,b) end)
gui.separator(w)
gui.button(w, "GAME VM CHECK", function()
    local ok, detail = G.ready()
    print("[G] " .. tostring(ok) .. " - " .. tostring(detail))
    if ok then
        local r, nick = pcall(function() return G.call("Player", "getNickname", 0) end)
        print("[G] nickname: " .. tostring(nick))
    end
end)
gui.separator(w)
gui.button(w, "DISABLE ALL", function()
    Actor:aimbot(false); Actor:killAura(0)
    Player:fly(false); Player:noclip(false); Player:sprint(false)
    Player:slowFall(false); Player:jumpFly(false); Player:noDrop(false)
    Vision:hitWalls(false); Vision:groundSee(false); Vision:airSee(false)
    World:setTimespeed(1)
    for k in pairs(S) do if type(S[k])=="boolean" then S[k]=false end end
    print("[zv] all off")
end)
gui.buttonColored(w, "CLOSE CLIENT", 0.95, 0.30, 0.28, function() running=false end)

-- ================= LOOP =================
local function cleanup()
    pcall(function()
        Actor:aimbot(false); Actor:killAura(0)
        Player:fly(false); Player:noclip(false); Player:sprint(false)
        Vision:hitWalls(false); Vision:groundSee(false); Vision:airSee(false)
        Player:setWalkSpeed(4); Player:setRunSpeed(7)
        Player:setScale(1.0); Player:setJump(1.0)
        World:setTimespeed(1)
    end)
    gui.reset(); print("[zv] unloaded")
end

local lastSig=""
local function refreshPlayers()
    local ok, list = pcall(Players.list)
    if not ok or not list then return end
    local names, sig = {}, {}
    for i,p in ipairs(list) do
        if i>8 then break end
        names[i]=string.format("#%d team %d", p.uid%100000, p.team)
        sig[i]=tostring(p.uid)
    end
    if #names==0 then names[1]="(no players nearby)" sig[1]="none" end
    local s=table.concat(sig, ",")
    if s~=lastSig then lastSig=s; gui.opts(w, lb, table.concat(names, "|")) end
end

print("[zv] v8 loaded - INSERT show, F1 panic, DOCTOR if broken")
local tick=0
while running and not panicNow do
    do local v=gui.get(w, scaleSlide) if v then S.scale=v; Player:setScale(v) end end
    do local v=gui.get(w, jumpSlide)  if v then S.jumpH=v; Player:setJump(v) end end
    do local v=gui.get(w, walkSlide)  if v then S.walkSpd=v end end
    do local v=gui.get(w, runSlide)   if v then S.runSpd=v end end
    do local v=gui.get(w, aimSlide)   if v and v~=S.aimRange then S.aimRange=v; if S.aimbot then Actor:aimbot(true,v) end end end
    do local v=gui.get(w, tSpd)       if v then S.timeSpd=v end end

    tick=tick+1
    if tick%4==0 then
        if S.god  then Player:setHealth(9999) end
        if S.heal then local hp=Player:getHp(); if hp and hp<9900 then Player:setHealth(9900) end end
        if S.food then Player:setHunger(100); Player:setStamina(100) end
        if S.speedOn then Player:setWalkSpeed(S.walkSpd); Player:setRunSpeed(S.runSpd) end
        if S.timeOn then World:setTimespeed(S.timeSpd) end
        refreshPlayers()
    end
    wait(0.05)
end
if panicNow then print("[zv] PANIC") end
cleanup()
