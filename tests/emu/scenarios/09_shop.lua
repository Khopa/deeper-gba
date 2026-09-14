-- @fresh
-- The counter between runs: the merchant, prices, refusals, purchases that
-- stick in the profile and gear that shapes the next run.
T.run(function()
  T.boot()
  T.menu_go(T.MENU.SHOP)
  T.check_eq(T.screen(), T.SCREEN.SHOP, "the title menu opens the counter")
  T.shot("counter_poor")
  T.press(T.K.A); T.wait(2)                       -- satchel at 200 with 0 in the bank: refused
  local p = T.profile()
  T.check_eq(p.upgrade[1], 0, "nothing bought without ore")
  emu:write16(CFG.sym.profile + CFG.off["profile.ore_bank"], 1000)
  T.press(T.K.B); T.wait(4)
  T.menu_go(T.MENU.SHOP)
  T.press(T.K.A); T.wait(2)                       -- satchel level 1
  p = T.profile()
  T.check_eq(p.upgrade[1], 1, "satchel bought")
  T.check_eq(p.ore_bank, 800, "200 ore spent")
  T.press(T.K.DOWN); T.press(T.K.DOWN); T.press(T.K.DOWN); T.press(T.K.DOWN)   -- helmet (after bedroll, flask, lantern)
  T.press(T.K.A); T.wait(2)
  T.press(T.K.DOWN); T.press(T.K.A); T.wait(2)  -- beard
  p = T.profile()
  T.check_eq(p.cosmetics, 3, "both cosmetics owned")
  T.check_eq(p.ore_bank, 500, "150 + 150 spent")
  T.shot("counter_bought")
  T.press(T.K.B); T.wait(4)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "B leaves the counter")
  -- the gear shows on the next run
  T.new_run()
  local r = T.run_state()
  T.check_eq(r.hints, 4, "the satchel adds a starting hint")
  T.shot("dwarf_dressed")
  -- and survives a reboot
  emu:reset()
  T.boot()
  p = T.profile()
  T.check_eq(p.upgrade[1], 1, "satchel still owned after reset")
  T.check_eq(p.cosmetics, 3, "cosmetics still owned after reset")
end)
