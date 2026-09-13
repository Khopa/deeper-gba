-- @fresh
-- @timeout 240
-- Random input for a while: the main loop must keep running (an occasional
-- lag frame on a heavy redraw is fine, a freeze is not).
T.run(function()
  T.boot()
  local keys = { T.K.A, T.K.B, T.K.L, T.K.R, T.K.UP, T.K.DOWN, T.K.LEFT, T.K.RIGHT, T.K.START, T.K.SELECT }
  local rng = 12345
  local function rand(n) rng = (rng * 1103515245 + 12345) % 2147483648; return rng % n end
  local f_start, waited, worst = T.frames(), 0, 0
  local last = f_start
  for i = 1, 3000 do
    local k = keys[1 + rand(#keys)]
    local w1, w2 = 1 + rand(3), 1 + rand(2)
    emu:addKey(k); T.wait(w1); emu:clearKey(k); T.wait(w2)
    waited = waited + w1 + w2
    local now = T.frames()
    if now == last then worst = worst + 1 end
    last = now
  end
  local advanced = T.frames() - f_start
  T.check(advanced >= waited * 9 / 10, string.format("frame counter kept running (%d of %d frames)", advanced, waited))
  T.check(worst < 100, "no long freeze (" .. worst .. " quiet samples)")
  T.shot("end")
end)
