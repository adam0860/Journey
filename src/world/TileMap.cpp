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
    if (c != '.' && c != 'D') return false;
    return !IsBlocked({x, z});
}

void TileMap::SetBlocked(TileCoord t, bool blocked) {
    if (!InBounds(t.x, t.z)) return;
    if (extraBlocked_.empty()) {
        if (!blocked) return;
        extraBlocked_.assign(static_cast<size_t>(width_) * height_, 0);
    }
    extraBlocked_[static_cast<size_t>(t.z) * width_ + t.x] = blocked ? 1 : 0;
}

bool TileMap::IsBlocked(TileCoord t) const {
    if (extraBlocked_.empty() || !InBounds(t.x, t.z)) return false;
    return extraBlocked_[static_cast<size_t>(t.z) * width_ + t.x] != 0;
}

bool TileMap::ParseText(const std::string& text, std::string* err) {
    width_ = height_ = 0;
    tiles_.clear();
    roofs_.clear();
    areas_.clear();
    trees_.clear();
    spots_.clear();
    rocks_.clear();
    stations_.clear();
    persons_.clear();
    pens_.clear();
    npcs_.clear();
    extraBlocked_.clear();
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
        } else if (key == "@tree") {
            TreeDef t;
            ls >> t.x >> t.z >> t.kind >> t.scale >> t.yawDegrees;
            if (!ls.fail()) trees_.push_back(t);
        } else if (key == "@spot") {
            SpotDef s;
            ls >> s.x >> s.z >> s.kind;
            if (!ls.fail()) spots_.push_back(s);
        } else if (key == "@rock") {
            RockDef r;
            ls >> r.x >> r.z >> r.kind >> r.yawDegrees;
            if (!ls.fail()) rocks_.push_back(r);
        } else if (key == "@station") {
            StationDef st;
            ls >> st.kind >> st.x >> st.z >> st.yawDegrees;
            if (!ls.fail()) stations_.push_back(st);
        } else if (key == "@person") {
            PersonDef p;
            ls >> p.key >> p.x >> p.z >> p.yawDegrees;
            if (!ls.fail()) persons_.push_back(p);
        } else if (key == "@pen") {
            PenDef p;
            ls >> p.name >> p.rect.x0 >> p.rect.z0 >> p.rect.x1 >> p.rect.z1;
            if (!ls.fail()) pens_.push_back(p);
        } else if (key == "@npc") {
            NpcDef n;
            ls >> n.species >> n.x >> n.z >> n.pen;
            if (!ls.fail()) npcs_.push_back(n);
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

const PenDef* TileMap::FindPen(const std::string& name) const {
    for (const PenDef& p : pens_)
        if (p.name == name) return &p;
    return nullptr;
}

const std::string& TileMap::AreaAt(int x, int z) const {
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it)
        if (it->rect.Contains(x, z)) return it->name;
    return kNoArea;
}

}  // namespace journey
