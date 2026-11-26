// Tile collision map. Pure C++ (no raylib) so it can be unit tested and reused anywhere.
#pragma once
#include <string>
#include <vector>

namespace journey {

struct TileCoord {
    int x = 0;
    int z = 0;
    bool operator==(const TileCoord& o) const { return x == o.x && z == o.z; }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

// Rectangle in tiles, x1/z1 exclusive.
struct TileRect {
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    bool Contains(int x, int z) const { return x >= x0 && x < x1 && z >= z0 && z < z1; }
};

struct RoofDef {
    std::string name;  // model file is roof_<name>.glb
    TileRect rect;     // roof is hidden while the player is inside this rectangle
};

struct AreaDef {
    std::string name;  // shown on screen when the player enters
    TileRect rect;
};

// A choppable tree. Its tile is marked 'T' (blocked) in the grid; the game draws the tree model itself.
struct TreeDef {
    float x = 0, z = 0;       // world position in tiles (tile centre = +0.5)
    std::string kind;         // model is tree_<kind>.glb
    float scale = 1.0f;
    float yawDegrees = 0.0f;
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

// A fishing spot: a swirl of bubbles on a water tile ('~') next to the bank. You stand on the shore and net fish from it.
struct SpotDef {
    float x = 0, z = 0;       // world position in tiles (tile centre = +0.5)
    std::string kind;         // "net" for now (rods, cages... later)
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

// A rock you can mine. Its tile is marked 'o' (blocked); you stand next to it. kind = "copper", "tin", "iron", "coal", "mithril".
struct RockDef {
    float x = 0, z = 0;       // world position in tiles (tile centre = +0.5)
    std::string kind;
    float yawDegrees = 0.0f;
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

// Something a smith uses: kind = "furnace" (smelts ore into bars) or "anvil" (hammers bars into gear). One blocked tile.
struct StationDef {
    float x = 0, z = 0;
    std::string kind;
    float yawDegrees = 0.0f;
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

// A human standing in the world (shopkeeper, smith, villager). `key` names a row of the table in game/People.cpp.
struct PersonDef {
    std::string key;
    float x = 0, z = 0;
    float yawDegrees = 0.0f;   // 0 = facing south (+z), 90 = east
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

// A fenced area (chicken coop, cow pen). Animals that live there never leave `rect` (x1/z1 exclusive).
struct PenDef {
    std::string name;
    TileRect rect;
};

// An animal that lives in a pen. species is "chicken", "cow" or "calf".
struct NpcDef {
    std::string species;
    float x = 0, z = 0;       // starting position (tile centre = +0.5)
    std::string pen;          // name of the PenDef it lives in
};

class TileMap {
public:
    // Parses the text produced by tools/build_town.py. Returns false and fills err on bad input.
    bool ParseText(const std::string& text, std::string* err = nullptr);

    // Test helper: build a map from rows of characters.
    static TileMap FromRows(const std::vector<std::string>& rows);

    int Width() const { return width_; }
    int Height() const { return height_; }
    bool InBounds(int x, int z) const { return x >= 0 && z >= 0 && x < width_ && z < height_; }
    char At(int x, int z) const { return InBounds(x, z) ? tiles_[z * width_ + x] : '#'; }
    bool Walkable(int x, int z) const;
    bool Walkable(TileCoord t) const { return Walkable(t.x, t.z); }

    // Arrows and spells fly over low things: open ground, doors, water and fences ('f') let them through, walls, trees and
    // tall props stop them. ProjectileClear checks every tile on the straight line between two tiles (not the two ends).
    bool ShotPasses(int x, int z) const;
    bool ProjectileClear(TileCoord from, TileCoord to) const;

    // Things that appear while playing (a campfire) block their tile on top of the map's own blocking.
    void SetBlocked(TileCoord t, bool blocked);
    bool IsBlocked(TileCoord t) const;

    TileCoord Spawn() const { return spawn_; }
    const std::vector<RoofDef>& Roofs() const { return roofs_; }
    const std::vector<AreaDef>& Areas() const { return areas_; }
    const std::vector<TreeDef>& Trees() const { return trees_; }
    const std::vector<SpotDef>& Spots() const { return spots_; }
    const std::vector<RockDef>& Rocks() const { return rocks_; }
    const std::vector<StationDef>& Stations() const { return stations_; }
    const std::vector<PersonDef>& Persons() const { return persons_; }
    const std::vector<PenDef>& Pens() const { return pens_; }
    const std::vector<NpcDef>& Npcs() const { return npcs_; }
    // The pen with this name, or nullptr.
    const PenDef* FindPen(const std::string& name) const;

    // Name of the last area containing the tile, or empty string.
    const std::string& AreaAt(int x, int z) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<char> tiles_;
    TileCoord spawn_{};
    std::vector<RoofDef> roofs_;
    std::vector<AreaDef> areas_;
    std::vector<TreeDef> trees_;
    std::vector<SpotDef> spots_;
    std::vector<RockDef> rocks_;
    std::vector<StationDef> stations_;
    std::vector<PersonDef> persons_;
    std::vector<PenDef> pens_;
    std::vector<NpcDef> npcs_;
    std::vector<char> extraBlocked_;   // runtime blocking (campfires); empty until first used
};

}  // namespace journey
