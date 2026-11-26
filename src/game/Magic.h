// Magic: runes, staffs, wizard robes and the spell list. Pure C++ (unit tested).
//
// A staff in the weapon hand turns every attack into the chosen spell. Each cast uses up runes from the bag (the elemental staffs
// supply their own element for free), gives Magic XP and hurts for the spell's own damage (the Magic level and the staff only
// decide whether it hits). Runes are bought in the shops or found on goblins. Item ids live in Inventory.h.
#pragma once
#include <string>
#include <vector>

#include "game/Combat.h"   // Rgb
#include "game/Inventory.h"

namespace journey {

enum class Rune { Air, Water, Earth, Fire, Mind, Chaos };
constexpr int kRuneCount = 6;

const char* RuneName(Rune r);   // "Air rune"
Item RuneItem(Rune r);
bool IsRune(Item item, Rune* r = nullptr);

struct StaffInfo {
    Item item;
    const char* name;
    const char* model;      // held_<model>.glb / drop_<model>.glb
    int accuracy;           // magic attack bonus
    int value;
    bool infinite;          // supplies endless runes of `element`
    Rune element;
    const char* examine;
};
constexpr int kStaffCount = 5;
const StaffInfo& GetStaff(int index);          // 0 staff, 1 air, 2 water, 3 earth, 4 fire
const StaffInfo* FindStaff(Item item);         // nullptr if it is not a staff
inline bool IsStaff(Item item) { return FindStaff(item) != nullptr; }

// Wizard robes: weak armour, good for casting.
struct RobeInfo {
    Item item;
    const char* name;
    const char* model;      // gear_<model>.glb (hat) or gear_<model>_<part>.glb
    int slot;               // 0 head, 1 body, 2 legs (EquipSlot order)
    int magic, defence;
    int value;
    const char* examine;
};
constexpr int kRobeCount = 3;
const RobeInfo& GetRobe(int index);
const RobeInfo* FindRobe(Item item);

struct RuneCost {
    Rune rune;
    int count;
};

struct SpellInfo {
    const char* key;        // "fire_strike" (the save file)
    const char* name;       // "Fire Strike"
    int level;              // Magic level needed
    int costCount;
    RuneCost cost[3];
    int maxHit;
    int xp;                 // Magic XP for casting it (plus 2 per point of damage)
    Rune element;           // its colour, and which staff gives its element for free
    const char* description;
};

const std::vector<SpellInfo>& AllSpells();
int SpellCount();
const SpellInfo& GetSpell(int index);
int FindSpell(const std::string& key);   // index, or -1
Rgb SpellColor(const SpellInfo& s);      // the glow of the spell

enum class CastCheck { Ok, LevelTooLow, NoRunes };
// Can the hero cast this now? `weapon` is the item in his hand (an elemental staff pays for its element).
CastCheck CheckCast(const SpellInfo& s, const Inventory& bag, Item weapon, int magicLevel);
// Takes the runes of one cast out of the bag (the runes a staff supplies are not taken).
void PayRunes(const SpellInfo& s, Inventory* bag, Item weapon);
// How many of a rune one cast of `s` takes from the bag with this weapon.
int RunesNeeded(const SpellInfo& s, Rune r, Item weapon);

}  // namespace journey
