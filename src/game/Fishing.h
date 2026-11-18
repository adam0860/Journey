// Net fishing rules. Pure C++ (unit tested).
//
// You stand on the bank next to a fishing spot and keep casting your net until the bag is full or you walk away.
// A cast takes 2 game ticks (1.2 s). At level 1 you mostly catch shrimps; from level 15 anchovies turn up as well.
#pragma once
#include <functional>

#include "game/Inventory.h"

namespace journey {

constexpr float kFishCastSeconds = 1.2f;
constexpr int kAnchovyLevel = 15;

// Chance (0..1) that a cast catches something, for a Fishing level 1..99.
float CatchChance(int fishingLevel);
// Which fish a successful cast brings in (`roll` in [0,1)).
Item RollCatch(int fishingLevel, double roll);
// Fishing XP for one raw fish, and the level you need to catch it.
int FishXp(Item rawFish);
int FishLevelRequired(Item rawFish);

enum class FishEvent { None, Cast, Caught };

// One in-progress fishing job at one spot. Unlike chopping it keeps going after a catch.
class FishSession {
public:
    void Start(int spotIndex) {
        spot_ = spotIndex;
        timer_ = 0.0f;
    }
    void Stop() { spot_ = -1; }
    bool Active() const { return spot_ >= 0; }
    int Spot() const { return spot_; }

    // Advance time; at most one event per call. Cast = a cast that caught nothing, Caught = a fish.
    FishEvent Update(float dt, int fishingLevel, const std::function<double()>& roll);

private:
    int spot_ = -1;
    float timer_ = 0.0f;
};

}  // namespace journey
