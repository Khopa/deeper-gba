-- @fresh
-- @timeout 900
-- Three more 15-layer descents on fixed seeds, each with a different way of
-- picking the next room (leftmost, rightmost, cycling), every room solved
-- from the banks: broad coverage of puzzles, node kinds and camps.
T.run(function()
  T.boot()
  local seeds = { 4242, 31337, 90210 }
  for si, seed in ipairs(seeds) do
    emu:write32(CFG.sym.debug_seed, seed)
    T.new_run(0)
    T.check_eq(T.run_state().seed, seed, "seed " .. seed .. " drives the run")
    local rooms, kinds, guard = 0, {}, 0
    while T.screen() ~= T.SCREEN.RUN_END and guard < 80 do
      guard = guard + 1
      if T.screen() == T.SCREEN.ROOM then
        local node = T.current_node()
        kinds[node.kind] = (kinds[node.kind] or 0) + 1
        T.solve_room()
        rooms = rooms + 1
        if T.screen() == T.SCREEN.ROOM then
          T.check(false, "seed " .. seed .. ": room at layer " .. T.run_state().layer .. " (family " .. node.family .. ") was not cleared")
          break
        end
      elseif T.screen() == T.SCREEN.FIGHT then T.fight()
      elseif T.screen() == T.SCREEN.MASH then T.mash()
      elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
      elseif T.screen() == T.SCREEN.SHOP then
        kinds[T.KIND.CAMP] = (kinds[T.KIND.CAMP] or 0) + 1
        T.leave_shop(nil)
      elseif T.screen() == T.SCREEN.MAP then
        local slots = T.next_slots()
        local pick
        if si == 1 then pick = "left" elseif si == 2 then pick = "right" else pick = slots[(guard % #slots) + 1] end
        T.map_go(pick)
      end
    end
    local r = T.run_state()
    T.check_eq(T.screen(), T.SCREEN.RUN_END, "seed " .. seed .. ": the run ended")
    T.check_eq(r.layer, r.layers - 1, "seed " .. seed .. ": at the core")
    T.check(r.lives >= 1, "seed " .. seed .. ": alive (" .. r.lives .. " lives)")
    T.check_eq(kinds[T.KIND.CORE], 1, "seed " .. seed .. ": one core")
    T.check_eq(kinds[T.KIND.CAMP], 2, "seed " .. seed .. ": two camps on the way")
    local desc = {}
    for k, v in pairs(kinds) do desc[#desc + 1] = k .. ":" .. v end
    table.sort(desc)
    T.log("seed " .. seed .. " rooms " .. rooms .. " ore " .. r.ore .. " time " .. r.frames .. " kinds " .. table.concat(desc, " "))
    T.check_eq(T.profile().runs_won, si, "victories counted")
    T.press(T.K.START); T.wait(6)
    T.check_eq(T.screen(), T.SCREEN.TITLE, "back to the title")
  end
  T.check(T.profile().ore_bank > 1000, "three victories banked ore (" .. T.profile().ore_bank .. ")")
end)
