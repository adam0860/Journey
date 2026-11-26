#include "game/Loot.h"

#include <algorithm>

#include "game/Gear.h"

namespace journey {

namespace {

// A tiny builder so the tables below read like the design document.
struct Table {
    LootTable t;
    Table(const char* key, int nothing) {
        t.key = key;
        t.nothing = nothing;
    }
    Table& Always(Item i, int lo = 1, int hi = 1) { return Add(LootTier::Always, i, lo, hi, 0); }
    Table& Common(Item i, int lo, int hi, int w) { return Add(LootTier::Common, i, lo, hi, w); }
    Table& Uncommon(Item i, int lo, int hi, int w) { return Add(LootTier::Uncommon, i, lo, hi, w); }
    Table& Rare(Item i, int lo, int hi, int w) { return Add(LootTier::Rare, i, lo, hi, w); }
    Table& Add(LootTier tier, Item i, int lo, int hi, int w) {
        t.drops.push_back({tier, i, lo, hi, w});
        return *this;
    }
};

Item G(Tier t, GearType g) { return GearItem(t, g); }

std::vector<LootTable> Build() {
    std::vector<LootTable> v;
    auto add = [&](const Table& t) { v.push_back(t.t); };

    // ---- the farm
    add(Table("chicken", 70).Always(Item::Bones).Always(Item::RawChicken).Common(Item::Egg, 1, 1, 30));
    add(Table("cow", 50).Always(Item::Bones).Always(Item::Cowhide).Common(Item::RawBeef, 1, 1, 50));
    add(Table("calf", 35).Always(Item::Bones).Common(Item::RawBeef, 1, 1, 40).Common(Item::Cowhide, 1, 1, 25));

    // ---- goblins: coins every time, now and then a weapon, a helmet, runes or arrows
    add(Table("goblin", 28)
            .Always(Item::Bones)
            .Always(Item::Coins, 3, 14)
            .Common(Item::AirRune, 2, 6, 20)
            .Common(Item::MindRune, 1, 4, 16)
            .Common(Item::BronzeArrows, 4, 14, 12)
            .Uncommon(Item::BronzeDarts, 3, 10, 8)
            .Uncommon(G(Tier::Bronze, GearType::Dagger), 1, 1, 6)
            .Uncommon(G(Tier::Bronze, GearType::Helm), 1, 1, 4)
            .Uncommon(Item::FireRune, 1, 3, 6));
    add(Table("goblin_warrior", 21)
            .Always(Item::Bones)
            .Always(Item::Coins, 10, 35)
            .Common(Item::Bread, 1, 1, 25)
            .Common(Item::WaterRune, 2, 5, 16)
            .Common(Item::EarthRune, 2, 5, 14)
            .Common(Item::IronArrows, 4, 12, 12)
            .Uncommon(G(Tier::Iron, GearType::Dagger), 1, 1, 6)
            .Rare(Item::ChaosRune, 1, 2, 6).Uncommon(G(Tier::Bronze, GearType::Scimitar), 1, 1, 4).Uncommon(Item::LeatherGloves, 1, 1, 4));

    // ---- the road and the wood
    add(Table("giant_rat", 50).Always(Item::Bones).Common(Item::Coins, 1, 6, 40).Uncommon(Item::AirRune, 1, 3, 10).Common(Item::RawRatMeat, 1, 1, 35));
    add(Table("wolf", 40)
            .Always(Item::Bones)
            .Common(Item::RawBeef, 1, 1, 40)
            .Common(Item::Coins, 4, 18, 40)
            .Uncommon(Item::Cowhide, 1, 1, 12).Common(Item::RawWolfMeat, 1, 1, 30));
    add(Table("dark_wolf", 30)
            .Always(Item::BigBones)
            .Common(Item::Coins, 20, 60, 50)
            .Common(Item::RawBeef, 1, 2, 30)
            .Uncommon(Item::IronOre, 1, 3, 12)
            .Rare(Item::MithrilOre, 1, 1, 3).Common(Item::RawWolfMeat, 1, 2, 25));

    // ---- the caves
    add(Table("giant_spider", 40)
            .Always(Item::Bones)
            .Common(Item::Coins, 8, 30, 50)
            .Common(Item::Coal, 1, 3, 20)
            .Uncommon(Item::IronOre, 1, 2, 12)
            .Rare(G(Tier::Steel, GearType::Dagger), 1, 1, 2));

    // ---- the crypt
    add(Table("zombie", 40)
            .Always(Item::Bones)
            .Common(Item::Coins, 10, 35, 50)
            .Common(Item::Logs, 1, 3, 20)
            .Uncommon(G(Tier::Iron, GearType::Helm), 1, 1, 8)
            .Uncommon(Item::MindRune, 3, 8, 12));
    add(Table("skeleton", 30)
            .Always(Item::Bones)
            .Common(Item::Coins, 15, 50, 50)
            .Common(Item::IronArrows, 6, 18, 20)
            .Common(Item::MindRune, 4, 12, 20)
            .Uncommon(G(Tier::Iron, GearType::Sword), 1, 1, 12)
            .Uncommon(G(Tier::Iron, GearType::Shield), 1, 1, 8)
            .Rare(G(Tier::Steel, GearType::Helm), 1, 1, 3)
            .Rare(G(Tier::Steel, GearType::Dagger), 1, 1, 4).Uncommon(G(Tier::Iron, GearType::Scimitar), 1, 1, 6).Rare(G(Tier::Steel, GearType::Chainbody), 1, 1, 2));

    // ---- the mountains and the desert
    add(Table("mountain_troll", 20)
            .Always(Item::BigBones)
            .Always(Item::Coins, 40, 120)
            .Common(Item::Coal, 3, 8, 30)
            .Common(Item::SteelBar, 1, 1, 14)
            .Uncommon(Item::MithrilOre, 1, 2, 20)
            .Uncommon(G(Tier::Steel, GearType::Axe), 1, 1, 8)
            .Rare(G(Tier::Mithril, GearType::Dagger), 1, 1, 2)
            .Rare(Item::TrollToothAmulet, 1, 1, 3).Uncommon(G(Tier::Steel, GearType::Warhammer), 1, 1, 6));
    add(Table("desert_mummy", 20)
            .Always(Item::BigBones)
            .Always(Item::Coins, 80, 200)
            .Common(Item::SteelArrows, 10, 30, 30)
            .Common(Item::FireRune, 8, 24, 30)
            .Uncommon(Item::ChaosRune, 3, 9, 20)
            .Uncommon(Item::MithrilBar, 1, 1, 8)
            .Rare(G(Tier::Mithril, GearType::Sword), 1, 1, 2)
            .Rare(Item::MummysAmulet, 1, 1, 3).Rare(G(Tier::Mithril, GearType::Scimitar), 1, 1, 2));
    add(Table("dragon", 0)
            .Always(Item::DragonBones)
            .Always(Item::Coins, 300, 800)
            .Common(Item::MithrilBar, 2, 5, 30)
            .Common(Item::FireRune, 30, 80, 24)
            .Uncommon(Item::MithrilArrows, 20, 60, 16)
            .Uncommon(Item::FireStaff, 1, 1, 8)
            .Rare(G(Tier::Mithril, GearType::Body), 1, 1, 4)
            .Rare(G(Tier::Mithril, GearType::Shield), 1, 1, 4)
            .Rare(G(Tier::Mithril, GearType::Legs), 1, 1, 4)
            .Rare(Item::AdamantBar, 1, 2, 5)
            .Rare(Item::UncutDragonstone, 1, 1, 4).Rare(G(Tier::Adamant, GearType::Scimitar), 1, 1, 2).Rare(G(Tier::Adamant, GearType::Longsword), 1, 1, 2).Rare(G(Tier::Adamant, GearType::Boots), 1, 1, 2));

    // ---- the Abyss: fighters carry what they fight with
    add(Table("abyss_warrior", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 40, 120)
            .Common(Item::SteelBar, 1, 2, 20)
            .Uncommon(G(Tier::Steel, GearType::Sword), 1, 1, 10)
            .Uncommon(G(Tier::Steel, GearType::Mace), 1, 1, 8)
            .Uncommon(G(Tier::Steel, GearType::Body), 1, 1, 5)
            .Rare(G(Tier::Mithril, GearType::Helm), 1, 1, 3).Uncommon(G(Tier::Steel, GearType::Scimitar), 1, 1, 8).Rare(G(Tier::Mithril, GearType::SquareShield), 1, 1, 3));
    add(Table("abyss_archer", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 40, 120)
            .Common(Item::SteelArrows, 15, 40, 40)
            .Common(Item::IronArrows, 20, 50, 30)
            .Uncommon(Item::OakLongbow, 1, 1, 10)
            .Rare(Item::YewShortbow, 1, 1, 3));
    add(Table("abyss_mage", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 40, 120)
            .Common(Item::FireRune, 10, 30, 30)
            .Common(Item::ChaosRune, 4, 12, 30)
            .Uncommon(Item::EarthStaff, 1, 1, 8)
            .Uncommon(Item::WizardRobe, 1, 1, 8)
            .Rare(Item::FireStaff, 1, 1, 3));
    add(Table("abyss_champion", 10)
            .Always(Item::BigBones)
            .Always(Item::Coins, 200, 500)
            .Common(Item::MithrilBar, 1, 3, 30)
            .Uncommon(G(Tier::Mithril, GearType::Sword), 1, 1, 8)
            .Uncommon(G(Tier::Mithril, GearType::Shield), 1, 1, 8)
            .Rare(G(Tier::Mithril, GearType::Body), 1, 1, 4)
            .Rare(G(Tier::Mithril, GearType::Axe), 1, 1, 3).Uncommon(G(Tier::Mithril, GearType::Longsword), 1, 1, 6).Rare(G(Tier::Adamant, GearType::Fullhelm), 1, 1, 3).Uncommon(G(Tier::Mithril, GearType::Gauntlets), 1, 1, 5)
            .Rare(Item::AbyssalAmulet, 1, 1, 2).Rare(Item::AbyssalRing, 1, 1, 3));
    // ---- the new animals
    add(Table("cave_spider", 45)
            .Always(Item::Bones)
            .Common(Item::Coins, 2, 10, 40)
            .Uncommon(Item::Coal, 1, 2, 12)
            .Uncommon(Item::IronOre, 1, 1, 8)
            .Rare(Item::UncutSapphire, 1, 1, 2));
    add(Table("scorpion", 35)
            .Always(Item::Bones)
            .Common(Item::Coins, 10, 40, 50)
            .Common(Item::Coal, 2, 5, 20)
            .Uncommon(Item::SilverOre, 1, 1, 12)
            .Uncommon(Item::GoldOre, 1, 1, 6)
            .Rare(Item::UncutEmerald, 1, 1, 3));
    add(Table("jackal", 40)
            .Always(Item::Bones)
            .Common(Item::RawBeef, 1, 1, 40)
            .Common(Item::Coins, 6, 24, 40)
            .Uncommon(Item::Cowhide, 1, 1, 12)
            .Rare(Item::IronOre, 1, 2, 6).Common(Item::RawWolfMeat, 1, 1, 25));
    add(Table("hellhound", 20)
            .Always(Item::BigBones)
            .Common(Item::Coins, 80, 200, 50)
            .Common(Item::RawBeef, 1, 2, 25)
            .Uncommon(Item::Coal, 5, 12, 20)
            .Uncommon(Item::AdamantOre, 1, 1, 6)
            .Rare(Item::AbyssalRing, 1, 1, 2)
            .Rare(Item::UncutDiamond, 1, 1, 3));
    add(Table("giant_mole", 40)
            .Always(Item::Bones)
            .Common(Item::Coins, 10, 40, 40)
            .Common(Item::Coal, 2, 6, 30)
            .Uncommon(Item::IronOre, 2, 4, 15)
            .Uncommon(Item::SilverOre, 1, 1, 8)
            .Rare(Item::UncutSapphire, 1, 1, 4)
            .Rare(Item::UncutEmerald, 1, 1, 2));
    add(Table("hobgoblin", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 8, 28)
            .Common(Item::Bread, 1, 1, 20)
            .Common(Item::IronArrows, 6, 16, 14)
            .Common(Item::EarthRune, 3, 8, 16)
            .Uncommon(G(Tier::Iron, GearType::Mace), 1, 1, 6)
            .Uncommon(G(Tier::Iron, GearType::Helm), 1, 1, 6)
            .Uncommon(Item::ChaosRune, 1, 3, 6)
            .Rare(G(Tier::Steel, GearType::Dagger), 1, 1, 3).Uncommon(G(Tier::Iron, GearType::Spear), 1, 1, 6).Uncommon(Item::LeatherBoots, 1, 1, 3));
    add(Table("imp", 30)
            .Always(Item::Bones)
            .Common(Item::Coins, 1, 12, 40)
            .Common(Item::MindRune, 3, 9, 20)
            .Common(Item::FireRune, 2, 6, 20)
            .Uncommon(Item::ChaosRune, 1, 3, 10)
            .Rare(Item::Staff, 1, 1, 4));
    add(Table("baby_dragon", 15)
            .Always(Item::BigBones)
            .Common(Item::Coins, 60, 160, 40)
            .Common(Item::IronBar, 1, 3, 15)
            .Common(Item::SteelBar, 1, 2, 14)
            .Common(Item::FireRune, 10, 25, 20)
            .Uncommon(Item::MithrilBar, 1, 1, 8)
            .Uncommon(Item::SteelArrows, 15, 40, 14)
            .Rare(Item::UncutRuby, 1, 1, 4)
            .Rare(Item::UncutDiamond, 1, 1, 2));
    add(Table("black_dragon", 0)
            .Always(Item::DragonBones)
            .Always(Item::Coins, 400, 1000)
            .Common(Item::MithrilBar, 2, 5, 25)
            .Common(Item::AdamantOre, 1, 2, 15)
            .Common(Item::FireRune, 40, 100, 20)
            .Uncommon(Item::AdamantArrows, 15, 40, 14)
            .Uncommon(Item::AdamantBar, 1, 1, 10)
            .Rare(G(Tier::Adamant, GearType::Sword), 1, 1, 3)
            .Rare(G(Tier::Adamant, GearType::Shield), 1, 1, 3)
            .Rare(G(Tier::Adamant, GearType::Helm), 1, 1, 3)
            .Rare(Item::UncutDragonstone, 1, 1, 3)
            .Rare(Item::AbyssalAmulet, 1, 1, 5).Rare(Item::AbyssalRing, 1, 1, 4));

    // ---- the new people
    add(Table("bandit", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 8, 40)
            .Common(Item::Bread, 1, 1, 25)
            .Common(Item::Chicken, 1, 1, 15)
            .Uncommon(G(Tier::Iron, GearType::Sword), 1, 1, 8)
            .Uncommon(Item::LeatherBody, 1, 1, 6)
            .Rare(Item::GoldRing, 1, 1, 3).Uncommon(G(Tier::Iron, GearType::Spear), 1, 1, 8).Uncommon(G(Tier::Iron, GearType::Chainbody), 1, 1, 5).Uncommon(Item::HardLeatherBoots, 1, 1, 6).Uncommon(G(Tier::Iron, GearType::Gauntlets), 1, 1, 4));
    add(Table("dark_wizard", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 15, 50)
            .Common(Item::AirRune, 5, 15, 25)
            .Common(Item::MindRune, 4, 12, 20)
            .Common(Item::ChaosRune, 2, 6, 18)
            .Uncommon(Item::WizardHat, 1, 1, 8)
            .Uncommon(Item::WizardRobe, 1, 1, 6)
            .Uncommon(Item::EarthStaff, 1, 1, 5)
            .Rare(Item::FireStaff, 1, 1, 3).Uncommon(Item::Cheese, 1, 2, 6));
    add(Table("barbarian", 25)
            .Always(Item::Bones)
            .Always(Item::Coins, 20, 60)
            .Common(Item::RawBeef, 1, 1, 20)
            .Common(Item::Bread, 1, 1, 15)
            .Common(Item::IronArrows, 8, 20, 14)
            .Uncommon(G(Tier::Iron, GearType::Axe), 1, 1, 8)
            .Uncommon(G(Tier::Steel, GearType::Mace), 1, 1, 4)
            .Uncommon(Item::Cowhide, 1, 1, 10)
            .Rare(G(Tier::Steel, GearType::Body), 1, 1, 2)
            .Rare(Item::SapphireRing, 1, 1, 2).Uncommon(G(Tier::Iron, GearType::Warhammer), 1, 1, 6).Uncommon(G(Tier::Steel, GearType::SquareShield), 1, 1, 4).Uncommon(G(Tier::Steel, GearType::Boots), 1, 1, 4));
    add(Table("ogre", 20)
            .Always(Item::BigBones)
            .Always(Item::Coins, 40, 130)
            .Common(Item::Coal, 3, 8, 25)
            .Common(Item::SteelBar, 1, 2, 14)
            .Uncommon(Item::MithrilOre, 1, 2, 12)
            .Uncommon(G(Tier::Steel, GearType::Mace), 1, 1, 8)
            .Uncommon(G(Tier::Steel, GearType::Legs), 1, 1, 5)
            .Rare(Item::TrollToothAmulet, 1, 1, 3)
            .Rare(Item::UncutRuby, 1, 1, 3).Uncommon(G(Tier::Steel, GearType::Warhammer), 1, 1, 6).Uncommon(G(Tier::Steel, GearType::Gauntlets), 1, 1, 4));
    add(Table("hill_giant", 20)
            .Always(Item::BigBones)
            .Always(Item::Coins, 50, 150)
            .Common(Item::Coal, 4, 10, 25)
            .Common(Item::SteelBar, 1, 2, 14)
            .Common(Item::RawBeef, 1, 2, 15)
            .Uncommon(G(Tier::Steel, GearType::Axe), 1, 1, 8)
            .Uncommon(Item::MithrilOre, 1, 2, 10)
            .Rare(Item::UncutRuby, 1, 1, 3)
            .Rare(G(Tier::Mithril, GearType::Mace), 1, 1, 2).Uncommon(G(Tier::Iron, GearType::Warhammer), 1, 1, 8).Uncommon(G(Tier::Iron, GearType::SquareShield), 1, 1, 6).Uncommon(G(Tier::Iron, GearType::Boots), 1, 1, 6));
    add(Table("skeleton_warrior", 30)
            .Always(Item::Bones)
            .Common(Item::Coins, 25, 80, 45)
            .Common(Item::SteelArrows, 8, 24, 20)
            .Common(Item::MindRune, 6, 16, 20)
            .Uncommon(G(Tier::Steel, GearType::Sword), 1, 1, 10)
            .Uncommon(G(Tier::Steel, GearType::Shield), 1, 1, 6)
            .Uncommon(G(Tier::Iron, GearType::Helm), 1, 1, 8)
            .Rare(G(Tier::Steel, GearType::Body), 1, 1, 3)
            .Rare(Item::UncutEmerald, 1, 1, 3).Uncommon(G(Tier::Steel, GearType::Longsword), 1, 1, 8).Uncommon(G(Tier::Steel, GearType::Fullhelm), 1, 1, 6).Uncommon(G(Tier::Steel, GearType::Boots), 1, 1, 6));
    add(Table("black_knight", 15)
            .Always(Item::BigBones)
            .Always(Item::Coins, 80, 220)
            .Common(Item::SteelBar, 1, 3, 20)
            .Common(Item::MithrilBar, 1, 2, 12)
            .Uncommon(G(Tier::Mithril, GearType::Sword), 1, 1, 6)
            .Uncommon(G(Tier::Mithril, GearType::Legs), 1, 1, 5)
            .Uncommon(G(Tier::Mithril, GearType::Helm), 1, 1, 5)
            .Rare(G(Tier::Adamant, GearType::Mace), 1, 1, 2)
            .Rare(Item::UncutDiamond, 1, 1, 3)
            .Rare(Item::AbyssalRing, 1, 1, 3).Rare(Item::AbyssalAmulet, 1, 1, 1).Uncommon(G(Tier::Mithril, GearType::Fullhelm), 1, 1, 6).Rare(G(Tier::Adamant, GearType::Chainbody), 1, 1, 3).Uncommon(G(Tier::Mithril, GearType::Boots), 1, 1, 5).Rare(G(Tier::Adamant, GearType::Gauntlets), 1, 1, 3));
    return v;
}

int RandInt(const std::function<double()>& roll, int lo, int hi) {   // inclusive
    return lo + std::min(hi - lo, static_cast<int>(roll() * (hi - lo + 1)));
}

}  // namespace

const char* LootTierName(LootTier t) {
    switch (t) {
        case LootTier::Always: return "Always";
        case LootTier::Common: return "Common";
        case LootTier::Uncommon: return "Uncommon";
        case LootTier::Rare: return "Rare";
    }
    return "";
}

const std::vector<LootTable>& AllLootTables() {
    static const std::vector<LootTable> tables = Build();
    return tables;
}

const LootTable* FindLootTable(const std::string& key) {
    for (const LootTable& t : AllLootTables())
        if (t.key == key) return &t;
    return nullptr;
}

std::vector<ItemStack> RollLootTable(const LootTable& table, const std::function<double()>& roll) {
    std::vector<ItemStack> out;
    int total = table.nothing;
    for (const LootDrop& d : table.drops) {
        if (d.tier == LootTier::Always) out.push_back({d.item, RandInt(roll, d.minCount, d.maxCount)});
        else total += d.weight;
    }
    if (total <= 0) return out;
    double pick = roll() * total;
    for (const LootDrop& d : table.drops) {
        if (d.tier == LootTier::Always) continue;
        if (pick < d.weight) {
            out.push_back({d.item, RandInt(roll, d.minCount, d.maxCount)});
            return out;
        }
        pick -= d.weight;
    }
    return out;   // landed in the "nothing" share
}

double DropChance(const LootTable& table, Item item) {
    int total = table.nothing;
    double chance = 0.0;
    for (const LootDrop& d : table.drops)
        if (d.tier != LootTier::Always) total += d.weight;
    for (const LootDrop& d : table.drops) {
        if (d.item != item) continue;
        if (d.tier == LootTier::Always) return 1.0;
        if (total > 0) chance += static_cast<double>(d.weight) / total;
    }
    return chance;
}

}  // namespace journey
