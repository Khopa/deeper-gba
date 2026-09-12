#include "lang.h"

static int current = LANG_FR;

static const char *const strings[LANG_COUNT][STR_COUNT] = {
    [LANG_FR] = {
        [STR_TITLE]       = "DEEPER",
        [STR_PRESS_START] = "APPUYEZ SUR START",
        [STR_CONTINUE]    = "CONTINUER",
        [STR_NEW_RUN]     = "NOUVELLE DESCENTE",
        [STR_DEPTH]       = "PROF.",
        [STR_LIVES]       = "VIES",
        [STR_ORE]         = "MINERAI",
        [STR_HINTS]       = "INDICES",
        [STR_ROOM_DIG]    = "FOUILLES",
        [STR_ROOM_VEIN]   = "FILONS",
        [STR_ROOM_BLOCK]  = "CAVITE",
        [STR_ROOM_TUNNEL] = "GALERIE",
        [STR_ROOM_LEDGER] = "CARNET",
        [STR_ROOM_NUGGET] = "PEPITES",
        [STR_HELP_DIG]    = "1 FOUILLE PAR LIGNE, COLONNE ET ROCHE. JAMAIS COTE A COTE.",
        [STR_KEY_A_DIG]   = "A CREUSER",
        [STR_KEY_B_MARK]  = "B MARQUER",
        [STR_KEY_L_HINT]  = "L INDICE",
        [STR_SOLVED]      = "SALLE DEGAGEE!",
        [STR_WRONG_DIG]   = "UNE FOUILLE EST MAL PLACEE",
        [STR_NO_HINTS]    = "PLUS D'INDICES",
        [STR_ORE_FOUND]   = "MINERAI TROUVE",
        [STR_NEXT]        = "A CONTINUER",
        [STR_CAMP]        = "CAMPEMENT: REPOS",
        [STR_RISK]        = "RISQUE",
        [STR_STABILITY]   = "STABILITE",
        [STR_COLLAPSE]    = "EBOULEMENT! -1 VIE",
        [STR_ABANDON_ASK] = "ABANDONNER LA SALLE? -1 VIE",
        [STR_YES_NO]      = "A OUI   B NON",
        [STR_CAMP_REST]   = "REPOS AU CAMPEMENT: +1 VIE",
        [STR_MAP_HELP]    = "< > CHOISIR    A DESCENDRE",
        [STR_RUN_WON]     = "LE NOYAU EST ATTEINT!",
        [STR_RUN_LOST]    = "LA DESCENTE S'ARRETE ICI",
        [STR_TOTAL_ORE]   = "MINERAI REMONTE",
    },
    [LANG_EN] = {
        [STR_TITLE]       = "DEEPER",
        [STR_PRESS_START] = "PRESS START",
        [STR_CONTINUE]    = "CONTINUE",
        [STR_NEW_RUN]     = "NEW DESCENT",
        [STR_DEPTH]       = "DEPTH",
        [STR_LIVES]       = "LIVES",
        [STR_ORE]         = "ORE",
        [STR_HINTS]       = "HINTS",
        [STR_ROOM_DIG]    = "DIG SITE",
        [STR_ROOM_VEIN]   = "VEINS",
        [STR_ROOM_BLOCK]  = "CAVITY",
        [STR_ROOM_TUNNEL] = "GALLERY",
        [STR_ROOM_LEDGER] = "LEDGER",
        [STR_ROOM_NUGGET] = "NUGGETS",
        [STR_HELP_DIG]    = "1 DIG PER ROW, COLUMN AND ROCK. NEVER TOUCHING.",
        [STR_KEY_A_DIG]   = "A DIG",
        [STR_KEY_B_MARK]  = "B MARK",
        [STR_KEY_L_HINT]  = "L HINT",
        [STR_SOLVED]      = "ROOM CLEARED!",
        [STR_WRONG_DIG]   = "A DIG IS MISPLACED",
        [STR_NO_HINTS]    = "NO HINTS LEFT",
        [STR_ORE_FOUND]   = "ORE FOUND",
        [STR_NEXT]        = "A CONTINUE",
        [STR_CAMP]        = "CAMP: REST",
        [STR_RISK]        = "RISK",
        [STR_STABILITY]   = "STABILITY",
        [STR_COLLAPSE]    = "CAVE-IN! -1 LIFE",
        [STR_ABANDON_ASK] = "LEAVE THE ROOM? -1 LIFE",
        [STR_YES_NO]      = "A YES   B NO",
        [STR_CAMP_REST]   = "REST AT THE CAMP: +1 LIFE",
        [STR_MAP_HELP]    = "< > CHOOSE    A DESCEND",
        [STR_RUN_WON]     = "THE CORE IS REACHED!",
        [STR_RUN_LOST]    = "THE DESCENT ENDS HERE",
        [STR_TOTAL_ORE]   = "ORE BROUGHT BACK",
    },
};

void lang_set(int lang) { current = clampi(lang, 0, LANG_COUNT - 1); }
int  lang_get(void) { return current; }

const char *S(int id)
{
    if (id < 0 || id >= STR_COUNT) return "";
    const char *s = strings[current][id];
    return s ? s : strings[LANG_EN][id];
}
