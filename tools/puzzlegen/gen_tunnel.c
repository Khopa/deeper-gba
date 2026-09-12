// TUNNEL generator: pick a few rock cells, find a random gallery covering
// every open cell (randomised DFS with a fewest-exits-first heuristic), then
// add numbered exits along it until the gallery is the only one; finally
// try to drop exits again to leave the leanest set that stays unique.
#include <string.h>
#include "gen.h"
#include "tunnel.h"

static const int dr[4] = { -1, 0, 1, 0 }, dc[4] = { 0, 1, 0, -1 };

static int nb_of(int n, int cell, int d)
{
    int r = cell / n + dr[d], c = cell % n + dc[d];
    if (r < 0 || r >= n || c < 0 || c >= n) return -1;
    return cell_at(n, r, c);
}

static bool connected(const TunnelPuzzle *p)
{
    int n = p->n, cells = n * n;
    uint8_t seen[PUZZLE_MAX_CELLS] = {0}, stack[PUZZLE_MAX_CELLS];
    int start = -1;
    for (int i = 0; i < cells && start < 0; i++) if (p->cell[i] != TUNNEL_ROCK) start = i;
    if (start < 0) return false;
    int sp = 0, count = 0;
    seen[start] = 1;
    stack[sp++] = (uint8_t)start;
    while (sp) {
        int c = stack[--sp];
        count++;
        for (int d = 0; d < 4; d++) {
            int nb = nb_of(n, c, d);
            if (nb >= 0 && !seen[nb] && p->cell[nb] != TUNNEL_ROCK) { seen[nb] = 1; stack[sp++] = (uint8_t)nb; }
        }
    }
    return count == p->open_cells;
}

typedef struct {
    const TunnelPuzzle *p;
    Rng *rng;
    uint8_t visited[PUZZLE_MAX_CELLS];
    uint8_t path[PUZZLE_MAX_CELLS];
    long nodes;
} Walk;

static int onward(const Walk *w, int cell)
{
    int k = 0;
    for (int d = 0; d < 4; d++) {
        int nb = nb_of(w->p->n, cell, d);
        if (nb >= 0 && !w->visited[nb] && w->p->cell[nb] != TUNNEL_ROCK) k++;
    }
    return k;
}

static bool walk_rec(Walk *w, int cell, int len)
{
    if (++w->nodes > 60000) return false;
    w->path[len - 1] = (uint8_t)cell;
    if (len == w->p->open_cells) return true;
    int cand[4], score[4], k = 0;
    for (int d = 0; d < 4; d++) {
        int nb = nb_of(w->p->n, cell, d);
        if (nb < 0 || w->visited[nb] || w->p->cell[nb] != 0) continue;
        cand[k] = nb;
        score[k] = onward(w, nb) * 4 + rng_below(w->rng, 4);   // Warnsdorff with a random tie-break
        k++;
    }
    // sort by score ascending (fewest onward options first)
    for (int i = 1; i < k; i++)
        for (int j = i; j > 0 && score[j] < score[j - 1]; j--) {
            int t = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = t;
            t = score[j]; score[j] = score[j - 1]; score[j - 1] = t;
        }
    for (int i = 0; i < k; i++) {
        w->visited[cand[i]] = 1;
        if (walk_rec(w, cand[i], len + 1)) return true;
        w->visited[cand[i]] = 0;
    }
    return false;
}

static uint64_t tunnel_key(const TunnelPuzzle *p)
{
    int n = p->n, cells = n * n;
    uint8_t g[PUZZLE_MAX_CELLS], rev[PUZZLE_MAX_CELLS], a[PUZZLE_MAX_CELLS], b[PUZZLE_MAX_CELLS];
    memcpy(g, p->cell, cells);
    for (int i = 0; i < cells; i++)
        rev[i] = (g[i] && g[i] != TUNNEL_ROCK) ? (uint8_t)(p->exits + 1 - g[i]) : g[i];
    canon_values(n, g, a);
    canon_values(n, rev, b);
    const uint8_t *best = memcmp(a, b, cells) <= 0 ? a : b;
    return fnv1a64(best, cells) ^ ((uint64_t)n << 56) ^ ((uint64_t)FAM_TUNNEL << 48);
}

static bool tunnel_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    TunnelPuzzle p;
    memset(&p, 0, sizeof p);
    p.n = (uint8_t)n;
    int cells = n * n;

    // rocks: a few solid cells, never breaking the open region apart
    int rocks = rng_below(rng, n + 2);
    p.open_cells = (uint8_t)cells;
    for (int k = 0; k < rocks; k++) {
        int c = rng_below(rng, cells);
        if (p.cell[c] == TUNNEL_ROCK) continue;
        p.cell[c] = TUNNEL_ROCK;
        p.open_cells--;
        if (!connected(&p)) { p.cell[c] = 0; p.open_cells++; }
    }
    // parity: a Hamiltonian path on a chessboard-coloured grid needs the
    // colour counts to differ by at most one
    int black = 0, white = 0;
    for (int i = 0; i < cells; i++)
        if (p.cell[i] != TUNNEL_ROCK) { if (((i / n) + (i % n)) & 1) black++; else white++; }
    if (black - white > 1 || white - black > 1) return false;

    // random gallery
    Walk w;
    memset(&w, 0, sizeof w);
    w.p = &p;
    w.rng = rng;
    int start;
    do start = rng_below(rng, cells); while (p.cell[start] == TUNNEL_ROCK);
    w.visited[start] = 1;
    if (!walk_rec(&w, start, 1)) return false;
    memcpy(p.path, w.path, p.open_cells);

    // exits: ends first, then intermediate ones until unique
    p.cell[p.path[0]] = 1;
    p.cell[p.path[p.open_cells - 1]] = 2;
    p.exits = 2;
    long effort = 0;
    while (tunnel_count_solutions(&p, 2, &effort) != 1) {
        if (p.exits >= TUNNEL_MAX_EXITS) return false;
        // insert a new exit in the middle of the longest stretch without one
        int best_a = 0, best_len = 0;
        for (int i = 1, prev = 0; i < p.open_cells; i++)
            if (p.cell[p.path[i]]) {
                if (i - prev > best_len) { best_len = i - prev; best_a = prev; }
                prev = i;
            }
        if (best_len < 2) return false;
        int pos = best_a + best_len / 2;
        if (best_len > 3) pos += rng_below(rng, 3) - 1;
        if (pos <= best_a) pos = best_a + 1;
        if (pos >= best_a + best_len) pos = best_a + best_len - 1;
        // renumber: exits keep path order
        p.exits++;
        int num = 1;
        for (int i = 0; i < p.open_cells; i++) {
            if (i == pos) p.cell[p.path[i]] = 0xFE;       // placeholder
            if (p.cell[p.path[i]] && p.cell[p.path[i]] != TUNNEL_ROCK) p.cell[p.path[i]] = (uint8_t)num++;
        }
    }
    // lean pass: drop intermediate exits that are not needed (half the time)
    if (rng_below(rng, 2)) {
        for (int e = 2; e < p.exits; e++) {
            TunnelPuzzle t = p;
            for (int i = 0; i < cells; i++) {
                if (t.cell[i] == e) t.cell[i] = 0;
                else if (t.cell[i] > e && t.cell[i] != TUNNEL_ROCK) t.cell[i]--;
            }
            t.exits--;
            long eff;
            if (tunnel_count_solutions(&t, 2, &eff) == 1) { p = t; e--; }
        }
    }
    if (tunnel_count_solutions(&p, 2, &effort) != 1) return false;

    uint64_t key = tunnel_key(&p);
    if (!hs_insert(seen, key)) return false;

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_TUNNEL;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)tunnel_difficulty(&p, effort);
    out->hdr.flags = p.exits;
    out->payload_len = tunnel_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_tunnel = { "tunnel", FAM_TUNNEL, TUNNEL_MIN_N, TUNNEL_MAX_N, tunnel_attempt };
