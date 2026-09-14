-- @fresh
-- A new run opens a DIG entrance room; placing the stored solution clears it
-- and pays ore, then the map shows up with choices.
T.run(function()
  T.boot()
  T.new_run()
  local r = T.run_state()
  T.check_eq(r.layer, 0, "entrance is layer 0")
  T.check_eq(r.lives, 1, "one life to start (the counter sells more)")
  T.check_eq(r.hints, 3, "three hints")
  T.check_eq(r.ore, 0, "no ore yet")
  T.check_eq(r.room_in_progress, 1, "room in progress flagged for the save")
  T.shot("entrance")
  T.check_eq(r.layers, 15, "the first descent is 15 layers long")
  local base = 10 + 5 * T.current_node().difficulty
  T.solve_dig()
  T.check_eq(T.screen(), T.SCREEN.MAP, "cleared room leads to the map")
  r = T.run_state()
  T.check(r.ore > base, "ore paid " .. r.ore .. " includes a speed bonus over the base " .. base)
  T.check(r.frames > 0, "the run clock runs (" .. r.frames .. " frames)")
  -- the map music streams: DirectSound A enabled, DMA1 armed on the FIFO
  T.check((emu:read16(0x04000082) & 0x0304) == 0x0304, "DirectSound A on, both sides, full volume")
  T.check((emu:read16(0x040000C6) & 0x8000) ~= 0, "DMA1 is feeding FIFO A")
  T.check_eq(emu:read32(0x040000C0) >= 0x08000000, true, "the DMA source is a ROM track")
  T.check_eq(r.lives, 1, "no life lost")
  T.check_eq(r.room_in_progress, 0, "no room in progress on the map")
  T.shot("map")
end)
