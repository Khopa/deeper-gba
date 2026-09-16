-- @fresh
-- @timeout 600
-- Monster encounters: on a 60-layer descent the scenario seeks fight nodes,
-- beats each monster with its sequences (the goblin, the orc, then the troll
-- and the fire demon deeper down), checks the reward, and that a lost fight
-- costs a life.
T.run(function()
  T.boot()
  T.give_lives(3, 3)
  emu:write8(CFG.sym.profile + CFG.off["profile.lengths_unlocked"], 3)
  emu:write32(CFG.sym.debug_seed, 4322)
  T.new_run(2)
  local fought, foes, guard = 0, {}, 0
  local lost_checked = false
  while T.screen() ~= T.SCREEN.RUN_END and guard < 200 do
    guard = guard + 1
    if T.screen() == T.SCREEN.FIGHT then
      local before = T.run_state()
      local node = T.current_node()
      T.wait(4)
      local foe = u32(CFG.sym.fight_foe)
      foes[foe] = (foes[foe] or 0) + 1
      if fought == 0 then T.shot("fight") end
      if not lost_checked and foe >= 2 then
        -- take the beating: press nothing, the bar drains and the monster strikes
        for _ = 1, 40 * 60 do
          T.wait(1)
          if T.screen() ~= T.SCREEN.FIGHT or u32(CFG.sym.fight_player_hp) == 0 then break end
        end
        T.check_eq(u32(CFG.sym.fight_player_hp), 0, "three strikes landed: the fight is lost")
        T.shot("fight_lost")
        T.wait(75); T.press(T.K.A); T.wait(6)
        T.check_eq(T.run_state().lives, before.lives - 1, "a lost fight costs a life")
        lost_checked = true
      else
        T.fight()
        local after = T.run_state()
        if after.layer == before.layer then
          T.check_eq(after.ore, before.ore + (10 + node.difficulty * 5) * 2, "a slain monster pays double base ore")
          if fought == 0 then T.shot("fight_won") end
        end
        fought = fought + 1
      end
    elseif T.screen() == T.SCREEN.MASH then T.mash()
    elseif T.screen() == T.SCREEN.ROOM then T.solve_room()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.SHOP then T.leave_shop(nil)
    elseif T.screen() == T.SCREEN.MAP then T.map_go(T.next_of(T.KIND.FIGHT) or "left") end
  end
  T.check(fought >= 3, fought .. " monsters slain")
  local kinds = 0
  for _ in pairs(foes) do kinds = kinds + 1 end
  T.check(kinds >= 3, kinds .. " kinds of monster met")
  T.check(lost_checked, "a fight was lost on purpose")
end)
