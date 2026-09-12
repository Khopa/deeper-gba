// LEDGER rules, packing, solver, generator, committed bank and adapter.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "ledger.h"
#include "gen.h"
#include "bank.h"
#include "puzzle.h"
#include "render.h"

// Fixture: generated 6x6 (2x3 boxes), difficulty 6, hidden singles only.
static const char *FIX[6] = {
    "43....",
    "...5..",
    "341...",
    "6....3",
    "......",
    "2..6..",
};

static void fixture(LedgerPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 6;
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 6; c++)
            p->given[cell_at(6, r, c)] = FIX[r][c] == '.' ? 0 : (uint8_t)(FIX[r][c] - '0');
    LedgerSolveStats st;
    LedgerBoard out;
    ledger_deduce(p, NULL, &st, &out);
    memcpy(p->solution, out.cell, 36);
}

TEST(box_geometry)
{
    CHECK_EQ(ledger_box_w(4), 2);
    CHECK_EQ(ledger_box_w(6), 3);
    CHECK_EQ(ledger_box_w(8), 4);
    CHECK_EQ(ledger_box_of(6, 0, 0), 0);
    CHECK_EQ(ledger_box_of(6, 0, 3), 1);
    CHECK_EQ(ledger_box_of(6, 2, 0), 2);
    CHECK_EQ(ledger_box_of(6, 5, 5), 5);
    CHECK_EQ(ledger_box_of(4, 3, 2), 3);
}

TEST(fixture_is_deducible_unique_and_consistent)
{
    LedgerPuzzle p;
    fixture(&p);
    LedgerSolveStats st;
    LedgerBoard out;
    ledger_deduce(&p, NULL, &st, &out);
    CHECK(st.solved);
    CHECK_EQ(st.hardest, LEDT_HIDDEN);
    CHECK_EQ(ledger_count_solutions(&p, 3), 1);
    CHECK(ledger_solved(&p, &out));
    for (int i = 0; i < 36; i++) if (p.given[i]) CHECK_EQ(out.cell[i], p.given[i]);
    CHECK_EQ(ledger_difficulty(&p, &st), 6);
}

TEST(conflicts_flag_duplicates_in_rows_columns_and_boxes)
{
    LedgerPuzzle p;
    fixture(&p);
    LedgerBoard b;
    ledger_board_init(&p, &b);
    CHECK_EQ(ledger_conflicts(&p, &b, NULL), 0);
    uint8_t f[PUZZLE_MAX_CELLS];
    b.cell[cell_at(6, 0, 5)] = 4;                 // row 0 already has a 4 at (0,0)
    CHECK_EQ(ledger_conflicts(&p, &b, f), 2);
    CHECK(f[cell_at(6, 0, 0)] && f[cell_at(6, 0, 5)]);
    ledger_board_init(&p, &b);
    b.cell[cell_at(6, 1, 2)] = 3;                 // box 0 already has a 3 at (0,1)
    CHECK_EQ(ledger_conflicts(&p, &b, f), 2);
    ledger_board_init(&p, &b);
    b.cell[cell_at(6, 4, 0)] = 6;                 // column 0 already has a 6 at (3,0)
    CHECK_EQ(ledger_conflicts(&p, &b, f), 2);
    CHECK(!ledger_solved(&p, &b));
}

TEST(pack_unpack_round_trip_and_rejections)
{
    LedgerPuzzle p, q;
    fixture(&p);
    uint8_t buf[128];
    int len = ledger_pack(&p, buf);
    CHECK_EQ(len, ledger_payload_len(6));
    CHECK_EQ(len, 36);
    CHECK(ledger_unpack(buf, 6, &q));
    CHECK_MEM(q.given, p.given, 36);
    CHECK_MEM(q.solution, p.solution, 36);
    CHECK(!ledger_unpack(buf, 5, &q));
    buf[18] = 0x00;                               // solution nibble 0
    CHECK(!ledger_unpack(buf, 6, &q));
}

TEST(next_step_reaches_the_solution_and_refuses_wrong_cells)
{
    LedgerPuzzle p;
    fixture(&p);
    LedgerBoard b;
    ledger_board_init(&p, &b);
    int guard = 0;
    for (;;) {
        int cell, value;
        if (!ledger_next_step(&p, &b, &cell, &value)) break;
        CHECK_EQ(b.cell[cell], 0);
        CHECK_EQ(value, p.solution[cell]);
        b.cell[cell] = (uint8_t)value;
        if (++guard > 100) break;
    }
    CHECK(ledger_solved(&p, &b));
    ledger_board_init(&p, &b);
    int i = 0;
    while (p.given[i]) i++;
    b.cell[i] = (uint8_t)(p.solution[i] % 6 + 1);
    int cell, value;
    CHECK(!ledger_next_step(&p, &b, &cell, &value));
}

TEST(generator_output_is_valid_unique_deducible_and_distinct)
{
    Rng rng;
    rng_init(&rng, 9);
    HashSet seen;
    hs_init(&seen);
    int made = 0, attempts = 0;
    static const int sizes[3] = { 4, 6, 8 };
    while (made < 45 && attempts < 3000) {
        attempts++;
        GenRecord rec;
        int n = sizes[made % 3];
        if (!gen_ledger.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        CHECK_EQ(rec.hdr.family, FAM_LEDGER);
        CHECK_EQ(rec.hdr.size, n);
        CHECK(rec.hdr.difficulty >= DIFF_MIN && rec.hdr.difficulty <= DIFF_MAX);
        LedgerPuzzle p;
        CHECK(ledger_unpack(rec.payload, n, &p));
        LedgerBoard full;
        memcpy(full.cell, p.solution, n * n);
        CHECK(ledger_solved(&p, &full));
        CHECK_EQ(ledger_count_solutions(&p, 2), 1);
        LedgerSolveStats st;
        ledger_deduce(&p, NULL, &st, NULL);
        CHECK(st.solved);
        CHECK_EQ(ledger_difficulty(&p, &st), rec.hdr.difficulty);
    }
    CHECK_EQ(made, 45);
    CHECK_EQ(seen.count, 45);
    hs_free(&seen);
}

static uint8_t *img;
static int img_len;

static void with_ledger_bank(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/ledger.bin", "rb");
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

TEST(committed_ledger_bank_is_sound)
{
    with_ledger_bank();
    int count = bank_count(FAM_LEDGER);
    CHECK(count >= 300);
    int prev = 0;
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_LEDGER, i, &e));
        CHECK_EQ(e.hdr->family, FAM_LEDGER);
        CHECK(e.hdr->difficulty >= prev);
        prev = e.hdr->difficulty;
        LedgerPuzzle p;
        CHECK(ledger_unpack(e.payload, e.hdr->size, &p));
        LedgerSolveStats st;
        ledger_deduce(&p, NULL, &st, NULL);
        CHECK(st.solved);
        CHECK_EQ(ledger_difficulty(&p, &st), e.hdr->difficulty);
        CHECK_EQ(ledger_count_solutions(&p, 2), 1);
    }
}

TEST(adapter_cycles_symbols_draws_boxes_and_saves)
{
    with_ledger_bank();
    BankEntry e;
    CHECK(bank_get(FAM_LEDGER, 0, &e));
    CHECK(puzzle_ops(FAM_LEDGER) == &ops_ledger);
    CHECK(ops_ledger.load(e.hdr, e.payload));
    int n = ops_ledger.size();
    LedgerPuzzle p;
    ledger_unpack(e.payload, e.hdr->size, &p);
    int free_ = 0;
    while (p.given[free_]) free_++;
    int r = free_ / n, c = free_ % n;
    CellView v;
    ops_ledger.cell(0, 0, &v);
    CHECK((v.edges & 9) == 9);
    ops_ledger.cell(LEDGER_BOX_H - 1, ledger_box_w(n) - 1, &v);
    CHECK((v.edges & 6) == 6);

    ActionResult ar = ops_ledger.action(r, c, ACT_A);
    CHECK(ar.changed);
    ops_ledger.cell(r, c, &v);
    CHECK_EQ(v.mark, MARK_DIGIT1);
    ar = ops_ledger.action(r, c, ACT_B);
    ar = ops_ledger.action(r, c, ACT_B);              // empty -> n
    ops_ledger.cell(r, c, &v);
    CHECK_EQ(v.mark, MARK_DIGIT1 + n - 1);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_ledger.save(buf);
    ops_ledger.action(r, c, ACT_A);                    // back to empty
    ops_ledger.cell(r, c, &v);
    CHECK_EQ(v.mark, MARK_NONE);
    CHECK(ops_ledger.restore(buf, len));
    ops_ledger.cell(r, c, &v);
    CHECK_EQ(v.mark, MARK_DIGIT1 + n - 1);
    ops_ledger.action(r, c, ACT_A);
    int hr, hc, guard = 0;
    while (!ops_ledger.solved() && guard++ < 100) CHECK_EQ(ops_ledger.hint(&hr, &hc), HINT_APPLIED);
    CHECK(ops_ledger.solved());
}

const TestCase ledger_tests[] = {
    T(box_geometry),
    T(fixture_is_deducible_unique_and_consistent),
    T(conflicts_flag_duplicates_in_rows_columns_and_boxes),
    T(pack_unpack_round_trip_and_rejections),
    T(next_step_reaches_the_solution_and_refuses_wrong_cells),
    T(generator_output_is_valid_unique_deducible_and_distinct),
    T(committed_ledger_bank_is_sound),
    T(adapter_cycles_symbols_draws_boxes_and_saves),
};
SUITE(ledger)
