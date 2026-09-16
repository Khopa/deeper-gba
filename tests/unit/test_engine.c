// Engine modules that run on the host: bank access (on the committed bank
// files), the DIG adapter behind the room screen, strings.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "bank.h"
#include "puzzle.h"
#include "mines.h"
extern Mines nugget_mines;
#include "render.h"
#include "lang.h"
#include "dig.h"

static uint8_t *bank_image;
static int bank_image_len;

static bool load_bank_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    bank_image_len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    free(bank_image);
    bank_image = malloc(bank_image_len);
    bool ok = fread(bank_image, 1, bank_image_len, f) == (size_t)bank_image_len;
    fclose(f);
    return ok;
}

static void with_dig_bank(void)
{
    bank_init();
    CHECK(load_bank_file("data/puzzles/dig.bin"));
    CHECK(bank_register(bank_image, bank_image_len));
}

TEST(bank_rejects_garbage)
{
    bank_init();
    uint8_t junk[64] = { 'X' };
    CHECK(!bank_register(junk, sizeof junk));
    CHECK(!bank_register(junk, 8));
    CHECK_EQ(bank_count(FAM_DIG), 0);
    BankEntry e;
    CHECK(!bank_get(FAM_DIG, 0, &e));
}

TEST(committed_dig_bank_is_sound)
{
    with_dig_bank();
    int count = bank_count(FAM_DIG);
    CHECK(count >= 400);
    int prev_diff = 0;
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_DIG, i, &e));
        CHECK_EQ(e.hdr->family, FAM_DIG);
        CHECK(e.hdr->difficulty >= prev_diff);          // sorted
        prev_diff = e.hdr->difficulty;
        DigPuzzle p;
        CHECK(dig_unpack(e.payload, e.hdr->size, &p));
        CHECK_EQ(dig_count_solutions(&p, 2), 1);
        DigSolveStats st;
        dig_deduce(&p, NULL, &st);
        CHECK(st.solved);
        CHECK_EQ(dig_difficulty(&p, &st), e.hdr->difficulty);
    }
    CHECK(!bank_get(FAM_DIG, count, NULL));
}

TEST(bank_range_matches_difficulties)
{
    with_dig_bank();
    int count = bank_count(FAM_DIG);
    for (int d = 1; d <= 10; d++) {
        int first, last;
        bank_range(FAM_DIG, d, d, &first, &last);
        CHECK(first <= last && last <= count);
        for (int i = first; i < last; i++) {
            BankEntry e;
            bank_get(FAM_DIG, i, &e);
            CHECK_EQ(e.hdr->difficulty, d);
        }
        if (first > 0) {
            BankEntry e;
            bank_get(FAM_DIG, first - 1, &e);
            CHECK(e.hdr->difficulty < d);
        }
    }
    int first, last;
    bank_range(FAM_DIG, 1, 10, &first, &last);
    CHECK_EQ(first, 0);
    CHECK_EQ(last, count);
    bank_range(FAM_DIG, 3, 5, &first, &last);
    CHECK(first < last);
}

// --- DIG adapter -----------------------------------------------------------------------

static void load_first(int difficulty)
{
    with_dig_bank();
    int first, last;
    bank_range(FAM_DIG, difficulty, difficulty, &first, &last);
    BankEntry e;
    CHECK(bank_get(FAM_DIG, first, &e));
    CHECK(ops_dig.load(e.hdr, e.payload));
}

TEST(adapter_cells_expose_regions_edges_and_marks)
{
    load_first(1);
    int n = ops_dig.size();
    CHECK(n >= DIG_MIN_N && n <= DIG_MAX_N);
    CellView v;
    ops_dig.cell(0, 0, &v);
    CHECK((v.edges & 9) == 9);              // top-left corner: north and west edges
    CHECK(v.pal >= PAL_REGION0 && v.pal < PAL_REGION0 + 8);
    CHECK_EQ(v.mark, MARK_NONE);
    ops_dig.cell(n - 1, n - 1, &v);
    CHECK((v.edges & 6) == 6);              // bottom-right: south and east
}

TEST(adapter_actions_toggle_and_flag_conflicts)
{
    load_first(1);
    CellView v;
    ActionResult r = ops_dig.action(0, 0, ACT_A);
    CHECK(r.changed && !r.mistake && !r.solved);
    ops_dig.cell(0, 0, &v);
    CHECK_EQ(v.mark, MARK_DIG);
    r = ops_dig.action(0, 1, ACT_A);        // adjacent: both in conflict
    CHECK(r.changed && r.mistake);
    ops_dig.cell(0, 0, &v);
    CHECK(v.conflict);
    CHECK_EQ(v.pal, PAL_CELL_CONFLICT);
    r = ops_dig.action(0, 1, ACT_A);        // toggle off
    CHECK(r.changed && !r.mistake);
    ops_dig.cell(0, 0, &v);
    CHECK(!v.conflict);
    r = ops_dig.action(1, 1, ACT_B);
    ops_dig.cell(1, 1, &v);
    CHECK_EQ(v.mark, MARK_CROSS);
    r = ops_dig.action(1, 1, ACT_B);
    ops_dig.cell(1, 1, &v);
    CHECK_EQ(v.mark, MARK_NONE);
}

TEST(adapter_hints_solve_the_puzzle_and_refuse_wrong_digs)
{
    load_first(6);
    int r, c, guard = 0;
    while (!ops_dig.solved() && guard++ < 200) {
        int h = ops_dig.hint(&r, &c);
        CHECK_EQ(h, HINT_APPLIED);
        if (h != HINT_APPLIED) break;
    }
    CHECK(ops_dig.solved());

    load_first(6);
    // find a cell that is not part of the solution and dig there
    int n = ops_dig.size();
    CellView v;
    ops_dig.action(0, 0, ACT_A);
    ops_dig.cell(0, 0, &v);
    int hr = -1, hc = -1;
    int h = ops_dig.hint(&hr, &hc);
    // either (0,0) is right (hint applies) or it is wrong (hint points at it)
    if (h == HINT_WRONG_PLACEMENT) { CHECK_EQ(hr, 0); CHECK_EQ(hc, 0); }
    else CHECK_EQ(h, HINT_APPLIED);
    (void)n;
}

TEST(adapter_save_restore_round_trip)
{
    load_first(3);
    ops_dig.action(0, 0, ACT_A);
    ops_dig.action(2, 2, ACT_B);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_dig.save(buf);
    CHECK(len > 0 && len <= ROOM_STATE_MAX);
    ops_dig.action(0, 0, ACT_A);            // clear
    CellView v;
    ops_dig.cell(0, 0, &v);
    CHECK_EQ(v.mark, MARK_NONE);
    CHECK(ops_dig.restore(buf, len));
    ops_dig.cell(0, 0, &v);
    CHECK_EQ(v.mark, MARK_DIG);
    ops_dig.cell(2, 2, &v);
    CHECK_EQ(v.mark, MARK_CROSS);
    CHECK(!ops_dig.restore(buf, 1));
}

TEST(strings_exist_in_every_language_and_fit_the_font)
{
    static const char allowed[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02\x03\x04\x05\x06+=*()";
    for (int l = 0; l < LANG_COUNT; l++) {
        lang_set(l);
        for (int id = 0; id < STR_COUNT; id++) {
            const char *s = S(id);
            CHECK(s && *s);
            for (const char *p = s; *p; p++) CHECK(strchr(allowed, *p) != NULL);
            CHECK(strlen(s) <= ((id >= STR_HELP_DIG && id <= STR_KEY_A_BREAK) || (id >= STR_LENGTH_UNLOCKED && id <= STR_HELMET_HELD) ? 90u : 30u));   // help lines are wrapped
        }
    }
    lang_set(LANG_FR);
    CHECK(strcmp(S(STR_ORE), "MINERAI") == 0);
    lang_set(LANG_EN);
    CHECK(strcmp(S(STR_ORE), "ORE") == 0);
}

// --- FIREDAMP room (family NUGGET): a small minesweeper laid out in play --------------

TEST(firedamp_room_lays_pockets_after_a_safe_first_break_and_saves)
{
    PuzzleHeader h = { FAM_NUGGET, 6, 4, 0 };
    uint8_t seed_a[4] = { 1, 2, 3, 4 }, seed_b[4] = { 9, 9, 9, 9 };
    CHECK(puzzle_ops(FAM_NUGGET) == &ops_nugget);
    CHECK(ops_nugget.bonus_ore == NULL);            // a real room: the reward is the node's
    CHECK(ops_nugget.immediate_mistakes);
    CHECK(ops_nugget.load(&h, seed_a));
    CHECK_EQ(ops_nugget.size(), 6);
    CHECK_EQ(mines_count_for(1), 4);
    CHECK_EQ(mines_count_for(4), 5);
    CHECK_EQ(mines_count_for(10), 7);
    CHECK_EQ(nugget_mines.count, 5);
    CHECK(!nugget_mines.placed);
    // the first break is always safe, and so are its neighbours
    ActionResult ar = ops_nugget.action(2, 2, ACT_A);
    CHECK(ar.changed && !ar.mistake);
    CHECK(nugget_mines.placed);
    int pockets = 0;
    for (int i = 0; i < 36; i++) pockets += nugget_mines.pocket[i];
    CHECK_EQ(pockets, 5);
    for (int r = 1; r <= 3; r++)
        for (int c = 1; c <= 3; c++) CHECK(!nugget_mines.pocket[cell_at(6, r, c)]);
    CellView v;
    ops_nugget.cell(2, 2, &v);
    CHECK_EQ(v.variant, 1);
    CHECK_EQ(v.mark, MARK_NONE);                    // no pocket around: a 0, and its neighbours opened too
    ops_nugget.cell(1, 1, &v);
    CHECK_EQ(v.variant, 1);
    // numbers count the pockets next door; a flag is a note that A respects
    int bare = 0;
    for (int i = 0; i < 36; i++) {
        ops_nugget.cell(i / 6, i % 6, &v);
        if (v.variant == 1) { bare++; CHECK_EQ(v.mark == MARK_NONE ? 0 : v.mark - MARK_DIGIT1 + 1, mines_adjacent(&nugget_mines, i)); }
    }
    CHECK(bare >= 9);
    int hidden = 0;
    while (nugget_mines.cell[hidden] == MINES_BARE) hidden++;
    ar = ops_nugget.action(hidden / 6, hidden % 6, ACT_B);
    CHECK(ar.changed);
    ops_nugget.cell(hidden / 6, hidden % 6, &v);
    CHECK_EQ(v.mark, MARK_CROSS);
    ar = ops_nugget.action(hidden / 6, hidden % 6, ACT_A);
    CHECK(!ar.changed);                              // flagged: A does nothing
    ops_nugget.action(hidden / 6, hidden % 6, ACT_B);
    // save, blow a pocket up (a mistake, the cell shows it), restore: the blast is undone
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_nugget.save(buf);
    CHECK_EQ(len, MINES_SAVE_LEN);
    int pocket = 0;
    while (!nugget_mines.pocket[pocket]) pocket++;
    ar = ops_nugget.action(pocket / 6, pocket % 6, ACT_A);
    CHECK(ar.changed && ar.mistake && !ar.solved);
    ops_nugget.cell(pocket / 6, pocket % 6, &v);
    CHECK_EQ(v.mark, MARK_ALERT);
    CHECK_EQ(v.conflict, 1);
    CHECK(ops_nugget.restore(buf, len));
    ops_nugget.cell(pocket / 6, pocket % 6, &v);
    CHECK_EQ(v.variant, 0);
    // break every safe cell: solved without any mistake; hints bare safe cells
    int mistakes = 0;
    for (int i = 0; i < 36; i++)
        if (!nugget_mines.pocket[i]) { ar = ops_nugget.action(i / 6, i % 6, ACT_A); mistakes += ar.mistake; }
    CHECK_EQ(mistakes, 0);
    CHECK(ops_nugget.solved());
    // the same seed lays the same pockets, another seed different ones
    uint8_t lay_a[36], lay_b[36];
    memcpy(lay_a, nugget_mines.pocket, 36);
    CHECK(ops_nugget.load(&h, seed_b));
    ops_nugget.action(2, 2, ACT_A);
    memcpy(lay_b, nugget_mines.pocket, 36);
    CHECK(memcmp(lay_a, lay_b, 36) != 0);
    CHECK(ops_nugget.load(&h, seed_a));
    ops_nugget.action(2, 2, ACT_A);
    CHECK_MEM(nugget_mines.pocket, lay_a, 36);
    // a hint on an untouched room lays the pockets and bares a safe cell
    CHECK(ops_nugget.load(&h, seed_a));
    int hr, hc;
    CHECK_EQ(ops_nugget.hint(&hr, &hc), HINT_APPLIED);
    CHECK(nugget_mines.placed);
    CHECK(!nugget_mines.pocket[cell_at(6, hr, hc)]);
}

const TestCase engine_tests[] = {
    T(firedamp_room_lays_pockets_after_a_safe_first_break_and_saves),
    T(bank_rejects_garbage),
    T(committed_dig_bank_is_sound),
    T(bank_range_matches_difficulties),
    T(adapter_cells_expose_regions_edges_and_marks),
    T(adapter_actions_toggle_and_flag_conflicts),
    T(adapter_hints_solve_the_puzzle_and_refuse_wrong_digs),
    T(adapter_save_restore_round_trip),
    T(strings_exist_in_every_language_and_fit_the_font),
};
SUITE(engine)
