-- @fresh
-- @timeout 600
-- Failing the heart costs a life and offers it again (another picture when
-- the bank has one) instead of ending the run; solving it with the last life
-- is still the victory.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 20260913)
  T.give_lives(3, 3)
  T.new_run(0)
  local guard = 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 80 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      local node = T.current_node()
      if node.family == T.FAM.HEART then break end
      T.solve_room()
    elseif T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.MAP then T.map_go(guard % 2 == 0 and "left" or "right") end
  end
  T.check_eq(T.screen(), T.SCREEN.ROOM, "at the heart")
  local r0 = T.run_state()
  T.check_eq(r0.layer, r0.layers - 1, "core layer")
  local first = T.current_node().puzzle
  T.heart_mistakes(4)
  T.wait(T.COLLAPSE_FRAMES + 2)
  T.shot("core_collapse")
  T.press(T.K.A); T.wait(8)
  T.check_eq(T.screen(), T.SCREEN.ROOM, "the heart is offered again")
  local r1 = T.run_state()
  T.check_eq(r1.lives, r0.lives - 1, "a life lost")
  T.check_eq(r1.layer, r1.layers - 1, "still at the core")
  T.check(T.current_node().puzzle ~= first, "another picture of the same size")
  T.check_eq(T.node(r1.layer, r1.slot).kind, T.KIND.CORE, "the node stays the core")
  T.shot("core_again")
  -- burn the lives down to one, then solve it
  while T.run_state().lives > 1 do
    T.heart_mistakes(4)
    T.wait(T.COLLAPSE_FRAMES + 2)
    T.press(T.K.A); T.wait(8)
    T.check_eq(T.screen(), T.SCREEN.ROOM, "offered again while lives last")
  end
  T.solve_room()
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "solved with the last life: the run is won")
  T.check_eq(T.profile().runs_won, 1, "counted as a victory")
  T.check_eq(T.profile().lengths_unlocked, 2, "the next length unlocked")
end)
