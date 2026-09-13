// The merchant's counter: the shopkeeper dwarf, his line of the moment, the
// goods of the current mode with prices, the balance. UP/DOWN browse, A buys,
// B leaves.
#ifndef SHOPSCREEN_H
#define SHOPSCREEN_H

#include "common.h"
#include "shop.h"

enum { SHOPSCREEN_RUNNING = 0, SHOPSCREEN_LEAVE };

void shop_enter(int mode, Profile *p, RunState *rs);   // rs may be NULL in SHOP_META
int  shop_update(void);
bool shop_bought_something(void);                      // since shop_enter (caller saves)

#endif
