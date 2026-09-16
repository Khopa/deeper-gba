-- @fresh
-- @timeout 900
-- The longest descent: with every length unlocked in the profile, a 60-layer
-- run on a fixed seed is played to the core (save block, map window, biomes
-- and difficulty curve all at full length).
T.run(function()
  T.boot()
  emu:write8(CFG.sym.profile + CFG.off["profile.lengths_unlocked"], 3)
  emu:write32(CFG.sym.debug_seed, 777)
  T.new_run(2)
  local r = T.run_state()
  T.check_eq(r.layers, 60, "sixty layers")
  T.check_eq(r.length_index, 2, "length index 2")
  local rooms, guard, hardest = 0, 0, 0
  local shopped = false
  while T.screen() ~= T.SCREEN.RUN_END and guard < 160 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      local node = T.current_node()
      if node.difficulty > hardest then hardest = node.difficulty end
      if node.family == T.FAM.HEART then T.shot("core15") end
      T.solve_room()
      rooms = rooms + 1
      if T.screen() == T.SCREEN.ROOM then
        T.check(false, "room at layer " .. T.run_state().layer .. " (family " .. node.family .. ") was not cleared")
        break
      end
    elseif T.screen() == T.SCREEN.FIGHT then T.fight()
    elseif T.screen() == T.SCREEN.MASH then T.mash()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.SHOP then
      -- first camp: buy a hint if affordable, check the effect; later camps: just leave
      local before = T.run_state()
      if not shopped and before.ore >= 30 then
        T.leave_shop(0)
        local after = T.run_state()
        T.check_eq(after.hints, before.hints + 1, "a hint bought at the camp")
        T.check_eq(after.ore, before.ore - 30, "paid 30 ore for it")
        shopped = true
      else
        T.leave_shop(nil)
      end
      T.check_eq(T.screen(), T.SCREEN.MAP, "leaving the counter shows the map")
    elseif T.screen() == T.SCREEN.MAP then
      T.map_go(guard % 3 == 0 and "left" or "right")
    end
  end
  r = T.run_state()
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "the run ended")
  T.check_eq(r.layer, 59, "at the core")
  T.check(r.lives >= 1, "no life lost (" .. r.lives .. ")")
  T.check(rooms >= 48, rooms .. " rooms solved (camps and crates are not rooms)")
  T.check_eq(hardest, 10, "the core is difficulty 10")
  local p = T.profile()
  T.check_eq(p.best_frames[3], r.frames, "best time recorded for 60 layers")
  T.check_eq(p.lengths_unlocked, 3, "nothing further to unlock")
  T.log("ore " .. r.ore .. " frames " .. r.frames)
  T.shot("victory")
end)
