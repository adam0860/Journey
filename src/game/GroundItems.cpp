#include "game/GroundItems.h"

#include <algorithm>

namespace journey {

int GroundItems::Drop(TileCoord t, ItemStack stack, float life) {
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
        for (size_t i = 0; i < pile->items.size(); ++i)
            if (pile->items[i].item == stack.item) {
                pile->items[i].count += stack.count;
                pile->ages[i] = 0.0f;   // the stack is refreshed as a whole
                pile->lives[i] = std::max(pile->lives[i], life);
                merged = true;
            }
    }
    if (!merged) {
        pile->items.insert(pile->items.begin(), stack);
        pile->ages.insert(pile->ages.begin(), 0.0f);
        pile->lives.insert(pile->lives.begin(), life);
    }
    return pile->id;
}

void GroundItems::Update(float dt) {
    for (Pile& p : piles_) {
        for (size_t i = p.items.size(); i-- > 0;) {   // backwards, so erasing keeps the indices ahead valid
            p.ages[i] += dt;
            if (p.ages[i] >= p.lives[i]) {
                p.items.erase(p.items.begin() + static_cast<long>(i));
                p.ages.erase(p.ages.begin() + static_cast<long>(i));
                p.lives.erase(p.lives.begin() + static_cast<long>(i));
            }
        }
    }
    piles_.erase(std::remove_if(piles_.begin(), piles_.end(), [](const Pile& p) { return p.items.empty(); }), piles_.end());
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
    if (added >= s.count) {
        p->items.erase(p->items.begin() + index);
        p->ages.erase(p->ages.begin() + index);
        p->lives.erase(p->lives.begin() + index);
    } else {
        p->items[index].count -= added;
    }
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
