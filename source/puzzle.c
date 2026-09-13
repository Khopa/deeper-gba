// Family registry: which adapter drives which family.
#include "puzzle.h"

const PuzzleOps *puzzle_ops(int family)
{
    switch (family) {
    case FAM_DIG:  return &ops_dig;
    case FAM_VEIN: return &ops_vein;
    case FAM_LEDGER: return &ops_ledger;
    case FAM_TUNNEL: return &ops_tunnel;
    case FAM_BLOCK: return &ops_block;
    case FAM_NUGGET: return &ops_nugget;
    case FAM_HEART: return &ops_heart;
    default:       return NULL;
    }
}
