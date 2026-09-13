-- Deeper — Lua helper library for the mGBA scenarios.
--
-- run.py prepends a CFG table (symbol addresses, struct offsets, output
-- directory, scenario name) and this file to every scenario, then runs the
-- result with `mGBA --script`. A scenario is a plain function passed to
-- T.run(); it runs as a coroutine that yields once per emulated frame.
--
--   T.run(function()
--     T.boot()                          -- power on, reach the title
--     T.new_run()                       -- start a descent: the entrance room
--     T.solve_with_hints()              -- L until the room is cleared
--     T.check_eq(T.screen(), T.SCREEN.MAP, "back on the map")
--   end)
--
-- Every T.check* call writes a PASS / FAIL line to the scenario log; run.py
-- turns those into the summary. Reading the ROM's memory (screen, run state)
-- is the primary way to assert; screenshots are for humans.

T = {}
local K = C.GBA_KEY
T.K = K

-- --- constants mirrored from the C enums ------------------------------------------
T.SCREEN = { TITLE = 0, RECORDS = 1, MAP = 2, ROOM = 3, RUN_END = 4, LANG = 5, LENGTH = 6 }
T.MENU   = { CONTINUE = 0, NEW = 1, RECORDS = 2 }
T.FAM    = { DIG = 0, VEIN = 1, BLOCK = 2, TUNNEL = 3, LEDGER = 4, NUGGET = 5 }
T.KIND   = { PUZZLE = 0, RISKY = 1, HINT = 2, LIFE = 3, CAMP = 4, CORE = 5 }
T.SLOTS  = 3
T.SOLVED_FRAMES = 90
T.COLLAPSE_FRAMES = 120
T.WALK_FRAMES = 40

-- --- logging -------------------------------------------------------------------
local logfile = io.open(CFG.out .. "/" .. CFG.scenario .. ".log", "w")
T.failures, T.checks = 0, 0

function T.log(s)
  logfile:write(s, "\n"); logfile:flush()
  console:log(s)
end

function T.check(cond, name)
  T.checks = T.checks + 1
  if cond then T.log("PASS " .. name) else T.failures = T.failures + 1; T.log("FAIL " .. name) end
  return cond
end

function T.check_eq(actual, expected, name)
  return T.check(actual == expected, string.format("%s (got %s, expected %s)", name, tostring(actual), tostring(expected)))
end

-- --- frames and input ------------------------------------------------------------
function T.wait(n) for _ = 1, n do coroutine.yield() end end

-- press a button: held `hold` frames (default 2), then released for 3 frames
-- so that two consecutive presses are seen as two edges by the game
function T.press(key, hold)
  emu:addKey(key); T.wait(hold or 2); emu:clearKey(key); T.wait(3)
end

function T.shot(name)
  emu:screenshot(CFG.out .. "/" .. CFG.scenario .. "_" .. name .. ".png")
end

-- --- reading the ROM's memory --------------------------------------------------
local function u8(a) return emu:read8(a) end
local function u16(a) return emu:read16(a) end
local function u32(a) return emu:read32(a) end
local S, O = CFG.sym, CFG.off

function T.screen() return u32(S.screen) end
function T.frames() return u32(S.frames) end
function T.menu_cursor() return u32(S.menu_cursor) end

function T.run_state()
  local b = S.run
  return {
    seed = u32(b + O["run.seed"]),
    layer = u8(b + O["run.layer"]),
    slot = u8(b + O["run.slot"]),
    lives = u8(b + O["run.lives"]),
    hints = u8(b + O["run.hints"]),
    ore = u16(b + O["run.ore"]),
    room_in_progress = u8(b + O["run.room_in_progress"]),
    layers = u8(b + O["run.layers"]),
    length_index = u8(b + O["run.length_index"]),
    frames = u32(b + O["run.frames"]),
  }
end

function T.profile()
  local b = S.profile
  return {
    lengths_unlocked = u8(b + O["profile.lengths_unlocked"]),
    best_frames = { u32(b + O["profile.best_frames"]), u32(b + O["profile.best_frames"] + 4), u32(b + O["profile.best_frames"] + 8) },
    last_frames = u32(b + O["profile.last_frames"]),
    runs_won = u16(b + O["profile.runs_won"]),
  }
end

function T.node(layer, slot)
  local b = S.run + O["run.node"] + (layer * T.SLOTS + slot) * O["run.node_stride"]
  return {
    present = u8(b), family = u8(b + O["node.family"]), kind = u8(b + O["node.kind"]),
    difficulty = u8(b + O["node.difficulty"]), puzzle = u16(b + O["node.puzzle"]),
  }
end

function T.current_node()
  local r = T.run_state()
  return T.node(r.layer, r.slot)
end

-- --- navigation -------------------------------------------------------------------
-- power on: the language screen comes first; A keeps the saved choice
function T.boot(lang)
  T.wait(30)
  T.check_eq(T.screen(), T.SCREEN.LANG, "language screen after boot")
  if lang then
    for _ = 1, 3 do
      if u32(S.lang_cursor) == lang then break end
      T.press(K.DOWN)
    end
  end
  T.press(K.A); T.wait(4)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "title after the language pick")
end

-- title menu: move to an entry and confirm
function T.menu_go(item)
  for _ = 1, 4 do
    if T.menu_cursor() == item then break end
    T.press(K.DOWN)
  end
  T.check_eq(T.menu_cursor(), item, "menu entry reached")
  T.press(K.A); T.wait(4)
end

-- New descent: the length screen, then the entrance room. `length` = 0 (15), 1 (30), 2 (60)
function T.new_run(length)
  T.menu_go(T.MENU.NEW)
  T.check_eq(T.screen(), T.SCREEN.LENGTH, "New descent asks for the length")
  for _ = 1, 3 do
    if u32(S.length_cursor) == (length or 0) then break end
    T.press(K.DOWN)
  end
  T.check_eq(u32(S.length_cursor), length or 0, "length selected")
  T.press(K.A); T.wait(4)
  T.check_eq(T.screen(), T.SCREEN.ROOM, "a new run opens the entrance room")
end

-- L until the room is cleared (hint tokens permitting), then A past the celebration
function T.solve_with_hints(max_hints)
  local before = T.run_state().layer
  for _ = 1, (max_hints or 9) do
    if T.screen() ~= T.SCREEN.ROOM then break end
    T.press(K.L); T.wait(2)
  end
  T.wait(T.SOLVED_FRAMES + 2)
  T.press(K.A); T.wait(6)
  return before
end

-- SELECT then A: give the room up (costs a life)
function T.abandon()
  T.press(K.SELECT); T.wait(2); T.press(K.A); T.wait(6)
end

-- on the map: pick the leftmost / rightmost reachable node and walk there
function T.map_go(dir)
  if dir == "left" then for _ = 1, 3 do T.press(K.LEFT) end
  elseif dir == "right" then for _ = 1, 3 do T.press(K.RIGHT) end end
  local before = T.run_state().layer
  T.press(K.A)
  -- the walk takes WALK_FRAMES; the arrival frame itself can run long (room
  -- set-up), so wait for the layer to change and then let things settle
  for _ = 1, T.WALK_FRAMES + 30 do
    T.wait(1)
    if T.run_state().layer ~= before then break end
  end
  T.wait(4)
end

-- --- scheduler --------------------------------------------------------------------
function T.run(scenario)
  local co = coroutine.create(function()
    scenario()
    T.log(string.format("END checks=%d failures=%d", T.checks, T.failures))
  end)
  local frame_no = 0
  callbacks:add("frame", function()
    -- optional recording (tools/make_fullrun.py): one screenshot every CFG.every frames
    if CFG.frames_dir then
      frame_no = frame_no + 1
      if frame_no % (CFG.every or 4) == 0 then
        emu:screenshot(string.format("%s/%06d.png", CFG.frames_dir, frame_no))
      end
    end
    if coroutine.status(co) ~= "dead" then
      local ok, err = coroutine.resume(co)
      if not ok then
        T.log("ERROR " .. tostring(err))
        emu:screenshot(CFG.out .. "/" .. CFG.scenario .. "_error.png")
        logfile:close()
        os.exit(1)
      end
    else
      logfile:close()
      os.exit(T.failures == 0 and 0 or 1)
    end
  end)
end

-- --- reading a DIG puzzle straight from the ROM bank ------------------------------
-- Bank format: docs/puzzle_bank.md. Returns n and the solution column per row.
function T.dig_solution(index)
  local b = S.bank_dig
  local off = u32(b + 32 + 4 * index)
  local n = u8(b + off + 1)
  local payload = b + off + 4
  local sol_base = payload + math.floor((n * n + 1) / 2)
  local sol = {}
  for r = 0, n - 1 do
    local byte = u8(sol_base + math.floor(r / 2))
    if r % 2 == 1 then sol[r] = math.floor(byte / 16) else sol[r] = byte % 16 end
  end
  return n, sol
end

-- Drive the cursor from (cr, cc) to (r, c) on an n x n grid (wrapping moves)
T.cur = { r = 0, c = 0 }
function T.cursor_reset() T.cur.r, T.cur.c = 0, 0 end
function T.goto_cell(r, c, n)
  while T.cur.r ~= r do
    local down = (r - T.cur.r + n) % n
    if down <= n / 2 then T.press(K.DOWN); T.cur.r = (T.cur.r + 1) % n
    else T.press(K.UP); T.cur.r = (T.cur.r - 1 + n) % n end
  end
  while T.cur.c ~= c do
    local right = (c - T.cur.c + n) % n
    if right <= n / 2 then T.press(K.RIGHT); T.cur.c = (T.cur.c + 1) % n
    else T.press(K.LEFT); T.cur.c = (T.cur.c - 1 + n) % n end
  end
end

-- Solve the current DIG room by placing the stored solution, then A past the celebration
function T.solve_dig()
  local node = T.current_node()
  T.check_eq(node.family, T.FAM.DIG, "current room is a DIG room")
  local n, sol = T.dig_solution(node.puzzle)
  T.cursor_reset()
  for r = 0, n - 1 do
    T.goto_cell(r, sol[r], n)
    T.press(K.A)
  end
  T.wait(T.SOLVED_FRAMES + 2)
  T.press(K.A); T.wait(6)
end

-- --- solving every family from the ROM banks ------------------------------------------
-- Bank format: docs/puzzle_bank.md. Returns n and the payload address.
local function bank_record(sym, index)
  local off = u32(sym + 32 + 4 * index)
  return u8(sym + off + 1), sym + off + 4
end
local function nibble(base, i)
  local byte = u8(base + (i >> 1))
  if i % 2 == 1 then return byte >> 4 else return byte & 15 end
end

-- press A `k` times on the current cell
local function taps(k) for _ = 1, k do T.press(K.A) end end

function T.solve_vein(index)
  local n, payload = bank_record(S.bank_vein, index)
  local sol = payload + ((n * n + 3) // 4)
  T.cursor_reset()
  for i = 0, n * n - 1 do
    local given = (u8(payload + (i >> 2)) >> ((i % 4) * 2)) & 3
    if given == 0 then
      local dark = (u8(sol + (i >> 3)) >> (i % 8)) & 1
      T.goto_cell(i // n, i % n, n)
      taps(dark == 1 and 2 or 1)
    end
  end
end

function T.solve_ledger(index)
  local n, payload = bank_record(S.bank_ledger, index)
  local half = (n * n + 1) // 2
  T.cursor_reset()
  for i = 0, n * n - 1 do
    if nibble(payload, i) == 0 then
      local v = nibble(payload + half, i)
      T.goto_cell(i // n, i % n, n)
      if v <= n // 2 then taps(v) else for _ = 1, n + 1 - v do T.press(K.B) end end
    end
  end
end

function T.solve_tunnel(index)
  local n, payload = bank_record(S.bank_tunnel, index)
  local open, start = 0, nil
  for i = 0, n * n - 1 do
    local v = nibble(payload, i)
    if v ~= 15 then open = open + 1 end
    if v == 1 then start = i end
  end
  local steps = payload + ((n * n + 1) // 2)
  local dr, dc = { -1, 0, 1, 0 }, { 0, 1, 0, -1 }
  local r, c = start // n, start % n
  T.cursor_reset()
  for i = 0, open - 2 do
    local d = (u8(steps + (i >> 2)) >> ((i % 4) * 2)) & 3
    r, c = r + dr[d + 1], c + dc[d + 1]
    T.goto_cell(r, c, n)
    T.press(K.A)
  end
end

function T.solve_block(index)
  local n, payload = bank_record(S.bank_block, index)
  local mask_len = (n * n + 7) // 8
  local count = u8(payload + mask_len)
  T.cursor_reset()
  for j = 0, count - 1 do
    local b = u8(payload + mask_len + 1 + 2 * j)
    local orient, anchor = b & 7, u8(payload + mask_len + 2 + 2 * j)
    -- current block: B until it is block j; R until it has the right orientation
    local guard = 0
    while u32(S.block_cur) ~= j and guard < 16 do T.press(K.B); guard = guard + 1 end
    T.check_eq(u32(S.block_cur), j, "block " .. j .. " selected")
    guard = 0
    while u8(S.block_orient + j) ~= orient and guard < 8 do T.press(K.R); guard = guard + 1 end
    T.goto_cell(anchor // n, anchor % n, n)
    T.press(K.A)
  end
end

-- the bonus room's layout: same xorshift as source/fam_nugget.c
function T.solve_nugget()
  local r = T.run_state()
  local s = (r.seed ~ ((0x9E3779B9 * (r.layer + 1)) & 0xFFFFFFFF)) & 0xFFFFFFFF
  if s == 0 then s = 0x9E3779B9 end
  local nug, placed = {}, 0
  while placed < 6 do
    s = (s ~ ((s << 13) & 0xFFFFFFFF)) & 0xFFFFFFFF
    s = (s ~ (s >> 17)) & 0xFFFFFFFF
    s = (s ~ ((s << 5) & 0xFFFFFFFF)) & 0xFFFFFFFF
    local i = s % 36
    if not nug[i] then nug[i] = true; placed = placed + 1 end
  end
  T.cursor_reset()
  for i = 0, 35 do
    if nug[i] then T.goto_cell(i // 6, i % 6, 6); T.press(K.A) end
  end
end

-- Solve whatever room is open, then A past the celebration
function T.solve_room()
  local node = T.current_node()
  if node.family == T.FAM.DIG then
    local n, sol = T.dig_solution(node.puzzle)
    T.cursor_reset()
    for r = 0, n - 1 do T.goto_cell(r, sol[r], n); T.press(K.A) end
  elseif node.family == T.FAM.VEIN then T.solve_vein(node.puzzle)
  elseif node.family == T.FAM.LEDGER then T.solve_ledger(node.puzzle)
  elseif node.family == T.FAM.TUNNEL then T.solve_tunnel(node.puzzle)
  elseif node.family == T.FAM.BLOCK then T.solve_block(node.puzzle)
  elseif node.family == T.FAM.NUGGET then T.solve_nugget()
  end
  T.wait(T.SOLVED_FRAMES + 2)
  T.press(K.A); T.wait(6)
end
