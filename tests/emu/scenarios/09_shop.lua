-- @fresh
-- The counter between runs: the merchant, prices, refusals, purchases that
-- stick in the profile and gear that shapes the next run; then a camp during
-- the run: a potion, keys and a rope paid with the run's ore.
T.run(function()
  T.boot()
  T.menu_go(T.MENU.SHOP)
  T.check_eq(T.screen(), T.SCREEN.SHOP, "the title menu opens the counter")
  T.shot("counter_poor")
  T.press(T.K.A); T.wait(2)                       -- beer at 120 with 0 in the bank: refused
  local p = T.profile()
  T.check_eq(p.upgrade[1], 0, "nothing bought without ore")
  emu:write16(CFG.sym.profile + CFG.off["profile.ore_bank"], 1000)
  T.press(T.K.B); T.wait(4)
  T.menu_go(T.MENU.SHOP)
  T.press(T.K.A); T.wait(2)                       -- beer level 1: two lives at most
  p = T.profile()
  T.check_eq(p.upgrade[1], 1, "beer bought")
  T.check_eq(p.ore_bank, 880, "120 ore spent")
  T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)  -- bread: unlocked by the beer, 200
  T.press(T.K.DOWN); T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)   -- keys: 80
  p = T.profile()
  T.check_eq(p.upgrade[2], 1, "bread bought")
  T.check_eq(p.upgrade[4], 1, "keys bought")
  T.check_eq(p.ore_bank, 600, "200 + 80 spent")
  T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)  -- boots: 300
  T.check_eq(T.profile().upgrade[5], 1, "boots bought")
  T.shot("counter_bought")
  T.press(T.K.B); T.wait(4)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "B leaves the counter")
  -- the gear shows on the next run
  emu:write32(CFG.sym.debug_seed, 20260913)
  T.new_run()
  local r = T.run_state()
  T.check_eq(r.hints, 2, "the keys add a starting hint")
  T.check_eq(r.lives, 2, "the bread adds a starting life")
  T.check_eq(r.max_lives, 2, "...within the beer's maximum")
  T.shot("run_geared")
  -- to the first camp: the run's ore buys a potion, a key and a rope
  T.solve_room()
  local guard = 0
  while T.screen() ~= T.SCREEN.SHOP and guard < 12 do
    guard = guard + 1
    if T.screen() == T.SCREEN.ROOM then T.solve_room()
    elseif T.screen() == T.SCREEN.FIGHT then T.fight()
    elseif T.screen() == T.SCREEN.MASH then T.mash()
    elseif T.screen() == T.SCREEN.CRATES then T.open_crates()
    elseif T.screen() == T.SCREEN.MAP then T.map_go(T.next_of(T.KIND.CAMP) or "left") end
  end
  T.check_eq(T.screen(), T.SCREEN.SHOP, "a camp was reached")
  emu:write16(CFG.sym.run + CFG.off["run.ore"], 500)
  emu:write8(CFG.sym.run + CFG.off["run.lives"], 1)
  T.wait(2)
  local before = T.run_state()
  T.press(T.K.UP); T.press(T.K.DOWN); T.wait(2)   -- redraw with the new balance
  T.shot("camp")
  T.press(T.K.A); T.wait(2)                       -- potion
  T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)  -- key
  T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)  -- rope
  r = T.run_state()
  T.check_eq(r.lives, 2, "the potion healed a life")
  T.check_eq(r.hints, before.hints + 1, "the key added a hint")
  T.check_eq(r.rope_rooms, 3, "the rope holds the next three rooms")
  T.check_eq(r.ore, 500 - 60 - 40 - 50, "paid with the run's ore")
  T.press(T.K.B); T.wait(4)
  T.map_go("left")
  T.check_eq(T.run_state().rope_rooms, 2, "the rope is used up room by room")
  -- and the gear survives a reboot
  emu:reset()
  T.boot()
  p = T.profile()
  T.check_eq(p.upgrade[1], 1, "beer still owned after reset")
  T.check_eq(p.upgrade[5], 1, "boots still owned after reset")
end)
