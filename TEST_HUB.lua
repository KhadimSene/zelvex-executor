-- ============================================================
--  ZELVEX TEST HUB - tame edition (self-contained, no web)
--  INSERT = show/hide menu   |   F2 = noclip   |   F3 = godmode
--  Stop the script to unload everything.
-- ============================================================

assert(gui.start(), "overlay failed to boot")

gui.reset()                                   -- clean slate on re-run
local w = gui.window("ZELVEX  -  test hub")

-- ── state ──────────────────────────────────────────────────
local godmode   = false
local noclip    = false
local speedId, flyId, fallId, dropId

-- ── MOVEMENT ───────────────────────────────────────────────
gui.label(w, "── MOVEMENT ─────────────")
speedId = gui.slider(w, "Walk speed", 1, 30, 4)
flyId   = gui.toggle(w, "Fly",         false, function(on) Player:fly(on)      end)
fallId  = gui.toggle(w, "Slow fall",   false, function(on) Player:slowFall(on) end)

-- ── SURVIVAL ───────────────────────────────────────────────
gui.label(w, "── SURVIVAL ──────────────")
dropId = gui.toggle(w, "Keep items on death", false,
    function(on) Player:noDrop(on) end)
gui.button(w, "Full HP",            function() Player:setHealth(9999) end)
gui.button(w, "Refill food + stamina", function()
    Player:setHunger(100)  Player:setStamina(100)
end)
gui.button(w, "Revive",             function() Player:revive(2) end)

-- ── INVENTORY ──────────────────────────────────────────────
gui.label(w, "── INVENTORY ─────────────")
gui.button(w, "Sort pack",   function() Item:sort()       end)
gui.button(w, "Repair all",  function() Item:repairAll()  end)

-- ── INFO ───────────────────────────────────────────────────
gui.label(w, "───────────────────────────")
gui.label(w, "INSERT hide/show | F2 noclip | F3 godmode")

-- ── hotkey edge detection ──────────────────────────────────
local prevF2, prevF3 = false, false
local function edge(vk, prev)
    local now = input.key(vk)
    local hit = now and not prev
    return hit, now
end

-- ── control loop ───────────────────────────────────────────
while true do
    -- apply walk-speed slider live
    local spd = gui.get(w, speedId)
    if spd then Player:setWalkSpeed(spd) end

    -- F2 = noclip flip
    local hit2, now2 = edge(0x71, prevF2)          -- VK_F2
    if hit2 then
        noclip = not noclip
        Player:noclip(noclip)
        print("[hub] noclip " .. (noclip and "ON" or "OFF"))
    end
    prevF2 = now2

    -- F3 = godmode flip
    local hit3, now3 = edge(0x72, prevF3)          -- VK_F3
    if hit3 then
        godmode = not godmode
        print("[hub] godmode " .. (godmode and "ON" or "OFF"))
    end
    prevF3 = now3

    if godmode then Player:setHealth(9999) end

    wait(0.1)
end
