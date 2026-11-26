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
    {Item::RawTrout,     Item::Trout,     Item::BurntFish, 15, 70, 49, 0.50f},
    {Item::RawSalmon,    Item::Salmon,    Item::BurntFish, 25, 90, 58, 0.55f},
    {Item::RawTuna,      Item::Tuna,      Item::BurntFish, 30, 100, 64, 0.55f},
    {Item::RawLobster,   Item::Lobster,   Item::BurntFish, 40, 120, 74, 0.60f},
    {Item::RawSwordfish, Item::Swordfish, Item::BurntFish, 45, 140, 86, 0.60f},
    {Item::RawSardine,   Item::Sardine,   Item::BurntFish,  5, 40, 38, 0.50f},
    {Item::RawHerring,   Item::Herring,   Item::BurntFish, 10, 50, 41, 0.50f},
    {Item::RawPike,      Item::Pike,      Item::BurntFish, 20, 80, 52, 0.55f},
    {Item::RawBass,      Item::Bass,      Item::BurntFish, 43, 130, 80, 0.60f},
    {Item::RawShark,     Item::Shark,     Item::BurntFish, 80, 210, 94, 0.65f},
    // ---- round 10: game meat, more fish, dough and everything baked from it
    {Item::RawRatMeat,   Item::RatMeat,   Item::BurntMeat,  1, 18, 34, 0.45f},
    {Item::BreadDough,   Item::Bread,     Item::BurntPie,   1, 40, 34, 0.45f},
    {Item::RawMackerel,  Item::Mackerel,  Item::BurntFish, 16, 60, 45, 0.50f},
    {Item::RawCod,       Item::Cod,       Item::BurntFish, 23, 75, 52, 0.55f},
    {Item::RawEel,       Item::Eel,       Item::BurntFish, 28, 95, 60, 0.55f},
    {Item::RawWolfMeat,  Item::WolfMeat,  Item::BurntMeat, 25, 90, 58, 0.55f},
    {Item::RawRedberryPie, Item::RedberryPie, Item::BurntPie, 10, 78, 44, 0.50f},
    {Item::RawMeatPie,   Item::MeatPie,   Item::BurntPie,  20, 110, 52, 0.50f},
    {Item::UncookedStew, Item::Stew,      Item::BurntMeat, 25, 117, 58, 0.55f},
    {Item::RawApplePie,  Item::ApplePie,  Item::BurntPie,  30, 130, 60, 0.55f},
    {Item::RawGardenPie, Item::GardenPie, Item::BurntPie,  34, 128, 64, 0.55f},
    {Item::RawPizza,     Item::PlainPizza, Item::BurntPie, 35, 143, 66, 0.55f},
    {Item::UncookedCake, Item::Cake,      Item::BurntPie,  40, 180, 70, 0.60f},
    {Item::RawFishPie,   Item::FishPie,   Item::BurntPie,  47, 164, 76, 0.60f},
    {Item::RawSeaTurtle, Item::SeaTurtle, Item::BurntFish, 82, 211, 98, 0.70f},
    {Item::RawMantaRay,  Item::MantaRay,  Item::BurntFish, 91, 216, 99, 0.70f},
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
