// The run map screen: five layers of the descent, the dwarf on the current
// node, the reachable rooms highlighted with a preview of what they hold.
#ifndef MAPSCREEN_H
#define MAPSCREEN_H

#include "common.h"
#include "run.h"

enum { MAP_RUNNING = 0, MAP_ARRIVED, MAP_QUIT };

void map_enter(RunState *rs);        // draw the map around the current node
int  map_update(RunState *rs);       // per frame; MAP_ARRIVED once run_go() happened

// Icon for a node (shared with the room header)
int  map_node_icon(const RunNode *n);

#endif
