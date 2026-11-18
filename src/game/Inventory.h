// The player's bag: 28 slots, like the classic game. Pure C++ (unit tested).
//
// Item ids are stored in the save file, so only ever add new items at the END of the enum (before Count).
#pragma once
#include <array>
#include <vector>

namespace journey {

enum class Item : int {
    None = 0,
    Logs,
    BronzeAxe,
    SmallNet,
    Tinderbox,
    RawShrimps,
    Shrimps,
    RawAnchovies,
    Anchovies,
    BurntFish,
    Bones,
    RawChicken,
    Chicken,
    RawBeef,
    Beef,
    BurntMeat,
    Egg,
    Cowhide,
    // ---- mining and smithing ----
    CopperOre,
    TinOre,
    IronOre,
    Coal,
    MithrilOre,
    BronzeBar,
    IronBar,
    SteelBar,
    MithrilBar,
    Hammer,
    BronzePickaxe,
    IronPickaxe,
    SteelPickaxe,
    MithrilPickaxe,
    // The 32 smithed weapons and armour: 4 metals x 8 pieces, in the order of Gear.h (dagger, sword, battleaxe, mace, helm,
    // shield, legs, body for bronze, then the same for iron...). GearItem() / GearOf() turn these into (tier, type).
    GearFirst,
    GearLast = GearFirst + 31,
    // ---- money and shop food ----
    Coins,
    Bread,
    Count
};

struct ItemInfo {
    const char* name;
    bool stackable;   // stackable items share one slot (coins, arrows...); everything else takes a slot each
    bool tool;        // starter tools cannot be dropped (there is no shop to buy a new one yet)
    bool raw;         // can be cooked on a fire
    bool food;        // can be eaten
    int heals;        // hit points restored when eaten
    int buryXp;       // Prayer XP for burying it (0 = cannot be buried)
    const char* examine;   // what "Examine" says
    const char* model;     // ground model file: assets/models/props/drop_<model>.glb (see tools/build_items.py)
};
const ItemInfo& GetItemInfo(Item item);

// What an item is worth in coins (shops sell at about this price and buy for a share of it). 0 = worthless (burnt food).
int ItemValue(Item item);
// Can a shop buy it? (not worthless, not a starter tool, not coins)
bool ItemSellable(Item item);

struct ItemStack {
    Item item = Item::None;
    int count = 0;
};

// What you can do with a bag item (buttons under the bag grid, and the long-press menu).
enum class ItemAction { Light, Cook, Eat, Bury, Wield, Wear, Use, Drop, Examine };
const char* ActionName(ItemAction a);

class Inventory {
public:
    static constexpr int kSlots = 28;

    // Adds `count` of an item. Returns how many were actually added (less than asked when the bag fills up).
    int Add(Item item, int count = 1);
    // Removes the whole stack in a slot and returns it ({None,0} for an empty slot).
    ItemStack TakeSlot(int slot);
    // Removes `count` of an item (from the first slots that hold it). Returns false, changing nothing, if there are fewer.
    bool Remove(Item item, int count = 1);

    const ItemStack& Slot(int slot) const { return slots_[slot]; }
    void SetSlot(int slot, ItemStack s) { slots_[slot] = s; }   // for loading a save / swapping raw food for cooked
    int Count(Item item) const;
    bool Has(Item item) const { return Count(item) > 0; }
    // First slot holding `item`, or -1.
    int FindSlot(Item item) const;
    // First slot holding any raw (cookable) food, or -1.
    int FindRawFood() const;
    int FreeSlots() const;
    bool Full() const { return FreeSlots() == 0; }

private:
    std::array<ItemStack, kSlots> slots_{};
};

// The buttons that make sense for this item right now, e.g. Light for logs only while you carry a tinderbox.
std::vector<ItemAction> ActionsFor(Item item, const Inventory& bag);

// Makes sure the bag holds the starting tools (bronze axe, small fishing net, tinderbox, bronze pickaxe, hammer).
// Returns how many were added.
int GiveStarterKit(Inventory* bag);

}  // namespace journey
