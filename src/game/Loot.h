// Loot tables: what a creature leaves behind. Pure C++ (unit tested). All of it is data (Loot.cpp).
//
// Like the design document says: every table has drops in four tiers - Always, Common, Uncommon, Rare. Everything in the
// Always tier drops every time. Then ONE more roll picks a single entry from the other tiers by weight (so a rare drop with
// weight 2 next to a common drop with weight 60 really is rare), or nothing at all (the table's `nothing` weight).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "game/Inventory.h"

namespace journey {

enum class LootTier { Always, Common, Uncommon, Rare };
const char* LootTierName(LootTier t);

struct LootDrop {
    LootTier tier;
    Item item;
    int minCount, maxCount;
    int weight;   // ignored for Always
};

struct LootTable {
    std::string key;                // "goblin"
    int nothing = 0;                // weight of "no extra drop"
    std::vector<LootDrop> drops;
};

const LootTable* FindLootTable(const std::string& key);   // nullptr if unknown
const std::vector<LootTable>& AllLootTables();

// Rolls a table. `roll` returns numbers in [0,1): one for the pick and one per count.
std::vector<ItemStack> RollLootTable(const LootTable& table, const std::function<double()>& roll);

// The chance (0..1) that one kill of this table drops this item at all (for tests and for examining a monster).
double DropChance(const LootTable& table, Item item);

}  // namespace journey
