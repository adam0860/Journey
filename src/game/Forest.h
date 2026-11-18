// Runtime state of every choppable tree: standing, or a stump that grows back after a few minutes.
// Pure C++ (unit tested). The map says where the trees are; this says which ones are cut right now.
#pragma once
#include <vector>

#include "world/TileMap.h"

namespace journey {

struct TreeState {
    TreeDef def;
    bool stump = false;
    float regrowLeft = 0.0f;   // seconds until a stump becomes a tree again
    float growAnim = 1.0f;     // 0..1, a freshly regrown tree scales up from a sapling
};

class Forest {
public:
    static constexpr float kRegrowSeconds = 180.0f;   // "a few minutes"
    static constexpr float kGrowAnimSeconds = 2.0f;

    void Init(const std::vector<TreeDef>& defs);
    void Update(float dt);

    const std::vector<TreeState>& Trees() const { return trees_; }
    // Index of the tree standing on this tile (stump or not), or -1.
    int IndexAtTile(TileCoord t) const;
    // Standing tree -> stump. Returns false if it was already a stump.
    bool Fell(int index);
    // Seconds until a stump regrows (0 if the tree is standing).
    float SecondsUntilRegrow(int index) const;

private:
    std::vector<TreeState> trees_;
};

}  // namespace journey
