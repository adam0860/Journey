#include "game/Cooking.h"

#include <algorithm>

namespace journey {

namespace {
const Recipe kRecipes[] = {
    //   raw                cooked           burnt             lvl xp  stop  burn1
    {Item::RawShrimps,   Item::Shrimps,   Item::BurntFish,  1, 30, 34, 0.45f},
    {Item::RawAnchovies, Item::Anchovies, Item::BurntFish,  1, 30, 34, 0.50f},
    {Item::RawChicken,   Item::Chicken,   Item::BurntMeat,  1, 30, 34, 0.45f},
    {Item::RawBeef,      Item::Beef,      Item::BurntMeat,  1, 30, 31, 0.45f},
};
}  // namespace

const Recipe* RecipeFor(Item raw) {
    for (const Recipe& r : kRecipes)
        if (r.raw == raw) return &r;
    return nullptr;
}

const Recipe* RecipeForCooked(Item cooked) {
    for (const Recipe& r : kRecipes)
        if (r.cooked == cooked) return &r;
    return nullptr;
}

float BurnChance(const Recipe& r, int level) {
    if (level >= r.burnStopLevel) return 0.0f;
    level = std::max(level, 1);
    return r.burnAtLevel1 * static_cast<float>(r.burnStopLevel - level) / static_cast<float>(r.burnStopLevel - 1);
}

}  // namespace journey
