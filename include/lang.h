// Player-facing strings. The font is upper-case only and has no accents, so
// every language is written without them (AE/OE/UE/SS for the German umlauts,
// no tilde in Spanish). Four languages: nothing is hard-coded in the screens.
#ifndef LANG_H
#define LANG_H

#include "common.h"

enum Lang { LANG_FR = 0, LANG_EN, LANG_ES, LANG_DE, LANG_COUNT };

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
    STR_ROOM_HEART,
    STR_KEY_B_ROCK,
    STR_KEY_A_MINE,     // "A MINER": fits the compact panel
    STR_KEY_L_HINT_SHORT,
    STR_HEART_REVEALED, // the picture is complete
    STR_MISTAKES,       // summary: "ERREURS"
    STR_SPEED_BONUS,    // "BONUS VITESSE"
    STR_PENALTY,        // "PENALITES"
    STR_CORE_AGAIN,     // the core was failed: a life lost, another try
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
    STR_HELP_HEART,
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
    STR_OPTIONS,         // title menu entry and options screen title
    STR_LANGUAGE,        // options: the language row
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
    STR_ITEM_POTION,
    STR_ITEM_POTION_DESC,
    STR_ITEM_KEY,
    STR_ITEM_KEY_DESC,
    STR_ITEM_ROPE,
    STR_ITEM_ROPE_DESC,
    STR_ITEM_BEER,
    STR_ITEM_BEER_DESC,
    STR_ITEM_BREAD,
    STR_ITEM_BREAD_DESC,
    STR_ITEM_HELMET,
    STR_ITEM_HELMET_DESC,
    STR_ITEM_KEYS,
    STR_ITEM_KEYS_DESC,
    STR_ITEM_BOOTS,
    STR_ITEM_BOOTS_DESC,
    STR_ROPE_USED,       // room message: the rope holds the room
    STR_HELMET_HELD,     // map message: the helmet took the blow
    STR_KEY_A_DIG,
    STR_KEY_B_MARK,
    STR_KEY_L_HINT,
    STR_SOLVED,
    STR_WRONG_DIG,      // hint refused: a dig is misplaced
    STR_NO_HINTS,
    STR_ORE_FOUND,      // "+%d MINERAI"
    STR_NEXT,
    STR_CAMP,           // map preview: rest stop
    STR_CRATES,         // "CAISSES": the crates node, screen title
    STR_FIGHT,          // "COMBAT": the fight node
    STR_WALL,           // "PAROI": the wall node
    STR_DIG_BANG,       // "CREUSE!"
    STR_WALL_HOLDS,     // "LA PAROI TIENT..."
    STR_WALL_BROKEN,    // "PERCEE!"
    STR_FOE_GOBLIN,
    STR_FOE_ORC,
    STR_FOE_TROLL,
    STR_FOE_DEMON,
    STR_DODGE,          // "ESQUIVE!"
    STR_FIGHT_WON,      // "TERRASSE!"
    STR_FIGHT_LOST,     // "ASSOMME! -1 VIE"
    STR_CRATES_PICK,    // "CHOISIS UNE CAISSE"
    STR_CRATE_EMPTY,    // "VIDE!"
    STR_CRATE_SMALL,    // "UN PEU DE MINERAI"
    STR_CRATE_BIG,      // "UN GROS FILON!"
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
    STR_RECORDS,        // title menu: the miner's logbook
    STR_RUNS,
    STR_RUNS_WON,
    STR_BEST_DEPTH,
    STR_BACK,
    STR_COUNT
};

void        lang_set(int lang);
const char *lang_name(int lang);   // "FRANCAIS", "ENGLISH", "ESPANOL", "DEUTSCH" (never translated)
int         lang_get(void);
const char *S(int id);

#endif
