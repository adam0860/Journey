// The content of the gathering skills: which trees, logs and axes there are, which fishing spots and fish. Pure C++ (unit tested).
//
// This is all DATA: adding an oak tree or a swordfish is one row in Skilling.cpp. The rules that use it are in Gameplay.
#pragma once
#include <string>
#include <vector>

#include "game/Gear.h"
#include "game/Inventory.h"

namespace journey {

// ------------------------------------------------------------------------------------------------ woodcutting and firemaking
struct TreeInfo {
    const char* kind;      // the map's tree kind: "round", "oak" ...
    const char* name;      // "Oak tree"
    int level;             // Woodcutting level needed
    int xp;                // Woodcutting XP for one log
    Item logs;
    float regrow;          // seconds until the stump grows back
    float difficulty;      // multiplies the chance of a log per swing (harder trees are slower)
    const char* examine;
};
const TreeInfo& GetTreeInfo(const std::string& kind);   // unknown kinds are ordinary trees

struct LogInfo {
    Item logs;
    int fmLevel;           // Firemaking level to burn it
    int fmXp;
};
const LogInfo* GetLogInfo(Item logs);   // nullptr if the item is not a kind of log
inline bool IsLog(Item i) { return GetLogInfo(i) != nullptr; }
// The first log in the bag the Firemaking level can burn (the lowest kind first), or Item::None.
Item BurnableLog(const Inventory& bag, int firemakingLevel);
// The first log of any kind in the bag, or Item::None.
Item AnyLog(const Inventory& bag);

// Axes (woodcutting): the starter bronze axe and three better ones.
Item BestHatchet(const Inventory& bag, int woodcuttingLevel);   // Item::None if there is none he can use
bool IsHatchet(Item item);
int HatchetLevel(Item item);        // Woodcutting level to use it
float HatchetSpeed(Item item);      // 1.0 bronze .. 1.9 mithril: multiplies the chance of a log per swing
// Chance (0..1) that one swing at this kind of tree gets a log.
float TreeChopChance(const TreeInfo& tree, int woodcuttingLevel, float hatchetSpeed);

// ------------------------------------------------------------------------------------------------ fishing
struct FishDef {
    Item raw;
    int level;             // Fishing level needed
    int xp;
    int weight;            // how common it is among the fish you can catch
};
struct SpotInfo {
    const char* kind;      // the map's spot kind: "net", "rod", "harpoon", "cage"
    const char* name;      // "Net fishing spot"
    const char* verb;      // "Net", "Lure", "Harpoon", "Cage" (the menu)
    Item tool;             // what you must carry
    Item bait;             // used up on every cast (Item::None = nothing)
    const char* start;     // "You cast out your net..."
    std::vector<FishDef> fish;
};
const SpotInfo& GetSpotInfo(const std::string& kind);   // unknown kinds are net spots
// The fish a Fishing level can catch here, picked by weight (`roll` in [0,1)). Item::None if the level is too low for any.
Item RollSpotCatch(const SpotInfo& spot, int fishingLevel, double roll);
const FishDef* FindFish(Item raw);   // nullptr if the item is not a fish

}  // namespace journey
