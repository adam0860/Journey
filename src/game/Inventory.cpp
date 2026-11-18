#include "game/Inventory.h"

namespace journey {

namespace {
const ItemInfo kInfo[static_cast<int>(Item::Count)] = {
    {"nothing", false},
    {"Logs", false},
};
}  // namespace

const ItemInfo& GetItemInfo(Item item) {
    const int i = static_cast<int>(item);
    return kInfo[(i >= 0 && i < static_cast<int>(Item::Count)) ? i : 0];
}

int Inventory::Add(Item item, int count) {
    if (item == Item::None || count <= 0) return 0;
    int added = 0;
    if (GetItemInfo(item).stackable) {
        for (ItemStack& s : slots_)
            if (s.item == item) {
                s.count += count;
                return count;
            }
    }
    for (ItemStack& s : slots_) {
        if (added >= count) break;
        if (s.item != Item::None) continue;
        if (GetItemInfo(item).stackable) {
            s = {item, count};
            return count;
        }
        s = {item, 1};
        ++added;
    }
    return added;
}

ItemStack Inventory::TakeSlot(int slot) {
    if (slot < 0 || slot >= kSlots) return {};
    const ItemStack s = slots_[slot];
    slots_[slot] = {};
    return s;
}

int Inventory::Count(Item item) const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == item) n += s.count;
    return n;
}

int Inventory::FreeSlots() const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == Item::None) ++n;
    return n;
}

}  // namespace journey
