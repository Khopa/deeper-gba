-- @fresh
-- @timeout 300
-- English from the language screen, sound switched off in the options: both
-- survive a reboot (profile block); an English run shows its screens (map,
-- room, help, camp counter, records) for a visual check.
T.run(function()
  T.boot(1)
  T.check_eq(T.profile().lang, 1, "English saved by the language screen")
  T.menu_go(T.MENU.OPTIONS)
  T.check_eq(T.screen(), T.SCREEN.OPTIONS, "options")
  T.press(T.K.A); T.wait(2)
  T.check_eq(T.profile().sound, 0, "sound off")
  T.shot("options_en")
  T.press(T.K.B); T.wait(4)
  emu:reset()
  T.wait(30)
  T.check_eq(T.screen(), T.SCREEN.LANG, "language screen after the reboot")
  T.check_eq(u32(CFG.sym.lang_cursor), 1, "English preselected")
  T.press(T.K.A); T.wait(4); T.press(T.K.START); T.wait(4)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "menu")
  T.check_eq(T.profile().sound, 0, "sound still off after the reboot")
  T.check_eq(T.profile().lang, 1, "still English")
  T.shot("title_en")
  T.menu_go(T.MENU.RECORDS)
  T.shot("records_en")
  T.press(T.K.B); T.wait(4)
  emu:write32(CFG.sym.debug_seed, 20260913)
  T.new_run(0)
  T.shot("room_en")
  T.press(T.K.SELECT); T.wait(4)
  T.shot("help_en")
  T.press(T.K.B); T.wait(4)
  T.press(T.K.START); T.wait(4)
  T.shot("pause_en")
  T.press(T.K.B); T.wait(4)
  T.solve_room()
  T.shot("map_en")
  -- to the first camp for the counter
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
  T.shot("shop_en")
  T.leave_shop(nil)
end)
