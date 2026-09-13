-- @fresh
-- @timeout 600
-- A complete descent on a fixed seed: every room solved from the ROM banks,
-- no life lost, the core reached. Also the source of docs/fullrun.gif.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 20260913)
  T.new_run()
  T.check_eq(T.run_state().seed, 20260913, "the debug seed drives the run")
  local rooms, families = 0, {}
  local guard = 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 80 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      local node = T.current_node()
      families[node.family] = (families[node.family] or 0) + 1
      T.solve_room()
      rooms = rooms + 1
      if T.screen() == T.SCREEN.ROOM then
        T.check(false, "room at layer " .. T.run_state().layer .. " (family " .. node.family .. ") was not cleared")
        break
      end
    elseif T.screen() == T.SCREEN.MAP then
      T.map_go(guard % 2 == 0 and "left" or "right")
    end
  end
  local r = T.run_state()
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "the run ended")
  T.check_eq(r.layer, T.LAYERS - 1, "at the core")
  T.check(r.lives >= 3, "no life lost (" .. r.lives .. ")")
  T.check(rooms >= 25, rooms .. " rooms solved")
  local kinds = 0
  for _ in pairs(families) do kinds = kinds + 1 end
  T.check(kinds >= 4, kinds .. " different families met")
  T.log("ore " .. r.ore .. " rooms " .. rooms)
  T.shot("victory")
  T.press(T.K.START); T.wait(6)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "back to the title")
end)
