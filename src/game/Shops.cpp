#include "game/Shops.h"

#include <algorithm>
#include <cmath>

#include "game/Archery.h"
#include "game/Gear.h"
#include "game/Magic.h"

namespace journey {

unsigned CategoryOf(Item item) {
    if (IsBow(item) || IsArrow(item)) return kCatRanged;
    if (IsStaff(item) || IsRune(item) || FindRobe(item)) return kCatMagic;
    Tier t;
    GearType g;
    if (GearOf(item, &t, &g)) {
        if (g == GearType::Pickaxe || g == GearType::Hatchet) return kCatTool;
        return SlotOf(g) == EquipSlot::Weapon ? kCatWeapon : kCatArmour;
    }
    const ItemInfo& info = GetItemInfo(item);
    switch (item) {
        case Item::CopperOre:
        case Item::TinOre:
        case Item::IronOre:
        case Item::Coal:
        case Item::MithrilOre:
        case Item::AdamantOre:
        case Item::SilverOre:
        case Item::GoldOre: return kCatOre;
        case Item::BronzeBar:
        case Item::IronBar:
        case Item::SteelBar:
        case Item::MithrilBar:
        case Item::AdamantBar:
        case Item::SilverBar:
        case Item::GoldBar: return kCatBar;
        case Item::Hammer:
        case Item::BronzeAxe:
        case Item::SmallNet:
        case Item::FlyFishingRod:
        case Item::Harpoon:
        case Item::LobsterPot:
        case Item::Feathers:
        case Item::Needle:
        case Item::Chisel:
        case Item::Knife:
        case Item::Spade:
        case Item::Candle:
        case Item::Tinderbox: return kCatTool;
        case Item::Bowstring:
        case Item::UnstrungShortbow:
        case Item::UnstrungLongbow:
        case Item::UnstrungOakShortbow:
        case Item::UnstrungOakLongbow:
        case Item::UnstrungYewShortbow:
        case Item::UnstrungYewLongbow: return kCatRanged;
        case Item::Flour:
        case Item::JugOfWater:
        case Item::Cheese:
        case Item::Tomato:
        case Item::Potato:
        case Item::CookingApple:
        case Item::BucketOfMilk:
        case Item::Onion:
        case Item::Cabbage:
        case Item::Redberries:
        case Item::PastryDough:
        case Item::PizzaBase:
        case Item::BurntPie: return kCatFood;
        default: break;
    }
    if (info.food || info.raw || item == Item::Egg || item == Item::BurntFish || item == Item::BurntMeat || item == Item::Bones ||
        item == Item::BigBones || item == Item::DragonBones)
        return kCatFood;
    return kCatMisc;
}

namespace {

std::vector<ShopDef> BuildDefs() {
    std::vector<ShopDef> v;
    auto gear = [](Tier t, GearType g) { return GearItem(t, g); };
    {
        ShopDef s{"general_town", "Journey General Store", 1.35f, 0.30f, kCatEverything, {}};
        s.stock = {{Item::Bread, 12}, {Item::Shrimps, 8}, {Item::Chicken, 6}, {Item::Beef, 6}, {Item::Logs, 20}, {Item::Tinderbox, 3},
                   {Item::SmallNet, 3}, {Item::BronzeAxe, 3}, {Item::Hammer, 3}, {Item::BronzePickaxe, 3},
                   {Item::Shortbow, 2}, {Item::BronzeArrows, 100}, {Item::Staff, 2}, {Item::AirRune, 40}, {Item::MindRune, 40},
                   {Item::FireRune, 30}, {Item::MillbrookScroll, 4}, {Item::LakesideScroll, 3}, {Item::ElmswickScroll, 3}, {Item::SaltmereScroll, 3},
                   {Item::Knife, 3}, {Item::Spade, 3}, {Item::Candle, 30}, {Item::Flour, 20}, {Item::JugOfWater, 20}};
        v.push_back(s);
    }
    {
        ShopDef s{"smith_town", "Torvald's Smithing Supplies", 1.15f, 0.45f, kCatOre | kCatBar | kCatTool | kCatWeapon | kCatArmour, {}};
        s.stock = {{Item::Hammer, 5}, {Item::BronzePickaxe, 3}, {Item::IronPickaxe, 2}, {Item::BronzeBar, 12}, {Item::IronBar, 6},
                   {Item::CopperOre, 20}, {Item::TinOre, 20}, {Item::Coal, 10}, {gear(Tier::Iron, GearType::Hatchet), 2},
                   {Item::SteelPickaxe, 2, "a_sharp_order"}, {Item::MithrilPickaxe, 1, "ore_for_the_smith"}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_general", "Millbrook General Store", 1.25f, 0.32f, kCatEverything, {}};
        s.stock = {{Item::Bread, 15}, {Item::Shrimps, 10}, {Item::Anchovies, 6}, {Item::Chicken, 8}, {Item::Beef, 8}, {Item::Logs, 30},
                   {Item::Tinderbox, 5}, {Item::SmallNet, 5}, {Item::BronzeAxe, 4}, {Item::Hammer, 4}, {Item::CastleScroll, 5},
                   {Item::LakesideScroll, 3}, {Item::Knife, 4}, {Item::Spade, 4}, {Item::Candle, 40}, {Item::Potato, 20}, {Item::Onion, 15}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_weapons", "Brannock's Blades", 1.0f, 0.48f, kCatWeapon, {}};
        for (Tier t : {Tier::Bronze, Tier::Iron, Tier::Steel})
            for (GearType g : {GearType::Dagger, GearType::Sword, GearType::Axe, GearType::Mace, GearType::Scimitar, GearType::Longsword, GearType::Spear})
                s.stock.push_back({gear(t, g), t == Tier::Steel ? 1 : (t == Tier::Iron ? 2 : 3)});
        for (GearType g : {GearType::Sword, GearType::Scimitar, GearType::Longsword, GearType::Spear})   // a smith's reward: mithril blades
            s.stock.push_back({gear(Tier::Mithril, g), 1, "a_knights_kit"});
        v.push_back(s);
    }
    {
        ShopDef s{"village_armour", "Mira's Armoury", 1.0f, 0.48f, kCatArmour, {}};
        for (Tier t : {Tier::Bronze, Tier::Iron, Tier::Steel})
            for (GearType g : {GearType::Helm, GearType::Shield, GearType::Legs, GearType::Body, GearType::Fullhelm, GearType::SquareShield, GearType::Chainbody,
                               GearType::Boots, GearType::Gauntlets})
                s.stock.push_back({gear(t, g), t == Tier::Steel ? 1 : (t == Tier::Iron ? 2 : 3)});
        for (GearType g : {GearType::Helm, GearType::Fullhelm, GearType::Shield, GearType::Chainbody, GearType::Body})
            s.stock.push_back({gear(Tier::Mithril, g), 1, "a_knights_kit"});
        v.push_back(s);
    }
    {
        ShopDef s{"village_ore", "Dunmore's Ore and Bars", 1.1f, 0.50f, kCatOre | kCatBar, {}};
        s.stock = {{Item::CopperOre, 25}, {Item::TinOre, 25}, {Item::IronOre, 15}, {Item::Coal, 20}, {Item::MithrilOre, 2},
                   {Item::BronzeBar, 15}, {Item::IronBar, 8}, {Item::SteelBar, 4}, {Item::MithrilBar, 1}, {gear(Tier::Steel, GearType::Hatchet), 1},
                   {gear(Tier::Iron, GearType::Hatchet), 2}, {Item::AdamantOre, 2, "hounds_of_the_abyss"}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_archery", "Larson's Bows and Arrows", 1.0f, 0.45f, kCatRanged, {}};
        s.stock = {{Item::Shortbow, 3}, {Item::Longbow, 3}, {Item::OakShortbow, 1}, {Item::OakLongbow, 1}, {Item::BronzeArrows, 300},
                   {Item::IronArrows, 200}, {Item::SteelArrows, 100},
                   {Item::Bowstring, 100}, {Item::Knife, 3}, {Item::YewShortbow, 1, "bowyers_trial"}, {Item::YewLongbow, 1, "bowyers_trial"},
                   {Item::MithrilArrows, 100, "bowyers_pride"},
                   {Item::BronzeDarts, 200}, {Item::IronDarts, 150}, {Item::SteelDarts, 100}, {Item::MithrilDarts, 60, "bowyers_pride"}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_magic", "Aldric's Arcane Supplies", 1.0f, 0.45f, kCatMagic, {}};
        s.stock = {{Item::AirRune, 200}, {Item::WaterRune, 150}, {Item::EarthRune, 150}, {Item::FireRune, 150}, {Item::MindRune, 200},
                   {Item::ChaosRune, 60, "dark_magic"}, {Item::Staff, 3}, {Item::AirStaff, 1, "wizards_test"}, {Item::WaterStaff, 1, "wizards_test"},
                   {Item::EarthStaff, 1, "wizards_test"}, {Item::FireStaff, 1, "wizards_test"}, {Item::WizardHat, 2}, {Item::WizardRobe, 2}, {Item::WizardSkirt, 2}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_cook", "Hilda's Kitchen", 1.2f, 0.50f, kCatFood, {}};
        s.stock = {{Item::Bread, 20}, {Item::Shrimps, 12}, {Item::Anchovies, 8}, {Item::Chicken, 10}, {Item::Beef, 10}, {Item::Egg, 6},
                   {Item::Flour, 40}, {Item::JugOfWater, 60}, {Item::Cheese, 15}, {Item::Tomato, 25}, {Item::Potato, 25}, {Item::CookingApple, 20},
                   {Item::BucketOfMilk, 12}, {Item::Onion, 20}, {Item::Cabbage, 12}, {Item::Redberries, 20}, {Item::PieDish, 3}, {Item::Bowl, 3}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_crafting", "Ellis's Crafting Supplies", 1.15f, 0.45f, kCatMisc | kCatTool, {}};
        s.stock = {{Item::Needle, 5}, {Item::Thread, 60}, {Item::Chisel, 3}, {Item::RingMould, 3}, {Item::AmuletMould, 3}, {Item::Leather, 12},
                   {Item::Clay, 20}, {Item::SilverBar, 6}, {Item::GoldBar, 3}, {Item::Studs, 200}, {Item::Knife, 3},
                   {Item::LeatherBoots, 3}, {Item::LeatherGloves, 3}};
        v.push_back(s);
    }
    {   // the king's guild: a skill cape for a hero who has mastered the skill (level 99), the quest cape for one who has finished every quest
        ShopDef s{"masters", "The Royal Guild of Masters", 1.0f, 0.0f, 0u, {}};
        for (int i = 0; i < CapeCount(); ++i) {
            const CapeInfo& c = GetCape(i);
            ShopStockDef d{c.item, 1};
            if (c.allQuests) d.reqQuestPoints = -1;
            else {
                d.reqLevel = 99;
                d.reqSkill = c.skill;
            }
            s.stock.push_back(d);
        }
        v.push_back(s);
    }
    {
        ShopDef s{"lakeside_fishing", "Gerrant's Fishing Supplies", 1.15f, 0.50f, kCatFood | kCatTool, {}};
        s.stock = {{Item::SmallNet, 6}, {Item::FlyFishingRod, 3}, {Item::Harpoon, 3}, {Item::LobsterPot, 3}, {Item::Feathers, 300},
                   {Item::Tinderbox, 4}, {Item::RawShrimps, 10}, {Item::Bread, 10}, {Item::CastleScroll, 3}, {Item::MillbrookScroll, 3}};
        v.push_back(s);
    }
    {   // Elmswick: a woodcutters' village
        ShopDef s{"elm_general", "Perrin's General Store", 1.25f, 0.32f, kCatEverything, {}};
        s.stock = {{Item::Bread, 15}, {Item::Anchovies, 6}, {Item::Chicken, 8}, {Item::Beef, 8}, {Item::Logs, 40}, {Item::Tinderbox, 5}, {Item::BronzeAxe, 4},
                   {Item::SmallNet, 4}, {Item::Hammer, 3}, {Item::Knife, 4}, {Item::Spade, 4}, {Item::Candle, 40}, {Item::Potato, 20}, {Item::Onion, 15},
                   {Item::CastleScroll, 4}, {Item::MillbrookScroll, 4}, {Item::SaltmereScroll, 3}};
        v.push_back(s);
    }
    {
        ShopDef s{"elm_woodcraft", "Hobb's Woodcraft", 1.1f, 0.48f, kCatTool | kCatRanged, {}};
        s.stock = {{Item::BronzeAxe, 4}, {gear(Tier::Iron, GearType::Hatchet), 3}, {gear(Tier::Steel, GearType::Hatchet), 2}, {Item::Logs, 60},
                   {Item::OakLogs, 40}, {Item::WillowLogs, 30}, {Item::YewLogs, 10}, {Item::Knife, 4}, {Item::Bowstring, 60}, {Item::Tinderbox, 4}};
        v.push_back(s);
    }
    {   // Saltmere: a harbour on the eastern sea
        ShopDef s{"salt_market", "Brine's Fish Market", 1.15f, 0.55f, kCatFood | kCatTool, {}};
        s.stock = {{Item::SmallNet, 6}, {Item::FlyFishingRod, 4}, {Item::Harpoon, 4}, {Item::LobsterPot, 4}, {Item::Feathers, 300}, {Item::Tinderbox, 4},
                   {Item::RawSardine, 20}, {Item::RawHerring, 20}, {Item::RawMackerel, 12}, {Item::RawCod, 12}, {Item::Trout, 8}, {Item::Salmon, 6},
                   {Item::Tuna, 4}, {Item::Lobster, 3}, {Item::Bread, 10}, {Item::Pot, 4}, {Item::CastleScroll, 3}, {Item::ElmswickScroll, 3}};
        v.push_back(s);
    }
    {   // Sunfield Farm
        ShopDef s{"sunfield_farm", "Ada's Farm Stall", 1.15f, 0.50f, kCatFood, {}};
        s.stock = {{Item::Egg, 20}, {Item::BucketOfMilk, 15}, {Item::Cheese, 15}, {Item::Flour, 40}, {Item::Potato, 30}, {Item::Onion, 25},
                   {Item::Cabbage, 20}, {Item::Tomato, 25}, {Item::Redberries, 20}, {Item::CookingApple, 20}, {Item::Beef, 10}, {Item::Chicken, 10}, {Item::Bread, 15}};
        v.push_back(s);
    }
    {   // the swamp witch
        ShopDef s{"swamp_witch", "Old Maggie's Brews and Runes", 1.1f, 0.45f, kCatMagic | kCatMisc, {}};
        s.stock = {{Item::AirRune, 150}, {Item::WaterRune, 150}, {Item::EarthRune, 150}, {Item::MindRune, 150}, {Item::ChaosRune, 40, "dark_magic"},
                   {Item::Candle, 40}, {Item::Bowl, 4}, {Item::WizardHat, 1}, {Item::WizardRobe, 1}};
        v.push_back(s);
    }
    return v;
}

}  // namespace

const std::vector<ShopDef>& AllShopDefs() {
    static const std::vector<ShopDef> defs = BuildDefs();
    return defs;
}

int ShopIndex(const std::string& key) {
    const std::vector<ShopDef>& d = AllShopDefs();
    for (size_t i = 0; i < d.size(); ++i)
        if (d[i].key == key) return static_cast<int>(i);
    return -1;
}

void Shops::Init() {
    lines_.clear();
    for (const ShopDef& d : AllShopDefs()) {
        std::vector<Line> lines;
        for (const ShopStockDef& s : d.stock) lines.push_back({s.item, s.count, s.count, 0.0f});
        lines_.push_back(std::move(lines));
    }
}

const ShopDef& Shops::Def(int shop) const {
    const std::vector<ShopDef>& d = AllShopDefs();
    return d[static_cast<size_t>(std::clamp(shop, 0, static_cast<int>(d.size()) - 1))];
}

bool Shops::Buys(int shop, Item item) const {
    return ItemSellable(item) && (Def(shop).buys & CategoryOf(item)) != 0;
}

int Shops::BuyPrice(int shop, Item item) const {
    const int v = ItemValue(item);
    return v <= 0 ? 0 : std::max(1, static_cast<int>(std::lround(v * Def(shop).markup)));
}

int Shops::SellPrice(int shop, Item item) const {
    if (!Buys(shop, item)) return 0;
    return std::max(1, static_cast<int>(std::floor(ItemValue(item) * Def(shop).sellRate)));
}

int Shops::Take(int shop, int line, int qty) {
    if (shop < 0 || shop >= Count()) return 0;
    std::vector<Line>& ls = lines_[shop];
    if (line < 0 || line >= static_cast<int>(ls.size())) return 0;
    const int n = std::clamp(qty, 0, ls[line].count);
    ls[line].count -= n;
    if (ls[line].count <= 0 && ls[line].base <= 0) ls.erase(ls.begin() + line);
    return n;
}

void Shops::Add(int shop, Item item, int qty) {
    if (shop < 0 || shop >= Count() || qty <= 0) return;
    std::vector<Line>& ls = lines_[shop];
    for (Line& l : ls)
        if (l.item == item) {
            l.count += qty;
            return;
        }
    if (static_cast<int>(ls.size()) >= kMaxShopLines) return;   // (the shelves are full: it is simply bought)
    ls.push_back({item, qty, 0, 0.0f});
}

void Shops::Update(float dt) {
    for (auto& ls : lines_) {
        for (size_t i = 0; i < ls.size();) {
            Line& l = ls[i];
            const int step = std::max(1, l.base / 25);   // arrows and runes come in hundreds: they refill in bigger steps
            if (l.count < l.base) {
                l.timer += dt;
                if (l.timer >= kRestockSeconds) {
                    l.timer = 0.0f;
                    l.count = std::min(l.base, l.count + step);
                }
            } else if (l.count > l.base) {
                l.timer += dt;
                if (l.timer >= kExcessSeconds) {
                    l.timer = 0.0f;
                    l.count = std::max(l.base, l.count - std::max(1, l.count / 25));
                }
            } else {
                l.timer = 0.0f;
            }
            if (l.count <= 0 && l.base <= 0) ls.erase(ls.begin() + static_cast<long>(i));
            else ++i;
        }
    }
}

}  // namespace journey
