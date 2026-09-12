// DIG rules, packing, solver and generator properties.
#include "test.h"
#include "dig.h"
#include "gen.h"

// Fixture: a generated 6x6 with a unique solution (difficulty 6, needs trials).
static const char *FIX_ROWS[6] = {
    "bbaaaa",
    "bbbaac",
    "bbbaac",
    "bbdccc",
    "eeeeec",
    "eeffec",
};
static const uint8_t FIX_SOL[6] = { 4, 1, 5, 2, 0, 3 };

static void fixture(DigPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 6;
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 6; c++) p->region[cell_at(6, r, c)] = (uint8_t)(FIX_ROWS[r][c] - 'a');
    memcpy(p->solution, FIX_SOL, 6);
}

static void solved_board(const DigPuzzle *p, DigBoard *b)
{
    memset(b, 0, sizeof *b);
    for (int r = 0; r < p->n; r++) b->cell[cell_at(p->n, r, p->solution[r])] = DIG_DIG;
}

TEST(empty_board_is_clean_and_unsolved)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 0);
    CHECK(!dig_solved(&p, &b));
}

TEST(solution_board_is_solved)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    solved_board(&p, &b);
    uint8_t conf[PUZZLE_MAX_CELLS];
    CHECK_EQ(dig_conflicts(&p, &b, conf), 0);
    CHECK(dig_solved(&p, &b));
    CHECK_EQ(dig_count_digs(&b), 6);
}

TEST(same_row_conflicts_flag_both_digs)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    b.cell[cell_at(6, 0, 0)] = DIG_DIG;
    b.cell[cell_at(6, 0, 4)] = DIG_DIG;
    uint8_t conf[PUZZLE_MAX_CELLS];
    CHECK_EQ(dig_conflicts(&p, &b, conf), 2);
    CHECK(conf[0] && conf[4]);
    CHECK(!conf[1]);
}

TEST(same_column_and_region_conflict)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    b.cell[cell_at(6, 0, 0)] = DIG_DIG;
    b.cell[cell_at(6, 4, 0)] = DIG_DIG;       // same column
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 2);
    memset(&b, 0, sizeof b);
    b.cell[cell_at(6, 0, 2)] = DIG_DIG;       // region a
    b.cell[cell_at(6, 2, 4)] = DIG_DIG;       // region a, different row/col, not adjacent
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 2);
}

TEST(diagonal_neighbours_conflict)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    b.cell[cell_at(6, 1, 1)] = DIG_DIG;       // region b
    b.cell[cell_at(6, 2, 2)] = DIG_DIG;       // region b too, but adjacency alone must flag
    p.region[cell_at(6, 2, 2)] = 3;           // move it to another region for the test
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 2);
}

TEST(crosses_never_conflict_or_solve)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    solved_board(&p, &b);
    for (int i = 0; i < 36; i++) if (b.cell[i] == DIG_EMPTY) b.cell[i] = DIG_CROSS;
    CHECK(dig_solved(&p, &b));
    memset(&b, DIG_CROSS, sizeof b);
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 0);
    CHECK(!dig_solved(&p, &b));
}

TEST(wrong_solution_not_solved_even_without_conflicts)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    solved_board(&p, &b);
    b.cell[cell_at(6, 5, 3)] = DIG_EMPTY;     // five digs, no conflicts
    CHECK_EQ(dig_conflicts(&p, &b, NULL), 0);
    CHECK(!dig_solved(&p, &b));
}

TEST(pack_unpack_round_trip_every_size)
{
    for (int n = DIG_MIN_N; n <= DIG_MAX_N; n++) {
        DigPuzzle p, q;
        memset(&p, 0, sizeof p);
        p.n = (uint8_t)n;
        for (int i = 0; i < n * n; i++) p.region[i] = (uint8_t)((i * 7 + i / n) % n);
        for (int r = 0; r < n; r++) p.solution[r] = (uint8_t)((r * 3) % n);
        uint8_t buf[128];
        int len = dig_pack(&p, buf);
        CHECK_EQ(len, dig_payload_len(n));
        CHECK(dig_unpack(buf, n, &q));
        CHECK_EQ(q.n, n);
        CHECK_MEM(q.region, p.region, n * n);
        CHECK_MEM(q.solution, p.solution, n);
    }
}

TEST(unpack_rejects_bad_sizes_and_values)
{
    DigPuzzle q;
    uint8_t buf[128] = {0};
    CHECK(!dig_unpack(buf, 4, &q));
    CHECK(!dig_unpack(buf, 9, &q));
    buf[0] = 0x70;                            // region 7 in a 5x5 grid
    CHECK(!dig_unpack(buf, 5, &q));
}

TEST(fixture_has_unique_solution_and_is_deducible)
{
    DigPuzzle p;
    fixture(&p);
    CHECK_EQ(dig_count_solutions(&p, 3), 1);
    DigSolveStats st;
    dig_deduce(&p, NULL, &st);
    CHECK(st.solved);
    CHECK_EQ(st.hardest, DIGT_TRIAL);
    CHECK_EQ(dig_difficulty(&p, &st), 6);
    // the solver found the stored solution
    for (int r = 0; r < 6; r++) CHECK(st.placed & bit64(cell_at(6, r, FIX_SOL[r])));
}

TEST(regions_as_rows_has_many_solutions)
{
    DigPuzzle p;
    memset(&p, 0, sizeof p);
    p.n = 5;
    for (int i = 0; i < 25; i++) p.region[i] = (uint8_t)(i / 5);
    CHECK(dig_count_solutions(&p, 10) >= 10);
}

TEST(deduce_uses_crosses_and_digs_of_the_start_board)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    solved_board(&p, &b);
    b.cell[cell_at(6, 5, 3)] = DIG_EMPTY;     // last dig missing: one single left
    DigSolveStats st;
    dig_deduce(&p, &b, &st);
    CHECK(st.solved);
    CHECK_EQ(st.steps[DIGT_SINGLE], 1);
    CHECK_EQ(st.hardest, DIGT_SINGLE);
}

TEST(next_step_walks_to_the_solution)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    int guard = 0;
    for (;;) {
        int cell, kind;
        if (!dig_next_step(&p, &b, &cell, &kind)) break;
        CHECK(b.cell[cell] == DIG_EMPTY);
        if (kind == DIG_DIG) CHECK_EQ(p.solution[cell / 6], cell % 6);
        else                 CHECK(p.solution[cell / 6] != cell % 6);
        b.cell[cell] = (uint8_t)kind;
        if (++guard > 100) break;
    }
    CHECK(dig_solved(&p, &b));
}

TEST(next_step_refuses_a_wrong_dig)
{
    DigPuzzle p; DigBoard b;
    fixture(&p);
    memset(&b, 0, sizeof b);
    b.cell[cell_at(6, 0, 0)] = DIG_DIG;       // not in the solution
    int cell, kind;
    CHECK(!dig_next_step(&p, &b, &cell, &kind));
}

// --- generator properties ------------------------------------------------------------

TEST(generator_output_is_valid_unique_deducible_and_distinct)
{
    Rng rng;
    rng_init(&rng, 42);
    HashSet seen;
    hs_init(&seen);
    int made = 0, attempts = 0;
    while (made < 60 && attempts < 5000) {
        attempts++;
        GenRecord rec;
        int n = DIG_MIN_N + made % (DIG_MAX_N - DIG_MIN_N + 1);
        if (!gen_dig.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        CHECK_EQ(rec.hdr.family, FAM_DIG);
        CHECK_EQ(rec.hdr.size, n);
        CHECK(rec.hdr.difficulty >= DIFF_MIN && rec.hdr.difficulty <= DIFF_MAX);
        DigPuzzle p;
        CHECK(dig_unpack(rec.payload, n, &p));
        // every region non-empty, exactly one dig per region
        int per_region[PUZZLE_MAX_N] = {0}, dig_region[PUZZLE_MAX_N] = {0};
        for (int i = 0; i < n * n; i++) per_region[p.region[i]]++;
        for (int r = 0; r < n; r++) dig_region[p.region[cell_at(n, r, p.solution[r])]]++;
        for (int k = 0; k < n; k++) { CHECK(per_region[k] > 0); CHECK_EQ(dig_region[k], 1); }
        DigBoard b;
        solved_board(&p, &b);
        CHECK(dig_solved(&p, &b));
        CHECK_EQ(dig_count_solutions(&p, 2), 1);
        DigSolveStats st;
        dig_deduce(&p, NULL, &st);
        CHECK(st.solved);
        CHECK_EQ(dig_difficulty(&p, &st), rec.hdr.difficulty);
    }
    CHECK_EQ(made, 60);
    CHECK_EQ(seen.count, 60);
    hs_free(&seen);
}

TEST(generator_is_deterministic)
{
    GenRecord a, b;
    for (int pass = 0; pass < 2; pass++) {
        Rng rng;
        rng_init(&rng, 99);
        HashSet seen;
        hs_init(&seen);
        GenRecord *out = pass ? &b : &a;
        while (!gen_dig.attempt(&rng, 6, &seen, out)) {}
        hs_free(&seen);
    }
    CHECK_EQ(a.payload_len, b.payload_len);
    CHECK_MEM(a.payload, b.payload, a.payload_len);
    CHECK_EQ(a.canon_key, b.canon_key);
}

TEST(canonical_form_ignores_symmetries)
{
    DigPuzzle p;
    fixture(&p);
    uint8_t base[PUZZLE_MAX_CELLS], other[PUZZLE_MAX_CELLS], rotated[PUZZLE_MAX_CELLS];
    canon_labels(6, p.region, base);
    for (int t = 0; t < 8; t++) {
        for (int r = 0; r < 6; r++)
            for (int c = 0; c < 6; c++) {
                int tr, tc;
                canon_transform_cell(6, t, r, c, &tr, &tc);
                rotated[tr * 6 + tc] = p.region[r * 6 + c];
            }
        canon_labels(6, rotated, other);
        CHECK_MEM(other, base, 36);
    }
    // and a genuinely different grid gets a different form
    p.region[0] = 2;
    canon_labels(6, p.region, other);
    CHECK(memcmp(other, base, 36) != 0);
}

TEST(hash_set_detects_duplicates_and_grows)
{
    HashSet h;
    hs_init(&h);
    for (uint64_t k = 1; k <= 5000; k++) CHECK(hs_insert(&h, k * 0x9E3779B97F4A7C15ull));
    for (uint64_t k = 1; k <= 5000; k++) CHECK(!hs_insert(&h, k * 0x9E3779B97F4A7C15ull));
    CHECK_EQ(h.count, 5000);
    hs_free(&h);
}

const TestCase dig_tests[] = {
    T(empty_board_is_clean_and_unsolved),
    T(solution_board_is_solved),
    T(same_row_conflicts_flag_both_digs),
    T(same_column_and_region_conflict),
    T(diagonal_neighbours_conflict),
    T(crosses_never_conflict_or_solve),
    T(wrong_solution_not_solved_even_without_conflicts),
    T(pack_unpack_round_trip_every_size),
    T(unpack_rejects_bad_sizes_and_values),
    T(fixture_has_unique_solution_and_is_deducible),
    T(regions_as_rows_has_many_solutions),
    T(deduce_uses_crosses_and_digs_of_the_start_board),
    T(next_step_walks_to_the_solution),
    T(next_step_refuses_a_wrong_dig),
    T(generator_output_is_valid_unique_deducible_and_distinct),
    T(generator_is_deterministic),
    T(canonical_form_ignores_symmetries),
    T(hash_set_detects_duplicates_and_grows),
};
SUITE(dig)
