#include "game/Fishing.h"

#include <algorithm>

#include "game/Skilling.h"

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
    const FishDef* f = FindFish(rawFish);
    return f ? f->xp : 0;
}

int FishLevelRequired(Item rawFish) {
    const FishDef* f = FindFish(rawFish);
    return f ? f->level : 1;
}

FishEvent FishSession::Update(float dt, int level, const std::function<double()>& roll) {
    if (!Active()) return FishEvent::None;
    timer_ += dt;
    if (timer_ < kFishCastSeconds) return FishEvent::None;
    timer_ -= kFishCastSeconds;
    return roll() < CatchChance(level) ? FishEvent::Caught : FishEvent::Cast;
}

}  // namespace journey
