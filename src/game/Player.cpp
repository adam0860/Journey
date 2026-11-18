#include "game/Player.h"

#include <cmath>

namespace journey {

void Player::Teleport(TileCoord t) {
    x_ = t.x + 0.5f;
    z_ = t.z + 0.5f;
    path_.clear();
}

void Player::FaceToward(float wx, float wz) {
    const float dx = wx - x_, dz = wz - z_;
    if (dx * dx + dz * dz > 1e-6f) facing_ = std::atan2(dx, dz);
}

void Player::SetPath(const std::vector<TileCoord>& tiles) {
    path_.assign(tiles.begin(), tiles.end());
}

TileCoord Player::Tile() const {
    return {static_cast<int>(std::floor(x_)), static_cast<int>(std::floor(z_))};
}

void Player::Update(float dt) {
    float budget = kRunSpeed * dt;  // distance we may travel this frame
    while (budget > 0.0f && !path_.empty()) {
        const float tx = path_.front().x + 0.5f;
        const float tz = path_.front().z + 0.5f;
        const float dx = tx - x_;
        const float dz = tz - z_;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 1e-4f) facing_ = std::atan2(dx, dz);
        if (dist <= budget) {
            x_ = tx;
            z_ = tz;
            budget -= dist;
            path_.pop_front();
        } else {
            x_ += dx / dist * budget;
            z_ += dz / dist * budget;
            budget = 0.0f;
        }
    }
    if (!path_.empty()) phase_ += dt * kRunSpeed * 3.2f;  // leg swing in step with ground speed
}

}  // namespace journey
