// Smithed gear: weapons, armour, shields and pickaxes in four metals. Pure C++ (unit tested).
//
// Everything here is DATA about items: which item is "an iron mace", what slot it goes in, its bonuses, the level you
// need to use it. The item ids live in Inventory.h (the 32 smithed pieces are consecutive, so an item is just
// "tier * 8 + type" - see GearItem / GearOf).
#pragma once
#include <string>

#include "game/Combat.h"   // Rgb
#include "game/Inventory.h"
#include "game/Skills.h"

namespace journey {

enum class Tier { Bronze, Iron, Steel, Mithril };
constexpr int kTierCount = 4;

// The things a smith can make. Pickaxe is a tool (it is not worn); the other eight are worn or wielded.
enum class GearType { Dagger, Sword, Axe, Mace, Helm, Shield, Legs, Body, Pickaxe };
constexpr int kGearTypeCount = 9;
constexpr int kWornTypeCount = 8;   // Dagger..Body

enum class EquipSlot { Head, Body, Legs, Weapon, Shield };
constexpr int kEquipSlotCount = 5;

// How the hero swings a weapon (which animation). Daggers stab, swords slash, battleaxes chop, maces crush.
enum class WeaponAnim { Unarmed, Stab, Slash, Chop, Crush };

const char* TierName(Tier t);        // "Bronze"
const char* TierKey(Tier t);         // "bronze"   (model and save-file names)
const char* GearTypeName(GearType g);   // "Dagger", "Battleaxe", "Medium helm"...
const char* GearTypeKey(GearType g);    // "dagger", "axe", "helm"... (model names)
const char* SlotName(EquipSlot s);      // "Head", "Weapon"...
Rgb TierColor(Tier t);               // the metal's colour (for icons)

// The item for a tier and type (Item::None for an invalid combination). Pickaxes are separate items.
Item GearItem(Tier t, GearType g);
// Is this item one of the smithed pieces (or a pickaxe)? Fills tier and type.
bool GearOf(Item item, Tier* t, GearType* g);
// The slot a worn type goes in. Only valid for the eight worn types.
EquipSlot SlotOf(GearType g);
// Where can this item be worn or wielded? false for everything else (including pickaxes).
bool Equippable(Item item, EquipSlot* slot);
WeaponAnim AnimOf(GearType g);

// Bonuses an item adds while worn (added up over the five slots by Equipment).
struct GearStats {
    int attack = 0;     // accuracy
    int strength = 0;   // max hit
    int defence = 0;    // armour
};
GearStats GetGearStats(Tier t, GearType g);
GearStats ItemStats(Item item);   // zeros for anything that is not gear

// The level needed to use a piece: Attack for weapons, Defence for armour, Mining for pickaxes.
Skill RequirementSkill(GearType g);
int TierRequirement(Tier t);          // 1 / 5 / 20 / 30
// How much faster a pickaxe digs (1.0 bronze .. 2.1 mithril).
float PickaxeSpeed(Tier t);

}  // namespace journey
