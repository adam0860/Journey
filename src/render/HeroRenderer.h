// Placeholder hero built from lit primitives, with a simple walk cycle.
// Swap for a Blender character later: only this class needs to change.
#pragma once
#include "game/Player.h"
#include "raylib.h"
#include "render/LitShader.h"

namespace journey {

class HeroRenderer {
public:
    void Load(const LitShader& shader);
    void Unload();
    void SetShadowSampling(const LitShader& lit, bool sample) { lit.SetShadowSampling(cube_, sample); lit.SetShadowSampling(sphere_, sample); }
    // blobShadow: the cheap round shadow used when there is no real shadow map (style A / shadow pass off)
    void Draw(const Player& player, bool blobShadow = true);

private:
    void Part(Model& m, Vector3 size, Vector3 pivotInPart, Vector3 hip, float swingRad, const Player& p, Color srgb);
    Model cube_{};
    Model sphere_{};
};

}  // namespace journey
