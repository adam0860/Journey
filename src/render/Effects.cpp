#include "render/Effects.h"

#include <algorithm>
#include <cmath>

#include "game/Gear.h"

namespace journey {

namespace {
constexpr float kBurstSeconds = 0.40f;

Color Alpha(Color c, float a) {
    return {c.r, c.g, c.b, static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255.0f)};
}
}  // namespace

void Effects::AddShot(const ShotFx& fx, float startY, float endY) {
    Shot s;
    s.fx = fx;
    s.y0 = startY;
    s.y1 = endY;
    shots_.push_back(s);
    if (shots_.size() > 24) shots_.erase(shots_.begin());
}

void Effects::Update(float dt) {
    for (Shot& s : shots_) s.age += dt;
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(),
                                [](const Shot& s) { return s.age > s.fx.delay + s.fx.flight + (s.fx.magic ? kBurstSeconds : 0.05f); }),
                 shots_.end());
}

// Where the missile is after a fraction u of its flight: a straight line from the hand to the target, lifted into a gentle arc
// (arrows drop a little; spells fly flat).
Vector3 Effects::PositionAt(const Shot& s, float u) {
    const float dx = s.fx.toX - s.fx.fromX, dz = s.fx.toZ - s.fx.fromZ;
    const float len = std::max(0.01f, std::sqrt(dx * dx + dz * dz));
    const float startOff = 0.45f;   // it leaves from in front of the hero, not from inside him
    const float ax = s.fx.fromX + dx / len * startOff, az = s.fx.fromZ + dz / len * startOff;
    Vector3 p;
    p.x = ax + (s.fx.toX - ax) * u;
    p.z = az + (s.fx.toZ - az) * u;
    p.y = s.y0 + (s.y1 - s.y0) * u;
    if (!s.fx.magic) p.y += 4.0f * u * (1.0f - u) * std::min(0.9f, 0.07f * len);
    return p;
}

void Effects::Draw() const {
    for (const Shot& s : shots_) {
        const float t = s.age - s.fx.delay;
        const float flight = std::max(0.02f, s.fx.flight);
        if (t < 0.0f) continue;
        const Color col = {s.fx.color.r, s.fx.color.g, s.fx.color.b, 255};
        if (!s.fx.magic) {
            if (t > flight) continue;
            const float u = std::clamp(t / flight, 0.0f, 1.0f);
            const Vector3 head = PositionAt(s, u);
            const Vector3 ahead = PositionAt(s, std::min(1.0f, u + 0.03f));
            Vector3 dir = {ahead.x - head.x, ahead.y - head.y, ahead.z - head.z};
            const float dl = std::max(0.0001f, std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z));
            dir = {dir.x / dl, dir.y / dl, dir.z / dl};
            const Vector3 tail = {head.x - dir.x * 0.55f, head.y - dir.y * 0.55f, head.z - dir.z * 0.55f};
            const Rgb m = TierColor(static_cast<Tier>(std::clamp(s.fx.metal, 0, kTierCount - 1)));
            DrawCylinderEx(tail, head, 0.013f, 0.013f, 5, {196, 150, 96, 255});
            DrawCylinderEx(head, {head.x + dir.x * 0.11f, head.y + dir.y * 0.11f, head.z + dir.z * 0.11f}, 0.032f, 0.0f, 5, {m.r, m.g, m.b, 255});
            DrawCylinderEx(tail, {tail.x + dir.x * 0.15f, tail.y + dir.y * 0.15f, tail.z + dir.z * 0.15f}, 0.036f, 0.012f, 4, {240, 240, 232, 255});
            continue;
        }
        // a spell: a glowing ball with a short trail, and a burst where it lands
        BeginBlendMode(BLEND_ADDITIVE);
        if (t <= flight) {
            const float u = std::clamp(t / flight, 0.0f, 1.0f);
            for (int k = 5; k >= 1; --k) {
                const Vector3 p = PositionAt(s, std::max(0.0f, u - 0.05f * k));
                DrawSphereEx(p, 0.13f - 0.018f * k, 6, 6, Alpha(col, 0.30f - 0.045f * k));
            }
            const Vector3 p = PositionAt(s, u);
            DrawSphereEx(p, 0.21f, 8, 8, Alpha(col, 0.32f));
            DrawSphereEx(p, 0.13f, 8, 8, Alpha(col, 0.85f));
            DrawSphereEx(p, 0.07f, 6, 6, Alpha({255, 250, 235, 255}, 0.95f));
        } else {
            const float v = std::clamp((t - flight) / kBurstSeconds, 0.0f, 1.0f);
            const Vector3 at = {s.fx.toX, s.y1, s.fx.toZ};
            DrawSphereEx(at, 0.18f + 0.55f * v, 10, 10, Alpha(col, 0.55f * (1.0f - v)));
            DrawSphereEx(at, 0.10f + 0.25f * v, 8, 8, Alpha({255, 250, 235, 255}, 0.8f * (1.0f - v)));
            for (int i = 0; i < 9; ++i) {   // sparks flying out
                const float ang = i * 2.399f, up = 0.3f + 0.2f * (i % 3);
                const float sp = 0.25f + 0.3f * v;
                const Vector3 q = {at.x + std::cos(ang) * sp, at.y + up * v - 0.4f * v * v, at.z + std::sin(ang) * sp};
                DrawSphereEx(q, 0.035f * (1.0f - 0.5f * v), 4, 4, Alpha(col, 1.0f - v));
            }
        }
        EndBlendMode();
    }
}

}  // namespace journey
