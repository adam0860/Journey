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

enum class Tier { Bronze, Iron, Steel, Mithril, Adamant };
constexpr int kTierCount = 5;

// The things a smith can make. Pickaxe and Hatchet (the woodcutter's axe) are tools (they are not worn); the rest are worn or wielded.
// (Arrows: fifteen arrowheads-and-shafts from a bar.) The last seven were added in round 10 and have their own run of item ids.
enum class GearType { Dagger, Sword, Axe, Mace, Helm, Shield, Legs, Body, Pickaxe, Hatchet, Arrows,
                      Scimitar, Longsword, Spear, Warhammer, Fullhelm, SquareShield, Chainbody,
                      Boots, Gauntlets };   // (round 11: boots and gauntlets have a third run of item ids)
constexpr int kGearTypeCount = 20;
constexpr int kWornTypeCount = 8;      // Dagger..Body (the first run of item ids: tier * 8 + type)
constexpr int kExtraTypeFirst = 11;    // Scimitar..Chainbody (the second run: tier * 7 + type - 11)
constexpr int kExtraTypeCount = 7;
constexpr int kExtra3First = 18;       // Boots, Gauntlets (the third run: tier * 2 + type - 18)
constexpr int kExtra3Count = 2;
// Is it something worn or wielded (not a tool and not ammunition)?
bool IsWornType(GearType g);

enum class EquipSlot { Head, Body, Legs, Weapon, Shield, Ammo, Ring, Amulet, Feet, Hands, Cape };   // (saved by number: only ever add at the end)
constexpr int kEquipSlotCount = 11;

// How the hero swings a weapon (which animation). Daggers stab, swords slash, battleaxes chop, maces crush.
// Bows shoot and staffs cast (they are not part of the smithed set: see Archery.h and Magic.h).
enum class WeaponAnim { Unarmed, Stab, Slash, Chop, Crush, Shoot, Cast };

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
// How many 0.6 s ticks one swing of a wielded weapon takes (Item::None = fists). Daggers are quick, axes and hammers slow; everything
// else (and bows and staffs) takes the usual 4. Gameplay turns this into the hero's swing speed and a matching damage scale.
int WeaponTicks(Item weapon);

// Skill capes: bought from the king for 99,000 coins by a hero who has reached level 99 in the skill (the Quest cape: who has finished every
// quest). They are worn on the back. Each gives a little protection and a bonus that suits its skill.
struct GearStats;
struct CapeInfo {
    Item item;
    const char* name;      // "Mining cape"
    const char* key;       // "mining": models are gear_cape_<key>.glb (worn) and drop_cape_<key>.glb (lying)
    Skill skill;           // the skill that must be 99 (ignored for the quest cape)
    bool allQuests;        // the quest cape
    Rgb main, trim;        // the cloth and its edging (for the icon and the model)
    int attack, strength, defence, ranged, magic;
};
int CapeCount();
const CapeInfo& GetCape(int index);
const CapeInfo* FindCape(Item item);
constexpr int kCapePrice = 99000;

// Bonuses an item adds while worn (added up over the slots by Equipment).
struct GearStats {
    int attack = 0;     // accuracy
    int strength = 0;   // max hit
    int defence = 0;    // armour
    int ranged = 0;     // bows: ranged accuracy
    int rangedStrength = 0;   // arrows: ranged max hit
    int magic = 0;      // staffs and robes: magic accuracy
};
GearStats GetGearStats(Tier t, GearType g);
GearStats ItemStats(Item item);   // zeros for anything that is not gear

// The level needed to use a piece: Attack for weapons, Defence for armour, Mining for pickaxes.
Skill RequirementSkill(GearType g);
// The skill and level needed to wield or wear ANY item (smithed gear, bows, arrows). false if it has no requirement.
bool ItemRequirement(Item item, Skill* skill, int* level);
int TierRequirement(Tier t);          // 1 / 5 / 20 / 30 / 40
// How much faster a pickaxe digs (1.0 bronze .. 2.1 mithril).
float PickaxeSpeed(Tier t);

}  // namespace journey
