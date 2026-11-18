#include "game/Gear.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {

constexpr const char* kTierNames[kTierCount] = {"Bronze", "Iron", "Steel", "Mithril"};
constexpr const char* kTierKeys[kTierCount] = {"bronze", "iron", "steel", "mithril"};
constexpr const char* kTypeNames[kGearTypeCount] = {"Dagger", "Sword", "Battleaxe", "Mace", "Medium helm", "Kiteshield", "Platelegs",
                                                    "Platebody", "Pickaxe"};
constexpr const char* kTypeKeys[kGearTypeCount] = {"dagger", "sword", "axe", "mace", "helm", "shield", "legs", "body", "pickaxe"};

// What a metal adds (a bronze piece is worth the first number, mithril the last). Each type then takes its share:
// daggers are accurate but weak, battleaxes clumsy but heavy, and body armour protects most.
constexpr int kAttack[kTierCount] = {6, 14, 24, 36};
constexpr int kStrength[kTierCount] = {5, 12, 20, 32};
constexpr int kDefence[kTierCount] = {4, 10, 18, 28};

struct Share {
    float attack, strength, defence;
};
constexpr Share kShare[kWornTypeCount] = {
    {1.15f, 0.60f, 0.0f},   // dagger
    {1.00f, 1.00f, 0.0f},   // sword
    {0.70f, 1.70f, 0.0f},   // battleaxe
    {0.85f, 1.30f, 0.0f},   // mace
    {0.0f, 0.0f, 0.60f},    // helm
    {0.0f, 0.0f, 0.90f},    // shield
    {0.0f, 0.0f, 1.10f},    // legs
    {0.0f, 0.0f, 1.70f},    // body
};

constexpr int kTierLevel[kTierCount] = {1, 5, 20, 30};
constexpr float kPickSpeed[kTierCount] = {1.0f, 1.3f, 1.65f, 2.1f};

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
    }
    return "";
}

Rgb TierColor(Tier t) {
    switch (t) {
        case Tier::Bronze: return {198, 124, 62};
        case Tier::Iron: return {128, 132, 142};
        case Tier::Steel: return {186, 194, 206};
        case Tier::Mithril: return {84, 118, 214};
    }
    return {255, 255, 255};
}

Item GearItem(Tier t, GearType g) {
    const int ti = static_cast<int>(t), gi = static_cast<int>(g);
    if (ti < 0 || ti >= kTierCount || gi < 0 || gi >= kGearTypeCount) return Item::None;
    if (g == GearType::Pickaxe) return static_cast<Item>(static_cast<int>(Item::BronzePickaxe) + ti);
    return static_cast<Item>(static_cast<int>(Item::GearFirst) + ti * kWornTypeCount + gi);
}

bool GearOf(Item item, Tier* t, GearType* g) {
    const int id = static_cast<int>(item);
    const int pick = static_cast<int>(Item::BronzePickaxe);
    if (id >= pick && id < pick + kTierCount) {
        if (t) *t = static_cast<Tier>(id - pick);
        if (g) *g = GearType::Pickaxe;
        return true;
    }
    const int first = static_cast<int>(Item::GearFirst);
    if (id >= first && id < first + kTierCount * kWornTypeCount) {
        if (t) *t = static_cast<Tier>((id - first) / kWornTypeCount);
        if (g) *g = static_cast<GearType>((id - first) % kWornTypeCount);
        return true;
    }
    return false;
}

EquipSlot SlotOf(GearType g) {
    switch (g) {
        case GearType::Helm: return EquipSlot::Head;
        case GearType::Body: return EquipSlot::Body;
        case GearType::Legs: return EquipSlot::Legs;
        case GearType::Shield: return EquipSlot::Shield;
        default: return EquipSlot::Weapon;
    }
}

bool Equippable(Item item, EquipSlot* slot) {
    GearType g;
    if (!GearOf(item, nullptr, &g) || g == GearType::Pickaxe) return false;
    if (slot) *slot = SlotOf(g);
    return true;
}

WeaponAnim AnimOf(GearType g) {
    switch (g) {
        case GearType::Dagger: return WeaponAnim::Stab;
        case GearType::Sword: return WeaponAnim::Slash;
        case GearType::Axe: return WeaponAnim::Chop;
        case GearType::Mace: return WeaponAnim::Crush;
        default: return WeaponAnim::Unarmed;
    }
}

GearStats GetGearStats(Tier t, GearType g) {
    GearStats s;
    const int gi = static_cast<int>(g), ti = static_cast<int>(t);
    if (gi < 0 || gi >= kWornTypeCount || ti < 0 || ti >= kTierCount) return s;
    s.attack = Scaled(kAttack[ti], kShare[gi].attack);
    s.strength = Scaled(kStrength[ti], kShare[gi].strength);
    s.defence = Scaled(kDefence[ti], kShare[gi].defence);
    return s;
}

GearStats ItemStats(Item item) {
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
        case GearType::Mace: return Skill::Attack;
        case GearType::Pickaxe: return Skill::Mining;
        default: return Skill::Defence;
    }
}

int TierRequirement(Tier t) { return kTierLevel[static_cast<int>(t)]; }
float PickaxeSpeed(Tier t) { return kPickSpeed[static_cast<int>(t)]; }

}  // namespace journey
