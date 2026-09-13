-- Continues from 02: walk to the next node, give the room up, lose a life.
T.run(function()
  T.boot()
  T.check_eq(T.menu_cursor(), T.MENU.CONTINUE, "a saved run puts the cursor on Continue")
  T.menu_go(T.MENU.CONTINUE)
  T.check_eq(T.screen(), T.SCREEN.MAP, "continue reopens the map")
  local before = T.run_state()
  T.map_go("right")
  local after = T.run_state()
  T.check_eq(after.layer, before.layer + 1, "walked one layer down")
  local node = T.current_node()
  if node.kind == T.KIND.CAMP then
    T.check_eq(T.screen(), T.SCREEN.MAP, "a camp keeps us on the map")
    T.check(after.lives >= before.lives, "camp never costs a life")
  else
    T.check_eq(T.screen(), T.SCREEN.ROOM, "a puzzle node opens a room")
    T.shot("room")
    T.abandon()
    T.check_eq(T.screen(), T.SCREEN.MAP, "abandoning returns to the map")
    T.check_eq(T.run_state().lives, before.lives - 1, "abandoning costs a life")
  end
  T.shot("map")
end)
