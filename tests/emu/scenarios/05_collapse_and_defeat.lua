-- @fresh
-- Conflicts only cost stability once they have stood for a couple of seconds;
-- four charged conflicts cave the entrance in (stability 4); losing every
-- life ends the run on the defeat screen; the profile then offers a fresh start.
T.run(function()
  T.boot()
  T.give_lives(3, 3)                            -- gear: three lives of three
  emu:write32(CFG.sym.debug_seed, 1212)
  T.new_run()
  T.check_eq(T.run_state().lives, 3, "the gear gives three lives")
  local n, sol = T.dig_solution(T.current_node().puzzle)
  T.cursor_reset()
  -- a conflict fixed quickly is free
  T.goto_cell(0, 0, n); T.press(T.K.A)
  T.goto_cell(0, 1, n); T.press(T.K.A)          -- adjacent: both red
  T.wait(60)
  T.press(T.K.A)                                -- removed before the delay runs out
  T.wait(130)
  T.check_eq(T.screen(), T.SCREEN.ROOM, "still playing")
  -- now leave the pair standing: each cell charges once (2 mistakes)
  T.press(T.K.A)
  T.wait(118)
  T.shot("burst")                               -- the explosion effect on the charged cells
  T.wait(12)
  T.shot("charged")
  T.check_eq(T.screen(), T.SCREEN.ROOM, "two charges do not cave the room in yet")
  -- toggle one off and on: both conflicts are fresh again and charge again (4)
  T.press(T.K.A); T.press(T.K.A)
  T.wait(130)
  T.wait(T.COLLAPSE_FRAMES + 2)
  T.shot("collapse")
  T.press(T.K.A); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.MAP, "after the cave-in the map shows up")
  T.check_eq(T.run_state().lives, 2, "a life was lost")
  -- burn the remaining lives by abandoning rooms
  local guard = 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 12 do
    guard = guard + 1
    if T.screen() == T.SCREEN.MAP then T.map_go("left") end
    if T.screen() == T.SCREEN.CRATES then T.open_crates() end
    if T.screen() == T.SCREEN.FIGHT then T.fight() end
    if T.screen() == T.SCREEN.MASH then T.mash() end
    if T.screen() == T.SCREEN.SHOP then T.leave_shop(nil) end
    if T.screen() == T.SCREEN.ROOM then T.abandon() end
  end
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "no lives left: run over")
  T.shot("defeat")
  T.press(T.K.START); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "back to the title")
  T.check_eq(T.menu_cursor(), T.MENU.NEW, "the finished run cannot be continued")
end)
