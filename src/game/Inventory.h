// The player's bag: 28 slots, like the classic game. Pure C++ (unit tested).
#pragma once
#include <array>

namespace journey {

enum class Item : int {
    None = 0,
    Logs,
    Count
};

struct ItemInfo {
    const char* name;
    bool stackable;   // stackable items share one slot (coins, arrows...); everything else takes a slot each
};
const ItemInfo& GetItemInfo(Item item);

struct ItemStack {
    Item item = Item::None;
    int count = 0;
};

class Inventory {
public:
    static constexpr int kSlots = 28;

    // Adds `count` of an item. Returns how many were actually added (less than asked when the bag fills up).
    int Add(Item item, int count = 1);
    // Removes the whole stack in a slot and returns it ({None,0} for an empty slot).
    ItemStack TakeSlot(int slot);

    const ItemStack& Slot(int slot) const { return slots_[slot]; }
    void SetSlot(int slot, ItemStack s) { slots_[slot] = s; }   // for loading a save
    int Count(Item item) const;
    int FreeSlots() const;
    bool Full() const { return FreeSlots() == 0; }

private:
    std::array<ItemStack, kSlots> slots_{};
};

}  // namespace journey
