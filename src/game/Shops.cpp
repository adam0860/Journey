#include "game/Shops.h"

#include <algorithm>
#include <cmath>

#include "game/Gear.h"

namespace journey {

unsigned CategoryOf(Item item) {
    Tier t;
    GearType g;
    if (GearOf(item, &t, &g)) {
        if (g == GearType::Pickaxe) return kCatTool;
        return SlotOf(g) == EquipSlot::Weapon ? kCatWeapon : kCatArmour;
    }
    const ItemInfo& info = GetItemInfo(item);
    switch (item) {
        case Item::CopperOre:
        case Item::TinOre:
        case Item::IronOre:
        case Item::Coal:
        case Item::MithrilOre: return kCatOre;
        case Item::BronzeBar:
        case Item::IronBar:
        case Item::SteelBar:
        case Item::MithrilBar: return kCatBar;
        case Item::Hammer:
        case Item::BronzeAxe:
        case Item::SmallNet:
        case Item::Tinderbox: return kCatTool;
        default: break;
    }
    if (info.food || info.raw || item == Item::Egg || item == Item::BurntFish || item == Item::BurntMeat || item == Item::Bones) return kCatFood;
    return kCatMisc;
}

namespace {

std::vector<ShopDef> BuildDefs() {
    std::vector<ShopDef> v;
    auto gear = [](Tier t, GearType g) { return GearItem(t, g); };
    {
        ShopDef s{"general_town", "Journey General Store", 1.35f, 0.30f, kCatEverything, {}};
        s.stock = {{Item::Bread, 12}, {Item::Shrimps, 8}, {Item::Chicken, 6}, {Item::Beef, 6}, {Item::Logs, 20}, {Item::Tinderbox, 3},
                   {Item::SmallNet, 3}, {Item::BronzeAxe, 3}, {Item::Hammer, 3}, {Item::BronzePickaxe, 3}};
        v.push_back(s);
    }
    {
        ShopDef s{"smith_town", "Torvald's Smithing Supplies", 1.15f, 0.45f, kCatOre | kCatBar | kCatTool | kCatWeapon | kCatArmour, {}};
        s.stock = {{Item::Hammer, 5}, {Item::BronzePickaxe, 3}, {Item::IronPickaxe, 2}, {Item::BronzeBar, 12}, {Item::IronBar, 6},
                   {Item::CopperOre, 20}, {Item::TinOre, 20}, {Item::Coal, 10}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_general", "Millbrook General Store", 1.25f, 0.32f, kCatEverything, {}};
        s.stock = {{Item::Bread, 15}, {Item::Shrimps, 10}, {Item::Anchovies, 6}, {Item::Chicken, 8}, {Item::Beef, 8}, {Item::Logs, 30},
                   {Item::Tinderbox, 5}, {Item::SmallNet, 5}, {Item::BronzeAxe, 4}, {Item::Hammer, 4}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_weapons", "Brannock's Blades", 1.0f, 0.48f, kCatWeapon, {}};
        for (Tier t : {Tier::Bronze, Tier::Iron, Tier::Steel})
            for (GearType g : {GearType::Dagger, GearType::Sword, GearType::Axe, GearType::Mace})
                s.stock.push_back({gear(t, g), t == Tier::Steel ? 1 : (t == Tier::Iron ? 2 : 3)});
        v.push_back(s);
    }
    {
        ShopDef s{"village_armour", "Mira's Armoury", 1.0f, 0.48f, kCatArmour, {}};
        for (Tier t : {Tier::Bronze, Tier::Iron, Tier::Steel})
            for (GearType g : {GearType::Helm, GearType::Shield, GearType::Legs, GearType::Body})
                s.stock.push_back({gear(t, g), t == Tier::Steel ? 1 : (t == Tier::Iron ? 2 : 3)});
        v.push_back(s);
    }
    {
        ShopDef s{"village_ore", "Dunmore's Ore and Bars", 1.1f, 0.50f, kCatOre | kCatBar, {}};
        s.stock = {{Item::CopperOre, 25}, {Item::TinOre, 25}, {Item::IronOre, 15}, {Item::Coal, 20}, {Item::MithrilOre, 2},
                   {Item::BronzeBar, 15}, {Item::IronBar, 8}, {Item::SteelBar, 4}, {Item::MithrilBar, 1}};
        v.push_back(s);
    }
    {
        ShopDef s{"village_cook", "Hilda's Kitchen", 1.2f, 0.50f, kCatFood, {}};
        s.stock = {{Item::Bread, 20}, {Item::Shrimps, 12}, {Item::Anchovies, 8}, {Item::Chicken, 10}, {Item::Beef, 10}, {Item::Egg, 6}};
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
            if (l.count < l.base) {
                l.timer += dt;
                if (l.timer >= kRestockSeconds) {
                    l.timer = 0.0f;
                    ++l.count;
                }
            } else if (l.count > l.base) {
                l.timer += dt;
                if (l.timer >= kExcessSeconds) {
                    l.timer = 0.0f;
                    --l.count;
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
