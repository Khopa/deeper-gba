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
    STR_KEY_A_DIG,
    STR_KEY_B_MARK,
    STR_KEY_L_HINT,
    STR_SOLVED,
    STR_WRONG_DIG,      // hint refused: a dig is misplaced
    STR_NO_HINTS,
    STR_ORE_FOUND,      // "+%d MINERAI"
    STR_NEXT,
    STR_COUNT
};

void        lang_set(int lang);
int         lang_get(void);
const char *S(int id);

#endif
