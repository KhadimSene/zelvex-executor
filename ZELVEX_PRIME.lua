-- Zelvex Prime Client v1.1
-- Built only against the documented Zelvex Lua 5.1 API.
-- INSERT = show/hide client.  F1 = panic/disable combat + movement.
-- ASCII-only labels are intentional.
-- v1.1 fixes: gui.get uses numeric IDs, panic flag set, tick throttled to 10/s

assert(gui.start())
gui.reset()
gui.accent(0.42, 0.72, 1.0)

local C = {
    name = "ZELVEX PRIME",
    version = "1.1",
    panicVK = 0x70, -- F1
}

local state = {
    modules = {},
    windows = {},
    widgetIds = {},  -- window -> {label = id}
    values = {},
    lastKeys = {},
    panic = false,
    hud = true,
    radar = true,
}

local function safe(fn) return pcall(fn) end
local function notify(s) print("[Prime] " .. tostring(s)) end

local function toggleModule(name, on)
    local m = state.modules[name]
    if not m then return end
    if m.on == on then return end
    m.on = on
    if on then
        if m.enable then safe(m.enable) end
        notify(name .. " ON")
    else
        if m.disable then safe(m.disable) end
        notify(name .. " OFF")
    end
end

local function addModule(cat, name, desc, key, enable, disable)
    state.modules[name] = {
        category=cat, name=name, desc=desc, on=false,
        key=key or 0, enable=enable, disable=disable
    }
end

local function keyPressed(vk)
    if not vk or vk == 0 then return false end
    local down = input.key(vk)
    local old = state.lastKeys[vk] or false
    state.lastKeys[vk] = down
    return down and not old
end

-- Store widget ID returned by gui.slider/gui.toggle into a table.
local function storeId(win, label, id)
    if not state.widgetIds[win] then state.widgetIds[win] = {} end
    state.widgetIds[win][label] = id
end

local function getId(win, label)
    local t = state.widgetIds[win]
    return t and t[label] or nil
end

-- =========================
-- CORE / PLAYER
-- =========================

addModule("PLAYER","God Mode","Continuously restores health",0x47,
    function() state.values.god=true end,
    function() state.values.god=false end)

addModule("MOVEMENT","Fly","Enable native flight",0x46,
    function() Player:fly(true) end,
    function() Player:fly(false) end)

addModule("MOVEMENT","Noclip","Walk through collision",0x58,
    function() Player:noclip(true) end,
    function() Player:noclip(false) end)

addModule("MOVEMENT","No Fall","Prevent falling damage/drop behavior",0x4E,
    function() Player:noDrop(true); Player:slowFall(true) end,
    function() Player:noDrop(false); Player:slowFall(false) end)

addModule("MOVEMENT","Sprint","Force sprint",0x52,
    function() Player:sprint(true) end,
    function() Player:sprint(false) end)

addModule("MOVEMENT","Jump Fly","Enable jump flight",0,
    function() Player:jumpFly(true) end,
    function() Player:jumpFly(false) end)

addModule("MOVEMENT","Speed","High movement speed",0x56,
    function() Player:setWalkSpeed(state.values.speed or 8) end,
    function() Player:setWalkSpeed(4) end)

addModule("PLAYER","Full Hunger","Keep hunger/stamina topped up",0,
    function() state.values.food=true end,
    function() state.values.food=false end)

addModule("PLAYER","Auto Heal","Continuously restore health",0,
    function() state.values.heal=true end,
    function() state.values.heal=false end)

addModule("PLAYER","No Slow Fall","Slow fall without full noclip",0,
    function() state.values.slow=true; Player:slowFall(true) end,
    function() state.values.slow=false; Player:slowFall(false) end)

-- =========================
-- COMBAT
-- =========================

addModule("COMBAT","Kill Aura","Attack nearby targets",0x4B,
    function() Actor:killAura(1) end,
    function() Actor:killAura(0) end)

addModule("COMBAT","Kill Aura All","Attack every target type",0,
    function() Actor:killAura(2) end,
    function() Actor:killAura(0) end)

addModule("COMBAT","Aimbot","Lock aim to nearest enemy",0x45,
    function() Actor:aimbot(true, state.values.aimRange or 3000) end,
    function() Actor:aimbot(false) end)

-- =========================
-- RENDER / VISION
-- =========================

addModule("RENDER","See Through Walls","Enable wall vision",0x5A,
    function() Vision:hitWalls(true) end,
    function() Vision:hitWalls(false) end)

addModule("RENDER","Ground See","Reveal ground through obstruction",0,
    function() Vision:groundSee(true) end,
    function() Vision:groundSee(false) end)

addModule("RENDER","Air See","Reveal airborne targets",0,
    function() Vision:airSee(true) end,
    function() Vision:airSee(false) end)

-- =========================
-- WORLD
-- =========================

addModule("WORLD","Fast Time","Speed up world time",0,
    function() World:setTimespeed(state.values.timeSpeed or 10) end,
    function() World:setTimespeed(1) end)

-- =========================
-- GUI
-- =========================

local combat = gui.window("PRIME - COMBAT")
state.windows.combat = combat
gui.section(combat, "COMBAT")

gui.toggle(combat, "Kill Aura", false, function(v) toggleModule("Kill Aura",v) end)
gui.dropdown(combat, "Kill Aura Mode", "Off|Enemies|All", 2, function(i)
    if state.modules["Kill Aura"].on or state.modules["Kill Aura All"].on then
        toggleModule("Kill Aura",false); toggleModule("Kill Aura All",false)
        if i == 2 then toggleModule("Kill Aura",true)
        elseif i == 3 then toggleModule("Kill Aura All",true) end
    end
end)
gui.toggle(combat, "Aimbot", false, function(v) toggleModule("Aimbot",v) end)
local aimSlider = gui.slider(combat, "Aim Range", 100, 5000, 3000)
storeId(combat, "Aim Range", aimSlider)
gui.label(combat, "F1 = PANIC / disable active modules")

local move = gui.window("PRIME - MOVEMENT")
state.windows.move = move
gui.section(move, "MOVEMENT")
gui.toggle(move, "Fly", false, function(v) toggleModule("Fly",v) end)
gui.toggle(move, "Noclip", false, function(v) toggleModule("Noclip",v) end)
gui.toggle(move, "No Fall", false, function(v) toggleModule("No Fall",v) end)
gui.toggle(move, "Sprint", false, function(v) toggleModule("Sprint",v) end)
gui.toggle(move, "Jump Fly", false, function(v) toggleModule("Jump Fly",v) end)
gui.toggle(move, "Speed", false, function(v) toggleModule("Speed",v) end)
local walkSlider = gui.slider(move, "Walk Speed", 4, 30, 8)
local runSlider = gui.slider(move, "Run Speed", 7, 50, 14)
storeId(move, "Walk Speed", walkSlider)
storeId(move, "Run Speed", runSlider)

local player = gui.window("PRIME - PLAYER")
state.windows.player = player
gui.section(player, "PLAYER")
gui.toggle(player, "God Mode", false, function(v) toggleModule("God Mode",v) end)
gui.toggle(player, "Auto Heal", false, function(v) toggleModule("Auto Heal",v) end)
gui.toggle(player, "Full Hunger", false, function(v) toggleModule("Full Hunger",v) end)
gui.toggle(player, "Slow Fall", false, function(v) toggleModule("No Slow Fall",v) end)
local scaleSlider = gui.slider(player, "Scale", 0.5, 3.0, 1.0)
storeId(player, "Scale", scaleSlider)
gui.button(player, "MAX HEALTH", function() Player:setHealth(9999); Player:setMaxHealth(9999) end)
gui.button(player, "REVIVE", function() Player:revive(0) end)
gui.button(player, "ADD STAR", function() Player:addStar(1) end)

local render = gui.window("PRIME - RENDER")
state.windows.render = render
gui.section(render, "VISION")
gui.toggle(render, "Wall Vision", false, function(v) toggleModule("See Through Walls",v) end)
gui.toggle(render, "Ground See", false, function(v) toggleModule("Ground See",v) end)
gui.toggle(render, "Air See", false, function(v) toggleModule("Air See",v) end)
gui.toggle(render, "Radar", true, function(v) state.radar=v end)
gui.radar(render, 110, 0, 3000)

local world = gui.window("PRIME - WORLD")
state.windows.world = world
gui.section(world, "WORLD")
gui.toggle(world, "Fast Time", false, function(v) toggleModule("Fast Time",v) end)
local timeSlider = gui.slider(world, "Time Speed", 1, 30, 10)
storeId(world, "Time Speed", timeSlider)
gui.button(world, "DAY", function() World:setHours(12) end)
gui.button(world, "NIGHT", function() World:setHours(0) end)
gui.button(world, "GIVE 1000 STAR", function() Player:addStar(1000) end)

local hub = gui.window("PRIME - HUB")
state.windows.hub = hub
gui.section(hub, "CLIENT")
gui.label(hub, "ZELVEX PRIME 1.1")
gui.label(hub, "Wurst / Meteor / LiquidBounce inspired")
gui.label(hub, "INSERT = menu")
gui.toggle(hub, "HUD", true, function(v) state.hud=v end)
gui.button(hub, "DISABLE ALL", function()
    for n,m in pairs(state.modules) do toggleModule(n,false) end
end)
gui.button(hub, "CLOSE CLIENT", function()
    state.panic = true
    gui.show(false)
end)

gui.section(hub, "KEYBINDS")
gui.keybind(hub, "Fly bind", 0x46, function() toggleModule("Fly", not state.modules["Fly"].on) end)
gui.keybind(hub, "Noclip bind", 0x58, function() toggleModule("Noclip", not state.modules["Noclip"].on) end)
gui.keybind(hub, "Kill Aura bind", 0x4B, function() toggleModule("Kill Aura", not state.modules["Kill Aura"].on) end)
gui.keybind(hub, "Aimbot bind", 0x45, function() toggleModule("Aimbot", not state.modules["Aimbot"].on) end)

-- =========================
-- TICK / HUD ENGINE
-- =========================

local function panic()
    for n,m in pairs(state.modules) do
        if m.on then toggleModule(n,false) end
    end
    Player:fly(false); Player:noclip(false); Player:sprint(false)
    Player:noDrop(false); Player:slowFall(false)
    Actor:aimbot(false); Actor:killAura(0)
    World:setTimespeed(1)
    state.panic = true
    notify("PANIC")
end

local tickCount = 0

Events.onTick(function()
    if state.panic then return end

    if keyPressed(C.panicVK) then panic() return end

    -- Read sliders via numeric widget IDs
    state.values.speed = gui.get(move, getId(move, "Walk Speed")) or 8
    state.values.runSpeed = gui.get(move, getId(move, "Run Speed")) or 14
    state.values.aimRange = gui.get(combat, getId(combat, "Aim Range")) or 3000
    state.values.timeSpeed = gui.get(world, getId(world, "Time Speed")) or 10

    local scale = gui.get(player, getId(player, "Scale"))
    if scale then Player:setScale(scale) end

    -- Throttle heavy operations to every 3 ticks (~100ms)
    tickCount = tickCount + 1
    if tickCount % 3 == 0 then
        if state.values.god then Player:setHealth(9999) end
        if state.values.heal then
            local hp = Player:getHp() or 0
            local mx = Player:getMaxHp() or 9999
            if hp < mx then Player:setHealth(mx) end
        end
        if state.values.food then
            Player:setHunger(999)
            Player:setStamina(999)
        end
        if state.modules["Speed"].on then
            Player:setWalkSpeed(state.values.speed)
        end
        if state.modules["Aimbot"].on then
            Actor:aimbot(true, state.values.aimRange)
        end
        if state.modules["Fast Time"].on then
            World:setTimespeed(state.values.timeSpeed)
        end
    end
end)

notify("Loaded. INSERT toggles the overlay.")

while not state.panic do
    wait(0.25)
end

panic()
gui.reset()
