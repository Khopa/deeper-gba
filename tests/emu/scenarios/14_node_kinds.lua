-- @fresh
-- @timeout 600
-- Steering by node kind: hint rooms add a token, life rooms and camps a life
-- (capped at five), risky rooms play with two stability and pay double, and a
-- room that is not a puzzle still needs solving. Seed chosen so the map
-- offers every kind on the way; a life is spent on purpose to see the cap.
T.run(function()
  T.boot()
  emu:write32(CFG.sym.debug_seed, 8888)
  T.give_lives(3, 1)                          -- room to heal: three max, one to start
  T.new_run(0)
  local seen = {}
  local guard = 0
  local on_map = T.run_state()             -- the state when the last step was chosen
  while T.screen() ~= T.SCREEN.RUN_END and guard < 80 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then
      local node = T.current_node()
      local before = T.run_state()
      local base = 10 + 5 * node.difficulty
      T.solve_room()
      if T.screen() == T.SCREEN.ROOM then
        T.check(false, "room at layer " .. before.layer .. " (family " .. node.family .. ", kind " .. node.kind .. ") was not cleared")
        break
      end
      local after = T.run_state()
      seen[node.kind] = (seen[node.kind] or 0) + 1
      local gained = after.ore - before.ore
      if node.kind == T.KIND.HINT then
        T.check_eq(after.hints, math.min(before.hints + 1, 9), "hint room: +1 token")
      elseif node.kind == T.KIND.LIFE then
        T.check_eq(after.lives, math.min(before.lives + 1, before.max_lives), "life room: +1 life, capped at the maximum")
      elseif node.kind == T.KIND.RISKY then
        T.check(gained >= 2 * base and gained <= 4 * base, "risky room pays double (" .. gained .. " for base " .. base .. ")")
      elseif node.kind == T.KIND.PUZZLE and node.family ~= T.FAM.NUGGET then
        T.check(gained >= base and gained <= 2 * base, "puzzle room pays its base plus a speed bonus (" .. gained .. " for base " .. base .. ")")
      end
    elseif T.screen() == T.SCREEN.FIGHT then T.fight()
    elseif T.screen() == T.SCREEN.MASH then T.mash()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.SHOP then
      -- the rest is taken on arrival, before the counter opens
      T.check_eq(T.run_state().lives, math.min(on_map.lives + 1, on_map.max_lives), "camp: +1 life, capped at the maximum")
      T.leave_shop(nil)
      seen[T.KIND.CAMP] = (seen[T.KIND.CAMP] or 0) + 1
    elseif T.screen() == T.SCREEN.MAP then
      on_map = T.run_state()
      -- prefer the kinds not yet seen, then risky rooms, then whatever is leftmost
      local pick = nil
      for _, k in ipairs({ T.KIND.HINT, T.KIND.LIFE, T.KIND.RISKY }) do
        if not seen[k] then pick = pick or T.next_of(k) end
      end
      T.map_go(pick or T.next_of(T.KIND.RISKY) or "left")
    end
  end
  T.check_eq(T.screen(), T.SCREEN.RUN_END, "the run ended")
  local r = T.run_state()
  T.check_eq(r.layer, r.layers - 1, "at the core")
  T.check(seen[T.KIND.HINT], "a hint room was met")
  T.check(seen[T.KIND.LIFE], "a life room was met")
  T.check(seen[T.KIND.RISKY], "a risky room was met")
  T.check_eq(seen[T.KIND.CAMP], 2, "two camps")
  T.check(r.lives <= 5, "lives never above five (" .. r.lives .. ")")
  T.log("lives " .. r.lives .. " hints " .. r.hints .. " ore " .. r.ore)
end)
