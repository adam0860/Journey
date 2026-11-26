#include "game/Crafting.h"

#include "game/Gear.h"

#include <algorithm>
#include <cctype>

namespace journey {

namespace {

constexpr Item N = Item::None;

const std::vector<CraftRecipe>& Table() {
    static const std::vector<CraftRecipe> t = {
        // ---- the tanner: hides to leather (two coins each)
        {CraftKind::Tan, Item::Leather, 1, 1, 0, {Item::Cowhide, Item::Coins, N}, {1, 2, 0}, N},
        {CraftKind::Tan, Item::HardLeather, 1, 10, 12, {Item::Cowhide, Item::Coins, N}, {2, 6, 0}, N},
        // ---- leatherwork (a needle and thread)
        {CraftKind::Leather, Item::LeatherCap, 1, 7, 25, {Item::Leather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::LeatherBody, 1, 14, 60, {Item::Leather, Item::Thread, N}, {3, 2, 0}, Item::Needle},
        {CraftKind::Leather, Item::LeatherChaps, 1, 18, 50, {Item::Leather, Item::Thread, N}, {2, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::LeatherGloves, 1, 3, 15, {Item::Leather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::LeatherBoots, 1, 8, 24, {Item::Leather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::HardLeatherGloves, 1, 23, 70, {Item::HardLeather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::HardLeatherBoots, 1, 25, 80, {Item::HardLeather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::StuddedBoots, 1, 36, 35, {Item::HardLeatherBoots, Item::Studs, N}, {1, 3, 0}, N},
        {CraftKind::Leather, Item::HardLeatherCap, 1, 22, 75, {Item::HardLeather, Item::Thread, N}, {1, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::HardLeatherChaps, 1, 27, 110, {Item::HardLeather, Item::Thread, N}, {2, 1, 0}, Item::Needle},
        {CraftKind::Leather, Item::HardLeatherBody, 1, 30, 150, {Item::HardLeather, Item::Thread, N}, {3, 2, 0}, Item::Needle},
        {CraftKind::Leather, Item::StuddedCap, 1, 33, 40, {Item::HardLeatherCap, Item::Studs, N}, {1, 4, 0}, N},
        {CraftKind::Leather, Item::StuddedChaps, 1, 38, 55, {Item::HardLeatherChaps, Item::Studs, N}, {1, 6, 0}, N},
        {CraftKind::Leather, Item::StuddedBody, 1, 41, 70, {Item::HardLeatherBody, Item::Studs, N}, {1, 8, 0}, N},
        // ---- gem cutting (a chisel)
        {CraftKind::Gem, Item::Opal, 1, 1, 15, {Item::UncutOpal, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Jade, 1, 13, 20, {Item::UncutJade, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::RedTopaz, 1, 16, 25, {Item::UncutRedTopaz, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Sapphire, 1, 20, 50, {Item::UncutSapphire, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Emerald, 1, 27, 68, {Item::UncutEmerald, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Ruby, 1, 34, 85, {Item::UncutRuby, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Diamond, 1, 43, 107, {Item::UncutDiamond, N, N}, {1, 0, 0}, Item::Chisel},
        {CraftKind::Gem, Item::Dragonstone, 1, 55, 138, {Item::UncutDragonstone, N, N}, {1, 0, 0}, Item::Chisel},
        // ---- jewellery (a gold bar, a gem and a mould, at the jeweller's bench)
        {CraftKind::Jewellery, Item::GoldRing, 1, 5, 15, {Item::GoldBar, N, N}, {1, 0, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::OpalRing, 1, 10, 28, {Item::GoldBar, Item::Opal, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::JadeRing, 1, 16, 32, {Item::GoldBar, Item::Jade, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::RedTopazRing, 1, 19, 36, {Item::GoldBar, Item::RedTopaz, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::OpalAmulet, 1, 13, 38, {Item::GoldBar, Item::Opal, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::JadeAmulet, 1, 19, 44, {Item::GoldBar, Item::Jade, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::RedTopazAmulet, 1, 22, 50, {Item::GoldBar, Item::RedTopaz, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::SapphireRing, 1, 20, 40, {Item::GoldBar, Item::Sapphire, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::EmeraldRing, 1, 27, 55, {Item::GoldBar, Item::Emerald, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::RubyRing, 1, 34, 70, {Item::GoldBar, Item::Ruby, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::GoldAmulet, 1, 8, 30, {Item::GoldBar, N, N}, {1, 0, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::SapphireAmulet, 1, 24, 65, {Item::GoldBar, Item::Sapphire, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::EmeraldAmulet, 1, 31, 80, {Item::GoldBar, Item::Emerald, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::RubyAmulet, 1, 40, 100, {Item::GoldBar, Item::Ruby, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::DiamondRing, 1, 43, 85, {Item::GoldBar, Item::Diamond, N}, {1, 1, 0}, Item::RingMould},
        {CraftKind::Jewellery, Item::DiamondAmulet, 1, 50, 108, {Item::GoldBar, Item::Diamond, N}, {1, 1, 0}, Item::AmuletMould},
        {CraftKind::Jewellery, Item::DragonstoneAmulet, 1, 55, 150, {Item::GoldBar, Item::Dragonstone, N}, {1, 1, 0}, Item::AmuletMould},
        // ---- pottery (clay, at the wheel)
        {CraftKind::Pottery, Item::Pot, 1, 1, 6, {Item::Clay, N, N}, {1, 0, 0}, N},
        {CraftKind::Pottery, Item::PieDish, 1, 7, 15, {Item::Clay, N, N}, {1, 0, 0}, N},
        {CraftKind::Pottery, Item::Bowl, 1, 8, 15, {Item::Clay, N, N}, {1, 0, 0}, N},
        // ---- the kitchen (Cooking): doughs and the raw dishes you then bake on a fire or a range. A pie needs a pie dish (it is not used up).
        {CraftKind::Kitchen, Item::BreadDough, 1, 1, 4, {Item::Flour, Item::JugOfWater, N}, {1, 1, 0}, N, Skill::Cooking},
        {CraftKind::Kitchen, Item::PastryDough, 1, 10, 8, {Item::Flour, Item::JugOfWater, N}, {2, 1, 0}, N, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawRedberryPie, 1, 10, 12, {Item::PastryDough, Item::Redberries, N}, {1, 1, 0}, Item::PieDish, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawMeatPie, 1, 20, 18, {Item::PastryDough, Item::RawBeef, N}, {1, 1, 0}, Item::PieDish, Skill::Cooking},
        {CraftKind::Kitchen, Item::UncookedStew, 1, 25, 20, {Item::Potato, Item::RawBeef, Item::JugOfWater}, {1, 1, 1}, Item::Bowl, Skill::Cooking},
        {CraftKind::Kitchen, Item::PizzaBase, 1, 25, 10, {Item::PastryDough, Item::Flour, N}, {1, 1, 0}, N, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawApplePie, 1, 30, 22, {Item::PastryDough, Item::CookingApple, N}, {1, 1, 0}, Item::PieDish, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawGardenPie, 1, 34, 24, {Item::PastryDough, Item::Tomato, Item::Onion}, {1, 1, 1}, Item::PieDish, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawPizza, 1, 35, 26, {Item::PizzaBase, Item::Cheese, Item::Tomato}, {1, 1, 1}, N, Skill::Cooking},
        {CraftKind::Kitchen, Item::UncookedCake, 1, 40, 50, {Item::Flour, Item::Egg, Item::BucketOfMilk}, {1, 2, 1}, N, Skill::Cooking},
        {CraftKind::Kitchen, Item::RawFishPie, 1, 47, 40, {Item::PastryDough, Item::Trout, Item::Potato}, {1, 1, 1}, Item::PieDish, Skill::Cooking},
        // ---- woodwork (a knife): carve logs into bows, then string them. Trains Woodcutting.
        {CraftKind::Fletching, Item::UnstrungShortbow, 1, 5, 10, {Item::Logs, N, N}, {1, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::UnstrungLongbow, 1, 10, 15, {Item::Logs, N, N}, {2, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::UnstrungOakShortbow, 1, 20, 33, {Item::OakLogs, N, N}, {1, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::UnstrungOakLongbow, 1, 25, 50, {Item::OakLogs, N, N}, {2, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::UnstrungYewShortbow, 1, 50, 85, {Item::YewLogs, N, N}, {1, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::UnstrungYewLongbow, 1, 55, 100, {Item::YewLogs, N, N}, {2, 0, 0}, Item::Knife, Skill::Woodcutting},
        {CraftKind::Fletching, Item::Shortbow, 1, 5, 10, {Item::UnstrungShortbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
        {CraftKind::Fletching, Item::Longbow, 1, 10, 15, {Item::UnstrungLongbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
        {CraftKind::Fletching, Item::OakShortbow, 1, 20, 33, {Item::UnstrungOakShortbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
        {CraftKind::Fletching, Item::OakLongbow, 1, 25, 50, {Item::UnstrungOakLongbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
        {CraftKind::Fletching, Item::YewShortbow, 1, 50, 85, {Item::UnstrungYewShortbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
        {CraftKind::Fletching, Item::YewLongbow, 1, 55, 100, {Item::UnstrungYewLongbow, Item::Bowstring, N}, {1, 1, 0}, N, Skill::Woodcutting},
    };
    return t;
}

const LeatherInfo kLeather[] = {
    {Item::LeatherCap, 0, 2, 2, "gear_leather_cap"},
    {Item::LeatherBody, 1, 5, 6, "gear_leather_body"},
    {Item::LeatherChaps, 2, 3, 4, "gear_leather_chaps"},
    {Item::HardLeatherCap, 0, 4, 3, "gear_hard_cap"},
    {Item::HardLeatherBody, 1, 9, 9, "gear_hard_body"},
    {Item::HardLeatherChaps, 2, 6, 6, "gear_hard_chaps"},
    {Item::StuddedCap, 0, 6, 4, "gear_studded_cap"},
    {Item::StuddedBody, 1, 13, 12, "gear_studded_body"},
    {Item::StuddedChaps, 2, 9, 8, "gear_studded_chaps"},
    {Item::LeatherBoots, static_cast<int>(EquipSlot::Feet), 1, 1, "gear_boots_leather"},
    {Item::HardLeatherBoots, static_cast<int>(EquipSlot::Feet), 3, 2, "gear_boots_hard"},
    {Item::StuddedBoots, static_cast<int>(EquipSlot::Feet), 4, 3, "gear_boots_studded"},
    {Item::LeatherGloves, static_cast<int>(EquipSlot::Hands), 1, 1, "gear_gloves_leather"},
    {Item::HardLeatherGloves, static_cast<int>(EquipSlot::Hands), 2, 2, "gear_gloves_hard"},
};

}  // namespace

const std::vector<CraftRecipe>& AllCraftRecipes() { return Table(); }

const char* CraftStation(CraftKind k) {
    switch (k) {
        case CraftKind::Tan: return "tanner";
        case CraftKind::Jewellery: return "jeweller";
        case CraftKind::Pottery: return "wheel";
        default: return "";   // (leatherwork, gems, the kitchen and woodwork can be done anywhere)
    }
}

const char* CraftTitle(CraftKind k) {
    switch (k) {
        case CraftKind::Tan: return "Tanner";
        case CraftKind::Leather: return "Leatherwork";
        case CraftKind::Gem: return "Gem cutting";
        case CraftKind::Jewellery: return "Jeweller's bench";
        case CraftKind::Pottery: return "Potter's wheel";
        case CraftKind::Kitchen: return "Kitchen";
        case CraftKind::Fletching: return "Woodwork";
    }
    return "";
}

bool CraftKindForUse(Item a, Item b, CraftKind* kind) {
    if (a == Item::None || b == Item::None || a == b) return false;   // (two of the same thing are not a recipe)
    for (const CraftRecipe& r : Table()) {
        if (CraftStation(r.kind)[0] != '\0') continue;   // only the kinds you can do anywhere
        auto uses = [&](Item i) { return r.tool == i || r.input[0] == i || r.input[1] == i || r.input[2] == i; };
        if (uses(a) && uses(b)) {
            if (kind) *kind = r.kind;
            return true;
        }
    }
    return false;
}

int CanCraft(const CraftRecipe& r, const Inventory& bag) {
    if (r.tool != Item::None && !bag.Has(r.tool)) return 0;
    int n = 1000;
    for (int i = 0; i < 3; ++i)
        if (r.input[i] != Item::None && r.count[i] > 0) n = std::min(n, bag.Count(r.input[i]) / r.count[i]);
    return n == 1000 ? 0 : n;
}

std::string IngredientText(const CraftRecipe& r) {
    std::string out;
    for (int i = 0; i < 3; ++i) {
        if (r.input[i] == Item::None || r.count[i] <= 0) continue;
        if (!out.empty()) out += " + ";
        std::string name = GetItemInfo(r.input[i]).name;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        out += std::to_string(r.count[i]) + " " + name;
    }
    return out;
}

const LeatherInfo* FindLeather(Item item) {
    for (const LeatherInfo& l : kLeather)
        if (l.item == item) return &l;
    return nullptr;
}

}  // namespace journey
