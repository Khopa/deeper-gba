#include "save.h"

// Every block starts with this header; the checksum covers the payload.
typedef struct {
    u32 magic;
    u16 version;
    u16 length;
    u32 checksum;
} BlockHeader;

typedef struct {
    RunState rs;
    RoomSave room;
} RunBlock;

static Profile profile;
static bool    run_valid;

// --- raw SRAM access (byte-wise, from IWRAM) ----------------------------------------

IWRAM_CODE static void sram_write(u32 off, const void *src, int len)
{
    const u8 *s = src;
    vu8 *d = (vu8 *)(MEM_SRAM + off);
    for (int i = 0; i < len; i++) d[i] = s[i];
}

IWRAM_CODE static void sram_read(u32 off, void *dst, int len)
{
    u8 *d = dst;
    const vu8 *s = (const vu8 *)(MEM_SRAM + off);
    for (int i = 0; i < len; i++) d[i] = s[i];
}

static u32 checksum(const void *data, int len)
{
    const u8 *p = data;
    u32 h = 2166136261u;
    for (int i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void block_write(u32 off, const void *payload, int len)
{
    BlockHeader h = { SAVE_MAGIC, SAVE_VERSION, (u16)len, checksum(payload, len) };
    sram_write(off, &h, sizeof h);
    sram_write(off + sizeof h, payload, len);
}

static bool block_read(u32 off, void *payload, int len)
{
    BlockHeader h;
    sram_read(off, &h, sizeof h);
    if (h.magic != SAVE_MAGIC || h.version != SAVE_VERSION || h.length != len) return false;
    sram_read(off + sizeof h, payload, len);
    return checksum(payload, len) == h.checksum;
}

static void block_invalidate(u32 off)
{
    BlockHeader h = { 0, 0, 0, 0 };
    sram_write(off, &h, sizeof h);
}

// --- profile ------------------------------------------------------------------------------

static void profile_defaults(void)
{
    memset(&profile, 0, sizeof profile);
    profile.sound = 1;
}

void save_init(void)
{
    REG_WAITCNT = (REG_WAITCNT & ~3) | WS_SRAM_8;      // 8 wait states for SRAM
    if (!block_read(SAVE_PROFILE_OFF, &profile, sizeof profile)) {
        profile_defaults();
        save_profile_commit();
    }
    RunBlock rb;
    run_valid = block_read(SAVE_RUN_OFF, &rb, sizeof rb);
}

Profile *save_profile(void) { return &profile; }

void save_profile_commit(void) { block_write(SAVE_PROFILE_OFF, &profile, sizeof profile); }

void save_profile_add_recent(int family, int index)
{
    profile.recent[profile.recent_head] = (u16)((family << 12) | (index & 0xFFF));
    profile.recent_head = (u8)((profile.recent_head + 1) % RECENT_MAX);
}

// --- run ------------------------------------------------------------------------------------

bool save_has_run(void) { return run_valid; }

void save_run_commit(const RunState *rs, const RoomSave *room)
{
    RunBlock rb;
    memset(&rb, 0, sizeof rb);
    rb.rs = *rs;
    if (room) rb.room = *room;
    rb.rs.room_in_progress = room && room->len ? 1 : 0;
    block_write(SAVE_RUN_OFF, &rb, sizeof rb);
    run_valid = true;
}

bool save_run_load(RunState *rs, RoomSave *room)
{
    RunBlock rb;
    if (!block_read(SAVE_RUN_OFF, &rb, sizeof rb)) { run_valid = false; return false; }
    *rs = rb.rs;
    if (room) {
        *room = rb.room;
        if (!rs->room_in_progress) room->len = 0;
    }
    return true;
}

void save_run_clear(void)
{
    block_invalidate(SAVE_RUN_OFF);
    run_valid = false;
}

// --- unlocks -------------------------------------------------------------------------------

u32 save_check_unlocks(void)
{
    u32 before = profile.powers;
    if (profile.best_depth >= 10) profile.powers |= 1u << POWER_LAMP;
    if (profile.best_depth >= 20) profile.powers |= 1u << POWER_TOUGH;
    if (profile.runs_won >= 1)    profile.powers |= 1u << POWER_SECOND_CHANCE;
    return profile.powers & ~before;
}
