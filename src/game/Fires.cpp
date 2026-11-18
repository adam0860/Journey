#include "game/Fires.h"

#include <algorithm>
#include <cstdlib>

namespace journey {

float LightChance(int level) {
    level = std::clamp(level, 1, 99);
    // 40% per attempt at level 1, rising in a straight line to 95% at level 99.
    return 0.40f + (0.95f - 0.40f) * (level - 1) / 98.0f;
}

LightEvent LightSession::Update(float dt, int level, const std::function<double()>& roll) {
    if (!active_) return LightEvent::None;
    timer_ += dt;
    if (timer_ < kLightAttemptSeconds) return LightEvent::None;
    timer_ -= kLightAttemptSeconds;
    if (roll() < LightChance(level)) {
        active_ = false;
        return LightEvent::Lit;
    }
    return LightEvent::Failed;
}

bool Fires::CanLight(const TileMap& map, TileCoord t) const {
    // 'D' (a doorway) is walkable but you may not build a fire in it.
    if (!map.InBounds(t.x, t.z) || map.At(t.x, t.z) != '.' || map.IsBlocked(t)) return false;
    return BurningAt(t) == nullptr;
}

int Fires::Light(TileMap* map, TileCoord t) {
    if (!CanLight(*map, t)) return -1;
    // Fresh fire replaces any ashes left on the tile.
    fires_.erase(std::remove_if(fires_.begin(), fires_.end(), [&](const Fire& f) { return f.tile == t; }), fires_.end());
    Fire f;
    f.id = nextId_++;
    f.tile = t;
    fires_.push_back(f);
    map->SetBlocked(t, true);
    return f.id;
}

void Fires::Update(float dt, TileMap* map) {
    for (Fire& f : fires_) {
        f.age += dt;
        if (f.Burning()) {
            f.burnLeft -= dt;
            if (!f.Burning()) {
                f.burnLeft = 0.0f;
                map->SetBlocked(f.tile, false);
            }
        } else {
            f.ashLeft -= dt;
        }
    }
    fires_.erase(std::remove_if(fires_.begin(), fires_.end(), [](const Fire& f) { return !f.Burning() && f.ashLeft <= 0.0f; }),
                 fires_.end());
}

const Fire* Fires::Find(int id) const {
    for (const Fire& f : fires_)
        if (f.id == id) return &f;
    return nullptr;
}

const Fire* Fires::BurningAt(TileCoord t) const {
    for (const Fire& f : fires_)
        if (f.Burning() && f.tile == t) return &f;
    return nullptr;
}

const Fire* Fires::NearestBurning(TileCoord from) const {
    const Fire* best = nullptr;
    int bestD = 1 << 30;
    for (const Fire& f : fires_) {
        if (!f.Burning()) continue;
        const int d = std::abs(f.tile.x - from.x) + std::abs(f.tile.z - from.z);
        if (d < bestD) {
            bestD = d;
            best = &f;
        }
    }
    return best;
}

}  // namespace journey
