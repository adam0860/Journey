#include "game/Fishing.h"

#include <algorithm>

namespace journey {

float CatchChance(int level) {
    level = std::clamp(level, 1, 99);
    // 35% per cast at level 1, rising in a straight line to 90% at level 99.
    return 0.35f + (0.90f - 0.35f) * (level - 1) / 98.0f;
}

Item RollCatch(int level, double roll) {
    if (level >= kAnchovyLevel && roll < 0.30) return Item::RawAnchovies;
    return Item::RawShrimps;
}

int FishXp(Item rawFish) {
    switch (rawFish) {
        case Item::RawShrimps: return 10;
        case Item::RawAnchovies: return 40;
        default: return 0;
    }
}

int FishLevelRequired(Item rawFish) {
    return rawFish == Item::RawAnchovies ? kAnchovyLevel : 1;
}

FishEvent FishSession::Update(float dt, int level, const std::function<double()>& roll) {
    if (!Active()) return FishEvent::None;
    timer_ += dt;
    if (timer_ < kFishCastSeconds) return FishEvent::None;
    timer_ -= kFishCastSeconds;
    return roll() < CatchChance(level) ? FishEvent::Caught : FishEvent::Cast;
}

}  // namespace journey
