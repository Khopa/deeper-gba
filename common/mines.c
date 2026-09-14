#include "mines.h"

static uint32_t next_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}

void mines_init(Mines *m, uint32_t seed, int count)
{
    memset(m, 0, sizeof *m);
    m->seed = seed ? seed : 0x9E3779B9u;
    if (count < MINES_MIN) count = MINES_MIN;
    if (count > MINES_MAX) count = MINES_MAX;
    m->count = (uint8_t)count;
}

static bool near(int a, int b)
{
    int dr = a / MINES_N - b / MINES_N, dc = a % MINES_N - b % MINES_N;
    return dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1;
}

void mines_place(Mines *m, int safe)
{
    uint32_t s = m->seed;
    memset(m->pocket, 0, sizeof m->pocket);
    int placed = 0, guard = 0;
    while (placed < m->count && guard++ < 4096) {
        int i = (int)(next_rand(&s) % MINES_CELLS);
        if (m->pocket[i] || near(i, safe)) continue;
        m->pocket[i] = 1;
        placed++;
    }
    m->placed = 1;
}

int mines_adjacent(const Mines *m, int i)
{
    int r = i / MINES_N, c = i % MINES_N, k = 0;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            if (!dr && !dc) continue;
            int rr = r + dr, cc = c + dc;
            if (rr >= 0 && rr < MINES_N && cc >= 0 && cc < MINES_N) k += m->pocket[cell_at(MINES_N, rr, cc)];
        }
    return k;
}

static int bare(Mines *m, int i)
{
    if (m->cell[i] == MINES_BARE) return 0;
    m->cell[i] = MINES_BARE;
    int n = 1;
    if (mines_adjacent(m, i) == 0) {               // nothing around: the neighbours are safe too
        int r = i / MINES_N, c = i % MINES_N;
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                int rr = r + dr, cc = c + dc;
                if (rr >= 0 && rr < MINES_N && cc >= 0 && cc < MINES_N && !m->pocket[cell_at(MINES_N, rr, cc)])
                    n += bare(m, cell_at(MINES_N, rr, cc));
            }
    }
    return n;
}

int mines_break(Mines *m, int i)
{
    if (i < 0 || i >= MINES_CELLS || m->cell[i] == MINES_BARE) return 0;
    if (!m->placed) mines_place(m, i);
    if (m->pocket[i]) { m->cell[i] = MINES_BARE; return -1; }
    return bare(m, i);
}

bool mines_toggle_flag(Mines *m, int i)
{
    if (i < 0 || i >= MINES_CELLS || m->cell[i] == MINES_BARE) return false;
    m->cell[i] = m->cell[i] == MINES_FLAG ? MINES_HIDDEN : MINES_FLAG;
    return true;
}

bool mines_solved(const Mines *m)
{
    if (!m->placed) return false;
    for (int i = 0; i < MINES_CELLS; i++)
        if (!m->pocket[i] && m->cell[i] != MINES_BARE) return false;
    return true;
}

int mines_safe_cell(const Mines *m)
{
    if (!m->placed) return -1;
    int any = -1;
    for (int i = 0; i < MINES_CELLS; i++) {
        if (m->pocket[i] || m->cell[i] == MINES_BARE) continue;
        if (any < 0) any = i;
        int r = i / MINES_N, c = i % MINES_N;
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                int rr = r + dr, cc = c + dc;
                if (rr >= 0 && rr < MINES_N && cc >= 0 && cc < MINES_N && m->cell[cell_at(MINES_N, rr, cc)] == MINES_BARE)
                    return i;
            }
    }
    return any;
}

int mines_count_for(int difficulty)
{
    int k = MINES_MIN + difficulty / 3;            // 4 at 1..2, 5 at 3..5, 6 at 6..8, 7 at 9..10
    return k > MINES_MAX ? MINES_MAX : k;
}

int mines_save(const Mines *m, uint8_t *buf)
{
    int pb = (MINES_CELLS + 7) / 8;
    memset(buf, 0, MINES_SAVE_LEN);
    buf[0] = m->placed;
    for (int i = 0; i < MINES_CELLS; i++) {
        if (m->pocket[i]) buf[1 + (i >> 3)] |= (uint8_t)(1 << (i & 7));
        buf[1 + pb + (i >> 2)] |= (uint8_t)((m->cell[i] & 3) << ((i & 3) * 2));
    }
    return MINES_SAVE_LEN;
}

bool mines_restore(Mines *m, const uint8_t *buf, int len)
{
    if (len < MINES_SAVE_LEN) return false;
    int pb = (MINES_CELLS + 7) / 8;
    m->placed = buf[0] ? 1 : 0;
    int pockets = 0;
    for (int i = 0; i < MINES_CELLS; i++) {
        m->pocket[i] = (uint8_t)((buf[1 + (i >> 3)] >> (i & 7)) & 1);
        pockets += m->pocket[i];
        int v = (buf[1 + pb + (i >> 2)] >> ((i & 3) * 2)) & 3;
        if (v > MINES_FLAG) return false;
        m->cell[i] = (uint8_t)v;
    }
    if (m->placed && pockets != m->count) return false;
    if (!m->placed) { memset(m->pocket, 0, sizeof m->pocket); }
    return true;
}
