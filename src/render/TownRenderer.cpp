#include "render/TownRenderer.h"

#include <string>

#include <cmath>
#include <cstdio>

namespace journey {

namespace {
constexpr int kChunkTiles = 16;
constexpr int kChunkGrid = 4;
constexpr float kDrawRadius = 62.0f;    // a little past the fog end so nothing pops
constexpr float kShadowRadius = 44.0f;  // chunks that can cast into the shadow map
}  // namespace

bool TownRenderer::Load(const TileMap& map, const LitShader& shader, const char* dir) {
    Unload();
    for (int cz = 0; cz < kChunkGrid; ++cz) {
        for (int cx = 0; cx < kChunkGrid; ++cx) {
            const char* path = TextFormat("%s/chunk_%d_%d.glb", dir, cx, cz);
            Chunk c;
            c.model = LoadModel(path);
            if (c.model.meshCount == 0) {
                TraceLog(LOG_ERROR, "TOWN: could not load %s", path);
                continue;
            }
            textures_.Share(c.model);
            shader.ApplyTo(c.model);
            c.cx = (cx + 0.5f) * kChunkTiles;
            c.cz = (cz + 0.5f) * kChunkTiles;
            chunks_.push_back(c);
        }
    }
    // The far ground + forest ring is one file (style A) or four quadrants (style B).
    const bool quadrants = std::string(dir).size() > 2 && std::string(dir).substr(std::string(dir).size() - 2) == "_b";
    const char* skirtFiles[5] = {"skirt", "skirt_0", "skirt_1", "skirt_2", "skirt_3"};
    for (int si = 0; si < 5; ++si) {
        if ((si == 0) == quadrants) continue;      // style B ships skirt_0..3, style A ships skirt
        const char* name = skirtFiles[si];
        const char* path = TextFormat("%s/%s.glb", dir, name);
        // (FileExists() cannot see files packed inside an Android APK, so just try to load and skip empties)
        Model m = LoadModel(path);
        if (m.meshCount == 0) continue;
        textures_.Share(m);
        shader.ApplyTo(m);
        skirts_.push_back(m);
    }

    for (const RoofDef& r : map.Roofs()) {
        Roof roof;
        roof.model = LoadModel(TextFormat("%s/roof_%s.glb", dir, r.name.c_str()));
        roof.rect = r.rect;
        if (roof.model.meshCount == 0) {
            TraceLog(LOG_WARNING, "TOWN: missing roof model for %s", r.name.c_str());
            continue;
        }
        textures_.Share(roof.model);
        shader.ApplyTo(roof.model);
        roofs_.push_back(roof);
    }
    TraceLog(LOG_INFO, "TOWN[%s]: %d chunks, %d roofs, %d skirt pieces; %d unique textures (%d duplicates merged)", dir,
             (int)chunks_.size(), (int)roofs_.size(), (int)skirts_.size(), textures_.UniqueCount(),
             textures_.DuplicatesDropped());
    return !chunks_.empty();
}

void TownRenderer::SetShadowSampling(const LitShader& lit, bool sample) {
    for (auto& c : chunks_) lit.SetShadowSampling(c.model, sample);
    for (auto& r : roofs_) lit.SetShadowSampling(r.model, sample);
    for (auto& s : skirts_) lit.SetShadowSampling(s, sample);
}

void TownRenderer::Unload() {
    for (auto& c : chunks_) { textures_.Detach(c.model); UnloadModel(c.model); }
    for (auto& r : roofs_) { textures_.Detach(r.model); UnloadModel(r.model); }
    for (auto& s : skirts_) { textures_.Detach(s); UnloadModel(s); }
    textures_.UnloadAll();
    chunks_.clear();
    roofs_.clear();
    skirts_.clear();
}

void TownRenderer::Draw(float focusX, float focusZ, TileCoord playerTile, bool shadowCaster) const {
    drawn_ = 0;
    if (!shadowCaster)
        for (const Model& s : skirts_) DrawModel(s, {0, 0, 0}, 1.0f, WHITE);
    const float radius = shadowCaster ? kShadowRadius : kDrawRadius;
    for (const Chunk& c : chunks_) {
        const float dx = c.cx - focusX;
        const float dz = c.cz - focusZ;
        if (std::sqrt(dx * dx + dz * dz) > radius) continue;
        DrawModel(c.model, {0, 0, 0}, 1.0f, WHITE);
        ++drawn_;
    }
    for (const Roof& r : roofs_) {
        // Inside = in the footprint (door tiles included). Step back out and the roof returns.
        if (r.rect.Contains(playerTile.x, playerTile.z)) continue;
        DrawModel(r.model, {0, 0, 0}, 1.0f, WHITE);
    }
}

}  // namespace journey
