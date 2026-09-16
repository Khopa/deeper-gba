-- @fresh
-- @timeout 600
-- The wall: on a 30-layer descent the scenario seeks wall nodes, hammers A
-- through the first ones (flash, chips, the shatter) and checks the reward,
-- then lets the clock run out on one: no ore, but no life lost either.
T.run(function()
  T.boot()
  T.give_lives(2, 2)
  emu:write8(CFG.sym.profile + CFG.off["profile.lengths_unlocked"], 3)
  emu:write32(CFG.sym.debug_seed, 2024)
  T.new_run(1)
  local broken, given_up, guard = 0, false, 0
  while T.screen() ~= T.SCREEN.RUN_END and guard < 120 do
    guard = guard + 1
    if T.screen() == T.SCREEN.MASH then
      local before = T.run_state()
      local node = T.current_node()
      T.wait(4)
      if broken == 0 then T.shot("wall") end
      if broken >= 1 and not given_up then
        T.mash(true, "wall_holds")
        T.check_eq(T.run_state().ore, before.ore, "the wall held: no ore")
        T.check_eq(T.run_state().lives, before.lives, "...and no life lost")
        given_up = true
      else
        if broken == 0 then
          -- a few blows, then a picture of the chips flying
          for _ = 1, 6 do emu:addKey(T.K.A); T.wait(2); emu:clearKey(T.K.A); T.wait(2) end
          T.wait(1)
          T.shot("wall_blows")
        end
        T.mash(false, broken == 0 and "wall" or nil)
        local after = T.run_state()
        T.check_eq(after.ore, before.ore + 10 + node.difficulty * 5, "a broken wall pays the base ore")
        broken = broken + 1
      end
    elseif T.screen() == T.SCREEN.FIGHT then T.fight()
    elseif T.screen() == T.SCREEN.ROOM then T.solve_room()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.MAP then T.map_go(T.next_of(T.KIND.WALL) or "left") end
  end
  T.check(broken >= 2, broken .. " walls broken")
  T.check(given_up, "a wall was left standing on purpose")
end)
