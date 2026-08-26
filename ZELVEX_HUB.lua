-- ================================================================
--   ZELVEX HUB v1 
--   INSERT hide/show  |  F1 panic  |  X CLOSE always visible
-- ================================================================

assert(gui.start(), "overlay failed - inject the DLL first")
gui.reset()
gui.accent(0.24, 0.55, 1.00)

local running, panicNow = true, false

local S = {
    god=false, heal=false, food=false, nodrop=false,
    jumpfly=false, slowfall=false, scale=1.0, jumpH=1.0,
    fly=false, noclip=false, sprint=false, speedOn=false,
    walkSpd=8, runSpd=14,
    aura=0, aimbot=false, aimRange=3000,
    mountAll=false, mineAll=false,
    wallsee=false, ground=false, airsee=false,
    timeOn=false, timeSpd=10,
}

local function doCall(fn)
    local ok, a, b = pcall(fn)
    if not ok then print("[zv] ERR: " .. tostring(a)); return nil, a end
    if a == nil and b then print("[zv] fail: " .. tostring(b)); return nil, b end
    return a, b
end

-- ================= WINDOW =================
local w = gui.window("Zelvex Hub")
gui.textColored(w, "ZELVEX HUB", 0.35, 0.62, 1.00)
gui.label(w, "mini world  v7.1  -  redz edition")
gui.buttonColored(w, "X  CLOSE  [INSERT hides]", 0.95, 0.30, 0.28, function() running=false end)
gui.separator(w)
gui.paragraph(w, "Join the community for updates and support.")
gui.buttonColored(w, "JOIN DISCORD", 0.18, 0.80, 0.44, function()
    print("[zv] discord.gg/your-invite")
end)
gui.separator(w)

-- ================= HOME =================
gui.tab(w, "Home")
gui.paragraph(w, "Welcome. Pick a page on the left. INSERT hides the menu.")
gui.button(w, "Print status", function()
    local hp = Player:getHp()
    local mx = Player:getMaxHp()
    local x, y, z = Player:getPos()
    print(string.format("[zv] hp %s/%s pos %s %s %s", tostring(hp), tostring(mx), tostring(x), tostring(y), tostring(z)))
    if not hp then print("[zv] not in world - join a map first") end
end)
gui.button(w, "DOCTOR CHECK", function()
    local ok, s = pcall(function() return native.state and native.state() or "no state" end)
    print("[zv] " .. tostring(s or ok))
    local c = Players.count and Players.count() or 0
    print("[zv] players: " .. tostring(c))
end)
gui.toggleDesc(w, "God Mode", "health locked at max", false, function(on) S.god=on end)
gui.toggleDesc(w, "Auto Heal", "restores when low", false, function(on) S.heal=on end)
gui.toggleDesc(w, "Full Hunger", "hunger and stamina max", false, function(on) S.food=on end)
gui.separator(w)
gui.keybind(w, "Panic", 0x70, function() panicNow=true end)

-- ================= PLAYER =================
gui.tab(w, "Player")
gui.toggleDesc(w, "God Mode", "health locked at max", false, function(on) S.god=on end)
gui.toggleDesc(w, "Auto Heal", "heal when below max", false, function(on) S.heal=on end)
gui.toggleDesc(w, "Full Hunger", "hunger and stamina full", false, function(on) S.food=on end)
gui.toggleDesc(w, "No Drop", "keep items on death", false, function(on)
    local ok, err = Player:noDrop(on)
    print(ok and ("[noDrop] " .. tostring(ok)) or ("[noDrop] fail: " .. tostring(err)))
    if ok then S.nodrop=on end
end)
gui.toggleDesc(w, "Jump Fly", "fly via jumping", false, function(on)
    local ok, err = Player:jumpFly(on)
    print(ok and ("[jumpFly] " .. tostring(ok)) or ("[jumpFly] fail: " .. tostring(err)))
    if ok then S.jumpfly=on end
end)
gui.toggleDesc(w, "Slow Fall", "fall slowly", false, function(on)
    local ok, err = Player:slowFall(on)
    print(ok and ("[slowFall] " .. tostring(ok)) or ("[slowFall] fail: " .. tostring(err)))
    if ok then S.slowfall=on end
end)
gui.separator(w)
gui.label(w, "Scale")
local scaleSlide = gui.slider(w, "scale", 0.5, 3.0, 1.0)
gui.label(w, "Jump height")
local jumpSlide = gui.slider(w, "jump height", 0.5, 5.0, 1.0)
gui.separator(w)
gui.button(w, "REVIVE", function() local ok, e=Player:revive(2); print(ok and "[revive] ok" or "[revive] fail: "..tostring(e)) end)
gui.sameLine(w)
gui.button(w, "ADD STAR", function() local ok, e=Player:addStar(1); print(ok and "[star] ok" or "[star] fail: "..tostring(e)) end)
gui.button(w, "MAX HEALTH", function() local ok, e=Player:setHealth(9999); print(ok and "[hp] ok" or "[hp] fail: "..tostring(e)) end)
gui.keybind(w, "Revive bind", 0x52, function() Player:revive(2) end)

-- ================= MOVEMENT =================
gui.tab(w, "Movement")
local flyTog = gui.toggleDesc(w, "Fly", "native flight", false, function(on)
    local ok, err = Player:fly(on)
    print(ok and ("[fly] " .. tostring(ok)) or ("[fly] fail: " .. tostring(err)))
    if ok then S.fly=on else S.fly=false end
end)
local ncTog  = gui.toggleDesc(w, "Noclip", "walk through walls", false, function(on)
    local ok, err = Player:noclip(on)
    print(ok and ("[noclip] " .. tostring(ok)) or ("[noclip] fail: " .. tostring(err)))
    if ok then S.noclip=on end
end)
local spTog  = gui.toggleDesc(w, "Sprint", "forced sprint", false, function(on)
    local ok, err = Player:sprint(on)
    print(ok and ("[sprint] " .. tostring(ok)) or ("[sprint] fail: " .. tostring(err)))
    if ok then S.sprint=on end
end)
local spdTog = gui.toggleDesc(w, "Speed Hack", "uses walk speed slider", false, function(on) S.speedOn=on end)
gui.keybind(w, "Fly bind", 0x46, function()
    local ok, err = Player:fly(not S.fly)
    if ok then S.fly=not S.fly; gui.set(w, flyTog, S.fly); print("[fly] " .. tostring(S.fly)) else print("[fly] fail: "..tostring(err)) end
end)
gui.keybind(w, "Noclip bind", 0x58, function()
    local ok, err = Player:noclip(not S.noclip)
    if ok then S.noclip=not S.noclip; gui.set(w, ncTog, S.noclip); print("[noclip] "..tostring(S.noclip)) else print("[noclip] fail: "..tostring(err)) end
end)
gui.keybind(w, "Sprint bind", 0x43, function()
    local ok, err = Player:sprint(not S.sprint)
    if ok then S.sprint=not S.sprint; gui.set(w, spTog, S.sprint) else print("[sprint] fail: "..tostring(err)) end
end)
gui.keybind(w, "Speed bind", 0x56, function() S.speedOn=not S.speedOn; gui.set(w, spdTog, S.speedOn) end)
gui.separator(w)
gui.label(w, "Walk speed")
local walkSlide = gui.slider(w, "walk", 4, 30, 8)
gui.label(w, "Run speed")
local runSlide = gui.slider(w, "run", 7, 50, 14)

-- ================= COMBAT =================
gui.tab(w, "Combat")
local auraDrop = gui.dropdown(w, "Kill Aura", "Off|Enemies|All", 1, function(i)
    local ok, err = Actor:killAura(i-1)
    print(ok and ("[aura] mode "..(i-1)) or ("[aura] fail: "..tostring(err)))
    if ok then S.aura=i-1 end
end)
gui.keybind(w, "Aura bind", 0x4B, function()
    local n=(S.aura+1)%3; local ok, err=Actor:killAura(n)
    if ok then S.aura=n; print("[aura] "..n) else print("[aura] fail: "..tostring(err)) end
end)
local aimTog = gui.toggleDesc(w, "Aimbot", "locks to nearest enemy", false, function(on)
    local ok, err=Actor:aimbot(on, S.aimRange)
    print(ok and ("[aim] "..tostring(on)) or ("[aim] fail: "..tostring(err)))
    if ok then S.aimbot=on end
end)
gui.label(w, "Aim range")
local aimSlide = gui.slider(w, "aim", 500, 9000, 3000)
gui.keybind(w, "Aimbot bind", 0x45, function()
    local ok, err=Actor:aimbot(not S.aimbot, S.aimRange)
    if ok then S.aimbot=not S.aimbot; gui.set(w, aimTog, S.aimbot) else print("[aim] fail: "..tostring(err)) end
end)
gui.separator(w)
gui.toggleDesc(w, "Mount All", "mount any actor", false, function(on)
    local ok, err=Actor:mountAll(on); print(ok and ("[mount] "..tostring(on)) or ("[mount] fail: "..tostring(err))); if ok then S.mountAll=on end
end)
gui.toggleDesc(w, "Mine All", "instant mine", false, function(on)
    local ok, err=Actor:mineAll(on); print(ok and ("[mine] "..tostring(on)) or ("[mine] fail: "..tostring(err))); if ok then S.mineAll=on end
end)
gui.buttonColored(w, "KILL ALL (HOST)", 0.90, 0.30, 0.30, function()
    local ok, err=Actor:killAllHost(1); print(ok and "[killAll] fired" or "[killAll] fail: "..tostring(err))
end)

-- ================= VISUAL =================
gui.tab(w, "Visual")
gui.toggleDesc(w, "See Through Walls", "wall vision", false, function(on)
    local ok, err=Vision:hitWalls(on); print(ok and ("[wall] "..tostring(on)) or ("[wall] fail: "..tostring(err))); if ok then S.wallsee=on end
end)
gui.toggleDesc(w, "Ground See", "see through ground", false, function(on)
    local ok, err=Vision:groundSee(on); print(ok and "[ground] ok" or "[ground] fail: "..tostring(err)); if ok then S.ground=on end
end)
gui.toggleDesc(w, "See In Air", "air vision", false, function(on)
    local ok, err=Vision:airSee(on); print(ok and "[air] ok" or "[air] fail: "..tostring(err)); if ok then S.airsee=on end
end)
gui.keybind(w, "Wallhack bind", 0x5A, function()
    local ok, err=Vision:hitWalls(not S.wallsee)
    if ok then S.wallsee=not S.wallsee; print("[wall] "..tostring(S.wallsee)) else print("[wall] fail: "..tostring(err)) end
end)

-- ================= PLAYERS =================
gui.tab(w, "Players")
gui.paragraph(w, "Live scan. Pick an entry then teleport.")
local lb = gui.listbox(w, "players", "scanning...", 1)
gui.separator(w)
local yOff = gui.input(w, "y offset", "3")
gui.button(w, "TELEPORT TO SELECTED", function()
    local off = tonumber(gui.get(w, yOff)) or 3
    local sel = gui.get(w, lb)
    local ok, pl = pcall(Players.list)
    if ok and pl and sel and pl[sel] then
        local done, err = Player:teleportTo(pl[sel].uid, 0, off, 0)
        print(done and ("[tp] -> " .. pl[sel].uid) or ("[tp] fail: " .. tostring(err)))
    else print("[tp] no target") end
end)
gui.button(w, "BRING SELECTED HERE", function()
    local sel = gui.get(w, lb)
    local ok, pl = pcall(Players.list)
    if ok and pl and sel and pl[sel] then
        local done, err = Player:bringPlayer(pl[sel].uid, 0, 2, 0)
        print(done and "[bring] ok" or ("[bring] fail: " .. tostring(err)))
    end
end)
local kickId = gui.input(w, "kick uid", "")
gui.buttonColored(w, "KICK PLAYER", 0.90, 0.30, 0.30, function()
    local id = tonumber(gui.get(w, kickId))
    if id then local ok, err=Player:kickRoom(id); print(ok and "[kick] ok" or "[kick] fail: "..tostring(err)) end
end)
gui.separator(w)
gui.toggle(w, "Radar", true, nil)
gui.radar(w, 110, 1, 6000)

-- ================= WORLD =================
gui.tab(w, "World")
gui.toggleDesc(w, "Fast Time", "speeds up clock", false, function(on)
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
    if m and m~="" then local ok, err=Chat:send(m); print(ok and "[chat] sent" or "[chat] fail: "..tostring(err)) end
end)
gui.separator(w)
gui.label(w, "Give item")
local gid = gui.input(w, "item id", "1105")
local gqty = gui.input(w, "qty", "1")
gui.buttonColored(w, "GIVE ITEMS", 0.24, 0.55, 1.00, function()
    local id=tonumber(gui.get(w,gid)); local q=tonumber(gui.get(w,gqty)) or 1
    if id then local ok, err=Item:give(id,q); print(ok and ("[give] "..q.." x "..id) or ("[give] fail: "..tostring(err))) end
end)
gui.button(w, "SORT PACK", function() local ok, e=Item:sort(); print(ok and "[sort] ok" or "[sort] fail: "..tostring(e)) end)
gui.sameLine(w)
gui.button(w, "REPAIR ALL", function() local ok, e=Item:repairAll(); print(ok and "[repair] ok" or "[repair] fail: "..tostring(e)) end)
gui.button(w, "UNLOCK ITEMS", function() local ok, e=Item:unlockLocked(true); print(ok and "[unlock] ok" or "[unlock] fail: "..tostring(e)) end)
gui.button(w, "DISCARD ALL", function() local ok, e=Item:discardAll(); print(ok and "[discard] ok" or "[discard] fail: "..tostring(e)) end)

-- ================= SYSTEM =================
gui.tab(w, "System")
gui.color(w, "Accent", 0.24, 0.55, 1.00, function(r,g,b) gui.accent(r,g,b) end)
gui.separator(w)
gui.button(w, "DISABLE ALL", function()
    Actor:aimbot(false); Actor:killAura(0); Actor:mountAll(false); Actor:mineAll(false)
    Player:fly(false); Player:noclip(false); Player:sprint(false)
    Player:slowFall(false); Player:jumpFly(false); Player:noDrop(false)
    Vision:hitWalls(false); Vision:groundSee(false); Vision:airSee(false)
    World:setTimespeed(1)
    for k in pairs(S) do if type(S[k])=="boolean" then S[k]=false end end
    S.walkSpd=8; S.runSpd=14; S.scale=1.0; S.jumpH=1.0; S.aimRange=3000; S.timeSpd=10
    print("[zv] all off")
end)
gui.buttonColored(w, "CLOSE CLIENT", 0.95, 0.30, 0.28, function() running=false end)

-- ================= LOOP =================
local function cleanup()
    pcall(function()
        Actor:aimbot(false); Actor:killAura(0); Actor:mountAll(false); Actor:mineAll(false)
        Player:fly(false); Player:noclip(false); Player:sprint(false)
        Player:slowFall(false); Player:jumpFly(false); Player:noDrop(false)
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

print("[zv] v7.1 loaded - INSERT show, F1 panic, X CLOSE top always visible")
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
