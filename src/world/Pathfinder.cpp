#include "world/Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>

namespace journey {

namespace {

constexpr int kStraight = 10;
constexpr int kDiagonal = 14;

int Octile(int dx, int dz) {
    dx = std::abs(dx);
    dz = std::abs(dz);
    return kStraight * (dx + dz) + (kDiagonal - 2 * kStraight) * std::min(dx, dz);
}

struct Node {
    int f;
    int h;
    int index;
    bool operator>(const Node& o) const { return f != o.f ? f > o.f : h > o.h; }
};

}  // namespace

PathResult FindPath(const TileMap& map, TileCoord start, TileCoord goal) {
    PathResult result;
    if (!map.Walkable(start) || !map.Walkable(goal)) return result;
    if (start == goal) {
        result.found = true;
        return result;
    }

    const int w = map.Width();
    const int total = w * map.Height();
    const int kInf = 1 << 28;
    std::vector<int> g(total, kInf);
    std::vector<int> parent(total, -1);
    std::vector<uint8_t> closed(total, 0);

    auto idx = [w](int x, int z) { return z * w + x; };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    const int s = idx(start.x, start.z);
    const int e = idx(goal.x, goal.z);
    g[s] = 0;
    open.push({Octile(goal.x - start.x, goal.z - start.z), Octile(goal.x - start.x, goal.z - start.z), s});

    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dz[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    while (!open.empty()) {
        Node cur = open.top();
        open.pop();
        if (closed[cur.index]) continue;
        closed[cur.index] = 1;
        if (cur.index == e) break;

        const int cx = cur.index % w;
        const int cz = cur.index / w;
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + dx[d];
            const int nz = cz + dz[d];
            if (!map.Walkable(nx, nz)) continue;
            const bool diagonal = d >= 4;
            // No cutting corners: both orthogonal neighbours must be open for a diagonal step.
            if (diagonal && (!map.Walkable(cx + dx[d], cz) || !map.Walkable(cx, cz + dz[d]))) continue;
            const int ni = idx(nx, nz);
            if (closed[ni]) continue;
            const int ng = g[cur.index] + (diagonal ? kDiagonal : kStraight);
            if (ng < g[ni]) {
                g[ni] = ng;
                parent[ni] = cur.index;
                const int h = Octile(goal.x - nx, goal.z - nz);
                open.push({ng + h, h, ni});
            }
        }
    }

    if (parent[e] == -1) return result;
    for (int i = e; i != s; i = parent[i]) result.tiles.push_back({i % w, i / w});
    std::reverse(result.tiles.begin(), result.tiles.end());
    result.found = true;
    return result;
}

PathResult FindPathToward(const TileMap& map, TileCoord start, TileCoord clicked, int maxRadius) {
    if (!map.InBounds(clicked.x, clicked.z)) return {};
    if (map.Walkable(clicked)) return FindPath(map, start, clicked);

    struct Candidate {
        TileCoord t;
        float dist;
    };
    std::vector<Candidate> candidates;
    for (int dz = -maxRadius; dz <= maxRadius; ++dz) {
        for (int dx = -maxRadius; dx <= maxRadius; ++dx) {
            TileCoord t{clicked.x + dx, clicked.z + dz};
            if (map.Walkable(t)) candidates.push_back({t, std::sqrt(float(dx * dx + dz * dz))});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {
        if (a.dist != b.dist) return a.dist < b.dist;
        auto d = [&](TileCoord t) { return std::abs(t.x - start.x) + std::abs(t.z - start.z); };
        return d(a.t) < d(b.t);
    });
    // Try the closest few; some may be sealed off (e.g. inside a building behind a wall).
    const size_t tries = std::min<size_t>(candidates.size(), 8);
    for (size_t i = 0; i < tries; ++i) {
        PathResult r = FindPath(map, start, candidates[i].t);
        if (r.found) return r;
    }
    return {};
}

}  // namespace journey
