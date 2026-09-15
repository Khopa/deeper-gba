#include "fightscreen.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "biome.h"
#include "foe.h"

// Layout: the monster fills the middle (a 64 px sprite shown 1.5x in a 128 px
// box), its hit points and attack bars under it on the pixel canvas, the key
// sequence on a dark band at the bottom with the cursor over the key to
// press, the fight's three hearts bottom left. Nothing else.
#define FOE_BOX_X   56
#define FOE_BOX_Y   0
#define BAR_X       60
#define BAR_W       120
#define HP_BAR_Y    114
#define ATK_BAR_Y   122
#define SEQ_TY      16
#define HEART_TY    19
#define END_FRAMES  70

uint8_t fight_seq[FOE_SEQ_MAX];
int fight_seq_len, fight_seq_pos, fight_foe_hp, fight_player_hp, fight_foe;

enum { FS_PLAY, FS_WON, FS_LOST };
static int state, timer, attack_left, msg_timer;
static FoeStats stats;
static uint32_t rng;
static int foe_str, ore;

static const int foe_names[FOE_COUNT] = { STR_FOE_GOBLIN, STR_FOE_ORC, STR_FOE_TROLL, STR_FOE_DEMON };
static const int key_icons[FK_COUNT] = { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_L, BTN_R };
static const u32 key_masks[FK_COUNT] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_L, KEY_R };

static void draw_bars(void)
{
    // hit points: red on dark; the attack bar: gold, draining
    int hp_w = stats.hp ? BAR_W * fight_foe_hp / stats.hp : 0;
    int atk_w = stats.attack_frames ? BAR_W * attack_left / stats.attack_frames : 0;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < BAR_W; x++) {
            canvas_plot(BAR_X + x, HP_BAR_Y + y, x < hp_w ? CANVAS_ALERT : CANVAS_LINE_DIM);
            canvas_plot(BAR_X + x, ATK_BAR_Y + y, x < atk_w ? CANVAS_LINE_LIT : CANVAS_LINE_DIM);
        }
}

static void draw_sequence(void)
{
    txt_clear_rect(0, SEQ_TY, TILES_W, 2);
    int x0 = (TILES_W - 2 * fight_seq_len) / 2;
    for (int i = 0; i < fight_seq_len; i++) button_icon(x0 + 2 * i, SEQ_TY, key_icons[fight_seq[i]]);
    if (state == FS_PLAY && fight_seq_pos < fight_seq_len)
        cursor_set_px((x0 + 2 * fight_seq_pos) * 8, SEQ_TY * 8, true);
    else
        cursor_set_px(0, 0, false);
}

static void draw_hearts(void)
{
    for (int i = 0; i < FOE_PLAYER_HP; i++)
        txt_puts(1 + i, HEART_TY, i < fight_player_hp ? "\x04" : "\x05", PAL_TXT_RED);
}

static void new_sequence(void)
{
    foe_sequence(&rng, stats.seq_len, fight_seq);
    fight_seq_len = stats.seq_len;
    fight_seq_pos = 0;
    draw_sequence();
}

static void message(int str_id, int pal)
{
    txt_clear_rect(4, HEART_TY, TILES_W - 4, 1);
    txt_puts_center(HEART_TY, S(str_id), pal);
    msg_timer = 60;
}

void fight_enter(const RunState *rs, const RunNode *node)
{
    int biome = biome_for_layer(rs->layer, rs->layers);
    rng = rs->seed ^ (0x7F4A7C15u * (rs->layer + 1)) ^ 0x2545F491u;
    fight_foe = foe_for_biome(biome, &rng);
    foe_stats(fight_foe, node->difficulty, &stats);
    fight_foe_hp = stats.hp;
    fight_player_hp = FOE_PLAYER_HP;
    attack_left = stats.attack_frames;
    ore = run_reward_ore(node);
    state = FS_PLAY;
    timer = msg_timer = 0;
    foe_str = foe_names[fight_foe];

    render_clear();
    render_set_biome(biome);
    music_play(MUS_FIGHT);
    modal_fill(0, 0, TILES_W, 2);
    modal_fill(0, SEQ_TY - 1, TILES_W, TILES_H - SEQ_TY + 1);
    txt_puts_center(0, S(foe_str), PAL_TXT_GOLD);
    canvas_clear();
    draw_bars();
    canvas_show(true);
    render_canvas_on_top(true);
    foe_show(fight_foe, FOE_BOX_X, FOE_BOX_Y, true);
    draw_hearts();
    new_sequence();
}

static void end_fight(int result)
{
    state = result;
    timer = 0;
    cursor_set_px(0, 0, false);
    txt_clear_rect(0, SEQ_TY, TILES_W, 2);
    txt_clear_rect(4, HEART_TY, TILES_W - 4, 1);
    txt_puts_center(SEQ_TY, S(result == FS_WON ? STR_FIGHT_WON : STR_FIGHT_LOST), result == FS_WON ? PAL_TXT_GOLD : PAL_TXT_RED);
    if (result == FS_WON) {
        char buf[8];
        int len = 0;
        buf[len++] = '+';
        if (ore >= 100) buf[len++] = (char)('0' + ore / 100);
        if (ore >= 10) buf[len++] = (char)('0' + (ore / 10) % 10);
        buf[len++] = (char)('0' + ore % 10);
        buf[len++] = 'M';
        buf[len] = 0;
        txt_puts_center(SEQ_TY + 1, buf, PAL_TXT_GOLD);
        sfx_play(SFX_SOLVED);
    } else {
        foe_show(fight_foe, FOE_BOX_X, FOE_BOX_Y, true);
        sfx_play(SFX_COLLAPSE);
    }
}

int fight_update(void)
{
    if (state != FS_PLAY) {
        if (++timer == END_FRAMES) { button_sprite(0, BTN_A, 13 * 8, HEART_TY * 8 - 4, true); txt_puts(16, HEART_TY, "OK", PAL_TXT_WHITE); }
        if (timer >= END_FRAMES && input_hit(KEY_A | KEY_START)) return state == FS_WON ? FIGHT_WON : FIGHT_LOST;
        return FIGHT_RUNNING;
    }
    if (msg_timer && --msg_timer == 0) { txt_clear_rect(4, HEART_TY, TILES_W - 4, 1); }

    // the attack bar drains; empty, the monster strikes
    if (--attack_left <= 0) {
        attack_left = stats.attack_frames;
        foe_lunge();
        if (foe_strike_lands(&rng, &stats)) {
            fight_player_hp--;
            draw_hearts();
            render_flash_frames(4);
            sfx_play(SFX_ERROR);
            if (fight_player_hp <= 0) { end_fight(FS_LOST); return FIGHT_RUNNING; }
        } else {
            message(STR_DODGE, PAL_TXT_WHITE);
            sfx_play(SFX_STEP);
        }
    }
    if ((attack_left & 3) == 0) draw_bars();

    // the keys: the expected one advances, any other of the eight resets
    for (int k = 0; k < FK_COUNT; k++) {
        if (!input_hit(key_masks[k])) continue;
        if (k == fight_seq[fight_seq_pos]) {
            fight_seq_pos++;
            sfx_play(SFX_MARK);
            if (fight_seq_pos >= fight_seq_len) {
                fight_foe_hp--;
                foe_hit();
                sfx_play(SFX_COLLECT);
                draw_bars();
                if (fight_foe_hp <= 0) { end_fight(FS_WON); return FIGHT_RUNNING; }
                new_sequence();
            } else draw_sequence();
        } else {
            fight_seq_pos = 0;
            sfx_play(SFX_ERROR);
            draw_sequence();
        }
        break;
    }
    return FIGHT_RUNNING;
}
