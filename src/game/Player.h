// The hero: tile-based pathing with smooth movement between tile centres.
#pragma once
#include <deque>
#include <vector>

#include "world/TileMap.h"

namespace journey {

class Player {
public:
    // Running speed. Classic RuneScape runs 2 tiles per 0.6s tick (~3.3 tiles/s).
    static constexpr float kRunSpeed = 3.3f;

    void Teleport(TileCoord t);
    void SetPath(const std::vector<TileCoord>& tiles);
    void Stop() { path_.clear(); }
    void Update(float dt);

    bool Moving() const { return !path_.empty(); }
    float X() const { return x_; }  // world position (tile units, tile centre = +0.5)
    float Z() const { return z_; }
    float FacingRadians() const { return facing_; }
    float WalkPhase() const { return phase_; }
    TileCoord Tile() const;
    const std::deque<TileCoord>& Path() const { return path_; }

private:
    float x_ = 0.5f;
    float z_ = 0.5f;
    float facing_ = 0.0f;  // 0 = facing +z (south)
    float phase_ = 0.0f;
    std::deque<TileCoord> path_;
};

}  // namespace journey
