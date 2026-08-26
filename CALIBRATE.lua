-- ================================================================
--  CALIBRATE.lua - W2S probe for Zelvex
--
--  PURPOSE: figure out what the engine's getPointToScreen numbers
--  actually mean so we can draw real 2D boxes/nameplates.
--
--  NEEDS: another player in the same world (second account,
--  friend, or public room). You cannot calibrate alone.
--
--  RUN IT, then follow the on-screen instructions. Keep the target
--  roughly at the described screen spot during each 3s stage.
--  Paste the final SUMMARY block back to me.
-- ================================================================

local W, H = Esp.screen()
print(("screen: %d x %d"):format(W or 0, H or 0))
if not W or W == 0 then
    print("ERR: display size unknown - open gui once (gui.start()+gui.show(true)) then rerun")
    return
end

-- pick a target that is not us
local myUid
if Player.getUid then myUid = Player:getUid() end
local target
for _, p in ipairs(Players.list()) do
    if not myUid or p.uid ~= myUid then target = p break end
end
if not target then
    print("ERR: no other player found. Need a second player in the world.")
    return
end
print(("target uid=%d pos=%.0f,%.0f,%.0f"):format(target.uid, target.x, target.y, target.z))

-- sample one w2s reading; returns sx,sy,sz or nil+err
local function probe()
    local a, b, c, err = Esp.w2s(target.uid)
    if not a then print("w2s ERR: " .. tostring(err)); return nil end
    return tonumber(a), tonumber(b), tonumber(c)
end

local function median(t)
    if #t == 0 then return nil end
    table.sort(t, function(x, y) return x < y end)
    return t[math.ceil(#t / 2)]
end

-- stages: hold the target at this spot while we sample
local stages = {
    { key="CENTER",     hint="put target DEAD CENTER (crosshair)" },
    { key="LEFT",       hint="turn so target sits at far LEFT edge" },
    { key="RIGHT",      hint="turn so target sits at far RIGHT edge" },
    { key="TOP",        hint="pitch up: target at TOP edge" },
    { key="BOTTOM",     hint="pitch down: target at BOTTOM edge" },
    { key="BEHIND",     hint="turn AROUND completely (target behind you)" },
}

local results = {}

print("\n=== CALIBRATION START - follow instructions ===")
for _, st in ipairs(stages) do
    print((">> %s   (%s)"):format(st.hint, st.key))
    wait(1.2)                       -- give you time to aim
    local sx, sy, sz = {}, {}, {}
    for i = 1, 12 do                -- ~3s of samples
        local a, b, c = probe()
        if a then sx[#sx+1]=a; sy[#sy+1]=b; sz[#sz+1]=c end
        wait(0.25)
    end
    results[st.key] = {
        x = median(sx), y = median(sy), z = median(sz), n = #sx,
    }
    local r = results[st.key]
    if r.x then
        print(("   %-7s n=%d  x=%g  y=%g  z=%g")
            :format(st.key, r.n, r.x, r.y, r.z or 0))
    else
        print("   " .. st.key .. ": no samples (error above)")
    end
end

-- distance + both positions for depth sanity
local mx, my, mz = Player:getPos()
local d = math.sqrt((mx-target.x)^2 + (my-target.y)^2 + (mz-target.z)^2)

print("\n=== SUMMARY - PASTE THIS BLOCK ===")
print(("screen=%dx%d  dist=%.0f  me=(%.0f,%.0f,%.0f) tgt=(%.0f,%.0f,%.0f)")
    :format(W, H, d, mx, my, mz, target.x, target.y, target.z))
for _, st in ipairs(stages) do
    local r = results[st.key]
    if r and r.x then
        print(("%-7s x=%g  y=%g  z=%g"):format(st.key, r.x, r.y, r.z or 0))
    else
        print(("%-7s NO DATA"):format(st.key))
    end
end
print("=== END SUMMARY ===")

print([[
What each line tells me:
 CENTER -> origin offset      LEFT/RIGHT -> x scale+direction
 TOP/BOTTOM -> y flip+scale   BEHIND -> depth sign/mirror]])
