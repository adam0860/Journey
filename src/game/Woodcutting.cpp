#include "game/Woodcutting.h"

#include <algorithm>

namespace journey {

float ChopChance(int level) {
    level = std::clamp(level, 1, 99);
    // 25% per swing at level 1, rising in a straight line to 78% at level 99 (about the classic curve).
    const float lo = 64.0f / 256.0f, hi = 200.0f / 256.0f;
    return lo + (hi - lo) * (level - 1) / 98.0f;
}

ChopEvent ChopSession::Update(float dt, int level, const std::function<double()>& roll) {
    if (!Active()) return ChopEvent::None;
    timer_ += dt;
    if (timer_ < kChopTickSeconds) return ChopEvent::None;
    timer_ -= kChopTickSeconds;
    if (roll() < ChopChance(level)) {
        Stop();
        return ChopEvent::GotLog;
    }
    return ChopEvent::Swing;
}

}  // namespace journey
