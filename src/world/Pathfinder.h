// A* on the tile grid. 8-way movement, no corner cutting. Pure C++.
#pragma once
#include <vector>

#include "world/TileMap.h"

namespace journey {

struct PathResult {
    bool found = false;
    std::vector<TileCoord> tiles;  // excludes the start tile, includes the goal
};

PathResult FindPath(const TileMap& map, TileCoord start, TileCoord goal);

// Picks where a click should send the player. A walkable tile is returned as-is. A blocked tile
// (a wall, a tree, a fountain) resolves to the closest walkable tile that can actually be reached.
// found == false when nothing reachable is within `maxRadius`.
PathResult FindPathToward(const TileMap& map, TileCoord start, TileCoord clicked, int maxRadius = 3);

}  // namespace journey
