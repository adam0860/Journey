#include "render/PropRenderer.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "rlgl.h"

namespace journey {

namespace {

constexpr float kDrawRange = 40.0f;   // tiles from the hero

// A flat ring on the XZ plane (faces up). Drawn without back-face culling so winding does not matter.
void DrawFlatRing(Vector3 c, float r0, float r1, Color col) {
    constexpr int kSeg = 28;
    for (int i = 0; i < kSeg; ++i) {
        const float a0 = 2.0f * PI * i / kSeg, a1 = 2.0f * PI * (i + 1) / kSeg;
        const Vector3 p0 = {c.x + std::cos(a0) * r0, c.y, c.z + std::sin(a0) * r0};
        const Vector3 p1 = {c.x + std::cos(a0) * r1, c.y, c.z + std::sin(a0) * r1};
        const Vector3 p2 = {c.x + std::cos(a1) * r1, c.y, c.z + std::sin(a1) * r1};
        const Vector3 p3 = {c.x + std::cos(a1) * r0, c.y, c.z + std::sin(a1) * r0};
        DrawTriangle3D(p0, p1, p2, col);
        DrawTriangle3D(p0, p2, p3, col);
    }
}

unsigned char A(float a) { return static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255.0f); }

}  // namespace

bool PropRenderer::Load(const LitShader& shader, const char* dir) {
    Unload();
    dir_ = dir;
    shader_ = &shader;
    fire_ = LoadModel(TextFormat("%s/fire.glb", dir));
    hasFire_ = fire_.meshCount > 0;
    if (hasFire_) {
        textures_.Share(fire_);
        shader.ApplyTo(fire_);
    } else {
        TraceLog(LOG_WARNING, "PROPS: missing %s/fire.glb (fires will be flames only)", dir);
    }
    return true;
}

void PropRenderer::Unload() {
    for (auto& kv : named_) {
        textures_.Detach(kv.second);
        UnloadModel(kv.second);
    }
    named_.clear();
    for (auto& kv : drops_) {
        textures_.Detach(kv.second);
        UnloadModel(kv.second);
    }
    drops_.clear();
    if (hasFire_) {
        textures_.Detach(fire_);
        UnloadModel(fire_);
    }
    hasFire_ = false;
    fire_ = Model{};
    textures_.UnloadAll();
}

void PropRenderer::SetShadowSampling(const LitShader& lit, bool sample) {
    if (hasFire_) lit.SetShadowSampling(fire_, sample);
    for (auto& kv : drops_) lit.SetShadowSampling(kv.second, sample);
    for (auto& kv : named_) lit.SetShadowSampling(kv.second, sample);
}

Model* PropRenderer::Named(const std::string& file) {
    auto it = named_.find(file);
    if (it != named_.end()) return it->second.meshCount > 0 ? &it->second : nullptr;
    Model m = LoadModel(TextFormat("%s/%s.glb", dir_.c_str(), file.c_str()));
    if (m.meshCount > 0) {
        textures_.Share(m);
        shader_->ApplyTo(m);
    } else {
        TraceLog(LOG_WARNING, "PROPS: missing %s/%s.glb", dir_.c_str(), file.c_str());
    }
    named_[file] = m;
    return m.meshCount > 0 ? &named_[file] : nullptr;
}

void PropRenderer::DrawRocks(const Rocks& rocks, float time, float focusX, float focusZ) {
    (void)time;
    for (const RockState& r : rocks.All()) {
        if ((r.def.x - focusX) * (r.def.x - focusX) + (r.def.z - focusZ) * (r.def.z - focusZ) > kDrawRange * kDrawRange) continue;
        Model* m = Named(r.depleted ? "rock_empty" : std::string("rock_") + GetRockInfo(r.kind).key);
        if (!m) continue;
        float sc = r.depleted ? 1.0f : 0.55f + 0.45f * r.growAnim;   // a rock that has just grown back swells up
        float shake = 0.0f;
        if (!r.depleted && r.hitAge < 0.3f) shake = std::sin(r.hitAge * 70.0f) * 0.045f * (1.0f - r.hitAge / 0.3f);   // the pick lands
        Matrix tr = MatrixScale(sc, sc, sc);
        tr = MatrixMultiply(tr, MatrixRotateY(r.def.yawDegrees * DEG2RAD));
        tr = MatrixMultiply(tr, MatrixTranslate(r.def.x + shake, 0.0f, r.def.z));
        m->transform = tr;
        DrawModel(*m, {0, 0, 0}, 1.0f, WHITE);
    }
}

void PropRenderer::DrawStations(const std::vector<StationDef>& stations, int busyStation, float time, float focusX, float focusZ) {
    for (size_t si = 0; si < stations.size(); ++si) {
        const StationDef& st = stations[si];
        if ((st.x - focusX) * (st.x - focusX) + (st.z - focusZ) * (st.z - focusZ) > kDrawRange * kDrawRange) continue;
        const bool furnace = st.kind == "furnace";
        Model* m = Named(furnace ? "furnace" : (st.kind == "anvil" ? "anvil" : st.kind.c_str()));   // (tanner, jeweller, wheel: a model of the same name)
        if (m) {
            const float sc = st.kind == "anvil" ? 1.35f : 1.0f;   // anvils are drawn a little bigger so they read at a distance
            Matrix tr = MatrixScale(sc, sc, sc);
            tr = MatrixMultiply(tr, MatrixRotateY(st.yawDegrees * DEG2RAD));
            tr = MatrixMultiply(tr, MatrixTranslate(st.x, 0.0f, st.z));
            m->transform = tr;
            DrawModel(*m, {0, 0, 0}, 1.0f, WHITE);
        }
        if (!furnace) continue;
        // ---- the furnace's fire: a glow at the mouth, flames licking out, and smoke from the chimney
        const float yaw = st.yawDegrees * DEG2RAD;
        const float fx = st.x + std::sin(yaw) * 0.40f, fz = st.z + std::cos(yaw) * 0.40f;   // the mouth
        const bool busy = static_cast<int>(si) == busyStation;
        const float power = busy ? 1.0f : 0.62f;
        const float flick = 0.88f + 0.12f * std::sin(time * 12.0f + si) + 0.05f * std::sin(time * 27.0f);
        BeginBlendMode(BLEND_ADDITIVE);
        // glow on the flagstones in front
        DrawCylinder({fx + std::sin(yaw) * 0.55f, 0.03f, fz + std::cos(yaw) * 0.55f}, 0.95f * power, 0.95f * power, 0.004f, 22,
                     Color{255, 120, 30, A(0.07f * flick * power)});
        DrawCylinder({fx + std::sin(yaw) * 0.40f, 0.034f, fz + std::cos(yaw) * 0.40f}, 0.55f * power, 0.55f * power, 0.004f, 20,
                     Color{255, 160, 50, A(0.12f * flick * power)});
        EndBlendMode();
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        auto tongue = [&](float side, float h, float r, float ph, Color c) {
            const float fl = (0.8f + 0.2f * std::sin(time * 10.0f + ph)) * power;
            const float lick = 0.04f * std::sin(time * 6.0f + ph * 2.0f);
            const float ox = std::cos(yaw) * side, oz = -std::sin(yaw) * side;
            DrawCylinderEx({fx + ox, 0.24f, fz + oz}, {fx + ox + std::sin(yaw) * (0.16f + lick) * fl, 0.24f + h * fl, fz + oz + std::cos(yaw) * (0.16f + lick) * fl},
                           r * power, 0.0f, 7, c);
        };
        tongue(0.0f, 0.34f, 0.12f, 0.0f, Color{255, 96, 18, 215});
        tongue(0.09f, 0.24f, 0.075f, 1.7f, Color{255, 130, 28, 210});
        tongue(-0.09f, 0.26f, 0.08f, 3.1f, Color{255, 130, 28, 210});
        tongue(0.0f, 0.22f, 0.07f, 0.9f, Color{255, 190, 50, 230});
        tongue(0.0f, 0.12f, 0.04f, 2.3f, Color{255, 240, 160, 240});
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        // chimney smoke: grey puffs that grow and fade as they rise (thicker while smelting)
        BeginBlendMode(BLEND_ALPHA);
        for (int i = 0; i < 6; ++i) {
            const float p = std::fmod(time * (busy ? 0.42f : 0.28f) + i / 6.0f + si * 0.37f, 1.0f);
            const float drift = 0.25f * p * p + 0.05f * std::sin(time + i * 2.0f);
            DrawSphereEx({st.x + drift, 2.25f + p * 1.5f, st.z + 0.06f * std::sin(time * 0.7f + i)}, 0.10f + 0.22f * p, 6, 6,
                         Color{150, 148, 146, A((1.0f - p) * (busy ? 0.42f : 0.26f))});
        }
        EndBlendMode();
        // a few sparks from the chimney top while it is busy, embers always
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < (busy ? 6 : 3); ++i) {
            const float p = std::fmod(time * 0.8f + i * 0.21f + si * 0.4f, 1.0f);
            DrawSphereEx({st.x + 0.08f * std::sin(time * 3.0f + i * 1.9f) * (1.0f + p), 2.25f + p * 1.1f, st.z + 0.05f * std::cos(time * 2.0f + i)},
                         0.02f * (1.0f - 0.5f * p), 4, 4, Color{255, 180, 70, A((1.0f - p) * 0.9f)});
        }
        EndBlendMode();
    }
}

void PropRenderer::DrawSparks(Vector3 at, float strike, float time, Color color) {
    if (strike < 0.0f || strike > 1.0f) return;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 9; ++i) {
        const float ang = i * 2.399f + std::floor(time * 3.0f) * 0.7f;     // golden-angle spread, different on every blow
        const float sp = 0.35f + 0.2f * (i % 3);
        const float up = 0.45f + 0.25f * ((i * 5) % 4);
        const float t = strike;
        const Vector3 pos = {at.x + std::cos(ang) * sp * t, at.y + up * t - 0.9f * t * t, at.z + std::sin(ang) * sp * t};
        DrawSphereEx(pos, 0.028f * (1.0f - 0.6f * t), 4, 4, Color{color.r, color.g, color.b, A((1.0f - t) * 1.1f)});
    }
    EndBlendMode();
}

Model* PropRenderer::DropModel(const char* name) {
    auto it = drops_.find(name);
    if (it != drops_.end()) return it->second.meshCount > 0 ? &it->second : nullptr;
    Model m = LoadModel(TextFormat("%s/drop_%s.glb", dir_.c_str(), name));
    if (m.meshCount > 0) {
        textures_.Share(m);
        shader_->ApplyTo(m);
    } else {
        TraceLog(LOG_WARNING, "PROPS: missing %s/drop_%s.glb", dir_.c_str(), name);
    }
    drops_[name] = m;
    return m.meshCount > 0 ? &drops_[name] : nullptr;
}

void PropRenderer::DrawGroundItems(const GroundItems& ground, float time, float focusX, float focusZ) {
    for (const Pile& p : ground.All()) {
        const float cx = p.tile.x + 0.5f, cz = p.tile.z + 0.5f;
        if ((cx - focusX) * (cx - focusX) + (cz - focusZ) * (cz - focusZ) > kDrawRange * kDrawRange) continue;
        const int shown = std::min<int>(static_cast<int>(p.items.size()), 4);
        // a soft shadow under the pile
        DrawCylinder({cx, 0.018f, cz}, 0.30f + 0.05f * shown, 0.30f + 0.05f * shown, 0.008f, 14, Fade(BLACK, 0.22f));
        for (int i = 0; i < shown; ++i) {
            const ItemStack& st = p.items[i];
            Model* m = DropModel(GetItemInfo(st.item).model);
            if (!m) continue;
            // the items lie scattered round the middle of the tile, each turned a different way
            const float ang = 1.9f + i * 2.4f + p.id * 0.7f;
            const float off = shown == 1 ? 0.0f : 0.17f;
            const float yaw = ang * 1.3f;
            Matrix tr = MatrixScale(1.3f, 1.3f, 1.3f);   // a little bigger than life so loot is easy to see
            tr = MatrixMultiply(tr, MatrixRotateY(yaw));
            tr = MatrixMultiply(tr, MatrixTranslate(cx + std::cos(ang) * off, 0.0f, cz + std::sin(ang) * off));
            m->transform = tr;
            DrawModel(*m, {0, 0, 0}, 1.0f, WHITE);
        }
        // a faint glint so a pile is easy to spot against the grass (it hovers just above the items)
        const float tw = 0.5f + 0.5f * std::sin(time * 3.0f + p.id);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphereEx({cx + 0.08f * std::sin(time * 1.3f + p.id), 0.30f + 0.04f * tw, cz + 0.08f * std::cos(time * 1.7f)}, 0.022f + 0.012f * tw, 4, 4,
                     Color{255, 236, 150, A(0.35f + 0.5f * tw)});
        EndBlendMode();
    }
}

void PropRenderer::DrawFires(const Fires& fires, float time, float focusX, float focusZ) {
    for (const Fire& f : fires.All()) {
        const float x = f.tile.x + 0.5f, z = f.tile.z + 0.5f;
        if ((x - focusX) * (x - focusX) + (z - focusZ) * (z - focusZ) > kDrawRange * kDrawRange) continue;
        const float yaw = f.id * 1.7f;   // each fire's logs lie a little differently

        // ---- the logs (and the stone ring): charred once it has burnt out
        if (hasFire_) {
            const float grow = std::min(1.0f, 0.4f + f.age * 1.5f);
            const float s = (f.Burning() ? grow : 0.9f) * (f.big ? 1.7f : 1.0f);
            Matrix tr = MatrixScale(s, s, s);
            tr = MatrixMultiply(tr, MatrixRotateY(yaw));
            tr = MatrixMultiply(tr, MatrixTranslate(x, 0.0f, z));
            fire_.transform = tr;
            DrawModel(fire_, {0, 0, 0}, 1.0f, f.Burning() ? WHITE : Color{70, 62, 58, 255});
        }
        if (!f.Burning()) {   // ashes, fading away
            const float a = std::clamp(f.ashLeft / 5.0f, 0.0f, 1.0f);
            DrawCylinder({x, 0.025f, z}, 0.42f, 0.42f, 0.012f, 16, Color{46, 42, 40, A(0.8f * a)});
            continue;
        }

        // Flames shrink in the last few seconds; they also swell in when the fire is lit.
        float power = std::min(1.0f, f.age / 0.8f);
        power *= std::min(1.0f, f.burnLeft / 8.0f);
        if (power <= 0.01f) continue;

        const float K = f.big ? 1.8f : 1.0f;   // a bonfire's flames and glow are bigger
        // ---- warm glow on the ground (additive)
        BeginBlendMode(BLEND_ADDITIVE);
        const float flick = 0.9f + 0.1f * std::sin(time * 13.0f + f.id) + 0.05f * std::sin(time * 29.0f);
        DrawCylinder({x, 0.03f, z}, 1.4f * K * power, 1.4f * K * power, 0.004f, 24, Color{255, 120, 30, A(0.05f * flick)});
        DrawCylinder({x, 0.032f, z}, 0.95f * K * power, 0.95f * K * power, 0.004f, 24, Color{255, 140, 40, A(0.08f * flick)});
        DrawCylinder({x, 0.034f, z}, 0.55f * K * power, 0.55f * K * power, 0.004f, 20, Color{255, 170, 60, A(0.14f * flick)});
        EndBlendMode();

        // ---- flames: stacked cones, drawn without writing depth so the bright core shows through the outer glow
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        auto cone = [&](float ox, float oz, float h, float r, float ph, Color c) {
            const float fl = (0.86f + 0.14f * std::sin(time * 11.0f + ph)) * power;
            const float sway = 0.05f * std::sin(time * 7.0f + ph * 2.0f);
            DrawCylinderEx({x + ox * K, 0.10f, z + oz * K}, {x + ox * K + sway, 0.10f + h * K * fl, z + oz * K + sway * 0.6f}, r * K * power, 0.0f, 8, c);
        };
        cone(0.0f, 0.0f, 0.62f, 0.18f, 0.0f, Color{255, 96, 18, 215});
        cone(0.09f, 0.04f, 0.40f, 0.10f, 1.7f, Color{255, 120, 24, 205});
        cone(-0.08f, -0.05f, 0.43f, 0.11f, 3.1f, Color{255, 120, 24, 205});
        cone(0.0f, 0.0f, 0.48f, 0.13f, 0.9f, Color{255, 180, 40, 225});
        cone(0.0f, 0.0f, 0.30f, 0.07f, 2.3f, Color{255, 238, 150, 240});
        rlDrawRenderBatchActive();
        rlEnableDepthMask();

        // ---- embers drifting up
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 6; ++i) {
            const float p = std::fmod(time * 0.55f + i * 0.17f + f.id * 0.31f, 1.0f);
            const float ang = i * 1.9f + f.id;
            const Vector3 pos = {x + std::cos(ang) * 0.12f * (1.0f + p) + 0.06f * std::sin(time * 3.0f + i),
                                 0.3f + p * 1.2f * power, z + std::sin(ang) * 0.12f * (1.0f + p)};
            DrawSphereEx(pos, 0.022f * (1.0f - 0.5f * p), 4, 4, Color{255, 190, 80, A((1.0f - p) * 0.9f)});
        }
        EndBlendMode();
    }
}

void PropRenderer::DrawFishSpots(const std::vector<SpotDef>& spots, float time, float focusX, float focusZ) {
    constexpr float kWaterY = -0.30f;   // just above the animated water surface (-0.35) and its foam
    rlDrawRenderBatchActive();   // (culling is a GL state: flush what is queued before and after changing it)
    rlDisableBackfaceCulling();
    for (size_t si = 0; si < spots.size(); ++si) {
        const SpotDef& s = spots[si];
        if ((s.x - focusX) * (s.x - focusX) + (s.z - focusZ) * (s.z - focusZ) > kDrawRange * kDrawRange) continue;
        const float ph = static_cast<float>(si) * 0.37f;
        // a dark swirl in the middle so the spot reads against the bright water, with three white ripples
        // spreading out from it, a third of a cycle apart
        const float pulse = 0.5f + 0.5f * std::sin(time * 2.2f + ph * 5.0f);
        DrawFlatRing({s.x, kWaterY - 0.004f, s.z}, 0.0f, 0.50f + 0.04f * pulse, Color{12, 56, 108, 150});
        DrawFlatRing({s.x, kWaterY - 0.003f, s.z}, 0.0f, 0.30f, Color{8, 40, 86, 150});
        for (int i = 0; i < 3; ++i) {
            const float p = std::fmod(time * 0.5f + i / 3.0f + ph, 1.0f);
            const float r = 0.12f + 0.62f * p;
            const float w = 0.10f * (1.0f - 0.5f * p);
            DrawFlatRing({s.x, kWaterY, s.z}, r, r + w, Color{245, 252, 255, A((1.0f - p) * 0.95f)});
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();

    for (size_t si = 0; si < spots.size(); ++si) {
        const SpotDef& s = spots[si];
        if ((s.x - focusX) * (s.x - focusX) + (s.z - focusZ) * (s.z - focusZ) > kDrawRange * kDrawRange) continue;
        const float ph = static_cast<float>(si) * 1.3f;
        // bubbles bobbing around the middle
        for (int i = 0; i < 7; ++i) {
            const float ang = time * 0.9f + i * 0.9f + ph;
            const float rad = 0.18f + 0.14f * std::sin(time * 1.3f + i * 2.0f + ph);
            const float bob = 0.035f * std::sin(time * 4.0f + i * 1.7f);
            DrawSphereEx({s.x + std::cos(ang) * rad, kWaterY + 0.03f + bob, s.z + std::sin(ang) * rad}, 0.045f + 0.012f * (i % 2), 5, 5,
                         Color{245, 252, 255, 235});
        }
        // now and then a little silver fish leaps out of the water
        const float cycle = 4.6f;
        const float lt = std::fmod(time + ph * 2.1f, cycle);
        const float leap = 0.7f;
        if (lt < leap) {
            const float p = lt / leap;
            const float dir = ph * 3.0f;
            const float dist = (p - 0.5f) * 0.7f;
            const float height = 0.42f * (1.0f - (2.0f * p - 1.0f) * (2.0f * p - 1.0f));
            const float pitch = -(2.0f * p - 1.0f) * 55.0f;   // nose up, then down
            rlPushMatrix();
            rlTranslatef(s.x + std::cos(dir) * dist, kWaterY + 0.02f + height, s.z + std::sin(dir) * dist);
            rlRotatef(-dir * RAD2DEG, 0, 1, 0);
            rlRotatef(pitch, 0, 0, 1);
            rlPushMatrix();
            rlScalef(1.0f, 0.42f, 0.38f);   // a flattened sphere is the body
            DrawSphereEx({0, 0, 0}, 0.11f, 6, 6, Color{190, 212, 232, 255});
            rlPopMatrix();
            DrawCube({-0.12f, 0, 0}, 0.09f, 0.07f, 0.012f, Color{150, 178, 205, 255});   // tail
            rlPopMatrix();
        }
    }
}

}  // namespace journey
