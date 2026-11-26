#include "render/CameraRig.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {
constexpr float kDeg2Rad = 3.14159265f / 180.0f;
constexpr float kMinDist = 6.0f;
constexpr float kMaxDist = 30.0f;
constexpr float kFocusHeight = 1.0f;
}  // namespace

void CameraRig::SnapTo(float x, float z) {
    fx_ = x;
    fz_ = z;
}

void CameraRig::Follow(float x, float z, float dt) {
    // Frame-rate independent smoothing.
    const float k = 1.0f - std::exp(-9.0f * dt);
    fx_ += (x - fx_) * k;
    fz_ += (z - fz_) * k;
}

void CameraRig::ZoomBy(float d) {
    dist_ = std::clamp(dist_ + d, kMinDist, kMaxDist);
}

void CameraRig::SetView(float yawDeg, float pitchDeg, float dist) {
    yaw_ = yawDeg;
    pitch_ = std::clamp(pitchDeg, 20.0f, 80.0f);
    dist_ = std::clamp(dist, kMinDist, kMaxDist);
}

namespace {
constexpr float kMaxPitch = 82.0f;
}

void CameraRig::Avoid(const std::function<bool(Vector3, Vector3)>& blocked, float dt) {
    const Vector3 focus = {fx_, kFocusHeight, fz_};
    const float yaw = yaw_ * kDeg2Rad;
    float need = 0.0f;   // the smallest lift that gives a clear view
    for (; pitch_ + need < kMaxPitch; need += 4.0f) {
        const float p = (pitch_ + need) * kDeg2Rad;
        const Vector3 at = {fx_ + std::sin(yaw) * std::cos(p) * dist_, kFocusHeight + std::sin(p) * dist_, fz_ + std::cos(yaw) * std::cos(p) * dist_};
        if (!blocked(focus, at)) break;
    }
    need = std::max(0.0f, std::min(need, kMaxPitch - pitch_));
    const float k = 1.0f - std::exp(-(need > lift_ ? 8.0f : 3.0f) * dt);   // up quickly, down gently
    lift_ += (need - lift_) * k;
}

Vector3 CameraRig::Position() const {
    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = (pitch_ + lift_) * kDeg2Rad;
    return {fx_ + std::sin(yaw) * std::cos(pitch) * dist_, kFocusHeight + std::sin(pitch) * dist_,
            fz_ + std::cos(yaw) * std::cos(pitch) * dist_};
}

Camera3D CameraRig::Get() const {
    Camera3D cam{};
    cam.position = Position();
    cam.target = {fx_, kFocusHeight, fz_};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 38.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

bool CameraRig::ScreenToGround(Vector2 screen, float* x, float* z) const {
    const Ray ray = GetScreenToWorldRay(screen, Get());
    if (ray.direction.y > -1e-4f) return false;
    const float t = -ray.position.y / ray.direction.y;
    *x = ray.position.x + ray.direction.x * t;
    *z = ray.position.z + ray.direction.z * t;
    return true;
}

}  // namespace journey
