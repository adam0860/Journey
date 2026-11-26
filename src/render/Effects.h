// Short-lived 3D effects: arrows flying, spells flying, and the burst where a spell lands.
//
// Gameplay says "a shot was made" (ShotFx) and when it will land; this only draws it. Everything is plain raylib primitives
// (cylinders, glowing spheres), so there are no model files.
#pragma once
#include <vector>

#include "game/Gameplay.h"
#include "raylib.h"

namespace journey {

class Effects {
public:
    // startY / endY: heights of the bow (or staff) and of the target's chest.
    void AddShot(const ShotFx& fx, float startY, float endY);
    void Update(float dt);
    void Draw() const;   // call between BeginMode3D and EndMode3D
    void Clear() { shots_.clear(); }
    int Count() const { return static_cast<int>(shots_.size()); }

private:
    struct Shot {
        ShotFx fx;
        float y0, y1;
        float age = 0.0f;
    };
    static Vector3 PositionAt(const Shot& s, float u);
    std::vector<Shot> shots_;
};

}  // namespace journey
