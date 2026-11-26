// Cooking rules. Pure C++ (unit tested). Adding a new food is one line in the recipe table (Cooking.cpp).
//
// You cook one raw item per 3 game ticks (1.8 s). Low levels burn food, which gives no XP; the burn chance falls
// in a straight line to zero at the recipe's "stop burning" level.
#pragma once
#include "game/Inventory.h"

namespace journey {

constexpr float kCookSeconds = 1.8f;

struct Recipe {
    Item raw, cooked, burnt;
    int level;            // Cooking level needed
    int xp;               // Cooking XP for a good result
    int burnStopLevel;    // from this level up it never burns
    float burnAtLevel1;   // burn chance at level 1
};

// The recipe for a raw item, or nullptr if it cannot be cooked.
const Recipe* RecipeFor(Item raw);
// The recipe that produced this cooked item, or nullptr.
const Recipe* RecipeForCooked(Item cooked);
float BurnChance(const Recipe& r, int cookingLevel);
// A cooking range counts as this many Cooking levels higher when working out whether the food burns.
constexpr int kRangeLevelBonus = 8;

}  // namespace journey
