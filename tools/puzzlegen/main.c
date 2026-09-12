// puzzlegen — build a puzzle bank for one family.
//
//   puzzlegen dig --count 400 --sizes 5,6,7,8 --seed 1 --out data/puzzles/dig.bin
//
// Options:
//   --count N        puzzles to produce (default 200)
//   --sizes a,b,...  grid sizes to cycle through (default: the family's range)
//   --seed S         RNG seed (default 1); the output is fully deterministic
//   --per-diff K     keep at most K puzzles per difficulty level (flattens the mix)
//   --max-attempts M give up after M attempts (default 2,000,000)
//   --dump           print every accepted puzzle as text
//   --out PATH       bank file to write (default: <family>.bin)
#include <stdlib.h>
#include <string.h>
#include "gen.h"
#include "dig.h"
#include "vein.h"
#include "ledger.h"

static const FamilyGen *families[] = { &gen_dig, &gen_vein, &gen_ledger };

void dump_record(FILE *f, const GenRecord *r)
{
    static const char *const names[FAM_COUNT] = { "dig", "vein", "block", "tunnel", "ledger", "nugget" };
    fprintf(f, "# %s %dx%d difficulty %d flags %d\n",
            names[r->hdr.family], r->hdr.size, r->hdr.size, r->hdr.difficulty, r->hdr.flags);
    if (r->hdr.family == FAM_DIG) {
        DigPuzzle p;
        dig_unpack(r->payload, r->hdr.size, &p);
        DigSolveStats st;
        dig_deduce(&p, NULL, &st);
        fprintf(f, "# steps single %d confine %d pairs %d trial %d\n",
                st.steps[DIGT_SINGLE], st.steps[DIGT_CONFINE], st.steps[DIGT_PAIRS], st.steps[DIGT_TRIAL]);
        for (int row = 0; row < p.n; row++) {
            for (int c = 0; c < p.n; c++) {
                int i = cell_at(p.n, row, c);
                fprintf(f, "%c%c", 'a' + p.region[i], p.solution[row] == c ? '*' : ' ');
            }
            fputc('\n', f);
        }
    }
    if (r->hdr.family == FAM_LEDGER) {
        LedgerPuzzle p;
        ledger_unpack(r->payload, r->hdr.size, &p);
        LedgerSolveStats st;
        ledger_deduce(&p, NULL, &st, NULL);
        fprintf(f, "# steps naked %d hidden %d locked %d\n", st.steps[LEDT_NAKED], st.steps[LEDT_HIDDEN], st.steps[LEDT_LOCKED]);
        for (int row = 0; row < p.n; row++) {
            for (int c = 0; c < p.n; c++) {
                int i = cell_at(p.n, row, c);
                fputc(p.given[i] ? '0' + p.given[i] : '.', f);
                fputc(c % ledger_box_w(p.n) == ledger_box_w(p.n) - 1 ? '|' : ' ', f);
            }
            fputc('\n', f);
            if (row % LEDGER_BOX_H == LEDGER_BOX_H - 1) fprintf(f, "%.*s\n", 2 * p.n, "----------------");
        }
    }
    if (r->hdr.family == FAM_VEIN) {
        VeinPuzzle p;
        vein_unpack(r->payload, r->hdr.size, &p);
        VeinSolveStats st;
        vein_deduce(&p, NULL, &st, NULL);
        fprintf(f, "# steps triple %d count %d trial %d\n", st.steps[VEINT_TRIPLE], st.steps[VEINT_COUNT], st.steps[VEINT_TRIAL]);
        for (int row = 0; row < p.n; row++) {
            for (int c = 0; c < p.n; c++) {
                int i = cell_at(p.n, row, c);
                fputc(p.given[i] == VEIN_LIGHT ? 'O' : p.given[i] == VEIN_DARK ? 'X' : '.', f);
                fputc(' ', f);
            }
            fputc('\n', f);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: puzzlegen <family> [options]\n");
        return 2;
    }
    const FamilyGen *fam = NULL;
    for (size_t i = 0; i < sizeof families / sizeof families[0]; i++)
        if (!strcmp(families[i]->name, argv[1])) fam = families[i];
    if (!fam) {
        fprintf(stderr, "unknown family %s\n", argv[1]);
        return 2;
    }

    int count = 200, per_diff = 0;
    long max_attempts = 2000000;
    uint64_t seed = 1;
    bool dump = false;
    char out_path[512];
    snprintf(out_path, sizeof out_path, "%s.bin", fam->name);
    int sizes[16], n_sizes = 0;

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--per-diff") && i + 1 < argc) per_diff = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-attempts") && i + 1 < argc) max_attempts = atol(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) snprintf(out_path, sizeof out_path, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--dump")) dump = true;
        else if (!strcmp(argv[i], "--sizes") && i + 1 < argc) {
            char *tok = strtok(argv[++i], ",");
            while (tok && n_sizes < 16) { sizes[n_sizes++] = atoi(tok); tok = strtok(NULL, ","); }
        } else {
            fprintf(stderr, "bad option %s\n", argv[i]);
            return 2;
        }
    }
    if (!n_sizes)
        for (int s = fam->min_size; s <= fam->max_size; s++) sizes[n_sizes++] = s;
    for (int i = 0; i < n_sizes; i++)
        if (sizes[i] < fam->min_size || sizes[i] > fam->max_size) {
            fprintf(stderr, "size %d out of range for %s\n", sizes[i], fam->name);
            return 2;
        }

    Rng rng;
    rng_init(&rng, seed);
    HashSet seen;
    hs_init(&seen);
    GenBank bank;
    gbank_init(&bank);

    int per_level[DIFF_MAX + 1] = {0}, per_size[PUZZLE_MAX_N + 1] = {0};
    long attempts = 0;
    int size_idx = 0;
    while (bank.count < count && attempts < max_attempts) {
        attempts++;
        GenRecord rec;
        int n = sizes[size_idx];
        size_idx = (size_idx + 1) % n_sizes;
        if (!fam->attempt(&rng, n, &seen, &rec)) continue;
        if (per_diff && per_level[rec.hdr.difficulty] >= per_diff) continue;
        per_level[rec.hdr.difficulty]++;
        per_size[n]++;
        gbank_add(&bank, &rec);
        if (dump) dump_record(stdout, &rec);
    }

    gbank_sort_by_difficulty(&bank);
    if (!gbank_write(&bank, out_path)) {
        fprintf(stderr, "cannot write %s\n", out_path);
        return 1;
    }
    fprintf(stderr, "%s: %d puzzles in %ld attempts -> %s\n", fam->name, bank.count, attempts, out_path);
    fprintf(stderr, "  difficulty:");
    for (int d = DIFF_MIN; d <= DIFF_MAX; d++) fprintf(stderr, " %d:%d", d, per_level[d]);
    fprintf(stderr, "\n  sizes:");
    for (int i = 0; i < n_sizes; i++) fprintf(stderr, " %d:%d", sizes[i], per_size[sizes[i]]);
    fprintf(stderr, "\n");

    gbank_free(&bank);
    hs_free(&seen);
    return bank.count < count ? 1 : 0;
}
