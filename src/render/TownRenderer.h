// Loads the Blender-built town chunks and draws only what is near the camera.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "game/Forest.h"
#include "raylib.h"
#include "render/LitShader.h"
#include "render/TextureCache.h"
#include "world/TileMap.h"

namespace journey {

class TownRenderer {
public:
    // dir: "assets/models/town" (style A) or "assets/models/town_b" (style B)
    bool Load(const TileMap& map, const LitShader& shader, const char* dir);
    void Unload();
    // Roofs of enterable buildings are skipped while the player stands inside them.
    // shadowCaster=true: only what can shadow the play area (no far skirt), within a tighter radius.
    void Draw(float focusX, float focusZ, TileCoord playerTile, bool shadowCaster = false) const;

    // The choppable trees (and stumps), drawn from the Forest's live state. camPos/camTarget are used to skip
    // trees behind the camera. blobShadows: soft dark disc under each tree (style B has no baked tree shadows).
    void DrawTrees(const Forest& forest, Vector3 camPos, Vector3 camTarget, float focusX, float focusZ, bool shadowCaster,
                   bool blobShadows) const;

    void SetShadowSampling(const LitShader& lit, bool sample);
    int ChunksDrawnLastFrame() const { return drawn_; }
    int UniqueTextures() const { return textures_.UniqueCount(); }

private:
    struct Chunk {
        Model model{};
        float cx = 0, cz = 0;
    };
    struct Roof {
        Model model{};
        TileRect rect;
    };
    std::vector<Chunk> chunks_;
    std::vector<Roof> roofs_;
    std::vector<Model> skirts_;
    mutable std::map<std::string, Model> treeModels_;   // "round" / "pine" / "tall" / "stump" (transform set per draw)
    TextureCache textures_;
    mutable int drawn_ = 0;
};

}  // namespace journey
