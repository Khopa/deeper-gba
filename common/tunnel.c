// TUNNEL rules, packing and search. See tunnel.h.
#include "tunnel.h"

static const int dr[4] = { -1, 0, 1, 0 }, dc[4] = { 0, 1, 0, -1 };

static int neighbour(int n, int cell, int dir)
{
    int r = cell / n + dr[dir], c = cell % n + dc[dir];
    if (r < 0 || r >= n || c < 0 || c >= n) return -1;
    return cell_at(n, r, c);
}

int tunnel_step_dir(int n, int from, int to)
{
    for (int d = 0; d < 4; d++) if (neighbour(n, from, d) == to) return d;
    return -1;
}

static int exit_cell(const TunnelPuzzle *p, int number)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++) if (p->cell[i] == number) return i;
    return -1;
}

// --- rules ---------------------------------------------------------------------------

void tunnel_board_init(const TunnelPuzzle *p, TunnelBoard *b)
{
    memset(b, 0, sizeof *b);
    int start = exit_cell(p, 1);
    if (start >= 0) {
        b->path[0] = (uint8_t)start;
        b->len = 1;
    }
}

static bool on_path(const TunnelBoard *b, int cell)
{
    for (int i = 0; i < b->len; i++) if (b->path[i] == cell) return true;
    return false;
}

int tunnel_next_exit(const TunnelPuzzle *p, const TunnelBoard *b)
{
    int next = 1;
    for (int i = 0; i < b->len; i++)
        if (p->cell[b->path[i]] == next) next++;
    return next;
}

bool tunnel_can_extend(const TunnelPuzzle *p, const TunnelBoard *b, int cell)
{
    if (!b->len) return false;
    int head = b->path[b->len - 1];
    if (tunnel_step_dir(p->n, head, cell) < 0) return false;
    if (p->cell[cell] == TUNNEL_ROCK || on_path(b, cell)) return false;
    int v = p->cell[cell];
    if (v && v != tunnel_next_exit(p, b)) return false;
    // the last exit may only be entered as the final cell
    if (v == p->exits && b->len + 1 != p->open_cells) return false;
    return true;
}

void tunnel_extend(TunnelBoard *b, int cell)
{
    if (b->len < PUZZLE_MAX_CELLS) b->path[b->len++] = (uint8_t)cell;
}

void tunnel_truncate(TunnelBoard *b, int len)
{
    if (len < 1) len = 1;
    if (len < b->len) b->len = (uint8_t)len;
}

bool tunnel_solved(const TunnelPuzzle *p, const TunnelBoard *b)
{
    if (b->len != p->open_cells) return false;
    int next = 1;
    for (int i = 0; i < b->len; i++) {
        int c = b->path[i];
        if (p->cell[c] == TUNNEL_ROCK) return false;
        if (i && tunnel_step_dir(p->n, b->path[i - 1], c) < 0) return false;
        for (int j = 0; j < i; j++) if (b->path[j] == c) return false;
        if (p->cell[c]) {
            if (p->cell[c] != next) return false;
            next++;
        }
    }
    return next == p->exits + 1 && p->cell[b->path[b->len - 1]] == p->exits;
}

int tunnel_links(const TunnelPuzzle *p, const TunnelBoard *b, int cell)
{
    int bits = 0;
    for (int i = 0; i < b->len; i++) {
        if (b->path[i] != cell) continue;
        if (i > 0) bits |= 1 << tunnel_step_dir(p->n, cell, b->path[i - 1]);
        if (i + 1 < b->len) bits |= 1 << tunnel_step_dir(p->n, cell, b->path[i + 1]);
    }
    return bits;
}

// --- packing --------------------------------------------------------------------------

int tunnel_payload_len(int n) { return (n * n + 1) / 2 + (n * n - 1 + 3) / 4; }

int tunnel_pack(const TunnelPuzzle *p, uint8_t *out)
{
    int n = p->n, cells = n * n, len = tunnel_payload_len(n);
    memset(out, 0, len);
    for (int i = 0; i < cells; i++) out[i >> 1] |= (uint8_t)((p->cell[i] & 15) << ((i & 1) * 4));
    uint8_t *steps = out + (cells + 1) / 2;
    for (int i = 0; i + 1 < p->open_cells; i++) {
        int d = tunnel_step_dir(n, p->path[i], p->path[i + 1]);
        steps[i >> 2] |= (uint8_t)((d & 3) << ((i & 3) * 2));
    }
    return len;
}

bool tunnel_unpack(const uint8_t *in, int n, TunnelPuzzle *p)
{
    if (n < TUNNEL_MIN_N || n > TUNNEL_MAX_N) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n, exits = 0, open = 0;
    for (int i = 0; i < cells; i++) {
        int v = (in[i >> 1] >> ((i & 1) * 4)) & 15;
        if (v > TUNNEL_MAX_EXITS && v != TUNNEL_ROCK) return false;
        p->cell[i] = (uint8_t)v;
        if (v != TUNNEL_ROCK) open++;
        if (v && v != TUNNEL_ROCK && v > exits) exits = v;
    }
    p->exits = (uint8_t)exits;
    p->open_cells = (uint8_t)open;
    for (int e = 1; e <= exits; e++) if (exit_cell(p, e) < 0) return false;
    // rebuild the path from the steps
    const uint8_t *steps = in + (cells + 1) / 2;
    int cur = exit_cell(p, 1);
    if (cur < 0) return false;
    p->path[0] = (uint8_t)cur;
    for (int i = 0; i + 1 < open; i++) {
        int d = (steps[i >> 2] >> ((i & 3) * 2)) & 3;
        cur = neighbour(n, cur, d);
        if (cur < 0) return false;
        p->path[i + 1] = (uint8_t)cur;
    }
    TunnelBoard b;
    b.len = (uint8_t)open;
    memcpy(b.path, p->path, open);
    return tunnel_solved(p, &b);
}

// --- search ---------------------------------------------------------------------------

typedef struct {
    const TunnelPuzzle *p;
    int n, cap, found;
    long nodes;
    uint8_t visited[PUZZLE_MAX_CELLS];
    int next_exit;
    int end_cell;
} Search;

// Pruning: every unvisited open cell must keep at least two ways in/out
// (one for the end cell), counting the head as a neighbour; and the
// unvisited cells must form one connected region reachable from the head.
static bool viable(Search *s, int head, int remaining)
{
    int n = s->n, cells = n * n;
    if (!remaining) return true;
    uint8_t stack[PUZZLE_MAX_CELLS], seen[PUZZLE_MAX_CELLS];
    memset(seen, 0, cells);
    int sp = 0, reached = 0;
    for (int d = 0; d < 4; d++) {
        int nb = neighbour(n, head, d);
        if (nb >= 0 && !s->visited[nb] && s->p->cell[nb] != TUNNEL_ROCK && !seen[nb]) {
            seen[nb] = 1;
            stack[sp++] = (uint8_t)nb;
        }
    }
    while (sp) {
        int c = stack[--sp];
        reached++;
        int ways = 0;
        for (int d = 0; d < 4; d++) {
            int nb = neighbour(n, c, d);
            if (nb < 0 || s->p->cell[nb] == TUNNEL_ROCK) continue;
            if (nb == head) { ways++; continue; }
            if (s->visited[nb]) continue;
            ways++;
            if (!seen[nb]) { seen[nb] = 1; stack[sp++] = (uint8_t)nb; }
        }
        if (ways < (c == s->end_cell ? 1 : 2)) return false;
    }
    return reached == remaining;
}

#define TUNNEL_NODE_LIMIT 200000L   // give up (report "not unique") beyond this

static void search_rec(Search *s, int head, int remaining)
{
    if (s->found >= s->cap) return;
    if (++s->nodes > TUNNEL_NODE_LIMIT) { s->found = s->cap; return; }
    if (!remaining) {
        if (head == s->end_cell && s->next_exit == s->p->exits + 1) s->found++;
        return;
    }
    if (!viable(s, head, remaining)) return;
    for (int d = 0; d < 4; d++) {
        int nb = neighbour(s->n, head, d);
        if (nb < 0 || s->visited[nb] || s->p->cell[nb] == TUNNEL_ROCK) continue;
        int v = s->p->cell[nb];
        if (v && v != s->next_exit) continue;
        if (v == s->p->exits && remaining != 1) continue;
        s->visited[nb] = 1;
        if (v) s->next_exit++;
        search_rec(s, nb, remaining - 1);
        if (v) s->next_exit--;
        s->visited[nb] = 0;
    }
}

int tunnel_count_solutions(const TunnelPuzzle *p, int cap, long *effort)
{
    Search s;
    memset(&s, 0, sizeof s);
    s.p = p;
    s.n = p->n;
    s.cap = cap;
    int start = exit_cell(p, 1);
    s.end_cell = exit_cell(p, p->exits);
    if (start < 0 || s.end_cell < 0) return 0;
    s.visited[start] = 1;
    s.next_exit = 2;
    search_rec(&s, start, p->open_cells - 1);
    if (effort) *effort = s.nodes;
    return s.found;
}

bool tunnel_next_step(const TunnelPuzzle *p, const TunnelBoard *b, int *cell)
{
    for (int i = 0; i < b->len; i++)
        if (b->path[i] != p->path[i]) { *cell = i; return false; }
    if (b->len >= p->open_cells) { *cell = b->len; return false; }
    *cell = p->path[b->len];
    return true;
}

// --- difficulty ----------------------------------------------------------------------------

int tunnel_difficulty(const TunnelPuzzle *p, long effort)
{
    // Search effort (nodes visited by the pruned solver) is the proxy for
    // how much look-ahead the gallery needs; sparse exits add planning.
    static const long band[] = { 40, 70, 110, 170, 280, 450, 800, 1500, 4000 };
    int d = 1;
    for (int i = 0; i < (int)(sizeof band / sizeof band[0]); i++) if (effort > band[i]) d++;
    int per_exit = p->open_cells / (p->exits > 1 ? p->exits : 1);
    if (per_exit >= 10) d++;
    return clamp_difficulty(d);
}
