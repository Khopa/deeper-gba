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
    STR_KEY_B_TAKE,
    STR_KEY_R_NEXT_BLOCK,
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
    STR_ABANDON_ASK,    // "SELECT: ABANDONNER?" prompt
    STR_YES_NO,         // "A OUI  B NON"
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
int         lang_get(void);
const char *S(int id);

#endif
