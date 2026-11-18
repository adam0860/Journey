// One input scheme for mouse AND touch, so the same code runs on Windows and Android.
//   tap / click      -> walk or interact
//   drag             -> rotate the camera
//   mouse wheel      -> zoom            two-finger pinch -> zoom, two-finger twist -> rotate
#pragma once
#include "raylib.h"

namespace journey {

struct PointerFrame {
    bool tap = false;
    Vector2 tapPos{0, 0};
    float rotateDegrees = 0.0f;  // camera yaw change this frame
    float zoomDelta = 0.0f;      // camera distance change this frame (positive = zoom out)
};

class PointerInput {
public:
    PointerFrame Poll();

private:
    bool pressing_ = false;
    bool dragging_ = false;
    bool gesture_ = false;  // a multi-touch gesture is in progress; ignore taps until fingers lift
    Vector2 down_{0, 0};
    Vector2 last_{0, 0};
    float prevDist_ = 0.0f;
    float prevAngle_ = 0.0f;
};

}  // namespace journey
