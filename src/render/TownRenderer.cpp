#include "render/TownRenderer.h"

#include <algorithm>
#include <string>

#include <cmath>
#include <cstdio>

#include "raymath.h"

namespace journey {

namespace {
constexpr int kChunkTiles = 16;
constexpr float kDrawRadius = 62.0f;    // a little past the fog end so nothing pops
constexpr float kShadowRadius = 44.0f;  // chunks that can cast into the shadow map
constexpr float kLoadRadius = 74.0f;    // chunks this close to the hero are kept in memory (a little past what is drawn)
constexpr float kFirstRadius = 48.0f;   // the first call after Load() brings in only the chunks this close (the game opens fast), the rest follow a few a frame
constexpr float kKeepRadius = 96.0f;    // ... and only dropped again beyond this one, so walking back and forth does not reload them
}  // namespace

void TownRenderer::LoadChunk(int index) {
    Chunk& c = chunks_[index];
    if (c.loaded) return;
    const int cx = index % chunksX_, cz = index / chunksX_;
    const char* path = TextFormat("%s/chunk_%d_%d.glb", dir_.c_str(), cx, cz);
    c.model = LoadModel(path);
    if (c.model.meshCount == 0) {
        TraceLog(LOG_ERROR, "TOWN: could not load %s", path);
        c.model = Model{};
        c.cx = -1e9f;   // never tried again, never drawn
        return;
    }
    textures_.Share(c.model);
    shader_->ApplyTo(c.model);
    if (sampleShadow_) shader_->SetShadowSampling(c.model, true);
    c.loaded = true;
    ++loadedCount_;
}

void TownRenderer::DropChunk(int index) {
    Chunk& c = chunks_[index];
    if (!c.loaded) return;
    textures_.Detach(c.model);
    UnloadModel(c.model);
    c.model = Model{};
    c.loaded = false;
    --loadedCount_;
}

void TownRenderer::Stream(float focusX, float focusZ, int budget) {
    if (!shader_) return;
    const bool first = !streamed_;
    streamed_ = true;
    struct Want {
        float d;
        int index;
    };
    std::vector<Want> want;
    for (int i = 0; i < static_cast<int>(chunks_.size()); ++i) {
        Chunk& c = chunks_[i];
        if (c.cx < -1e8f) continue;   // missing file
        const float dx = c.cx - focusX, dz = c.cz - focusZ;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (c.loaded) {
            if (d > kKeepRadius) DropChunk(i);
        } else if (d <= (first ? kFirstRadius : kLoadRadius)) {
            want.push_back({d, i});
        }
    }
    std::sort(want.begin(), want.end(), [](const Want& a, const Want& b) { return a.d < b.d; });
    int n = 0;
    for (const Want& w : want) {
        if (!first && n >= budget) break;   // (the first call loads everything near him at once)
        LoadChunk(w.index);
        ++n;
    }
}

bool TownRenderer::Load(const TileMap& map, const LitShader& shader, const char* dir) {
    Unload();
    dir_ = dir;
    shader_ = &shader;
    streamed_ = false;
    chunksX_ = (map.Width() + kChunkTiles - 1) / kChunkTiles;
    chunksZ_ = (map.Height() + kChunkTiles - 1) / kChunkTiles;
    chunks_.assign(static_cast<size_t>(chunksX_) * chunksZ_, Chunk{});
    for (int cz = 0; cz < chunksZ_; ++cz)
        for (int cx = 0; cx < chunksX_; ++cx) {
            Chunk& c = chunks_[static_cast<size_t>(cz) * chunksX_ + cx];
            c.cx = (cx + 0.5f) * kChunkTiles;
            c.cz = (cz + 0.5f) * kChunkTiles;
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
        roof.box = GetModelBoundingBox(roof.model);
        textures_.Share(roof.model);
        shader.ApplyTo(roof.model);
        roofs_.push_back(roof);
    }
    // Tree + stump models (one per kind used by the map, plus the stump).
    std::vector<std::string> kinds = {"stump"};
    for (const TreeDef& t : map.Trees())
        if (std::find(kinds.begin(), kinds.end(), t.kind) == kinds.end()) kinds.push_back(t.kind);
    for (const std::string& k : kinds) {
        const std::string file = (k == "stump") ? "stump" : "tree_" + k;
        Model m = LoadModel(TextFormat("%s/%s.glb", dir, file.c_str()));
        if (m.meshCount == 0) {
            TraceLog(LOG_WARNING, "TOWN: missing %s/%s.glb", dir, file.c_str());
            continue;
        }
        textures_.Share(m);
        shader.ApplyTo(m);
        treeModels_[k] = m;
    }
    TraceLog(LOG_INFO, "TOWN[%s]: %d chunks (streamed), %d roofs, %d skirt pieces", dir, (int)chunks_.size(), (int)roofs_.size(), (int)skirts_.size());
    return !chunks_.empty();
}

void TownRenderer::SetShadowSampling(const LitShader& lit, bool sample) {
    sampleShadow_ = sample;
    for (auto& c : chunks_)
        if (c.loaded) lit.SetShadowSampling(c.model, sample);
    for (auto& r : roofs_) lit.SetShadowSampling(r.model, sample);
    for (auto& s : skirts_) lit.SetShadowSampling(s, sample);
    for (auto& kv : treeModels_) lit.SetShadowSampling(kv.second, sample);
}

void TownRenderer::Unload() {
    for (auto& c : chunks_)
        if (c.loaded) { textures_.Detach(c.model); UnloadModel(c.model); }
    for (auto& r : roofs_) { textures_.Detach(r.model); UnloadModel(r.model); }
    for (auto& s : skirts_) { textures_.Detach(s); UnloadModel(s); }
    for (auto& kv : treeModels_) { textures_.Detach(kv.second); UnloadModel(kv.second); }
    treeModels_.clear();
    textures_.UnloadAll();
    chunks_.clear();
    roofs_.clear();
    skirts_.clear();
    loadedCount_ = 0;
    streamed_ = false;
}

void TownRenderer::Draw(float focusX, float focusZ, TileCoord playerTile, bool shadowCaster) const {
    drawn_ = 0;
    if (!shadowCaster)
        for (const Model& s : skirts_) DrawModel(s, {0, 0, 0}, 1.0f, WHITE);
    const float radius = shadowCaster ? kShadowRadius : kDrawRadius;
    for (const Chunk& c : chunks_) {
        if (!c.loaded) continue;
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

bool TownRenderer::SegmentBlocked(Vector3 a, Vector3 b, TileCoord playerTile) const {
    const float d[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
    const float o[3] = {a.x, a.y, a.z};
    for (const Roof& r : roofs_) {
        if (r.rect.Contains(playerTile.x, playerTile.z)) continue;
        // the box, a little fat, from the ground to the top of the roof (a thin roof model is skipped: it is not a building)
        if (r.box.max.y < 1.5f) continue;
        const float lo[3] = {r.box.min.x - 0.1f, 0.0f, r.box.min.z - 0.1f};
        const float hi[3] = {r.box.max.x + 0.1f, r.box.max.y, r.box.max.z + 0.1f};
        if (a.x >= lo[0] && a.x <= hi[0] && a.z >= lo[2] && a.z <= hi[2]) continue;   // he is at its very wall: that is not "behind" it
        float t0 = 0.0f, t1 = 1.0f;
        bool hit = true;
        for (int i = 0; i < 3 && hit; ++i) {   // the slab test, axis by axis
            if (std::fabs(d[i]) < 1e-6f) {
                if (o[i] < lo[i] || o[i] > hi[i]) hit = false;
            } else {
                float ta = (lo[i] - o[i]) / d[i], tb = (hi[i] - o[i]) / d[i];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) hit = false;
            }
        }
        if (hit) return true;
    }
    return false;
}

void TownRenderer::DrawTrees(const Forest& forest, Vector3 camPos, Vector3 camTarget, float focusX, float focusZ,
                             bool shadowCaster, bool blobShadows) const {
    const float radius = shadowCaster ? kShadowRadius : kDrawRadius;
    // Camera heading on the ground; trees well behind the camera are not on screen.
    float fx = camTarget.x - camPos.x, fz = camTarget.z - camPos.z;
    const float fl = std::sqrt(fx * fx + fz * fz);
    if (fl > 1e-4f) { fx /= fl; fz /= fl; }
    for (const TreeState& t : forest.Trees()) {
        const float dx = t.def.x - focusX, dz = t.def.z - focusZ;
        if (dx * dx + dz * dz > radius * radius) continue;
        if (!shadowCaster && (t.def.x - camPos.x) * fx + (t.def.z - camPos.z) * fz < -4.0f) continue;
        auto it = treeModels_.find(t.stump ? "stump" : t.def.kind);
        if (it == treeModels_.end()) continue;
        // A freshly regrown tree grows from a sapling (ease-out).
        const float g = t.stump ? 1.0f : t.growAnim;
        const float grow = 0.2f + 0.8f * (1.0f - (1.0f - g) * (1.0f - g));
        const float sc = t.def.scale * grow;
        Model& m = it->second;
        Matrix tr = MatrixScale(sc, sc, sc);
        tr = MatrixMultiply(tr, MatrixRotateY(t.def.yawDegrees * DEG2RAD));
        tr = MatrixMultiply(tr, MatrixTranslate(t.def.x, 0.0f, t.def.z));
        m.transform = tr;
        DrawModel(m, {0, 0, 0}, 1.0f, WHITE);
    }
    if (blobShadows && !shadowCaster) {
        for (const TreeState& t : forest.Trees()) {
            const float dx = t.def.x - focusX, dz = t.def.z - focusZ;
            if (dx * dx + dz * dz > radius * radius) continue;
            if ((t.def.x - camPos.x) * fx + (t.def.z - camPos.z) * fz < -4.0f) continue;
            const float r = (t.stump ? 0.55f : 1.25f * (0.2f + 0.8f * t.growAnim)) * t.def.scale;
            DrawCylinder({t.def.x, 0.02f, t.def.z}, r, r, 0.01f, 14, Fade(BLACK, t.stump ? 0.22f : 0.26f));
        }
    }
}

}  // namespace journey
