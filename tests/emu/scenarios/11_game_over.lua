-- @fresh
-- A run lost on purpose: the entrance caves in (four charged conflicts), the
-- next rooms are given up, the defeat screen and the profile add up (depth,
-- ore banked, no victory, nothing to continue), and a fresh run starts clean.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 555)
  T.give_lives(3, 3)
  T.new_run(0)
  local before = T.run_state()
  T.check_eq(before.lives, 3, "three lives to start (from the gear)")
  T.check_eq(before.hints, 3, "three hints to start")
  T.dig_mistakes(4)
  T.wait(T.COLLAPSE_FRAMES + 2)
  T.press(T.K.A); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.MAP, "cave-in: back on the map")
  local r = T.run_state()
  T.check_eq(r.lives, 2, "one life lost")
  T.check_eq(r.ore, 0, "no ore from a caved room")
  T.check_eq(r.layer, 0, "still at the entrance layer")
  -- a failed room still lets the descent go on
  T.map_go("left")
  T.check_eq(T.run_state().layer, 1, "the map moved on past the failed room")
  local guard, gave_up = 0, 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 12 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then T.abandon(); gave_up = gave_up + 1
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.MAP then T.map_go("right") end
  end
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "run over")
  T.check_eq(gave_up, 2, "two rooms given up ended it")
  r = T.run_state()
  T.check_eq(r.lives, 0, "no lives left")
  T.shot("defeat")
  local p = T.profile()
  T.check_eq(p.runs_won, 0, "no victory")
  T.check_eq(p.ore_bank, r.ore, "the ore of a lost run is banked all the same")
  T.check_eq(p.lengths_unlocked, 1, "nothing unlocked by a defeat")
  T.press(T.K.START); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "title")
  T.check_eq(T.menu_cursor(), T.MENU.NEW, "nothing to continue")
  -- a new run starts from scratch
  T.new_run(0)
  r = T.run_state()
  T.check_eq(r.lives, 3, "fresh lives")
  T.check_eq(r.ore, 0, "fresh ore")
  T.check_eq(r.layer, 0, "fresh depth")
end)
