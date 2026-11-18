#include "render/HeroRenderer.h"

#include <cmath>

#include "raymath.h"

namespace journey {

void HeroRenderer::Load(const LitShader& shader) {
    cube_ = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    sphere_ = LoadModelFromMesh(GenMeshSphere(0.5f, 10, 10));
    shader.ApplyTo(cube_);
    shader.ApplyTo(sphere_);
}

void HeroRenderer::Unload() {
    UnloadModel(cube_);
    UnloadModel(sphere_);
}

// Draws one body part. The part is a unit mesh scaled to `size`; it swings about `hip`
// (a point in hero space, feet = origin) around the x axis, with `pivotInPart` the pivot's
// offset from the part's centre.
void HeroRenderer::Part(Model& m, Vector3 size, Vector3 pivotInPart, Vector3 hip, float swingRad, const Player& p,
                        Color srgb) {
    Matrix t = MatrixScale(size.x, size.y, size.z);
    t = MatrixMultiply(t, MatrixTranslate(-pivotInPart.x, -pivotInPart.y, -pivotInPart.z));
    t = MatrixMultiply(t, MatrixRotateX(swingRad));
    t = MatrixMultiply(t, MatrixTranslate(hip.x, hip.y, hip.z));
    t = MatrixMultiply(t, MatrixRotateY(p.FacingRadians()));
    t = MatrixMultiply(t, MatrixTranslate(p.X(), 0.0f, p.Z()));
    m.transform = t;
    DrawModel(m, {0, 0, 0}, 1.0f, ToLinear(srgb));
}

void HeroRenderer::Draw(const Player& p, bool blobShadow) {
    const float swing = p.Moving() ? std::sin(p.WalkPhase()) * 0.75f : 0.0f;
    const float bob = p.Moving() ? std::fabs(std::sin(p.WalkPhase())) * 0.05f : 0.0f;
    const Color shirt = {52, 104, 206, 255};
    const Color pants = {74, 56, 44, 255};
    const Color skin = {226, 182, 140, 255};
    const Color hair = {92, 58, 32, 255};
    const Color boots = {38, 32, 30, 255};

    // legs: pivot at the top (hip), so the part's centre is 0.4 below the pivot
    const float legH = 0.78f;
    Part(cube_, {0.22f, legH, 0.26f}, {0, legH / 2, 0}, {-0.15f, legH + bob, 0}, swing, p, pants);
    Part(cube_, {0.22f, legH, 0.26f}, {0, legH / 2, 0}, {0.15f, legH + bob, 0}, -swing, p, pants);
    Part(cube_, {0.24f, 0.14f, 0.34f}, {0, legH / 2 - 0.05f, -0.03f}, {-0.15f, legH + bob, 0}, swing, p, boots);
    Part(cube_, {0.24f, 0.14f, 0.34f}, {0, legH / 2 - 0.05f, -0.03f}, {0.15f, legH + bob, 0}, -swing, p, boots);
    // torso
    Part(cube_, {0.56f, 0.66f, 0.32f}, {0, 0, 0}, {0, legH + 0.33f + bob, 0}, 0, p, shirt);
    // arms
    Part(cube_, {0.15f, 0.62f, 0.17f}, {0, 0.26f, 0}, {-0.37f, legH + 0.6f + bob, 0}, -swing, p, shirt);
    Part(cube_, {0.15f, 0.62f, 0.17f}, {0, 0.26f, 0}, {0.37f, legH + 0.6f + bob, 0}, swing, p, shirt);
    Part(cube_, {0.14f, 0.14f, 0.16f}, {0, 0.38f, 0}, {-0.37f, legH + 0.6f + bob, 0}, -swing, p, skin);
    Part(cube_, {0.14f, 0.14f, 0.16f}, {0, 0.38f, 0}, {0.37f, legH + 0.6f + bob, 0}, swing, p, skin);
    // head + hair
    Part(sphere_, {0.46f, 0.5f, 0.46f}, {0, 0, 0}, {0, legH + 0.66f + 0.32f + bob, 0}, 0, p, skin);
    Part(cube_, {0.5f, 0.2f, 0.5f}, {0, 0, 0}, {0, legH + 0.66f + 0.5f + bob, -0.02f}, 0, p, hair);

    // soft blob shadow
    if (blobShadow) DrawCylinder({p.X(), 0.02f, p.Z()}, 0.46f, 0.46f, 0.01f, 16, Fade(BLACK, 0.28f));
}

}  // namespace journey
