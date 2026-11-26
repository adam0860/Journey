#include "game/Smithing.h"

#include <algorithm>

namespace journey {

namespace {

const SmeltRecipe kSmelt[kSmeltCount] = {
    {Tier::Bronze, Item::BronzeBar, 1, 6, {Item::CopperOre, Item::TinOre}, {1, 1}, 1.0f},
    {Tier::Iron, Item::IronBar, 15, 13, {Item::IronOre, Item::None}, {1, 0}, 0.5f},
    {Tier::Steel, Item::SteelBar, 30, 18, {Item::IronOre, Item::Coal}, {1, 2}, 1.0f},
    {Tier::Mithril, Item::MithrilBar, 50, 30, {Item::MithrilOre, Item::Coal}, {1, 4}, 1.0f},
    {Tier::Adamant, Item::AdamantBar, 70, 48, {Item::AdamantOre, Item::Coal}, {1, 6}, 1.0f},
    {Tier::Bronze, Item::SilverBar, 20, 14, {Item::SilverOre, Item::None}, {1, 0}, 1.0f},   // (the tier is ignored for these two)
    {Tier::Bronze, Item::GoldBar, 40, 56, {Item::GoldOre, Item::None}, {1, 0}, 1.0f},
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
    {1, 1},    // axe (woodcutting)
    {1, 2},    // arrows (fifteen from a bar)
    {2, 6},    // scimitar
    {3, 8},    // longsword
    {2, 7},    // spear
    {4, 14},   // warhammer
    {2, 9},    // full helm
    {2, 11},   // square shield
    {3, 13},   // chainbody
    {1, 5},    // boots
    {1, 9},    // gauntlets
};
constexpr int kFirstLevel[kTierCount] = {1, 15, 30, 50, 70};   // the level at which the metal can be worked
constexpr int kXpPerBar[kTierCount] = {13, 25, 38, 50, 62};

}  // namespace

const SmeltRecipe& GetSmeltRecipe(Tier t) { return kSmelt[std::clamp(static_cast<int>(t), 0, kSmeltCount - 1)]; }
Item BarFor(Tier t) { return kSmelt[std::clamp(static_cast<int>(t), 0, kSmeltCount - 1)].bar; }

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
    if (g == GearType::Arrows) r.yield = 15;
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
