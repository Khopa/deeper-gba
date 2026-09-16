-- @fresh
-- @timeout 300
-- The same entrance solved at once, then again after forty seconds: the speed
-- bonus shrinks with the clock, which counts play time only (not the pause).
-- Hints are spent even when the room is then given up; with none left, L
-- does nothing but complain.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 2024)
  T.give_lives(2, 2)
  T.new_run(0)
  local d = T.current_node().difficulty
  local base = 10 + 5 * d
  T.solve_room()
  local quick = T.run_state()
  T.check_eq(quick.hints, 3, "no hint used")
  T.check(quick.ore > base, "solved at once: the speed bonus pays on top of " .. base .. " (got " .. quick.ore .. ")")
  T.check(quick.ore <= 2 * base, "and never more than double")
  T.check(quick.frames < 60 * 20, "the run clock counts play time (" .. quick.frames .. " frames)")
  -- save & quit from the next room, then start the same seed afresh
  T.map_go("left")
  T.press(T.K.START); T.wait(2); T.press(T.K.DOWN); T.press(T.K.DOWN); T.press(T.K.A); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "saved and quit to the title")
  T.check_eq(T.menu_cursor(), T.MENU.CONTINUE, "the run can be continued")
  emu:write32(CFG.sym.debug_seed, 2024)
  T.new_run(0)
  T.check_eq(T.run_state().seed, 2024, "same seed again")
  T.check_eq(T.run_state().layer, 0, "a new run replaces the saved one")
  -- forty seconds: twenty of them in the pause menu, which must not count
  T.wait(20 * 60)
  T.press(T.K.START); T.wait(20 * 60); T.press(T.K.B); T.wait(2)
  local paused = T.run_state()
  T.check(paused.frames < 25 * 60, "the pause stopped the run clock (" .. paused.frames .. " frames)")
  T.wait(20 * 60)
  T.solve_room()
  local slow = T.run_state()
  T.check(slow.ore < quick.ore, "solved later: less ore (" .. slow.ore .. " < " .. quick.ore .. ")")
  T.check(slow.ore >= base, "but never below the base reward")
  T.check(slow.frames >= 40 * 60, "the clock counted the wait (" .. slow.frames .. " frames)")
  -- hints: spent even when the room is given up afterwards; none left = a complaint
  local guard = 0
  while T.screen() ~= T.SCREEN.ROOM and guard < 6 do
    guard = guard + 1
    if T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.FIGHT then T.fight()
    elseif T.screen() == T.SCREEN.MASH then T.mash()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.MAP then T.map_go(T.next_of(T.KIND.PUZZLE, T.FAM.DIG) or T.next_of(T.KIND.PUZZLE) or "left") end
  end
  T.check_eq(T.screen(), T.SCREEN.ROOM, "next room")
  T.press(T.K.L); T.wait(2)
  T.press(T.K.L); T.wait(2)
  T.press(T.K.L); T.wait(2)
  T.shot("hinted")
  T.press(T.K.L); T.wait(2)            -- a fourth: nothing left
  T.shot("no_hints")
  T.abandon()
  local after = T.run_state()
  T.check_eq(after.hints, 0, "three hints spent")
  T.check_eq(after.lives, 1, "giving up cost a life")
end)
