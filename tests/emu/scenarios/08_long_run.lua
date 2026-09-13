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
  while T.screen() ~= T.SCREEN.RUN_END and guard < 160 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      local node = T.current_node()
      if node.difficulty > hardest then hardest = node.difficulty end
      T.solve_room()
      rooms = rooms + 1
      if T.screen() == T.SCREEN.ROOM then
        T.check(false, "room at layer " .. T.run_state().layer .. " (family " .. node.family .. ") was not cleared")
        break
      end
    elseif T.screen() == T.SCREEN.MAP then
      T.map_go(guard % 3 == 0 and "left" or "right")
    end
  end
  r = T.run_state()
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "the run ended")
  T.check_eq(r.layer, 59, "at the core")
  T.check(r.lives >= 3, "no life lost (" .. r.lives .. ")")
  T.check(rooms >= 55, rooms .. " rooms solved")
  T.check_eq(hardest, 10, "the core is difficulty 10")
  local p = T.profile()
  T.check_eq(p.best_frames[3], r.frames, "best time recorded for 60 layers")
  T.check_eq(p.lengths_unlocked, 3, "nothing further to unlock")
  T.log("ore " .. r.ore .. " frames " .. r.frames)
  T.shot("victory")
end)
