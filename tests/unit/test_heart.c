// HEART (the core's picture) rules, line solver, packing, generator, bank, adapter and run wiring.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "heart.h"
#include "gen.h"
#include "bank.h"
#include "puzzle.h"
#include "render.h"
#include "run.h"

// Fixture: a 10x10 drawing (a gem) that is solvable line by line
static const char *GEM[10] = {
    "....##....",
    "...####...",
    "..######..",
    ".########.",
    "##########",
    ".###..###.",
    "..######..",
    "...####...",
    "....##....",
    "....##....",
};

static void fixture(HeartPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 10;
    for (int r = 0; r < 10; r++)
        for (int c = 0; c < 10; c++) p->picture[cell_at(10, r, c)] = GEM[r][c] == '#';
}

TEST(clues_list_the_runs_in_order)
{
    HeartPuzzle p;
    fixture(&p);
    uint8_t cl[HEART_MAX_N];
    CHECK_EQ(heart_line_clues(&p, 5, cl), 2);          // ".###..###."
    CHECK_EQ(cl[0], 3);
    CHECK_EQ(cl[1], 3);
    CHECK_EQ(heart_line_clues(&p, 10 + 4, cl), 2);     // column 4: five ore, a gap, four ore
    CHECK_EQ(cl[0], 5);
    CHECK_EQ(cl[1], 4);
    CHECK_EQ(heart_line_clues(&p, 10 + 0, cl), 1);     // column 0: one cell
    CHECK_EQ(cl[0], 1);
    HeartPuzzle empty;
    memset(&empty, 0, sizeof empty);
    empty.n = 10;
    CHECK_EQ(heart_line_clues(&empty, 3, cl), 1);
    CHECK_EQ(cl[0], 0);
}

TEST(line_solver_settles_forced_cells_and_detects_contradictions)
{
    uint8_t cells[10];
    // a run of 8 in 10 cells: the middle six are forced
    uint8_t c8 = 8;
    memset(cells, HEART_UNKNOWN, 10);
    CHECK(heart_line_solve(10, &c8, 1, cells));
    CHECK_EQ(cells[0], HEART_UNKNOWN);
    CHECK_EQ(cells[1], HEART_UNKNOWN);
    for (int k = 2; k < 8; k++) CHECK_EQ(cells[k], HEART_ORE);
    CHECK_EQ(cells[9], HEART_UNKNOWN);
    // clue 0: everything rock
    uint8_t c0 = 0;
    memset(cells, HEART_UNKNOWN, 10);
    CHECK(heart_line_solve(10, &c0, 1, cells));
    for (int k = 0; k < 10; k++) CHECK_EQ(cells[k], HEART_ROCK);
    // 3 3 with a known ore at 4 (touching neither placement): still fine, the
    // middle gap is forced when both runs are pinned
    uint8_t c33[2] = { 3, 3 };
    memset(cells, HEART_UNKNOWN, 10);
    cells[0] = HEART_ORE;
    cells[9] = HEART_ORE;
    CHECK(heart_line_solve(10, c33, 2, cells));
    for (int k = 0; k < 3; k++) CHECK_EQ(cells[k], HEART_ORE);
    for (int k = 3; k < 7; k++) CHECK_EQ(cells[k], HEART_ROCK);
    for (int k = 7; k < 10; k++) CHECK_EQ(cells[k], HEART_ORE);
    // contradiction: ore where the clue says nothing fits
    memset(cells, HEART_UNKNOWN, 10);
    cells[0] = HEART_ORE;
    cells[1] = HEART_ROCK;
    uint8_t c2 = 2;
    CHECK(!heart_line_solve(10, &c2, 1, cells));
}

TEST(fixture_is_solvable_line_by_line_without_givens)
{
    HeartPuzzle p;
    fixture(&p);
    HeartBoard b;
    heart_board_init(&p, &b);
    int passes = 0;
    CHECK(heart_deduce(&p, &b, &passes));
    CHECK(passes >= 1);
    CHECK(heart_solved(&p, &b));
    for (int i = 0; i < 100; i++) CHECK_EQ(b.cell[i] == HEART_ORE, p.picture[i] != 0);
}

TEST(rules_conflicts_solved_and_line_done)
{
    HeartPuzzle p;
    fixture(&p);
    HeartBoard b;
    heart_board_init(&p, &b);
    CHECK(!heart_solved(&p, &b));
    CHECK(!heart_line_done(&p, &b, 0));
    b.cell[cell_at(10, 0, 4)] = HEART_ORE;
    b.cell[cell_at(10, 0, 5)] = HEART_ORE;
    CHECK(heart_line_done(&p, &b, 0));            // "2" satisfied
    b.cell[cell_at(10, 0, 0)] = HEART_ORE;         // wrong ore
    uint8_t conflict[HEART_MAX_CELLS];
    CHECK_EQ(heart_conflicts(&p, &b, conflict), 1);
    CHECK_EQ(conflict[0], 1);
    CHECK(!heart_line_done(&p, &b, 0));
    b.cell[0] = HEART_ROCK;                        // a rock note is never a conflict
    CHECK_EQ(heart_conflicts(&p, &b, NULL), 0);
    for (int i = 0; i < 100; i++) b.cell[i] = p.picture[i] ? HEART_ORE : HEART_UNKNOWN;
    CHECK(heart_solved(&p, &b));                   // rock need not be noted
}

TEST(pack_unpack_roundtrip_and_layout_limits)
{
    HeartPuzzle p, q;
    fixture(&p);
    p.given[7] = 1;
    p.given[99] = 1;
    uint8_t buf[128];
    CHECK_EQ(heart_pack(&p, buf), heart_payload_len(10));
    CHECK_EQ(heart_payload_len(10), 26);
    CHECK_EQ(heart_payload_len(15), 58);
    CHECK(heart_unpack(buf, 10, &q));
    CHECK_MEM(q.picture, p.picture, 100);
    CHECK_MEM(q.given, p.given, 100);
    CHECK(!heart_unpack(buf, 9, &q));
    CHECK(heart_fits_layout(&p));
    // a checkerboard row has too many runs for the screen
    HeartPuzzle busy;
    memset(&busy, 0, sizeof busy);
    busy.n = 10;
    for (int c = 0; c < 10; c += 2) busy.picture[c] = 1;
    CHECK(!heart_fits_layout(&busy));
    CHECK_EQ(heart_col_clue_rows(15), 4);
    CHECK_EQ(heart_col_clue_rows(12), 5);
    CHECK_EQ(heart_difficulty(10), 4);
    CHECK_EQ(heart_difficulty(15), 10);
}

TEST(generator_keeps_every_drawing_once_and_makes_it_line_solvable)
{
    Rng rng;
    rng_init(&rng, 3);
    HashSet seen;
    hs_init(&seen);
    int made = 0, per_size[HEART_MAX_N + 1] = {0};
    for (int attempt = 0; attempt < 600; attempt++) {
        GenRecord rec;
        int n = heart_sizes[attempt % HEART_SIZE_COUNT];
        if (!gen_heart.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        per_size[n]++;
        CHECK_EQ(rec.hdr.family, FAM_HEART);
        CHECK_EQ(rec.hdr.size, n);
        CHECK_EQ(rec.hdr.difficulty, heart_difficulty(n));
        CHECK_EQ(rec.payload_len, heart_payload_len(n));
        HeartPuzzle p;
        CHECK(heart_unpack(rec.payload, n, &p));
        CHECK(heart_fits_layout(&p));
        HeartBoard b;
        heart_board_init(&p, &b);
        CHECK(heart_deduce(&p, &b, NULL));         // unique: settled line by line
        CHECK(heart_solved(&p, &b));
        int givens = 0;
        for (int i = 0; i < n * n; i++) givens += p.given[i];
        CHECK_EQ(givens, rec.hdr.flags);
        CHECK(givens <= n);                        // a drawing needing more is a bad drawing
    }
    CHECK(made >= 6);                              // the placeholder drawings
    CHECK_EQ(made, seen.count);                    // each exactly once
    for (int s = 0; s < HEART_SIZE_COUNT; s++) CHECK(per_size[heart_sizes[s]] >= 1);
    hs_free(&seen);
}

// --- committed bank + adapter --------------------------------------------------------------

static uint8_t *img;
static int img_len;

static void with_heart_bank(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/heart.bin", "rb");
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

TEST(committed_heart_bank_has_every_size_and_solves)
{
    with_heart_bank();
    int count = bank_count(FAM_HEART);
    CHECK(count >= 6);
    int per_size[HEART_MAX_N + 1] = {0};
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_HEART, i, &e));
        CHECK_EQ(e.hdr->family, FAM_HEART);
        CHECK_EQ(e.payload_len, heart_payload_len(e.hdr->size));
        HeartPuzzle p;
        CHECK(heart_unpack(e.payload, e.hdr->size, &p));
        HeartBoard b;
        heart_board_init(&p, &b);
        CHECK(heart_deduce(&p, &b, NULL));
        per_size[e.hdr->size]++;
    }
    for (int s = 0; s < HEART_SIZE_COUNT; s++) {
        int first, last, d = heart_difficulty(heart_sizes[s]);
        bank_range(FAM_HEART, d - 1, d + 1, &first, &last);   // the run builder's first window
        CHECK_EQ(last - first, per_size[heart_sizes[s]]);     // ...holds exactly that size
        CHECK(per_size[heart_sizes[s]] >= 1);
    }
}

TEST(adapter_marks_notes_hints_conflicts_and_saves)
{
    with_heart_bank();
    BankEntry e;
    CHECK(bank_get(FAM_HEART, 0, &e));
    CHECK(puzzle_ops(FAM_HEART) == &ops_heart);
    CHECK(ops_heart.small_cells);
    CHECK(ops_heart.line_clues != NULL);
    CHECK(ops_heart.load(e.hdr, e.payload));
    int n = ops_heart.size();
    HeartPuzzle p;
    heart_unpack(e.payload, e.hdr->size, &p);
    int ore = -1, rock = -1;
    for (int i = 0; i < n * n; i++) {
        if (p.given[i]) continue;
        if (p.picture[i] && ore < 0) ore = i;
        if (!p.picture[i] && rock < 0) rock = i;
    }
    CHECK(ore >= 0 && rock >= 0);
    CellView v;
    ops_heart.cell(ore / n, ore % n, &v);
    CHECK_EQ(v.variant, 0);
    CHECK_EQ(v.mark, MARK_NONE);
    // A on an ore cell: ore, no mistake; A on a rock cell: a conflict the room will charge later
    ActionResult r = ops_heart.action(ore / n, ore % n, ACT_A);
    CHECK(r.changed && !r.mistake && !r.solved);
    ops_heart.cell(ore / n, ore % n, &v);
    CHECK_EQ(v.variant, 1);
    CHECK_EQ(v.pal, PAL_CELL_HILITE);
    r = ops_heart.action(rock / n, rock % n, ACT_A);
    CHECK(r.changed && r.mistake);
    ops_heart.cell(rock / n, rock % n, &v);
    CHECK_EQ(v.conflict, 1);
    CHECK_EQ(v.pal, PAL_CELL_CONFLICT);
    int hr, hc;
    CHECK_EQ(ops_heart.hint(&hr, &hc), HINT_WRONG_PLACEMENT);
    CHECK_EQ(cell_at(n, hr, hc), rock);
    // B turns it into a rock note: no conflict; B again clears it
    r = ops_heart.action(rock / n, rock % n, ACT_B);
    ops_heart.cell(rock / n, rock % n, &v);
    CHECK_EQ(v.variant, 2);
    CHECK_EQ(v.conflict, 0);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_heart.save(buf);
    CHECK_EQ(len, (n * n + 3) / 4);
    CHECK(len <= ROOM_STATE_MAX);
    r = ops_heart.action(rock / n, rock % n, ACT_B);
    ops_heart.cell(rock / n, rock % n, &v);
    CHECK_EQ(v.variant, 0);
    CHECK(ops_heart.restore(buf, len));
    ops_heart.cell(rock / n, rock % n, &v);
    CHECK_EQ(v.variant, 2);
    // clues: the line of the marked ore is not done yet, the whole picture solves it
    uint8_t cl[16];
    bool done;
    int count = ops_heart.line_clues(ore / n, cl, &done);
    CHECK(count >= 1);
    // hints reveal cells until the picture is complete
    int guard = 0;
    while (!ops_heart.solved() && guard++ < 200) CHECK_EQ(ops_heart.hint(&hr, &hc), HINT_APPLIED);
    CHECK(ops_heart.solved());
    CHECK_EQ(ops_heart.hint(&hr, &hc), HINT_NOTHING);
}

TEST(run_puts_the_heart_at_the_core_only_sized_by_length)
{
    bool avail[FAM_COUNT] = { [FAM_DIG] = true, [FAM_VEIN] = true, [FAM_HEART] = true };
    run_set_available_families(avail);
    for (int len = 0; len < 3; len++) {
        RunState rs;
        run_new(&rs, 100 + len, len, NULL, 0);
        int hearts = 0;
        for (int l = 0; l < rs.layers; l++)
            for (int s = 0; s < RUN_SLOTS; s++) {
                const RunNode *nd = &rs.node[l][s];
                if (!nd->present) continue;
                if (nd->family == FAM_HEART) {
                    hearts++;
                    CHECK_EQ(nd->kind, NODE_CORE);
                    CHECK_EQ(nd->difficulty, heart_difficulty(heart_sizes[len]));
                    CHECK_EQ(run_reward_ore(nd), (10 + nd->difficulty * 5) * 3);
                } else {
                    CHECK(nd->kind != NODE_CORE);
                }
            }
        CHECK_EQ(hearts, 1);
    }
    bool no_heart[FAM_COUNT] = { [FAM_DIG] = true };
    run_set_available_families(no_heart);
    RunState rs;
    run_new(&rs, 5, 0, NULL, 0);
    CHECK_EQ(rs.node[rs.layers - 1][1].family, FAM_DIG);   // without a bank the core stays a dig
}

const TestCase heart_tests[] = {
    T(clues_list_the_runs_in_order),
    T(line_solver_settles_forced_cells_and_detects_contradictions),
    T(fixture_is_solvable_line_by_line_without_givens),
    T(rules_conflicts_solved_and_line_done),
    T(pack_unpack_roundtrip_and_layout_limits),
    T(generator_keeps_every_drawing_once_and_makes_it_line_solvable),
    T(committed_heart_bank_has_every_size_and_solves),
    T(adapter_marks_notes_hints_conflicts_and_saves),
    T(run_puts_the_heart_at_the_core_only_sized_by_length),
};
SUITE(heart)
