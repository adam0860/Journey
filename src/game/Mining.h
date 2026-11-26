// Mining rules and the state of the rocks. Pure C++ (unit tested).
//
// You stand next to a rock and swing your pickaxe until you get an ore (or walk away). A swing takes 2.4 seconds
// (a faster pickaxe swings sooner). Every rock gives ONE ore and then turns into bare stone that grows back after a
// while - quickly for copper and tin, slowly for coal and mithril.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "game/Gear.h"
#include "game/Inventory.h"
#include "world/TileMap.h"

namespace journey {

enum class RockKind { Copper, Tin, Iron, Coal, Mithril, Clay, Silver, Gold, Gems, Adamant };
constexpr int kRockKindCount = 10;

struct RockInfo {
    const char* name;     // "Copper rocks"
    const char* key;      // "copper" (the map file and model names)
    Item ore;
    int level;            // Mining level needed
    int xp;               // Mining XP for one ore
    float respawn;        // seconds until the rock grows back
    float baseChance;     // chance that a swing yields ore, at exactly the required level
    const char* examine;
};
const RockInfo& GetRockInfo(RockKind k);
bool RockKindFromKey(const std::string& key, RockKind* out);

constexpr float kMineSwingSeconds = 2.4f;
constexpr float kRockGrowSeconds = 1.2f;   // a rock that has grown back scales up for a moment

// Chance (0..1) that one swing gets the ore, for a Mining level.
float MineChance(RockKind k, int miningLevel);

// What a successful swing gives (`roll` in [0,1)): the rock's ore, or for gem rocks one of three uncut gems.
Item RockYield(RockKind k, double roll);

// The best pickaxe in the bag that the Mining level allows (Item::None if there is none).
Item BestPickaxe(const Inventory& bag, int miningLevel);

enum class MineEvent { None, Swing, GotOre };

// One in-progress mining job on one rock.
class MineSession {
public:
    void Start(int rockIndex) {
        rock_ = rockIndex;
        timer_ = 0.0f;
    }
    void Stop() { rock_ = -1; }
    bool Active() const { return rock_ >= 0; }
    int Rock() const { return rock_; }
    // How far through the current swing (0..1); the pickaxe lands where this wraps round.
    float Phase() const { return timer_ / kMineSwingSeconds; }
    // Advance time; at most one event per call. `speed` = the pickaxe's speed (Gear.h PickaxeSpeed).
    MineEvent Update(float dt, RockKind kind, int miningLevel, float speed, const std::function<double()>& roll);

private:
    int rock_ = -1;
    float timer_ = 0.0f;
};

struct RockState {
    RockDef def;
    RockKind kind = RockKind::Copper;
    bool depleted = false;
    float respawnLeft = 0.0f;
    float growAnim = 1.0f;   // 0..1 after growing back
    float hitAge = 99.0f;    // seconds since the last swing at it (the renderer shakes it a little)
};

class Rocks {
public:
    void Init(const std::vector<RockDef>& defs);
    void Update(float dt);

    const std::vector<RockState>& All() const { return rocks_; }
    int IndexAtTile(TileCoord t) const;
    // Fresh rock -> bare rock. Returns false if it already was.
    bool Deplete(int index);
    void Hit(int index) {
        if (index >= 0 && index < static_cast<int>(rocks_.size())) rocks_[index].hitAge = 0.0f;
    }
    float SecondsUntilRespawn(int index) const;

private:
    std::vector<RockState> rocks_;
};

}  // namespace journey
