// Smelting ore into bars (at a furnace) and smithing bars into gear (at an anvil, with a hammer). Pure C++ (unit tested).
//
// Smelting takes 2.4 s per bar, hammering 3 s per piece. Bronze is copper + tin, iron is a plain iron ore (but half of
// them are too impure and are lost), steel adds coal, and mithril needs a lot of coal.
#pragma once
#include "game/Gear.h"
#include "game/Inventory.h"

namespace journey {

constexpr float kSmeltSeconds = 2.4f;
constexpr float kSmithSeconds = 3.0f;

// The furnace makes the five metal bars and, from the rarer ores, silver and gold (the last two are not a Tier: index 5 and 6).
constexpr int kSmeltCount = 7;

struct SmeltRecipe {
    Tier tier;
    Item bar;
    int level;        // Smithing level needed
    int xp;           // Smithing XP for one bar
    Item ore[2];      // what goes in (second slot may be Item::None)
    int count[2];
    float success;    // chance that the ore melts (iron is the unreliable one)
};
const SmeltRecipe& GetSmeltRecipe(Tier t);
Item BarFor(Tier t);
// Which metal a bar is (false if the item is not a bar).
bool TierOfBar(Item bar, Tier* t);

struct SmithRecipe {
    Tier tier;
    GearType type;
    Item product;
    int bars;
    int level;
    int xp;
    int yield = 1;   // how many come out of one make (arrows: fifteen)
};
SmithRecipe GetSmithRecipe(Tier t, GearType g);

// How many pieces the bag could still smelt / smith right now (limited by the ore, bars and free slots).
int CanSmelt(const SmeltRecipe& r, const Inventory& bag);
int CanSmith(const SmithRecipe& r, const Inventory& bag);

}  // namespace journey
