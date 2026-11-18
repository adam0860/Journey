#include "game/Smithing.h"

#include <algorithm>

namespace journey {

namespace {

const SmeltRecipe kSmelt[kTierCount] = {
    {Tier::Bronze, Item::BronzeBar, 1, 6, {Item::CopperOre, Item::TinOre}, {1, 1}, 1.0f},
    {Tier::Iron, Item::IronBar, 15, 13, {Item::IronOre, Item::None}, {1, 0}, 0.5f},
    {Tier::Steel, Item::SteelBar, 30, 18, {Item::IronOre, Item::Coal}, {1, 2}, 1.0f},
    {Tier::Mithril, Item::MithrilBar, 50, 30, {Item::MithrilOre, Item::Coal}, {1, 4}, 1.0f},
};

// Bars for each piece, and how many levels above the metal's first level you need to make it.
struct Shape {
    int bars;
    int levelOffset;
};
constexpr Shape kShape[kGearTypeCount] = {
    {1, 0},    // dagger
    {2, 4},    // sword
    {3, 10},   // battleaxe
    {1, 2},    // mace
    {1, 3},    // medium helm
    {3, 12},   // kiteshield
    {3, 16},   // platelegs
    {5, 18},   // platebody
    {2, 5},    // pickaxe
};
constexpr int kFirstLevel[kTierCount] = {1, 15, 30, 50};   // the level at which the metal can be worked
constexpr int kXpPerBar[kTierCount] = {13, 25, 38, 50};

}  // namespace

const SmeltRecipe& GetSmeltRecipe(Tier t) { return kSmelt[static_cast<int>(t)]; }
Item BarFor(Tier t) { return kSmelt[static_cast<int>(t)].bar; }

bool TierOfBar(Item bar, Tier* t) {
    for (int i = 0; i < kTierCount; ++i)
        if (kSmelt[i].bar == bar) {
            if (t) *t = static_cast<Tier>(i);
            return true;
        }
    return false;
}

SmithRecipe GetSmithRecipe(Tier t, GearType g) {
    const int ti = static_cast<int>(t), gi = static_cast<int>(g);
    SmithRecipe r;
    r.tier = t;
    r.type = g;
    r.product = GearItem(t, g);
    r.bars = kShape[gi].bars;
    r.level = kFirstLevel[ti] + kShape[gi].levelOffset;
    r.xp = r.bars * kXpPerBar[ti];
    return r;
}

int CanSmelt(const SmeltRecipe& r, const Inventory& bag) {
    int n = 1000;
    for (int i = 0; i < 2; ++i)
        if (r.ore[i] != Item::None && r.count[i] > 0) n = std::min(n, bag.Count(r.ore[i]) / r.count[i]);
    return n == 1000 ? 0 : n;
}

int CanSmith(const SmithRecipe& r, const Inventory& bag) {
    return r.bars > 0 ? bag.Count(BarFor(r.tier)) / r.bars : 0;
}

}  // namespace journey
