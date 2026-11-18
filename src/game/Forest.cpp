#include "game/Forest.h"

#include <algorithm>

namespace journey {

void Forest::Init(const std::vector<TreeDef>& defs) {
    trees_.clear();
    trees_.reserve(defs.size());
    for (const TreeDef& d : defs) {
        TreeState s;
        s.def = d;
        trees_.push_back(s);
    }
}

void Forest::Update(float dt) {
    for (TreeState& t : trees_) {
        if (t.stump) {
            t.regrowLeft -= dt;
            if (t.regrowLeft <= 0.0f) {
                t.stump = false;
                t.regrowLeft = 0.0f;
                t.growAnim = 0.0f;   // pop back as a sapling and grow
            }
        } else if (t.growAnim < 1.0f) {
            t.growAnim = std::min(1.0f, t.growAnim + dt / kGrowAnimSeconds);
        }
    }
}

int Forest::IndexAtTile(TileCoord tile) const {
    for (size_t i = 0; i < trees_.size(); ++i)
        if (trees_[i].def.Tile() == tile) return static_cast<int>(i);
    return -1;
}

bool Forest::Fell(int index) {
    if (index < 0 || index >= static_cast<int>(trees_.size())) return false;
    TreeState& t = trees_[index];
    if (t.stump) return false;
    t.stump = true;
    t.regrowLeft = kRegrowSeconds;
    return true;
}

float Forest::SecondsUntilRegrow(int index) const {
    if (index < 0 || index >= static_cast<int>(trees_.size())) return 0.0f;
    return trees_[index].stump ? trees_[index].regrowLeft : 0.0f;
}

}  // namespace journey
