#include "game/Gear.h"

#include "game/Archery.h"
#include "game/Crafting.h"
#include "game/Magic.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {

constexpr const char* kTierNames[kTierCount] = {"Bronze", "Iron", "Steel", "Mithril", "Adamant"};
constexpr const char* kTierKeys[kTierCount] = {"bronze", "iron", "steel", "mithril", "adamant"};
constexpr const char* kTypeNames[kGearTypeCount] = {"Dagger", "Sword", "Battleaxe", "Mace", "Medium helm", "Kiteshield", "Platelegs",
                                                    "Platebody", "Pickaxe", "Axe", "Arrows", "Scimitar", "Longsword", "Spear", "Warhammer",
                                                    "Full helm", "Square shield", "Chainbody", "Boots", "Gauntlets"};
constexpr const char* kTypeKeys[kGearTypeCount] = {"dagger", "sword", "axe", "mace", "helm", "shield", "legs", "body", "pickaxe", "hatchet", "arrows",
                                                   "scimitar", "longsword", "spear", "warhammer", "fullhelm", "squareshield", "chainbody", "boots", "gauntlets"};

// What a metal adds (a bronze piece is worth the first number, mithril the last). Each type then takes its share:
// daggers are accurate but weak, battleaxes clumsy but heavy, and body armour protects most.
constexpr int kAttack[kTierCount] = {6, 14, 24, 36, 50};
constexpr int kStrength[kTierCount] = {5, 12, 20, 32, 46};
constexpr int kDefence[kTierCount] = {4, 10, 18, 28, 40};

struct Share {
    float attack, strength, defence;
};
constexpr Share kShare[kGearTypeCount] = {
    {1.15f, 0.60f, 0.0f},   // dagger
    {1.00f, 1.00f, 0.0f},   // sword
    {0.70f, 1.70f, 0.0f},   // battleaxe
    {0.85f, 1.30f, 0.0f},   // mace
    {0.0f, 0.0f, 0.60f},    // helm
    {0.0f, 0.0f, 0.90f},    // shield
    {0.0f, 0.0f, 1.10f},    // legs
    {0.0f, 0.0f, 1.70f},    // body
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0},   // (pickaxe, axe, arrows are tools: no bonuses)
    {1.30f, 0.80f, 0.0f},   // scimitar: quick and accurate, but it does not hit as hard
    {1.05f, 1.20f, 0.0f},   // longsword: a sword with a longer reach into the strength column
    {1.00f, 0.95f, 0.0f},   // spear: steady and honest
    {0.55f, 2.10f, 0.0f},   // warhammer: slow to land, awful when it does
    {0.0f, 0.0f, 0.85f},    // full helm: more cover than the medium helm
    {0.0f, 0.0f, 1.15f},    // square shield: broader than the kiteshield
    {0.0f, 0.0f, 1.35f},    // chainbody: lighter mail, less protection than plate
    {0.0f, 0.0f, 0.45f},    // boots
    {0.15f, 0.20f, 0.40f},  // gauntlets: a little help for the fists as well
};

constexpr int kTierLevel[kTierCount] = {1, 5, 20, 30, 40};
constexpr float kPickSpeed[kTierCount] = {1.0f, 1.3f, 1.65f, 2.1f, 2.6f};

int Scaled(int base, float share) {
    return share <= 0.0f ? 0 : std::max(1, static_cast<int>(std::lround(base * share)));
}

}  // namespace

const char* TierName(Tier t) { return kTierNames[static_cast<int>(t)]; }
const char* TierKey(Tier t) { return kTierKeys[static_cast<int>(t)]; }
const char* GearTypeName(GearType g) { return kTypeNames[static_cast<int>(g)]; }
const char* GearTypeKey(GearType g) { return kTypeKeys[static_cast<int>(g)]; }

const char* SlotName(EquipSlot s) {
    switch (s) {
        case EquipSlot::Head: return "Head";
        case EquipSlot::Body: return "Body";
        case EquipSlot::Legs: return "Legs";
        case EquipSlot::Weapon: return "Weapon";
        case EquipSlot::Shield: return "Shield";
        case EquipSlot::Ammo: return "Quiver";
        case EquipSlot::Ring: return "Ring";
        case EquipSlot::Amulet: return "Amulet";
        case EquipSlot::Feet: return "Feet";
        case EquipSlot::Hands: return "Hands";
        case EquipSlot::Cape: return "Cape";
    }
    return "";
}

Rgb TierColor(Tier t) {
    switch (t) {
        case Tier::Bronze: return {198, 124, 62};
        case Tier::Iron: return {128, 132, 142};
        case Tier::Steel: return {186, 194, 206};
        case Tier::Mithril: return {84, 118, 214};
        case Tier::Adamant: return {70, 168, 108};
    }
    return {255, 255, 255};
}

bool IsWornType(GearType g) { return g != GearType::Pickaxe && g != GearType::Hatchet && g != GearType::Arrows; }

Item GearItem(Tier t, GearType g) {
    const int ti = static_cast<int>(t), gi = static_cast<int>(g);
    if (ti < 0 || ti >= kTierCount || gi < 0 || gi >= kGearTypeCount) return Item::None;
    if (gi >= kExtra3First) return static_cast<Item>(static_cast<int>(Item::Gear3First) + ti * kExtra3Count + (gi - kExtra3First));   // (round 11: boots, gauntlets)
    if (gi >= kExtraTypeFirst) return static_cast<Item>(static_cast<int>(Item::Gear2First) + ti * kExtraTypeCount + (gi - kExtraTypeFirst));   // (round 10: all five metals)
    if (t == Tier::Adamant) return static_cast<Item>(static_cast<int>(Item::AdamantFirst) + gi);   // (the fifth metal has its own run of ids)
    if (g == GearType::Pickaxe) return static_cast<Item>(static_cast<int>(Item::BronzePickaxe) + ti);
    if (g == GearType::Arrows) return static_cast<Item>(static_cast<int>(Item::BronzeArrows) + ti);
    if (g == GearType::Hatchet) return ti == 0 ? Item::BronzeAxe : static_cast<Item>(static_cast<int>(Item::IronAxe) + ti - 1);   // (the starter axe is the bronze one)
    return static_cast<Item>(static_cast<int>(Item::GearFirst) + ti * kWornTypeCount + gi);
}

bool GearOf(Item item, Tier* t, GearType* g) {
    const int id = static_cast<int>(item);
    {   // round 11: boots and gauntlets
        const int b = static_cast<int>(Item::Gear3First);
        if (id >= b && id < b + kTierCount * kExtra3Count) {
            if (t) *t = static_cast<Tier>((id - b) / kExtra3Count);
            if (g) *g = static_cast<GearType>(kExtra3First + (id - b) % kExtra3Count);
            return true;
        }
    }
    {   // round 10: scimitar, longsword, spear, warhammer, full helm, square shield and chainbody, tier by tier
        const int b = static_cast<int>(Item::Gear2First);
        if (id >= b && id < b + kTierCount * kExtraTypeCount) {
            if (t) *t = static_cast<Tier>((id - b) / kExtraTypeCount);
            if (g) *g = static_cast<GearType>(kExtraTypeFirst + (id - b) % kExtraTypeCount);
            return true;
        }
    }
    {   // adamant: dagger..body, pickaxe and axe (the arrows are not gear, as with the other metals)
        const int a = static_cast<int>(Item::AdamantFirst);
        if (id >= a && id < a + static_cast<int>(GearType::Arrows)) {
            if (t) *t = Tier::Adamant;
            if (g) *g = static_cast<GearType>(id - a);
            return true;
        }
    }
    if (item == Item::BronzeAxe || (id >= static_cast<int>(Item::IronAxe) && id <= static_cast<int>(Item::MithrilAxe))) {
        if (t) *t = item == Item::BronzeAxe ? Tier::Bronze : static_cast<Tier>(id - static_cast<int>(Item::IronAxe) + 1);
        if (g) *g = GearType::Hatchet;
        return true;
    }
    const int pick = static_cast<int>(Item::BronzePickaxe);
    if (id >= pick && id < pick + 4) {   // (bronze..mithril: adamant is above)
        if (t) *t = static_cast<Tier>(id - pick);
        if (g) *g = GearType::Pickaxe;
        return true;
    }
    const int first = static_cast<int>(Item::GearFirst);
    if (id >= first && id < first + 4 * kWornTypeCount) {
        if (t) *t = static_cast<Tier>((id - first) / kWornTypeCount);
        if (g) *g = static_cast<GearType>((id - first) % kWornTypeCount);
        return true;
    }
    return false;
}

EquipSlot SlotOf(GearType g) {
    switch (g) {
        case GearType::Helm:
        case GearType::Fullhelm: return EquipSlot::Head;
        case GearType::Body:
        case GearType::Chainbody: return EquipSlot::Body;
        case GearType::Legs: return EquipSlot::Legs;
        case GearType::Boots: return EquipSlot::Feet;
        case GearType::Gauntlets: return EquipSlot::Hands;
        case GearType::Shield:
        case GearType::SquareShield: return EquipSlot::Shield;
        default: return EquipSlot::Weapon;
    }
}


namespace {
// The skill capes. Colours: (cloth, edging). Bonuses: attack, strength, defence, ranged, magic.
const CapeInfo kCapes[] = {
    {Item::AttackCape, "Attack cape", "attack", Skill::Attack, false, {178, 38, 30}, {232, 190, 70}, 5, 0, 2, 0, 0},
    {Item::StrengthCape, "Strength cape", "strength", Skill::Strength, false, {212, 116, 26}, {74, 42, 16}, 0, 5, 2, 0, 0},
    {Item::DefenceCape, "Defence cape", "defence", Skill::Defence, false, {59, 111, 208}, {206, 212, 224}, 0, 0, 8, 0, 0},
    {Item::HitpointsCape, "Hitpoints cape", "hitpoints", Skill::Hitpoints, false, {236, 232, 224}, {208, 40, 40}, 0, 0, 4, 0, 0},
    {Item::RangedCape, "Ranged cape", "ranged", Skill::Ranged, false, {31, 94, 58}, {156, 110, 60}, 0, 0, 2, 6, 0},
    {Item::MagicCape, "Magic cape", "magic", Skill::Magic, false, {90, 63, 176}, {232, 190, 70}, 0, 0, 2, 0, 8},
    {Item::PrayerCape, "Prayer cape", "prayer", Skill::Prayer, false, {242, 240, 224}, {232, 190, 70}, 0, 0, 4, 0, 0},
    {Item::MiningCape, "Mining cape", "mining", Skill::Mining, false, {107, 111, 120}, {198, 124, 62}, 0, 0, 2, 0, 0},
    {Item::SmithingCape, "Smithing cape", "smithing", Skill::Smithing, false, {74, 77, 85}, {236, 130, 40}, 0, 0, 2, 0, 0},
    {Item::FishingCape, "Fishing cape", "fishing", Skill::Fishing, false, {47, 160, 192}, {240, 244, 248}, 0, 0, 2, 0, 0},
    {Item::CookingCape, "Cooking cape", "cooking", Skill::Cooking, false, {138, 75, 42}, {244, 232, 200}, 0, 0, 2, 0, 0},
    {Item::WoodcuttingCape, "Woodcutting cape", "woodcutting", Skill::Woodcutting, false, {94, 142, 52}, {96, 62, 34}, 0, 0, 2, 0, 0},
    {Item::FiremakingCape, "Firemaking cape", "firemaking", Skill::Firemaking, false, {216, 74, 26}, {250, 214, 80}, 0, 0, 2, 0, 0},
    {Item::CraftingCape, "Crafting cape", "crafting", Skill::Crafting, false, {192, 160, 80}, {110, 72, 40}, 0, 0, 2, 0, 0},
    {Item::QuestCape, "Quest cape", "quest", Skill::Attack, true, {28, 42, 92}, {232, 190, 70}, 2, 2, 4, 2, 2},
};
constexpr int kCapeCount = static_cast<int>(sizeof(kCapes) / sizeof(kCapes[0]));
}  // namespace

int CapeCount() { return kCapeCount; }
const CapeInfo& GetCape(int index) { return kCapes[index < 0 ? 0 : (index >= kCapeCount ? kCapeCount - 1 : index)]; }
const CapeInfo* FindCape(Item item) {
    for (const CapeInfo& c : kCapes)
        if (c.item == item) return &c;
    return nullptr;
}

namespace {
// Jewellery the jeweller makes: a ring or an amulet, each with a little bonus from its gem.
struct JewelleryInfo {
    Item item;
    EquipSlot slot;
    GearStats stats;
};
const JewelleryInfo* FindJewellery(Item item) {
    static const JewelleryInfo kJewellery[] = {
        {Item::GoldRing, EquipSlot::Ring, {0, 0, 1, 0, 0, 0}},
        {Item::SapphireRing, EquipSlot::Ring, {0, 0, 1, 0, 0, 4}},
        {Item::EmeraldRing, EquipSlot::Ring, {0, 0, 1, 4, 0, 0}},
        {Item::RubyRing, EquipSlot::Ring, {0, 4, 1, 0, 0, 0}},
        {Item::GoldAmulet, EquipSlot::Amulet, {0, 0, 2, 0, 0, 0}},
        {Item::SapphireAmulet, EquipSlot::Amulet, {0, 0, 2, 0, 0, 7}},
        {Item::EmeraldAmulet, EquipSlot::Amulet, {0, 0, 2, 7, 0, 0}},
        {Item::RubyAmulet, EquipSlot::Amulet, {3, 7, 2, 0, 0, 0}},
        {Item::OpalRing, EquipSlot::Ring, {0, 0, 1, 0, 0, 2}},
        {Item::OpalAmulet, EquipSlot::Amulet, {0, 0, 2, 0, 0, 3}},
        {Item::JadeRing, EquipSlot::Ring, {0, 0, 1, 2, 0, 0}},
        {Item::JadeAmulet, EquipSlot::Amulet, {0, 0, 2, 3, 0, 0}},
        {Item::RedTopazRing, EquipSlot::Ring, {0, 2, 1, 0, 0, 0}},
        {Item::RedTopazAmulet, EquipSlot::Amulet, {1, 3, 2, 0, 0, 0}},
        {Item::DiamondRing, EquipSlot::Ring, {1, 3, 2, 3, 0, 3}},
        {Item::DiamondAmulet, EquipSlot::Amulet, {2, 4, 3, 5, 0, 5}},
        {Item::DragonstoneAmulet, EquipSlot::Amulet, {5, 8, 4, 8, 0, 8}},
        {Item::MummysAmulet, EquipSlot::Amulet, {6, 8, 3, 0, 0, 0}},
        {Item::TrollToothAmulet, EquipSlot::Amulet, {0, 10, 6, 0, 0, 0}},
        {Item::AbyssalAmulet, EquipSlot::Amulet, {8, 12, 7, 10, 0, 10}},
        {Item::AbyssalRing, EquipSlot::Ring, {4, 5, 4, 5, 0, 5}},
    };
    for (const JewelleryInfo& j : kJewellery)
        if (j.item == item) return &j;
    return nullptr;
}
}  // namespace

bool Equippable(Item item, EquipSlot* slot) {
    if (FindCape(item)) {
        if (slot) *slot = EquipSlot::Cape;
        return true;
    }
    if (const JewelleryInfo* j = FindJewellery(item)) {
        if (slot) *slot = j->slot;
        return true;
    }
    if (IsBow(item) || IsStaff(item)) {
        if (slot) *slot = EquipSlot::Weapon;
        return true;
    }
    if (IsArrow(item)) {
        if (slot) *slot = EquipSlot::Ammo;
        return true;
    }
    if (const RobeInfo* robe = FindRobe(item)) {
        if (slot) *slot = static_cast<EquipSlot>(robe->slot);
        return true;
    }
    if (const LeatherInfo* leather = FindLeather(item)) {
        if (slot) *slot = static_cast<EquipSlot>(leather->slot);
        return true;
    }
    GearType g;
    if (!GearOf(item, nullptr, &g) || g == GearType::Pickaxe || g == GearType::Hatchet) return false;
    if (slot) *slot = SlotOf(g);
    return true;
}

WeaponAnim AnimOf(GearType g) {
    switch (g) {
        case GearType::Dagger:
        case GearType::Spear: return WeaponAnim::Stab;
        case GearType::Sword:
        case GearType::Scimitar:
        case GearType::Longsword: return WeaponAnim::Slash;
        case GearType::Axe: return WeaponAnim::Chop;
        case GearType::Mace:
        case GearType::Warhammer: return WeaponAnim::Crush;
        default: return WeaponAnim::Unarmed;
    }
}

int WeaponTicks(Item weapon) {
    if (const BowInfo* bow = FindBow(weapon)) return std::string(bow->model).find("longbow") != std::string::npos ? 5 : 3;   // a shortbow shoots quickly, a longbow draws slowly
    GearType g;
    if (!GearOf(weapon, nullptr, &g)) return kDefaultSwingTicks;
    switch (g) {
        case GearType::Dagger: return 3;
        case GearType::Longsword:
        case GearType::Axe: return 5;
        case GearType::Warhammer: return 6;
        default: return kDefaultSwingTicks;
    }
}

GearStats GetGearStats(Tier t, GearType g) {
    GearStats s;
    const int gi = static_cast<int>(g), ti = static_cast<int>(t);
    if (gi < 0 || gi >= kGearTypeCount || !IsWornType(g) || ti < 0 || ti >= kTierCount) return s;
    s.attack = Scaled(kAttack[ti], kShare[gi].attack);
    s.strength = Scaled(kStrength[ti], kShare[gi].strength);
    s.defence = Scaled(kDefence[ti], kShare[gi].defence);
    return s;
}

GearStats ItemStats(Item item) {
    if (const CapeInfo* c = FindCape(item)) {
        GearStats s;
        s.attack = c->attack;
        s.strength = c->strength;
        s.defence = c->defence;
        s.ranged = c->ranged;
        s.magic = c->magic;
        return s;
    }
    if (const JewelleryInfo* j = FindJewellery(item)) return j->stats;
    if (const BowInfo* bow = FindBow(item)) {
        GearStats s;
        s.ranged = bow->accuracy;
        return s;
    }
    if (const ArrowInfo* arrow = FindArrow(item)) {
        GearStats s;
        s.rangedStrength = arrow->strength;
        if (arrow->thrown) s.ranged = arrow->strength / 2 + 2;   // (a bow gives the aim for arrows; a dart brings its own)
        return s;
    }
    if (const StaffInfo* staff = FindStaff(item)) {
        GearStats s;
        s.magic = staff->accuracy;
        return s;
    }
    if (const RobeInfo* robe = FindRobe(item)) {
        GearStats s;
        s.magic = robe->magic;
        s.defence = robe->defence;
        return s;
    }
    if (const LeatherInfo* leather = FindLeather(item)) {
        GearStats s;
        s.defence = leather->defence;
        s.ranged = leather->ranged;
        return s;
    }
    Tier t;
    GearType g;
    if (!GearOf(item, &t, &g)) return {};
    return GetGearStats(t, g);
}

Skill RequirementSkill(GearType g) {
    switch (g) {
        case GearType::Dagger:
        case GearType::Sword:
        case GearType::Axe:
        case GearType::Mace:
        case GearType::Scimitar:
        case GearType::Longsword:
        case GearType::Spear:
        case GearType::Warhammer: return Skill::Attack;
        case GearType::Pickaxe: return Skill::Mining;
        case GearType::Hatchet: return Skill::Woodcutting;
        default: return Skill::Defence;
    }
}

bool ItemRequirement(Item item, Skill* skill, int* level) {
    Tier t;
    GearType g;
    if (GearOf(item, &t, &g)) {
        static const int kHatchetLevel[kTierCount] = {1, 6, 21, 31, 41};
        if (skill) *skill = RequirementSkill(g);
        if (level) *level = g == GearType::Hatchet ? kHatchetLevel[static_cast<int>(t)] : TierRequirement(t);
        return true;
    }
    if (const BowInfo* bow = FindBow(item)) {
        if (skill) *skill = Skill::Ranged;
        if (level) *level = bow->level;
        return true;
    }
    if (const ArrowInfo* arrow = FindArrow(item)) {
        if (skill) *skill = Skill::Ranged;
        if (level) *level = arrow->level;
        return true;
    }
    return false;   // staffs and robes can be worn by anybody
}

int TierRequirement(Tier t) { return kTierLevel[static_cast<int>(t)]; }
float PickaxeSpeed(Tier t) { return kPickSpeed[static_cast<int>(t)]; }

}  // namespace journey
