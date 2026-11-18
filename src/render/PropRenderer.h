// Things that come and go while you play: campfires (logs model + animated flames), the fishing spots on the river,
// mineable rocks, the smith's furnace and anvils, and the sparks that fly when you work metal or stone.
// Models live in assets/models/props (built by tools/build_items.py); flames, glow, ripples and bubbles are drawn
// with plain raylib shapes so they animate for free.
#pragma once
#include <vector>

#include <map>
#include <string>

#include "game/Fires.h"
#include "game/GroundItems.h"
#include "game/Mining.h"
#include "raylib.h"
#include "render/LitShader.h"
#include "render/TextureCache.h"
#include "world/TileMap.h"

namespace journey {

class PropRenderer {
public:
    bool Load(const LitShader& shader, const char* dir = "assets/models/props");
    void Unload();
    void SetShadowSampling(const LitShader& lit, bool sample);

    // focus = where the hero stands; props far away are skipped.
    void DrawFires(const Fires& fires, float time, float focusX, float focusZ);
    // Piles of items on the ground (loot, things you dropped): the little models, a soft shadow and a faint glint.
    void DrawGroundItems(const GroundItems& ground, float time, float focusX, float focusZ);
    void DrawFishSpots(const std::vector<SpotDef>& spots, float time, float focusX, float focusZ);
    // Rocks (full of ore, or mined out and growing back). A struck rock shakes for a moment.
    void DrawRocks(const Rocks& rocks, float time, float focusX, float focusZ);
    // Furnaces (a glowing mouth, flames and chimney smoke; brighter while `busyStation` is being used) and anvils.
    void DrawStations(const std::vector<StationDef>& stations, int busyStation, float time, float focusX, float focusZ);
    // A burst of sparks (strike: 0 = the moment of impact, then the sparks fly out and die by 1).
    void DrawSparks(Vector3 at, float strike, float time, Color color);

private:
    Model* DropModel(const char* name);
    Model* Named(const std::string& file);   // assets/models/props/<file>.glb, loaded on first use
    std::map<std::string, Model> named_;
    std::map<std::string, Model> drops_;   // "bones" -> drop_bones.glb (loaded on first use)
    std::string dir_;
    const LitShader* shader_ = nullptr;
    Model fire_{};
    bool hasFire_ = false;
    TextureCache textures_;
};

}  // namespace journey
