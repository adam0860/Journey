// What the hero is wearing and wielding: head, body, legs, weapon and shield. Pure C++ (unit tested).
//
// Worn pieces add their bonuses to the combat maths (Gameplay hands Bonuses() to PlayerFighter) and change how the hero
// looks and swings. Putting something on swaps it with what was there, which goes back into the bag.
#pragma once
#include <array>
#include <string>

#include "game/Gear.h"
#include "game/Inventory.h"
#include "game/Skills.h"

namespace journey {

struct EquipBonuses {
    int attack = 0, strength = 0, defence = 0;
    int ranged = 0, rangedStrength = 0, magic = 0;
};

class Equipment {
public:
    Item Get(EquipSlot s) const { return slots_[static_cast<int>(s)]; }
    void Set(EquipSlot s, Item item) { slots_[static_cast<int>(s)] = item; }   // for loading a save
    // The quiver: how many arrows of the kind in the Ammo slot you carry. Zero arrows empties the slot.
    int AmmoCount() const { return ammoCount_; }
    void SetAmmo(Item arrows, int count);
    int UseAmmo(int n = 1);   // takes up to n arrows out of the quiver; returns how many were taken
    bool Empty() const;
    bool IsWorn(Item item) const;

    // The bonuses of everything worn, added together.
    EquipBonuses Total() const;
    // The weapon's swing (Unarmed with nothing in the hand).
    WeaponAnim Anim() const;
    bool HasShield() const { return Get(EquipSlot::Shield) != Item::None; }

private:
    std::array<Item, kEquipSlotCount> slots_{};
    int ammoCount_ = 0;
};

enum class EquipResult { Done, NotWearable, LevelTooLow, BagFull };

// Wields / wears the item in bag slot `bagSlot`. The piece that was in that equipment slot takes its place in the bag.
// `needed` (optional) gets "Attack level 5" style text when the level is too low.
EquipResult EquipFromBag(Equipment* eq, Inventory* bag, int bagSlot, const Skills& skills, std::string* needed = nullptr);
// Takes a piece off and puts it in the bag (BagFull if there is no room, NotWearable if the slot is empty).
EquipResult Unequip(Equipment* eq, Inventory* bag, EquipSlot slot);

}  // namespace journey
