// Classic-RuneScape style follow camera: fixed tilt, rotate around the hero, zoom in and out.
#pragma once
#include <functional>

#include "raylib.h"

namespace journey {

class CameraRig {
public:
    void SnapTo(float x, float z);
    void Follow(float x, float z, float dt);
    void RotateDegrees(float d) { yaw_ += d; }
    void ZoomBy(float d);
    void SetView(float yawDeg, float pitchDeg, float dist);

    // Keep a building out of the way: if the line from the hero to the camera is blocked (`blocked(from, to)`), the camera climbs
    // (a steeper angle, up to straight above) until it can see him over the top. It eases up and back down, so nothing jumps.
    void Avoid(const std::function<bool(Vector3, Vector3)>& blocked, float dt);

    Camera3D Get() const;
    Vector3 Position() const;

    // Where does a screen pixel hit the ground plane (y = 0)? false if it points at the sky.
    bool ScreenToGround(Vector2 screen, float* x, float* z) const;

private:
    float yaw_ = 0.0f;     // degrees; 0 = camera south of the hero looking north
    float pitch_ = 46.0f;  // degrees above the horizon
    float dist_ = 22.0f;
    float lift_ = 0.0f;    // degrees added to pitch_ to clear a building (eased)
    float fx_ = 0.0f, fz_ = 0.0f;
};

}  // namespace journey
