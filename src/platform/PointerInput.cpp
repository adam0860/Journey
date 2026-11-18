#include "platform/PointerInput.h"

#include <cmath>

namespace journey {

namespace {
constexpr float kDragPixels = 10.0f;      // movement before a press becomes a camera drag
constexpr double kLongPressSeconds = 0.5; // hold still this long and it is a long press
constexpr float kDegreesPerPixel = 0.35f;
constexpr float kWheelZoom = 1.4f;        // distance units per mouse wheel notch
constexpr float kPinchZoom = 0.03f;       // distance units per pixel of pinch
constexpr float kRad2Deg = 180.0f / 3.14159265f;

float Dist(Vector2 a, Vector2 b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
}  // namespace

PointerFrame PointerInput::Poll() {
    PointerFrame out;
    const float dpi = GetWindowScaleDPI().x > 0.0f ? GetWindowScaleDPI().x : 1.0f;

    // --- two-finger gesture (touch screens) ---
    if (GetTouchPointCount() >= 2) {
        const Vector2 a = GetTouchPosition(0);
        const Vector2 b = GetTouchPosition(1);
        const float d = Dist(a, b);
        const float ang = std::atan2(b.y - a.y, b.x - a.x);
        if (gesture_ && prevDist_ > 0.0f) {
            out.zoomDelta = (prevDist_ - d) * kPinchZoom;
            float da = (ang - prevAngle_) * kRad2Deg;
            if (da > 180.0f) da -= 360.0f;
            if (da < -180.0f) da += 360.0f;
            out.rotateDegrees = -da;
        }
        prevDist_ = d;
        prevAngle_ = ang;
        gesture_ = true;
        pressing_ = false;
        return out;
    }
    if (gesture_) {
        prevDist_ = 0.0f;
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) gesture_ = false;
        return out;
    }

    // --- single pointer: mouse, or one finger ---
    const Vector2 m = GetMousePosition();
    out.hoverPos = m;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        out.context = true;
        out.contextPos = m;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        pressing_ = true;
        dragging_ = false;
        contextFired_ = false;
        pressTime_ = GetTime();
        down_ = last_ = m;
    }
    if (pressing_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (!dragging_ && Dist(m, down_) > kDragPixels * dpi) dragging_ = true;
        if (dragging_) out.rotateDegrees = -(m.x - last_.x) * kDegreesPerPixel;
        last_ = m;
        if (!dragging_ && !contextFired_ && GetTime() - pressTime_ >= kLongPressSeconds) {   // held still: long press
            contextFired_ = true;
            out.context = true;
            out.contextPos = down_;
        }
    }
    if (pressing_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (!dragging_ && !contextFired_) {
            out.tap = true;
            out.tapPos = m;
        }
        pressing_ = false;
    }
    out.zoomDelta -= GetMouseWheelMove() * kWheelZoom;
    return out;
}

}  // namespace journey
