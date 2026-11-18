#include "game/Inventory.h"

#include <cctype>
#include <deque>
#include <string>

#include "game/Gear.h"
#include "game/Smithing.h"

namespace journey {

namespace {
// name, stackable, tool, raw, food, heals, buryXp, examine, ground model
const ItemInfo kBase[] = {
    {"nothing",           false, false, false, false, 0, 0, "Nothing to see here.", "bundle"},
    {"Logs",              false, false, false, false, 0, 0, "A number of wooden logs.", "logs"},
    {"Bronze axe",        false, true,  false, false, 0, 0, "A woodcutter's axe.", "bundle"},
    {"Small fishing net", false, true,  false, false, 0, 0, "Useful for catching small fish.", "bundle"},
    {"Tinderbox",         false, true,  false, false, 0, 0, "Useful for lighting a fire.", "bundle"},
    {"Raw shrimps",       false, false, true,  false, 0, 0, "I should cook this first.", "shrimp_raw"},
    {"Shrimps",           false, false, false, true,  3, 0, "Some nicely cooked shrimps.", "shrimp_cooked"},
    {"Raw anchovies",     false, false, true,  false, 0, 0, "I should cook this first.", "fish_raw"},
    {"Anchovies",         false, false, false, true,  1, 0, "Some nicely cooked anchovies.", "fish_cooked"},
    {"Burnt fish",        false, false, false, false, 0, 0, "Oops!", "fish_burnt"},
    {"Bones",             false, false, false, false, 0, 5, "Ew, a bone.", "bones"},
    {"Raw chicken",       false, false, true,  false, 0, 0, "I should cook this first.", "drumstick_raw"},
    {"Chicken",           false, false, false, true,  3, 0, "Mmm, this looks tasty.", "drumstick_cooked"},
    {"Raw beef",          false, false, true,  false, 0, 0, "I should cook this first.", "steak_raw"},
    {"Beef",              false, false, false, true,  3, 0, "Mmm, this looks tasty.", "steak_cooked"},
    {"Burnt meat",        false, false, false, false, 0, 0, "Oops!", "steak_burnt"},
    {"Egg",               false, false, false, true,  1, 0, "A nice fresh egg.", "egg"},
    {"Cowhide",           false, false, false, false, 0, 0, "Hair-covered hide from a cow.", "hide"},
    {"Copper ore",        false, false, false, false, 0, 0, "This could be smelted into a bronze bar.", "ore_copper"},
    {"Tin ore",           false, false, false, false, 0, 0, "This could be smelted into a bronze bar.", "ore_tin"},
    {"Iron ore",          false, false, false, false, 0, 0, "This needs to be smelted in a furnace.", "ore_iron"},
    {"Coal",              false, false, false, false, 0, 0, "Fuel for the furnace, and the secret of steel.", "ore_coal"},
    {"Mithril ore",       false, false, false, false, 0, 0, "A rare blue-grey ore. It needs a lot of coal.", "ore_mithril"},
    {"Bronze bar",        false, false, false, false, 0, 0, "A bar of bronze, ready for the anvil.", "bar_bronze"},
    {"Iron bar",          false, false, false, false, 0, 0, "A bar of iron, ready for the anvil.", "bar_iron"},
    {"Steel bar",         false, false, false, false, 0, 0, "A bar of steel, ready for the anvil.", "bar_steel"},
    {"Mithril bar",       false, false, false, false, 0, 0, "A bar of mithril, ready for the anvil.", "bar_mithril"},
    {"Hammer",            false, true,  false, false, 0, 0, "Good for hitting things. Smiths need one.", "hammer"},
};
constexpr int kBaseCount = static_cast<int>(sizeof(kBase) / sizeof(kBase[0]));
static_assert(kBaseCount == static_cast<int>(Item::BronzePickaxe), "kBase must list every item before the pickaxes");

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The whole table. Smithed gear is generated (4 metals x 9 types), so the names live in a pool that never moves.
struct Table {
    std::vector<ItemInfo> info;
    std::deque<std::string> pool;
    const char* Keep(const std::string& s) {
        pool.push_back(s);
        return pool.back().c_str();
    }
    Table() {
        info.assign(static_cast<size_t>(Item::Count), kBase[0]);
        for (int i = 0; i < kBaseCount; ++i) info[i] = kBase[i];
        {   // money: the first stackable item
            ItemInfo c = kBase[0];
            c.name = "Coins";
            c.stackable = true;
            c.examine = "Lovely money!";
            c.model = "coins";
            info[static_cast<size_t>(Item::Coins)] = c;
            ItemInfo b = kBase[0];
            b.name = "Bread";
            b.food = true;
            b.heals = 5;
            b.examine = "Nice crusty bread.";
            b.model = "bread";
            info[static_cast<size_t>(Item::Bread)] = b;
        }
        for (int ti = 0; ti < kTierCount; ++ti) {
            for (int gi = 0; gi < kGearTypeCount; ++gi) {
                const Tier t = static_cast<Tier>(ti);
                const GearType g = static_cast<GearType>(gi);
                const Item item = GearItem(t, g);
                const std::string tier = Lower(TierName(t));
                const std::string type = Lower(GearTypeName(g));
                std::string text;
                switch (g) {
                    case GearType::Dagger: text = "A short, sharp " + tier + " dagger."; break;
                    case GearType::Sword: text = "A well-balanced " + tier + " sword."; break;
                    case GearType::Axe: text = "A heavy " + tier + " battleaxe."; break;
                    case GearType::Mace: text = "A spiked " + tier + " mace."; break;
                    case GearType::Helm: text = "A sturdy " + tier + " helmet."; break;
                    case GearType::Shield: text = "A large " + tier + " kiteshield."; break;
                    case GearType::Legs: text = "Some " + tier + " plate armour for my legs."; break;
                    case GearType::Body: text = "A " + tier + " breastplate that covers my chest."; break;
                    case GearType::Pickaxe: text = "Used for mining. A " + tier + " pickaxe."; break;
                }
                ItemInfo ii = kBase[0];
                ii.name = Keep(std::string(TierName(t)) + " " + type);
                ii.examine = Keep(text);
                ii.model = Keep(std::string(TierKey(t)) + "_" + GearTypeKey(g));
                ii.tool = (g == GearType::Pickaxe && t == Tier::Bronze);   // the starter pickaxe cannot be dropped
                info[static_cast<size_t>(item)] = ii;
            }
        }
    }
};
const Table& GetTable() {
    static const Table table;
    return table;
}
}  // namespace

const ItemInfo& GetItemInfo(Item item) {
    const int i = static_cast<int>(item);
    const Table& t = GetTable();
    return t.info[(i >= 0 && i < static_cast<int>(Item::Count)) ? i : 0];
}

int ItemValue(Item item) {
    switch (item) {
        case Item::Logs: return 4;
        case Item::BronzeAxe: return 16;
        case Item::SmallNet: return 6;
        case Item::Tinderbox: return 4;
        case Item::RawShrimps: return 4;
        case Item::Shrimps: return 8;
        case Item::RawAnchovies: return 10;
        case Item::Anchovies: return 18;
        case Item::Bones: return 2;
        case Item::RawChicken: return 3;
        case Item::Chicken: return 9;
        case Item::RawBeef: return 4;
        case Item::Beef: return 12;
        case Item::Egg: return 3;
        case Item::Cowhide: return 14;
        case Item::CopperOre: return 8;
        case Item::TinOre: return 8;
        case Item::IronOre: return 28;
        case Item::Coal: return 34;
        case Item::MithrilOre: return 130;
        case Item::BronzeBar: return 22;
        case Item::IronBar: return 60;
        case Item::SteelBar: return 130;
        case Item::MithrilBar: return 330;
        case Item::Hammer: return 6;
        case Item::Bread: return 10;
        case Item::Coins: return 1;
        default: break;
    }
    Tier t;
    GearType g;
    if (GearOf(item, &t, &g)) {   // worth the bars it is made of, plus a little for the smith's work
        static const int kBar[kTierCount] = {22, 60, 130, 330};
        return static_cast<int>(GetSmithRecipe(t, g).bars * kBar[static_cast<int>(t)] * 1.15 + 0.5);
    }
    return 0;
}

bool ItemSellable(Item item) {
    return item != Item::None && item != Item::Coins && ItemValue(item) > 0 && !GetItemInfo(item).tool;
}

const char* ActionName(ItemAction a) {
    switch (a) {
        case ItemAction::Light: return "Light";
        case ItemAction::Cook: return "Cook";
        case ItemAction::Eat: return "Eat";
        case ItemAction::Bury: return "Bury";
        case ItemAction::Wield: return "Wield";
        case ItemAction::Wear: return "Wear";
        case ItemAction::Use: return "Use";
        case ItemAction::Drop: return "Drop";
        case ItemAction::Examine: return "Examine";
    }
    return "";
}

int Inventory::Add(Item item, int count) {
    if (item == Item::None || count <= 0) return 0;
    int added = 0;
    if (GetItemInfo(item).stackable) {
        for (ItemStack& s : slots_)
            if (s.item == item) {
                s.count += count;
                return count;
            }
    }
    for (ItemStack& s : slots_) {
        if (added >= count) break;
        if (s.item != Item::None) continue;
        if (GetItemInfo(item).stackable) {
            s = {item, count};
            return count;
        }
        s = {item, 1};
        ++added;
    }
    return added;
}

ItemStack Inventory::TakeSlot(int slot) {
    if (slot < 0 || slot >= kSlots) return {};
    const ItemStack s = slots_[slot];
    slots_[slot] = {};
    return s;
}

bool Inventory::Remove(Item item, int count) {
    if (item == Item::None || count <= 0 || Count(item) < count) return false;
    for (ItemStack& s : slots_) {
        if (count <= 0) break;
        if (s.item != item) continue;
        const int take = s.count < count ? s.count : count;
        s.count -= take;
        count -= take;
        if (s.count <= 0) s = {};
    }
    return true;
}

int Inventory::Count(Item item) const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == item) n += s.count;
    return n;
}

int Inventory::FindSlot(Item item) const {
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i].item == item) return i;
    return -1;
}

int Inventory::FindRawFood() const {
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i].item != Item::None && GetItemInfo(slots_[i].item).raw) return i;
    return -1;
}

int Inventory::FreeSlots() const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == Item::None) ++n;
    return n;
}

std::vector<ItemAction> ActionsFor(Item item, const Inventory& bag) {
    std::vector<ItemAction> out;
    if (item == Item::None) return out;
    const ItemInfo& info = GetItemInfo(item);
    // The most useful button first (a tap on the item's first button is the "default" action), then the common ones.
    if (info.food) out.push_back(ItemAction::Eat);
    if (info.buryXp > 0) out.push_back(ItemAction::Bury);
    if (info.raw) out.push_back(ItemAction::Cook);
    EquipSlot slot;
    if (Equippable(item, &slot)) out.push_back((slot == EquipSlot::Weapon || slot == EquipSlot::Shield) ? ItemAction::Wield : ItemAction::Wear);
    if (item == Item::Logs && bag.Has(Item::Tinderbox)) out.push_back(ItemAction::Light);
    if (item == Item::Tinderbox && bag.Has(Item::Logs)) out.push_back(ItemAction::Light);
    out.push_back(ItemAction::Use);
    if (!info.tool) out.push_back(ItemAction::Drop);
    out.push_back(ItemAction::Examine);
    return out;
}

int GiveStarterKit(Inventory* bag) {
    int added = 0;
    for (Item tool : {Item::BronzeAxe, Item::SmallNet, Item::Tinderbox, Item::BronzePickaxe, Item::Hammer})
        if (!bag->Has(tool)) added += bag->Add(tool);
    return added;
}

}  // namespace journey
