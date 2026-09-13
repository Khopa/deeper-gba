-- @fresh
-- Four visible mistakes cave the entrance in (stability 4); losing every life
-- ends the run on the defeat screen; the profile then offers a fresh start.
T.run(function()
  T.boot()
  T.new_run()
  local n, sol = T.dig_solution(T.current_node().puzzle)
  T.cursor_reset()
  -- two adjacent digs conflict at once, then toggling the second one on/off repeats the mistake
  T.goto_cell(0, 0, n); T.press(T.K.A)
  T.goto_cell(0, 1, n); T.press(T.K.A)      -- mistake 1
  T.press(T.K.A); T.press(T.K.A)            -- off, on: mistake 2
  T.press(T.K.A); T.press(T.K.A)            -- mistake 3
  T.press(T.K.A); T.press(T.K.A)            -- mistake 4 -> cave-in
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
    if T.screen() == T.SCREEN.ROOM then T.abandon() end
  end
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "no lives left: run over")
  T.shot("defeat")
  T.press(T.K.START); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "back to the title")
  T.check_eq(T.menu_cursor(), T.MENU.NEW, "the finished run cannot be continued")
end)
