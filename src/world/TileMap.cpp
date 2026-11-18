#include "world/TileMap.h"

#include <sstream>

namespace journey {

namespace {
const std::string kNoArea;

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
}  // namespace

bool TileMap::Walkable(int x, int z) const {
    char c = At(x, z);
    return c == '.' || c == 'D';
}

bool TileMap::ParseText(const std::string& text, std::string* err) {
    width_ = height_ = 0;
    tiles_.clear();
    roofs_.clear();
    areas_.clear();
    spawn_ = {};

    std::istringstream in(text);
    std::string line;
    bool rows = false;
    std::vector<std::string> grid;
    int declaredW = 0, declaredH = 0;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty()) continue;
        if (rows) {
            grid.push_back(line);
            continue;
        }
        if (line.rfind("//", 0) == 0) continue;
        if (line[0] != '@') continue;
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "@size") {
            ls >> declaredW >> declaredH;
        } else if (key == "@spawn") {
            ls >> spawn_.x >> spawn_.z;
        } else if (key == "@roof") {
            RoofDef r;
            ls >> r.name >> r.rect.x0 >> r.rect.z0 >> r.rect.x1 >> r.rect.z1;
            if (!ls.fail()) roofs_.push_back(r);
        } else if (key == "@area") {
            AreaDef a;
            ls >> a.rect.x0 >> a.rect.z0 >> a.rect.x1 >> a.rect.z1;
            std::getline(ls, a.name);
            a.name = Trim(a.name);
            if (!ls.fail() || !a.name.empty()) areas_.push_back(a);
        } else if (key == "@rows") {
            rows = true;
        }
    }

    if (grid.empty()) {
        if (err) *err = "map has no rows";
        return false;
    }
    height_ = static_cast<int>(grid.size());
    width_ = static_cast<int>(grid[0].size());
    for (const auto& r : grid) {
        if (static_cast<int>(r.size()) != width_) {
            if (err) *err = "map rows have different lengths";
            return false;
        }
    }
    if ((declaredW && declaredW != width_) || (declaredH && declaredH != height_)) {
        if (err) *err = "@size does not match the rows";
        return false;
    }
    tiles_.reserve(static_cast<size_t>(width_) * height_);
    for (const auto& r : grid) tiles_.insert(tiles_.end(), r.begin(), r.end());
    return true;
}

TileMap TileMap::FromRows(const std::vector<std::string>& rows) {
    TileMap m;
    m.height_ = static_cast<int>(rows.size());
    m.width_ = rows.empty() ? 0 : static_cast<int>(rows[0].size());
    for (const auto& r : rows) m.tiles_.insert(m.tiles_.end(), r.begin(), r.end());
    return m;
}

const std::string& TileMap::AreaAt(int x, int z) const {
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it)
        if (it->rect.Contains(x, z)) return it->name;
    return kNoArea;
}

}  // namespace journey
