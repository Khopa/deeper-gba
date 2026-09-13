// Player-facing strings. The font is upper-case only and has no accents, so
// French strings are written without them. Two languages from the start so
// nothing is hard-coded in the screens.
#ifndef LANG_H
#define LANG_H

#include "common.h"

enum Lang { LANG_FR = 0, LANG_EN, LANG_COUNT };

enum StrId {
    STR_TITLE = 0,
    STR_PRESS_START,
    STR_CONTINUE,
    STR_NEW_RUN,
    STR_DEPTH,          // "PROF." / "DEPTH"
    STR_LIVES,
    STR_ORE,
    STR_HINTS,
    STR_ROOM_DIG,       // family names as room titles
    STR_ROOM_VEIN,
    STR_ROOM_BLOCK,
    STR_ROOM_TUNNEL,
    STR_ROOM_LEDGER,
    STR_ROOM_NUGGET,
    STR_HELP_DIG,       // one-line rule reminder per family
    STR_HELP_VEIN,
    STR_KEY_A_ORE,
    STR_KEY_B_ERASE,
    STR_HELP_LEDGER,
    STR_KEY_A_NEXT,
    STR_KEY_B_PREV,
    STR_HELP_TUNNEL,
    STR_KEY_B_BACK,
    STR_HELP_BLOCK,
    STR_KEY_A_PLACE,
    STR_KEY_B_NEXT_BLOCK,
    STR_KEY_R_TURN,
    STR_HELP_NUGGET,
    STR_KEY_A_BREAK,
    STR_BIOME_EARTH,
    STR_BIOME_ROCK,
    STR_BIOME_ICE,
    STR_BIOME_LAVA,
    STR_BIOME_CRYSTAL,
    STR_BIOME_CORE,
    STR_SOUND,
    STR_ON,
    STR_OFF,
    STR_LANG_PROMPT,     // language screen title
    STR_LENGTH_PROMPT,   // "LONGUEUR DE LA DESCENTE"
    STR_LAYERS,          // "PALIERS" (after the number)
    STR_LOCKED,
    STR_UNLOCK_LENGTH,   // "ATTEINDRE LE NOYAU A"  (followed by the shorter length)
    STR_TIME,            // "TEMPS"
    STR_BEST_TIME,       // "RECORD"
    STR_LAST_TIME,       // "DERNIER TEMPS"
    STR_NEW_RECORD,
    STR_LENGTH_UNLOCKED, // "NOUVELLE PROFONDEUR!"
    STR_SHOP,            // title menu entry
    STR_SHOP_TITLE,
    STR_SHOP_HELLO_1,
    STR_SHOP_HELLO_2,
    STR_SHOP_HELLO_3,
    STR_SHOP_HELLO_META,
    STR_SHOP_THANKS,
    STR_SHOP_TOO_POOR,
    STR_SHOP_SOLD_OUT,
    STR_SHOP_KEYS,
    STR_SOLD,
    STR_BANK,
    STR_ITEM_HINT,
    STR_ITEM_HINT_DESC,
    STR_ITEM_LIFE,
    STR_ITEM_LIFE_DESC,
    STR_ITEM_PROP,
    STR_ITEM_PROP_DESC,
    STR_ITEM_SATCHEL,
    STR_ITEM_SATCHEL_DESC,
    STR_ITEM_FLASK,
    STR_ITEM_FLASK_DESC,
    STR_ITEM_LANTERN,
    STR_ITEM_LANTERN_DESC,
    STR_ITEM_HELMET,
    STR_ITEM_HELMET_DESC,
    STR_ITEM_BEARD,
    STR_ITEM_BEARD_DESC,
    STR_PROP_USED,       // room message: a prop shores up the room
    STR_KEY_A_DIG,
    STR_KEY_B_MARK,
    STR_KEY_L_HINT,
    STR_SOLVED,
    STR_WRONG_DIG,      // hint refused: a dig is misplaced
    STR_NO_HINTS,
    STR_ORE_FOUND,      // "+%d MINERAI"
    STR_NEXT,
    STR_CAMP,           // map preview: rest stop
    STR_RISK,           // "RISQUE" tag
    STR_STABILITY,      // room panel: mistake budget
    STR_COLLAPSE,       // room lost: too many mistakes
    STR_PAUSE,          // pause modal title
    STR_RESUME,
    STR_GIVE_UP,        // "ABANDONNER (-1 VIE)"
    STR_SAVE_QUIT,      // "SAUVER ET QUITTER"
    STR_CONTROLS,       // help modal: "COMMANDES"
    STR_KEY_MOVE,
    STR_KEY_HELP,
    STR_KEY_PAUSE,
    STR_CLOSE,
    STR_CHOOSE,
    STR_CAMP_REST,      // "+1 VIE" message on the map
    STR_MAP_HELP,       // "< > CHOISIR   A DESCENDRE"
    STR_RUN_WON,
    STR_RUN_LOST,
    STR_TOTAL_ORE,
    STR_RECORDS,        // title menu: the miner's logbook (stats + powers)
    STR_RUNS,
    STR_RUNS_WON,
    STR_BEST_DEPTH,
    STR_POWERS,
    STR_POWER_LAMP,
    STR_POWER_TOUGH,
    STR_POWER_SECOND,
    STR_UNLOCK_LAMP,
    STR_UNLOCK_TOUGH,
    STR_UNLOCK_SECOND,
    STR_NEW_POWER,
    STR_BACK,
    STR_COUNT
};

void        lang_set(int lang);
const char *lang_name(int lang);   // "FRANCAIS", "ENGLISH" (never translated)
int         lang_get(void);
const char *S(int id);

#endif
