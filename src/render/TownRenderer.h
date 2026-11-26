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
    // (the ground chunks themselves are streamed in around the hero by Stream(): the world is far bigger than what is ever on screen)
    bool Load(const TileMap& map, const LitShader& shader, const char* dir);
    void Unload();
    // Load the chunks near (focusX, focusZ) and drop the far ones. The first call after Load() brings in everything near him at once;
    // after that at most `budget` chunks are loaded per call, so walking never stalls.
    void Stream(float focusX, float focusZ, int budget = 1);
    // Roofs of enterable buildings are skipped while the player stands inside them.
    // shadowCaster=true: only what can shadow the play area (no far skirt), within a tighter radius.
    void Draw(float focusX, float focusZ, TileCoord playerTile, bool shadowCaster = false) const;

    // The choppable trees (and stumps), drawn from the Forest's live state. camPos/camTarget are used to skip
    // trees behind the camera. blobShadows: soft dark disc under each tree (style B has no baked tree shadows).
    void DrawTrees(const Forest& forest, Vector3 camPos, Vector3 camTarget, float focusX, float focusZ, bool shadowCaster,
                   bool blobShadows) const;

    // Does the line from `a` to `b` pass through a building (its walls and roof, as one box from the ground up)? The building the hero
    // stands in does not count. The camera uses this to stay out of the way (CameraRig::Avoid).
    bool SegmentBlocked(Vector3 a, Vector3 b, TileCoord playerTile) const;

    void SetShadowSampling(const LitShader& lit, bool sample);
    int ChunksDrawnLastFrame() const { return drawn_; }
    int ChunksLoaded() const { return loadedCount_; }
    int UniqueTextures() const { return textures_.UniqueCount(); }

private:
    struct Chunk {
        Model model{};
        float cx = 0, cz = 0;
        bool loaded = false;
    };
    void LoadChunk(int index);
    void DropChunk(int index);
    struct Roof {
        Model model{};
        TileRect rect;
        BoundingBox box{};   // the roof's extent in the world (the walls below it are taken to reach the ground)
    };
    std::vector<Chunk> chunks_;       // one per 16x16 tile chunk of the map (cz * chunksX_ + cx); `loaded` says whether its model is in memory
    int chunksX_ = 0, chunksZ_ = 0;
    std::string dir_;
    const LitShader* shader_ = nullptr;
    bool streamed_ = false;           // has Stream() run since Load()?
    bool sampleShadow_ = false;
    int loadedCount_ = 0;
    std::vector<Roof> roofs_;
    std::vector<Model> skirts_;
    mutable std::map<std::string, Model> treeModels_;   // "round" / "pine" / "tall" / "stump" (transform set per draw)
    TextureCache textures_;
    mutable int drawn_ = 0;
};

}  // namespace journey
