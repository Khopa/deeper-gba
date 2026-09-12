// VEIN rules, packing, solver, generator and adapter.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "vein.h"
#include "gen.h"
#include "bank.h"
#include "puzzle.h"
#include "render.h"

// Fixture: generated 6x6, difficulty 7, needs two trials. O = light, X = dark.
static const char *FIX[6] = {
    "..XO.O",
    "O....O",
    "X.OX..",
    ".X....",
    "O..O..",
    ".X..O.",
};

static void fixture(VeinPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 6;
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 6; c++)
            p->given[cell_at(6, r, c)] = FIX[r][c] == 'O' ? VEIN_LIGHT : FIX[r][c] == 'X' ? VEIN_DARK : VEIN_EMPTY;
    // the solution is what the solver finds; tests below check it is consistent
    VeinSolveStats st;
    VeinBoard out;
    vein_deduce(p, NULL, &st, &out);
    memcpy(p->solution, out.cell, 36);
}

TEST(fixture_is_deducible_unique_and_consistent)
{
    VeinPuzzle p;
    fixture(&p);
    VeinSolveStats st;
    VeinBoard out;
    vein_deduce(&p, NULL, &st, &out);
    CHECK(st.solved);
    CHECK_EQ(st.hardest, VEINT_TRIAL);
    CHECK_EQ(st.steps[VEINT_TRIAL], 2);
    CHECK_EQ(vein_count_solutions(&p, 3), 1);
    CHECK(vein_solved(&p, &out));
    for (int i = 0; i < 36; i++) if (p.given[i]) CHECK_EQ(out.cell[i], p.given[i]);
    CHECK_EQ(vein_difficulty(&p, &st), 7);
}

TEST(conflicts_flag_triples_and_overfull_lines)
{
    VeinPuzzle p;
    fixture(&p);
    VeinBoard b;
    vein_board_init(&p, &b);
    CHECK_EQ(vein_conflicts(&p, &b, NULL), 0);
    CHECK(!vein_solved(&p, &b));
    // a horizontal triple in row 3: cells (3,2),(3,3) dark next to the given dark at (3,1)
    b.cell[cell_at(6, 3, 2)] = VEIN_DARK;
    b.cell[cell_at(6, 3, 3)] = VEIN_DARK;
    uint8_t f[PUZZLE_MAX_CELLS];
    CHECK_EQ(vein_conflicts(&p, &b, f), 3);
    CHECK(f[cell_at(6, 3, 1)] && f[cell_at(6, 3, 2)] && f[cell_at(6, 3, 3)]);
    // four lights in row 1 (given O at both ends + two more, spaced): over-full
    vein_board_init(&p, &b);
    b.cell[cell_at(6, 1, 2)] = VEIN_LIGHT;
    b.cell[cell_at(6, 1, 4)] = VEIN_LIGHT;
    CHECK_EQ(vein_conflicts(&p, &b, f), 4);
    CHECK(f[cell_at(6, 1, 0)] && f[cell_at(6, 1, 5)]);
}

TEST(pack_unpack_round_trip_and_rejections)
{
    VeinPuzzle p, q;
    fixture(&p);
    uint8_t buf[128];
    int len = vein_pack(&p, buf);
    CHECK_EQ(len, vein_payload_len(6));
    CHECK_EQ(len, 9 + 5);
    CHECK(vein_unpack(buf, 6, &q));
    CHECK_MEM(q.given, p.given, 36);
    CHECK_MEM(q.solution, p.solution, 36);
    CHECK(!vein_unpack(buf, 7, &q));          // odd
    CHECK(!vein_unpack(buf, 4, &q));
    buf[0] = 0x03;                            // given value 3
    CHECK(!vein_unpack(buf, 6, &q));
}

TEST(next_step_reaches_the_solution_and_refuses_wrong_cells)
{
    VeinPuzzle p;
    fixture(&p);
    VeinBoard b;
    vein_board_init(&p, &b);
    int guard = 0;
    for (;;) {
        int cell, value;
        if (!vein_next_step(&p, &b, &cell, &value)) break;
        CHECK_EQ(b.cell[cell], VEIN_EMPTY);
        CHECK_EQ(value, p.solution[cell]);
        b.cell[cell] = (uint8_t)value;
        if (++guard > 100) break;
    }
    CHECK(vein_solved(&p, &b));
    vein_board_init(&p, &b);
    int i = 0;
    while (p.given[i]) i++;
    b.cell[i] = (uint8_t)(3 - p.solution[i]);
    int cell, value;
    CHECK(!vein_next_step(&p, &b, &cell, &value));
}

TEST(generator_output_is_valid_unique_deducible_and_distinct)
{
    Rng rng;
    rng_init(&rng, 7);
    HashSet seen;
    hs_init(&seen);
    int made = 0, attempts = 0;
    while (made < 40 && attempts < 2000) {
        attempts++;
        GenRecord rec;
        int n = made & 1 ? 8 : 6;
        if (!gen_vein.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        CHECK_EQ(rec.hdr.family, FAM_VEIN);
        CHECK_EQ(rec.hdr.size, n);
        CHECK(rec.hdr.difficulty >= DIFF_MIN && rec.hdr.difficulty <= DIFF_MAX);
        VeinPuzzle p;
        CHECK(vein_unpack(rec.payload, n, &p));
        VeinBoard full;
        memcpy(full.cell, p.solution, n * n);
        CHECK(vein_solved(&p, &full));
        int givens = 0;
        for (int i = 0; i < n * n; i++) givens += p.given[i] != 0;
        CHECK(givens <= n * n * 2 / 3);
        CHECK_EQ(vein_count_solutions(&p, 2), 1);
        VeinSolveStats st;
        vein_deduce(&p, NULL, &st, NULL);
        CHECK(st.solved);
        CHECK_EQ(vein_difficulty(&p, &st), rec.hdr.difficulty);
    }
    CHECK_EQ(made, 40);
    CHECK_EQ(seen.count, 40);
    hs_free(&seen);
}

// --- committed bank + adapter --------------------------------------------------------------

static uint8_t *img;
static int img_len;

static void with_vein_bank(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/vein.bin", "rb");
        CHECK(f != NULL);
        if (!f) return;
        fseek(f, 0, SEEK_END);
        img_len = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        img = malloc(img_len);
        CHECK(fread(img, 1, img_len, f) == (size_t)img_len);
        fclose(f);
    }
    bank_init();
    CHECK(bank_register(img, img_len));
}

TEST(committed_vein_bank_is_sound)
{
    with_vein_bank();
    int count = bank_count(FAM_VEIN);
    CHECK(count >= 300);
    int prev = 0;
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_VEIN, i, &e));
        CHECK_EQ(e.hdr->family, FAM_VEIN);
        CHECK(e.hdr->difficulty >= prev);
        prev = e.hdr->difficulty;
        VeinPuzzle p;
        CHECK(vein_unpack(e.payload, e.hdr->size, &p));
        VeinSolveStats st;
        vein_deduce(&p, NULL, &st, NULL);
        CHECK(st.solved);
        CHECK_EQ(vein_difficulty(&p, &st), e.hdr->difficulty);
        CHECK_EQ(vein_count_solutions(&p, 2), 1);
    }
    int first, last;
    bank_range(FAM_VEIN, 1, 2, &first, &last);
    CHECK(last > first);
}

TEST(adapter_cycles_free_cells_only_and_saves)
{
    with_vein_bank();
    BankEntry e;
    CHECK(bank_get(FAM_VEIN, 0, &e));
    CHECK(puzzle_ops(FAM_VEIN) == &ops_vein);
    CHECK(ops_vein.load(e.hdr, e.payload));
    int n = ops_vein.size();
    VeinPuzzle p;
    vein_unpack(e.payload, e.hdr->size, &p);
    int given = -1, free_ = -1;
    for (int i = 0; i < n * n; i++) {
        if (p.given[i] && given < 0) given = i;
        if (!p.given[i] && free_ < 0) free_ = i;
    }
    CellView v;
    ops_vein.cell(given / n, given % n, &v);
    CHECK(v.mark == MARK_ORE_LIGHT || v.mark == MARK_ORE_DARK);
    CHECK_EQ(v.pal, PAL_REGION0 + 5);
    ActionResult r = ops_vein.action(given / n, given % n, ACT_A);
    CHECK(!r.changed);

    r = ops_vein.action(free_ / n, free_ % n, ACT_A);
    CHECK(r.changed);
    ops_vein.cell(free_ / n, free_ % n, &v);
    CHECK_EQ(v.mark, MARK_ORE_LIGHT);
    r = ops_vein.action(free_ / n, free_ % n, ACT_A);
    ops_vein.cell(free_ / n, free_ % n, &v);
    CHECK_EQ(v.mark, MARK_ORE_DARK);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_vein.save(buf);
    r = ops_vein.action(free_ / n, free_ % n, ACT_B);
    ops_vein.cell(free_ / n, free_ % n, &v);
    CHECK_EQ(v.mark, MARK_NONE);
    CHECK(ops_vein.restore(buf, len));
    ops_vein.cell(free_ / n, free_ % n, &v);
    CHECK_EQ(v.mark, MARK_ORE_DARK);

    // hints finish the puzzle
    int hr, hc, guard = 0;
    ops_vein.action(free_ / n, free_ % n, ACT_B);
    while (!ops_vein.solved() && guard++ < 100) CHECK_EQ(ops_vein.hint(&hr, &hc), HINT_APPLIED);
    CHECK(ops_vein.solved());
}

const TestCase vein_tests[] = {
    T(fixture_is_deducible_unique_and_consistent),
    T(conflicts_flag_triples_and_overfull_lines),
    T(pack_unpack_round_trip_and_rejections),
    T(next_step_reaches_the_solution_and_refuses_wrong_cells),
    T(generator_output_is_valid_unique_deducible_and_distinct),
    T(committed_vein_bank_is_sound),
    T(adapter_cycles_free_cells_only_and_saves),
};
SUITE(vein)
