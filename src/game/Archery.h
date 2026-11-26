// Archery: bows and arrows. Pure C++ (unit tested).
//
// A bow goes in the weapon hand and arrows in the quiver slot (EquipSlot::Ammo). Every shot uses one arrow, and a quarter of
// them fall to the ground near the target where they can be picked up again. A bow shoots from a distance (its range, in
// tiles) and over low things such as fences: see TileMap::ShotPasses.
//
// Bows give an accuracy bonus, arrows give the strength (max hit) bonus: that is the whole of the numbers. Both need Ranged
// levels to use. The item ids live in Inventory.h.
#pragma once
#include "game/Inventory.h"

namespace journey {

struct BowInfo {
    Item item;
    const char* name;
    const char* model;       // "shortbow": model files are held_<model>.glb and drop_<model>.glb
    int level;               // Ranged level to wield it
    int range;               // how many tiles away it can shoot
    int accuracy;            // ranged attack bonus
    int value;
    const char* examine;
};

struct ArrowInfo {
    Item item;
    const char* name;
    const char* model;
    int metal;               // 0 bronze .. 4 adamant (the same order as Tier in Gear.h)
    int level;               // Ranged level to use them
    int strength;            // ranged strength bonus (raises the max hit)
    int value;               // coins for ONE arrow
    const char* examine;
    bool thrown = false;     // a dart: thrown by hand, so it needs no bow (and nothing in the weapon slot)
};

constexpr int kBowCount = 6;
constexpr int kArrowKinds = 5;
constexpr int kDartKinds = 5;
constexpr int kThrowRange = 5;      // how far a dart flies (tiles)
constexpr int kThrowTicks = 3;      // a dart is thrown every 3 ticks (a quick, light attack)

const BowInfo& GetBow(int index);            // 0..5: shortbow, longbow, oak shortbow, oak longbow, yew shortbow, yew longbow
const BowInfo* FindBow(Item item);           // nullptr if the item is not a bow
const ArrowInfo& GetArrow(int index);        // 0..4: bronze .. adamant
const ArrowInfo& GetDart(int index);         // 0..4: bronze .. adamant darts (also found by FindArrow: they live in the quiver slot)
const ArrowInfo* FindArrow(Item item);       // nullptr if the item is not an arrow
inline bool IsBow(Item item) { return FindBow(item) != nullptr; }
inline bool IsArrow(Item item) { return FindArrow(item) != nullptr; }   // (arrows and darts: whatever goes in the quiver slot)
inline bool IsThrown(Item item) {
    const ArrowInfo* a = FindArrow(item);
    return a && a->thrown;
}

// Fraction of the arrows you fire that land on the ground (the rest break). Used with a roll in [0,1).
constexpr double kArrowSurvival = 0.25;
// How far an arrow can fly when the style is "longrange" (every bow gets this many extra tiles).
constexpr int kLongrangeBonus = 2;

}  // namespace journey
