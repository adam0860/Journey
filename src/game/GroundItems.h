// Items lying on the ground: what a dead animal drops, and what you drop from your bag. Pure C++ (unit tested).
//
// Everything on one tile forms a pile. Tap a pile to walk onto it and pick the items up. Piles disappear after a while.
#pragma once
#include <vector>

#include "game/Inventory.h"
#include "world/TileMap.h"

namespace journey {

struct Pile {
    int id = 0;
    TileCoord tile;
    std::vector<ItemStack> items;   // top of the pile first (the most recent drop)
    float age = 0.0f;               // seconds since the last item landed on it
};

class GroundItems {
public:
    static constexpr float kLifeSeconds = 240.0f;   // four minutes

    // Puts an item on a tile (joining the pile there if there is one). Returns the pile id.
    int Drop(TileCoord t, ItemStack stack);
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
