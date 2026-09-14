-- @fresh
-- @timeout 600
-- Save & quit in the middle of the heart, reboot, continue: the marks, the
-- rock notes, the cursor and the run clock come back, and the picture can be
-- finished from there.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 20260913)
  T.new_run(0)
  local guard = 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 80 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      if T.current_node().family == T.FAM.HEART then break end
      T.solve_room()
    elseif T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.MAP then T.map_go(guard % 2 == 0 and "left" or "right") end
  end
  T.check_eq(T.screen(), T.SCREEN.ROOM, "at the heart")
  local node = T.current_node()
  local n, payload = (function()
    local off = u32(CFG.sym.bank_heart + 32 + 4 * node.puzzle)
    return u8(CFG.sym.bank_heart + off + 1), CFG.sym.bank_heart + off + 4
  end)()
  local bytes = (n * n + 7) // 8
  -- the first two free ore cells marked, one rock cell noted
  local marked, noted = {}, nil
  T.cursor_reset()
  for i = 0, n * n - 1 do
    local ore = (u8(payload + (i >> 3)) >> (i % 8)) & 1
    local given = (u8(payload + bytes + (i >> 3)) >> (i % 8)) & 1
    if given == 0 and ore == 1 and #marked < 2 then
      T.goto_cell(i // n, i % n, n); T.press(T.K.A); marked[#marked + 1] = i
    elseif given == 0 and ore == 0 and not noted then
      T.goto_cell(i // n, i % n, n); T.press(T.K.B); noted = i
    end
    if #marked == 2 and noted then break end
  end
  T.goto_cell(3, 4, n)
  T.wait(10)
  T.shot("before")
  local before = T.run_state()
  T.press(T.K.START); T.wait(2); T.press(T.K.DOWN); T.press(T.K.DOWN); T.press(T.K.A); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "saved and quit")
  emu:reset()
  T.boot()
  T.check_eq(T.menu_cursor(), T.MENU.CONTINUE, "Continue offered")
  T.menu_go(T.MENU.CONTINUE)
  T.check_eq(T.screen(), T.SCREEN.ROOM, "back in the room")
  local after = T.run_state()
  T.check_eq(after.layer, before.layer, "same layer (the core)")
  T.check_eq(after.seed, before.seed, "same run")
  T.check(after.frames >= before.frames, "the run clock kept its count")
  T.check_eq(u32(CFG.sym.room_cur_r), 3, "cursor row restored")
  T.check_eq(u32(CFG.sym.room_cur_c), 4, "cursor column restored")
  T.check_eq(T.current_node().puzzle, node.puzzle, "same picture")
  T.shot("after")
  -- finish: every free ore cell except the two already marked (A would clear them)
  T.cur.r, T.cur.c = 3, 4
  for i = 0, n * n - 1 do
    local ore = (u8(payload + (i >> 3)) >> (i % 8)) & 1
    local given = (u8(payload + bytes + (i >> 3)) >> (i % 8)) & 1
    if ore == 1 and given == 0 and i ~= marked[1] and i ~= marked[2] then
      T.goto_cell(i // n, i % n, n); T.press(T.K.A)
    end
  end
  T.finish_room()
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "the restored marks counted: the picture is complete")
  T.check_eq(T.profile().runs_won, 1, "victory")
end)
