-- @fresh
-- Marks placed in a room survive a reboot: Continue reopens the very room.
T.run(function()
  T.boot()
  T.new_run()
  local n, sol = T.dig_solution(T.current_node().puzzle)
  T.cursor_reset()
  T.goto_cell(0, sol[0], n)
  T.press(T.K.A)                              -- one correct dig
  T.goto_cell(1, (sol[0] + 3) % n, n)
  T.press(T.K.B)                              -- one cross
  T.wait(10)
  T.shot("before_reboot")
  local seed = T.run_state().seed
  emu:reset()
  T.boot()
  T.check_eq(T.menu_cursor(), T.MENU.CONTINUE, "Continue is offered")
  T.menu_go(T.MENU.CONTINUE)
  T.check_eq(T.screen(), T.SCREEN.ROOM, "resume lands in the room")
  T.check_eq(T.run_state().seed, seed, "same run")
  T.check_eq(T.run_state().layer, 0, "same layer")
  T.shot("after_reboot")
  -- finish it: the cursor came back where it was saved; A over the cross digs there
  T.cur.r, T.cur.c = 1, (sol[0] + 3) % n
  for r = 1, n - 1 do
    T.goto_cell(r, sol[r], n)
    T.press(T.K.A)
  end
  T.finish_room()
  T.check_eq(T.screen(), T.SCREEN.MAP, "resumed room can be finished")
end)
