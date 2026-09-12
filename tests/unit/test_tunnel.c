// TUNNEL rules, packing, search, generator, committed bank and adapter.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "tunnel.h"
#include "gen.h"
#include "bank.h"
#include "puzzle.h"
#include "render.h"

// Fixture: generated 5x5, two exits, five rocks, one gallery of 20 cells.
static const char *FIX[5] = {
    "#..#2",
    "#....",
    "....#",
    ".#...",
    ".1...",
};

static void fixture_cells(TunnelPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 5;
    int open = 0, exits = 0;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++) {
            char ch = FIX[r][c];
            int v = ch == '#' ? TUNNEL_ROCK : ch == '.' ? 0 : ch - '0';
            p->cell[cell_at(5, r, c)] = (uint8_t)v;
            if (v != TUNNEL_ROCK) open++;
            if (v && v != TUNNEL_ROCK && v > exits) exits = v;
        }
    p->open_cells = (uint8_t)open;
    p->exits = (uint8_t)exits;
}

// Brute force through the rules API: fills p->path with the unique gallery.
static bool search(const TunnelPuzzle *p, TunnelBoard *b, TunnelPuzzle *out)
{
    if (tunnel_solved(p, b)) { memcpy(out->path, b->path, b->len); return true; }
    int n = p->n;
    for (int c = 0; c < n * n; c++) {
        if (!tunnel_can_extend(p, b, c)) continue;
        tunnel_extend(b, c);
        if (search(p, b, out)) return true;
        tunnel_truncate(b, b->len - 1);
    }
    return false;
}

static void fixture(TunnelPuzzle *p)
{
    fixture_cells(p);
    TunnelBoard b;
    tunnel_board_init(p, &b);
    CHECK(search(p, &b, p));
}

TEST(fixture_has_exactly_one_gallery)
{
    TunnelPuzzle p;
    fixture(&p);
    long effort = 0;
    CHECK_EQ(tunnel_count_solutions(&p, 3, &effort), 1);
    CHECK(effort > 0);
    CHECK_EQ(p.path[0], cell_at(5, 4, 1));
    CHECK_EQ(p.path[p.open_cells - 1], cell_at(5, 0, 4));
    CHECK(tunnel_difficulty(&p, effort) >= 1);
}

TEST(rules_extension_order_and_solved)
{
    TunnelPuzzle p;
    fixture(&p);
    TunnelBoard b;
    tunnel_board_init(&p, &b);
    CHECK_EQ(b.len, 1);
    CHECK_EQ(b.path[0], cell_at(5, 4, 1));
    CHECK_EQ(tunnel_next_exit(&p, &b), 2);
    CHECK(!tunnel_can_extend(&p, &b, cell_at(5, 3, 1)));      // rock
    CHECK(!tunnel_can_extend(&p, &b, cell_at(5, 2, 1)));      // not adjacent
    CHECK(tunnel_can_extend(&p, &b, cell_at(5, 4, 0)));
    CHECK(tunnel_can_extend(&p, &b, cell_at(5, 4, 2)));
    tunnel_extend(&b, cell_at(5, 4, 2));
    CHECK(!tunnel_can_extend(&p, &b, cell_at(5, 4, 1)));      // revisit
    CHECK_EQ(tunnel_links(&p, &b, cell_at(5, 4, 1)), 2);      // east link only
    CHECK_EQ(tunnel_links(&p, &b, cell_at(5, 4, 2)), 8);
    CHECK_EQ(tunnel_links(&p, &b, cell_at(5, 0, 0)), 0);
    tunnel_truncate(&b, 1);
    CHECK_EQ(b.len, 1);
    tunnel_truncate(&b, 0);
    CHECK_EQ(b.len, 1);                                        // never below the start

    // the last exit cannot be entered before every cell is dug
    TunnelBoard almost;
    tunnel_board_init(&p, &almost);
    for (int i = 1; i < p.open_cells - 2; i++) tunnel_extend(&almost, p.path[i]);
    // head is now two cells before the end; the end is adjacent only if the
    // solution goes there next, so test with the solution's own order
    tunnel_extend(&almost, p.path[p.open_cells - 2]);
    CHECK(tunnel_can_extend(&p, &almost, p.path[p.open_cells - 1]));
    tunnel_extend(&almost, p.path[p.open_cells - 1]);
    CHECK(tunnel_solved(&p, &almost));

    // any other order that reaches the end early is refused
    TunnelBoard early;
    tunnel_board_init(&p, &early);
    CHECK(!tunnel_can_extend(&p, &early, cell_at(5, 0, 4)));
}

TEST(pack_unpack_round_trip_and_rejections)
{
    TunnelPuzzle p, q;
    fixture(&p);
    uint8_t buf[128];
    int len = tunnel_pack(&p, buf);
    CHECK_EQ(len, tunnel_payload_len(5));
    CHECK(tunnel_unpack(buf, 5, &q));
    CHECK_EQ(q.exits, 2);
    CHECK_EQ(q.open_cells, 20);
    CHECK_MEM(q.cell, p.cell, 25);
    CHECK_MEM(q.path, p.path, 20);
    CHECK(!tunnel_unpack(buf, 4, &q));
    buf[13] ^= 0x03;                                           // corrupt a step
    CHECK(!tunnel_unpack(buf, 5, &q));
}

TEST(next_step_follows_the_solution_and_spots_deviations)
{
    TunnelPuzzle p;
    fixture(&p);
    TunnelBoard b;
    tunnel_board_init(&p, &b);
    int cell, guard = 0;
    while (tunnel_next_step(&p, &b, &cell) && guard++ < 60) {
        CHECK(tunnel_can_extend(&p, &b, cell));
        tunnel_extend(&b, cell);
    }
    CHECK(tunnel_solved(&p, &b));
    CHECK(!tunnel_next_step(&p, &b, &cell));
    CHECK_EQ(cell, b.len);

    tunnel_board_init(&p, &b);
    int wrong = p.path[1] == cell_at(5, 4, 0) ? cell_at(5, 4, 2) : cell_at(5, 4, 0);
    tunnel_extend(&b, wrong);
    CHECK(!tunnel_next_step(&p, &b, &cell));
    CHECK_EQ(cell, 1);
}

TEST(generator_output_is_valid_unique_and_distinct)
{
    Rng rng;
    rng_init(&rng, 21);
    HashSet seen;
    hs_init(&seen);
    int made = 0, attempts = 0;
    while (made < 30 && attempts < 3000) {
        attempts++;
        GenRecord rec;
        int n = 5 + made % 3;
        if (!gen_tunnel.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        CHECK_EQ(rec.hdr.family, FAM_TUNNEL);
        CHECK_EQ(rec.hdr.size, n);
        CHECK(rec.hdr.difficulty >= DIFF_MIN && rec.hdr.difficulty <= DIFF_MAX);
        TunnelPuzzle p;
        CHECK(tunnel_unpack(rec.payload, n, &p));
        CHECK(p.exits >= 2 && p.exits <= TUNNEL_MAX_EXITS);
        CHECK_EQ(rec.hdr.flags, p.exits);
        long effort;
        CHECK_EQ(tunnel_count_solutions(&p, 2, &effort), 1);
        CHECK_EQ(tunnel_difficulty(&p, effort), rec.hdr.difficulty);
    }
    CHECK_EQ(made, 30);
    CHECK_EQ(seen.count, 30);
    hs_free(&seen);
}

static uint8_t *img;
static int img_len;

static void with_tunnel_bank(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/tunnel.bin", "rb");
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

TEST(committed_tunnel_bank_is_sound)
{
    with_tunnel_bank();
    int count = bank_count(FAM_TUNNEL);
    CHECK(count >= 300);
    int prev = 0;
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_TUNNEL, i, &e));
        CHECK_EQ(e.hdr->family, FAM_TUNNEL);
        CHECK(e.hdr->difficulty >= prev);
        prev = e.hdr->difficulty;
        TunnelPuzzle p;
        CHECK(tunnel_unpack(e.payload, e.hdr->size, &p));
        long effort;
        CHECK_EQ(tunnel_count_solutions(&p, 2, &effort), 1);
        CHECK_EQ(tunnel_difficulty(&p, effort), e.hdr->difficulty);
    }
}

TEST(adapter_digs_backs_up_truncates_and_saves)
{
    with_tunnel_bank();
    BankEntry e;
    CHECK(bank_get(FAM_TUNNEL, 0, &e));
    CHECK(puzzle_ops(FAM_TUNNEL) == &ops_tunnel);
    CHECK(ops_tunnel.load(e.hdr, e.payload));
    TunnelPuzzle p;
    tunnel_unpack(e.payload, e.hdr->size, &p);
    int n = p.n;
    CellView v;
    ops_tunnel.cell(p.path[0] / n, p.path[0] % n, &v);
    CHECK_EQ(v.variant, 1);                                    // the start is already dug
    CHECK_EQ(v.mark, MARK_DIGIT1);
    CHECK_EQ(v.pal, PAL_CELL_HILITE);                          // and is the head

    // follow the solution for three cells, then back up and re-dig
    for (int i = 1; i <= 3 && i < p.open_cells; i++) {
        ActionResult r = ops_tunnel.action(p.path[i] / n, p.path[i] % n, ACT_A);
        CHECK(r.changed && !r.mistake);
    }
    ops_tunnel.cell(p.path[1] / n, p.path[1] % n, &v);
    CHECK_EQ(v.variant, 1);
    CHECK(v.edges != 0);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_tunnel.save(buf);
    CHECK_EQ(len, 1 + 4);
    ActionResult r = ops_tunnel.action(0, 0, ACT_B);
    CHECK(r.changed);
    r = ops_tunnel.action(p.path[1] / n, p.path[1] % n, ACT_A);   // truncate back to cell 1
    CHECK(r.changed);
    ops_tunnel.cell(p.path[2] / n, p.path[2] % n, &v);
    CHECK_EQ(v.variant, 0);
    CHECK(ops_tunnel.restore(buf, len));
    ops_tunnel.cell(p.path[3] / n, p.path[3] % n, &v);
    CHECK_EQ(v.variant, 1);
    CHECK(!ops_tunnel.restore(buf, 2));

    // a rock next to the head is a mistake, a far cell is nothing
    int head = p.path[3];
    bool found_rock = false;
    for (int d = 0; d < 4 && !found_rock; d++) {
        static const int dr[4] = { -1, 0, 1, 0 }, dc[4] = { 0, 1, 0, -1 };
        int rr = head / n + dr[d], cc = head % n + dc[d];
        if (rr < 0 || rr >= n || cc < 0 || cc >= n) continue;
        if (p.cell[cell_at(n, rr, cc)] == TUNNEL_ROCK) {
            r = ops_tunnel.action(rr, cc, ACT_A);
            CHECK(!r.changed && r.mistake);
            found_rock = true;
        }
    }
    (void)found_rock;

    int hr, hc, guard = 0;
    while (!ops_tunnel.solved() && guard++ < 100) CHECK_EQ(ops_tunnel.hint(&hr, &hc), HINT_APPLIED);
    CHECK(ops_tunnel.solved());
}

const TestCase tunnel_tests[] = {
    T(fixture_has_exactly_one_gallery),
    T(rules_extension_order_and_solved),
    T(pack_unpack_round_trip_and_rejections),
    T(next_step_follows_the_solution_and_spots_deviations),
    T(generator_output_is_valid_unique_and_distinct),
    T(committed_tunnel_bank_is_sound),
    T(adapter_digs_backs_up_truncates_and_saves),
};
SUITE(tunnel)
