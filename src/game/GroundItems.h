// Items lying on the ground: what a dead animal drops, and what you drop from your bag. Pure C++ (unit tested).
//
// Everything on one tile forms a pile. Tap a pile to walk onto it and pick the items up. Every item vanishes 60 seconds after it
// landed (each one has its own timer, so dropping something new on an old pile does not keep the old things alive).
#pragma once
#include <vector>

#include "game/Inventory.h"
#include "world/TileMap.h"

namespace journey {

struct Pile {
    int id = 0;
    TileCoord tile;
    std::vector<ItemStack> items;   // top of the pile first (the most recent drop)
    std::vector<float> ages;        // seconds since each item landed (same order as items; GroundItems keeps them in step)
    std::vector<float> lives;       // how long each one may lie there (60 s, but what you drop when you die stays longer)
};

class GroundItems {
public:
    static constexpr float kLifeSeconds = 60.0f;        // how long an item lies on the ground
    static constexpr float kDeathLifeSeconds = 300.0f;  // ...except what a dying hero leaves behind: five minutes to come back for it

    // Puts an item on a tile (joining the pile there if there is one). Returns the pile id.
    int Drop(TileCoord t, ItemStack stack, float life = kLifeSeconds);
    void Update(float dt);

    const std::vector<Pile>& All() const { return piles_; }
    const Pile* Find(int id) const;
    const Pile* AtTile(TileCoord t) const;

    // Moves one item (by index in the pile) into the bag. Returns false if the bag is full or there is no such item.
    bool TakeOne(int pileId, int index, Inventory* bag, ItemStack* taken = nullptr);
    // Moves everything that fits into the bag. Returns what was taken (the pile keeps the rest).
    std::vector<ItemStack> TakeAll(int pileId, Inventory* bag);

private:
    Pile* FindMutable(int id);
    void RemoveIfEmpty(int id);

    std::vector<Pile> piles_;
    int nextId_ = 1;
};

}  // namespace journey
