// BLOCK shapes, rules, packing, search, generator, committed bank and adapter.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "block.h"
#include "gen.h"
#include "bank.h"
#include "puzzle.h"
#include "render.h"

// Fixture: 5x5 cavity with four rocks, five blocks (shapes 6 5 17 7 1),
// unique tiling shown by letters.
static const char *FIX[5] = {
    "aaabb",
    "a#bbc",
    "#ddcc",
    "e#ddc",
    "eed#c",
};

static void fixture(BlockPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = 5;
    BlockShape parts[5];
    memset(parts, 0, sizeof parts);
    int minr[5], minc[5];
    for (int k = 0; k < 5; k++) { minr[k] = 99; minc[k] = 99; }
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++) {
            char ch = FIX[r][c];
            if (ch == '#') continue;
            p->cavity[cell_at(5, r, c)] = 1;
            p->open_cells++;
            int k = ch - 'a';
            parts[k].r[parts[k].size] = (uint8_t)r;
            parts[k].c[parts[k].size] = (uint8_t)c;
            parts[k].size++;
            if (r < minr[k]) minr[k] = r;
            if (c < minc[k]) minc[k] = c;
        }
    p->count = 5;
    for (int k = 0; k < 5; k++) {
        int shape, orient;
        CHECK(block_shape_identify(&parts[k], &shape, &orient));
        p->shape[k] = (uint8_t)shape;
        p->sol_orient[k] = (uint8_t)orient;
        p->sol_anchor[k] = (uint8_t)cell_at(5, minr[k], minc[k]);
    }
}

TEST(catalogue_has_every_free_polyomino_of_size_3_to_5)
{
    int by_size[6] = {0}, total_orients = 0;
    for (int s = 0; s < BLOCK_SHAPES; s++) {
        BlockShape sh;
        block_shape_get(s, 0, &sh);
        by_size[sh.size]++;
        int no = block_shape_orients(s);
        CHECK(no == 1 || no == 2 || no == 4 || no == 8);
        total_orients += no;
        // every orientation identifies back to the same shape
        for (int o = 0; o < no; o++) {
            BlockShape t;
            block_shape_get(s, o, &t);
            int s2, o2;
            CHECK(block_shape_identify(&t, &s2, &o2));
            CHECK_EQ(s2, s);
            CHECK_EQ(o2, o);
        }
    }
    for (int s = 0; s < BLOCK_SHAPES; s++) CHECK_EQ(block_orient_count[s], block_shape_orients(s));
    CHECK_EQ(by_size[3], 2);
    CHECK_EQ(by_size[4], 5);
    CHECK_EQ(by_size[5], 12);
    CHECK_EQ(total_orients, 2 + 4 + 2 + 1 + 4 + 4 + 8 + 63);   // I3 L3 | I O T S L | 12 pentominoes: 63
    BlockShape bad = { .size = 3 };
    bad.r[0] = 0; bad.c[0] = 0; bad.r[1] = 1; bad.c[1] = 1; bad.r[2] = 2; bad.c[2] = 2;   // diagonal
    int s, o;
    CHECK(!block_shape_identify(&bad, &s, &o));
}

TEST(fixture_tiles_uniquely_and_solution_fits)
{
    BlockPuzzle p;
    fixture(&p);
    CHECK_EQ(p.open_cells, 21);
    long effort;
    CHECK_EQ(block_count_solutions(&p, 3, &effort), 1);
    CHECK(effort > 0);
    BlockBoard b;
    block_board_init(&b);
    for (int k = 0; k < p.count; k++) {
        CHECK(block_fits(&p, &b, k, p.sol_orient[k], p.sol_anchor[k]));
        b.placed[k] = 1;
        b.orient[k] = p.sol_orient[k];
        b.anchor[k] = p.sol_anchor[k];
    }
    CHECK(block_solved(&p, &b));
    CHECK_EQ(block_piece_at(&p, &b, cell_at(5, 0, 0)), 0);
    CHECK_EQ(block_piece_at(&p, &b, cell_at(5, 1, 1)), -1);
    CHECK_EQ(block_difficulty(&p, effort), 5);
}

TEST(fits_rejects_rock_overlap_and_overflow)
{
    BlockPuzzle p;
    fixture(&p);
    BlockBoard b;
    block_board_init(&b);
    CHECK(block_fits(&p, &b, 0, p.sol_orient[0], p.sol_anchor[0]));
    b.placed[0] = 1; b.orient[0] = p.sol_orient[0]; b.anchor[0] = p.sol_anchor[0];
    // piece 1 (shape 5: S tetromino) overlapping piece 0 at the origin
    CHECK(!block_fits(&p, &b, 1, 0, 0));
    // anchored so it sticks out of the area
    CHECK(!block_fits(&p, &b, 1, 0, cell_at(5, 4, 4)));
    // a piece over a rock at (1,1)
    CHECK(!block_fits(&p, &b, 2, 0, cell_at(5, 1, 1)));
    CHECK(!block_solved(&p, &b));
}

TEST(pack_unpack_round_trip_and_rejections)
{
    BlockPuzzle p, q;
    fixture(&p);
    uint8_t buf[128];
    int len = block_pack(&p, buf);
    CHECK_EQ(len, block_payload_len(&p));
    CHECK_EQ(len, 4 + 1 + 10);
    CHECK(block_unpack(buf, 5, len, &q));
    CHECK_EQ(q.count, 5);
    CHECK_EQ(q.open_cells, 21);
    CHECK_MEM(q.shape, p.shape, 5);
    CHECK_MEM(q.sol_anchor, p.sol_anchor, 5);
    CHECK(!block_unpack(buf, 5, len - 1, &q));      // truncated
    buf[6] ^= 0x07;                                 // orientation of piece 0
    CHECK(!block_unpack(buf, 5, len, &q));
}

TEST(next_step_places_the_solution_and_flags_misplaced_pieces)
{
    BlockPuzzle p;
    fixture(&p);
    BlockBoard b;
    block_board_init(&b);
    int piece, orient, anchor, guard = 0;
    while (block_next_step(&p, &b, &piece, &orient, &anchor) && guard++ < 10) {
        CHECK(!b.placed[piece]);
        CHECK(block_fits(&p, &b, piece, orient, anchor));
        b.placed[piece] = 1;
        b.orient[piece] = (uint8_t)orient;
        b.anchor[piece] = (uint8_t)anchor;
    }
    CHECK(block_solved(&p, &b));
    CHECK(!block_next_step(&p, &b, &piece, &orient, &anchor));
    CHECK_EQ(piece, -1);

    // place piece 1 somewhere legal but wrong: find any fit that is not the solution
    block_board_init(&b);
    bool placed = false;
    for (int a = 0; a < 25 && !placed; a++)
        for (int o = 0; o < block_shape_orients(p.shape[1]) && !placed; o++)
            if (block_fits(&p, &b, 1, o, a) && !(o == p.sol_orient[1] && a == p.sol_anchor[1])) {
                b.placed[1] = 1; b.orient[1] = (uint8_t)o; b.anchor[1] = (uint8_t)a;
                placed = true;
            }
    CHECK(placed);
    CHECK(!block_next_step(&p, &b, &piece, &orient, &anchor));
    CHECK_EQ(piece, 1);
}

TEST(generator_output_is_valid_unique_and_distinct)
{
    Rng rng;
    rng_init(&rng, 31);
    HashSet seen;
    hs_init(&seen);
    int made = 0, attempts = 0;
    while (made < 30 && attempts < 200000) {
        attempts++;
        GenRecord rec;
        int n = made % 3 == 2 ? 6 : 5;
        if (!gen_block.attempt(&rng, n, &seen, &rec)) continue;
        made++;
        CHECK_EQ(rec.hdr.family, FAM_BLOCK);
        CHECK_EQ(rec.hdr.size, n);
        CHECK(rec.hdr.difficulty >= DIFF_MIN && rec.hdr.difficulty <= DIFF_MAX);
        BlockPuzzle p;
        CHECK(block_unpack(rec.payload, n, rec.payload_len, &p));
        CHECK_EQ(rec.hdr.flags, p.count);
        CHECK(p.count >= 3 && p.count <= BLOCK_MAX_PIECES);
        long effort;
        CHECK_EQ(block_count_solutions(&p, 2, &effort), 1);
        CHECK_EQ(block_difficulty(&p, effort), rec.hdr.difficulty);
    }
    CHECK_EQ(made, 30);
    CHECK_EQ(seen.count, 30);
    hs_free(&seen);
}

static uint8_t *img;
static int img_len;

static void with_block_bank(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/block.bin", "rb");
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

TEST(committed_block_bank_is_sound)
{
    with_block_bank();
    int count = bank_count(FAM_BLOCK);
    CHECK(count >= 300);
    int prev = 0;
    for (int i = 0; i < count; i++) {
        BankEntry e;
        CHECK(bank_get(FAM_BLOCK, i, &e));
        CHECK_EQ(e.hdr->family, FAM_BLOCK);
        CHECK(e.hdr->difficulty >= prev);
        prev = e.hdr->difficulty;
        BlockPuzzle p;
        CHECK(block_unpack(e.payload, e.hdr->size, e.payload_len, &p));
        CHECK_EQ(e.payload_len, block_payload_len(&p));
        long effort;
        CHECK_EQ(block_count_solutions(&p, 2, &effort), 1);
        CHECK_EQ(block_difficulty(&p, effort), e.hdr->difficulty);
    }
}

extern int block_cur;
extern uint8_t block_orient[BLOCK_MAX_PIECES];

TEST(adapter_places_turns_takes_back_and_saves)
{
    with_block_bank();
    BankEntry e;
    CHECK(bank_get(FAM_BLOCK, 0, &e));
    CHECK(puzzle_ops(FAM_BLOCK) == &ops_block);
    CHECK(ops_block.load(e.hdr, e.payload));
    BlockPuzzle p;
    block_unpack(e.payload, e.hdr->size, e.payload_len, &p);
    int n = p.n;
    CHECK(ops_block.tray != NULL && ops_block.aux != NULL && ops_block.ghost != NULL);
    {   // the tray lists every piece: the first is in hand, none placed yet, the end is -1
        uint8_t rc[16];
        bool cur = false;
        CHECK(ops_block.tray(0, rc, 16, &cur) >= 3);
        CHECK(cur);
        for (int k = 1; k < p.count; k++) { CHECK(ops_block.tray(k, rc, 16, &cur) >= 3); CHECK(!cur); }
        CHECK_EQ(ops_block.tray(p.count, rc, 16, &cur), -1);
    }
    CHECK(!ops_block.immediate_mistakes);
    CHECK_EQ(block_cur, 0);

    // an empty cavity cell is a hole, rock is rock
    int hole = 0;
    while (!p.cavity[hole]) hole++;
    CellView v;
    ops_block.cell(hole / n, hole % n, &v);
    CHECK_EQ(v.variant, 1);

    // B cycles through the blocks still to place, R turns the current one
    ActionResult r = ops_block.action(0, 0, ACT_B);
    CHECK(r.changed);
    CHECK_EQ(block_cur, 1);
    for (int k = 2; k <= p.count; k++) ops_block.action(0, 0, ACT_B);
    CHECK_EQ(block_cur, 0);                       // count presses: wrapped round
    int no = block_shape_orients(p.shape[0]);
    for (int k = 0; k < no; k++) { CHECK_EQ(block_orient[0], k); ops_block.aux(); }
    CHECK_EQ(block_orient[0], 0);

    // play the solution: select each piece, turn it, place it at its anchor
    for (int j = 0; j < p.count; j++) {
        while (block_cur != j) CHECK(ops_block.action(0, 0, ACT_B).changed);
        while (block_orient[j] != p.sol_orient[j]) ops_block.aux();
        int ar = p.sol_anchor[j] / n, ac = p.sol_anchor[j] % n;
        uint8_t cells[16];
        bool fits;
        int count = ops_block.ghost(ar, ac, cells, &fits);
        CHECK(fits);
        CHECK(count >= 3);
        r = ops_block.action(ar, ac, ACT_A);
        CHECK(r.changed);
    }
    CHECK(ops_block.solved());
    CHECK_EQ(block_cur, -1);
    uint8_t buf[ROOM_STATE_MAX];
    int len = ops_block.save(buf);
    CHECK_EQ(len, 1 + 3 * p.count);

    // A on a placed block takes it back (nothing can be placed on a full board)
    int a0 = p.sol_anchor[0];
    int cells0[BLOCK_MAX_SIZE];
    block_piece_cells(&p, 0, p.sol_orient[0], a0, cells0);
    r = ops_block.action(cells0[0] / n, cells0[0] % n, ACT_A);
    CHECK(r.changed);
    CHECK(!ops_block.solved());
    CHECK_EQ(block_cur, 0);
    CHECK_EQ(block_orient[0], p.sol_orient[0]);
    ops_block.cell(cells0[0] / n, cells0[0] % n, &v);
    CHECK_EQ(v.variant, 1);
    // ghost at a wrong spot does not fit, at the right one it does; A re-places it
    bool fits;
    uint8_t gc[16];
    ops_block.ghost(n - 1, n - 1, gc, &fits);
    CHECK(!fits);
    r = ops_block.action(a0 / n, a0 % n, ACT_A);
    CHECK(r.changed && r.solved);

    // restore from the saved full board after emptying one piece
    ops_block.action(cells0[0] / n, cells0[0] % n, ACT_A);
    CHECK(ops_block.restore(buf, len));
    CHECK(ops_block.solved());
    CHECK(!ops_block.restore(buf, 2));

    // hints still work from scratch
    CHECK(ops_block.load(e.hdr, e.payload));
    int hr, hc, guard = 0;
    while (!ops_block.solved() && guard++ < 20) CHECK_EQ(ops_block.hint(&hr, &hc), HINT_APPLIED);
    CHECK(ops_block.solved());
}

TEST(every_bank_puzzle_can_be_played_through_the_adapter)
{
    with_block_bank();
    int count = bank_count(FAM_BLOCK);
    for (int i = 0; i < count; i++) {
        BankEntry e;
        bank_get(FAM_BLOCK, i, &e);
        CHECK(ops_block.load(e.hdr, e.payload));
        BlockPuzzle p;
        block_unpack(e.payload, e.hdr->size, e.payload_len, &p);
        int n = p.n;
        for (int j = 0; j < p.count; j++) {
            int guard = 0;
            while (block_cur != j && guard++ < 16) ops_block.action(0, 0, ACT_B);
            while (block_orient[j] != p.sol_orient[j]) ops_block.aux();
            ActionResult r = ops_block.action(p.sol_anchor[j] / n, p.sol_anchor[j] % n, ACT_A);
            if (!r.changed) { CHECK(r.changed); break; }
        }
        CHECK(ops_block.solved());
    }
}

const TestCase block_tests[] = {
    T(every_bank_puzzle_can_be_played_through_the_adapter),
    T(catalogue_has_every_free_polyomino_of_size_3_to_5),
    T(fixture_tiles_uniquely_and_solution_fits),
    T(fits_rejects_rock_overlap_and_overflow),
    T(pack_unpack_round_trip_and_rejections),
    T(next_step_places_the_solution_and_flags_misplaced_pieces),
    T(generator_output_is_valid_unique_and_distinct),
    T(committed_block_bank_is_sound),
    T(adapter_places_turns_takes_back_and_saves),
};
SUITE(block)
