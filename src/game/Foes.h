// Other players as opponents: the link between the hero's combat (Gameplay) and the simulated adventurers (Adventurers). Pure C++.
//
// In the Abyss, and only there, the hero may fight another adventurer. A foe is named by an id that starts at kFoeIdBase (so it can travel
// through the hero's combat code, which names creatures by plain ids). Attacking a player who is not skulled and has not attacked you puts a
// SKULL over your head for ten minutes; a skulled player who dies loses everything they carry, and whoever is near can pick it up.
#pragma once
#include <string>

#include <algorithm>
#include <cstdlib>

#include "game/Combat.h"
#include "world/TileMap.h"

namespace journey {

constexpr int kFoeIdBase = 1000000;            // foe id = kFoeIdBase + adventurer id
constexpr float kSkullSeconds = 600.0f;        // ten minutes
// Places where dying costs you (the Abyss): you keep only the three most valuable items (none if you are skulled), the rest drops where you fell.
constexpr int kItemsKeptOnDeath = 3;
inline bool IsFoeId(int id) { return id >= kFoeIdBase; }

// The Abyss has a LEVEL that grows with depth (as the wilderness does in RuneScape): 1 just inside the gate, kAbyssMaxLevel at the far end. Two players
// may fight only if their combat levels are within that many levels of each other. Just inside, only players of nearly the same level can fight;
// deep down, a level-20 can be attacked by a level-70.
constexpr int kAbyssMaxLevel = 60;
inline int AbyssLevelAt(const TileMap& map, int x, int z) {
    if (map.AreaAt(x, z) != "The Abyss") return 0;
    for (const AreaDef& a : map.Areas()) {
        if (a.name != "The Abyss") continue;
        const int span = std::max(1, a.rect.z1 - a.rect.z0 - 1);
        return std::clamp(1 + (z - a.rect.z0) * (kAbyssMaxLevel - 1) / span, 1, kAbyssMaxLevel);
    }
    return 0;
}
// May two players fight, one at Abyss level `levelA` and the other at `levelB` (0 = not in the Abyss)? The shallower of the two decides.
inline bool CanFightPlayers(int combatA, int combatB, int levelA, int levelB) {
    const int level = std::min(levelA, levelB);
    return level > 0 && std::abs(combatA - combatB) <= level;
}

struct FoeView {
    int id = 0;               // the foe id (kFoeIdBase + adventurer id)
    std::string name;
    float x = 0, z = 0;
    int hp = 0, maxHp = 1;
    int combat = 1;
    bool skulled = false;
    bool alive = false;
    Fighter fighter;          // their numbers as the defender (levels + worn gear)
};

class FoeSource {
public:
    virtual ~FoeSource() = default;
    virtual bool Get(int foeId, FoeView* out) const = 0;
    // The hero hit them for `damage` (0 = a miss; they notice and fight back). True if that killed them (their things are on the ground).
    virtual bool HeroHits(int foeId, int damage) = 0;
    // A blow of theirs killed the hero (for their record).
    virtual void HeroKilledBy(int foeId) { (void)foeId; }
};

}  // namespace journey
