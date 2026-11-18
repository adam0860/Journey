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

    TileCoord Spawn() const { return spawn_; }
    const std::vector<RoofDef>& Roofs() const { return roofs_; }
    const std::vector<AreaDef>& Areas() const { return areas_; }

    // Name of the last area containing the tile, or empty string.
    const std::string& AreaAt(int x, int z) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<char> tiles_;
    TileCoord spawn_{};
    std::vector<RoofDef> roofs_;
    std::vector<AreaDef> areas_;
};

}  // namespace journey
