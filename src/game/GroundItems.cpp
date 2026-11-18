#include "game/GroundItems.h"

#include <algorithm>

namespace journey {

int GroundItems::Drop(TileCoord t, ItemStack stack) {
    if (stack.item == Item::None || stack.count <= 0) return -1;
    Pile* pile = nullptr;
    for (Pile& p : piles_)
        if (p.tile == t) pile = &p;
    if (!pile) {
        Pile p;
        p.id = nextId_++;
        p.tile = t;
        piles_.push_back(p);
        pile = &piles_.back();
    }
    bool merged = false;
    if (GetItemInfo(stack.item).stackable) {
        for (ItemStack& s : pile->items)
            if (s.item == stack.item) {
                s.count += stack.count;
                merged = true;
            }
    }
    if (!merged) pile->items.insert(pile->items.begin(), stack);
    pile->age = 0.0f;
    return pile->id;
}

void GroundItems::Update(float dt) {
    for (Pile& p : piles_) p.age += dt;
    piles_.erase(std::remove_if(piles_.begin(), piles_.end(), [](const Pile& p) { return p.age >= kLifeSeconds; }), piles_.end());
}

const Pile* GroundItems::Find(int id) const {
    for (const Pile& p : piles_)
        if (p.id == id) return &p;
    return nullptr;
}

Pile* GroundItems::FindMutable(int id) {
    for (Pile& p : piles_)
        if (p.id == id) return &p;
    return nullptr;
}

const Pile* GroundItems::AtTile(TileCoord t) const {
    for (const Pile& p : piles_)
        if (p.tile == t) return &p;
    return nullptr;
}

void GroundItems::RemoveIfEmpty(int id) {
    piles_.erase(std::remove_if(piles_.begin(), piles_.end(), [id](const Pile& p) { return p.id == id && p.items.empty(); }),
                 piles_.end());
}

bool GroundItems::TakeOne(int pileId, int index, Inventory* bag, ItemStack* taken) {
    Pile* p = FindMutable(pileId);
    if (!p || index < 0 || index >= static_cast<int>(p->items.size())) return false;
    ItemStack s = p->items[index];
    const int added = bag->Add(s.item, s.count);
    if (added <= 0) return false;
    if (taken) *taken = {s.item, added};
    if (added >= s.count) p->items.erase(p->items.begin() + index);
    else p->items[index].count -= added;
    RemoveIfEmpty(pileId);
    return true;
}

std::vector<ItemStack> GroundItems::TakeAll(int pileId, Inventory* bag) {
    std::vector<ItemStack> out;
    const Pile* p = Find(pileId);
    if (!p) return out;
    // Walk the pile from the top; an item that does not fit stays, the rest are still taken.
    size_t i = 0;
    while (true) {
        const Pile* now = Find(pileId);
        if (!now || i >= now->items.size()) break;
        const size_t before = now->items.size();
        ItemStack got;
        if (TakeOne(pileId, static_cast<int>(i), bag, &got)) out.push_back(got);
        const Pile* after = Find(pileId);
        if (!after) break;                       // that was the last item
        if (after->items.size() == before) ++i;  // still there (partly taken, or no room): look at the next one
    }
    return out;
}

}  // namespace journey
