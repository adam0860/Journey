// One input scheme for mouse AND touch, so the same code runs on Windows and Android.
//   tap / click      -> walk or interact
//   long press / right click -> open the context menu (every option for what is under the finger)
//   drag             -> rotate the camera
//   mouse wheel      -> zoom            two-finger pinch -> zoom, two-finger twist -> rotate
#pragma once
#include "raylib.h"

namespace journey {

struct PointerFrame {
    bool tap = false;
    Vector2 tapPos{0, 0};
    bool context = false;          // long press (touch) or right click (mouse): show the menu
    Vector2 contextPos{0, 0};
    Vector2 hoverPos{0, 0};        // where the mouse is (meaningful on desktop only)
    float rotateDegrees = 0.0f;  // camera yaw change this frame
    float zoomDelta = 0.0f;      // camera distance change this frame (positive = zoom out)
};

class PointerInput {
public:
    PointerFrame Poll();

private:
    bool pressing_ = false;
    bool dragging_ = false;
    bool contextFired_ = false;   // this press already opened a menu: its release is not a tap
    double pressTime_ = 0.0;
    bool gesture_ = false;  // a multi-touch gesture is in progress; ignore taps until fingers lift
    Vector2 down_{0, 0};
    Vector2 last_{0, 0};
    float prevDist_ = 0.0f;
    float prevAngle_ = 0.0f;
};

}  // namespace journey
