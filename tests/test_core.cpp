// Tests for the pure-C++ game core (no window, no GPU). Run with: ctest
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "world/Pathfinder.h"
#include "world/TileMap.h"

using namespace journey;

static int g_failed = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

static void TestStraightPath() {
    TileMap m = TileMap::FromRows({".....", ".....", "....."});
    PathResult r = FindPath(m, {0, 0}, {4, 0});
    CHECK(r.found);
    CHECK(r.tiles.size() == 4);
    CHECK(r.tiles.back() == (TileCoord{4, 0}));
}

static void TestDiagonalIsShorter() {
    TileMap m = TileMap::FromRows({"....", "....", "....", "...."});
    PathResult r = FindPath(m, {0, 0}, {3, 3});
    CHECK(r.found);
    CHECK(r.tiles.size() == 3);  // three diagonal steps, not six straight ones
}

static void TestWallBlocksAndRoutesAround() {
    TileMap m = TileMap::FromRows({".#...",
                                   ".#.#.",
                                   ".#.#.",
                                   "...#."});
    PathResult r = FindPath(m, {0, 0}, {2, 0});
    CHECK(r.found);
    for (const TileCoord& t : r.tiles) CHECK(m.Walkable(t));
    CHECK(r.tiles.size() > 4);  // had to go all the way around the wall
}

static void TestNoCornerCutting() {
    // Diagonal step from (0,0) to (1,1) would squeeze between two walls; it must be refused.
    TileMap m = TileMap::FromRows({".#", "#."});
    PathResult r = FindPath(m, {0, 0}, {1, 1});
    CHECK(!r.found);
}

static void TestUnreachable() {
    TileMap m = TileMap::FromRows({".#.", ".#.", ".#."});
    CHECK(!FindPath(m, {0, 0}, {2, 0}).found);
}

static void TestBlockedGoal() {
    TileMap m = TileMap::FromRows({"...", ".#.", "..."});
    CHECK(!FindPath(m, {0, 0}, {1, 1}).found);
}

static void TestClickOnWallWalksNextToIt() {
    TileMap m = TileMap::FromRows({".....", "..#..", "....."});
    PathResult r = FindPathToward(m, {0, 1}, {2, 1});
    CHECK(r.found);
    CHECK(!r.tiles.empty());
    TileCoord end = r.tiles.back();
    CHECK(m.Walkable(end));
    CHECK(std::abs(end.x - 2) <= 1 && std::abs(end.z - 1) <= 1);
}

static void TestClickInsideSealedRoomFails() {
    TileMap m = TileMap::FromRows({"#####", "#...#", "#...#", "#####", "....."});
    PathResult r = FindPathToward(m, {0, 4}, {2, 1});
    CHECK(!r.found);
}

static void TestDoorsAreWalkable() {
    TileMap m = TileMap::FromRows({"###", "#D#", "#.#"});
    CHECK(m.Walkable(1, 1));
    CHECK(!m.Walkable(0, 0));
    CHECK(!m.Walkable(-1, 0));
    CHECK(!m.Walkable(5, 5));
}

static void TestParse() {
    const std::string text =
        "// comment\n@size 4 3\n@spawn 1 1\n@roof keep 0 0 2 2\n@area 0 0 4 3 Big Place\n@area 1 1 3 3 Small Place\n@rows\n"
        "####\n#..#\n####\n";
    TileMap m;
    std::string err;
    CHECK(m.ParseText(text, &err));
    CHECK(m.Width() == 4 && m.Height() == 3);
    CHECK(m.Spawn() == (TileCoord{1, 1}));
    CHECK(m.Roofs().size() == 1 && m.Roofs()[0].name == "keep");
    CHECK(m.AreaAt(1, 1) == "Small Place");  // later areas win where they overlap
    CHECK(m.AreaAt(0, 0) == "Big Place");
    CHECK(!m.ParseText("@size 9 9\n@rows\n##\n##\n", &err));  // @size mismatch is rejected
}

// Loads the real generated map and makes sure the important places connect.
static void TestRealTownMap(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping real-map test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    std::string err;
    CHECK(m.ParseText(ss.str(), &err));
    CHECK(m.Width() == 64 && m.Height() == 64);
    CHECK(m.Walkable(m.Spawn()));

    struct Goal { const char* name; TileCoord t; };
    const Goal goals[] = {
        {"castle gate", {32, 24}},        {"throne room carpet", {32, 10}}, {"inside the bank", {45, 33}},
        {"inside the inn", {45, 48}},     {"market square", {30, 38}},      {"river pier end", {2, 33}},
        {"east road exit", {63, 40}},     {"south road exit", {32, 63}},    {"castle courtyard", {22, 10}},
    };
    for (const Goal& g : goals) {
        if (!m.Walkable(g.t)) {
            std::printf("  FAIL goal %s at %d,%d is not walkable (%c)\n", g.name, g.t.x, g.t.z, m.At(g.t.x, g.t.z));
            ++g_failed;
            continue;
        }
        PathResult r = FindPath(m, m.Spawn(), g.t);
        if (!r.found) {
            std::printf("  FAIL cannot walk from spawn to %s\n", g.name);
            ++g_failed;
        }
    }
    // Walls must actually block: the keep's side wall is not walkable.
    CHECK(!m.Walkable(24, 10));
    // But its door is.
    CHECK(m.Walkable(32, 15));
}

int main(int argc, char** argv) {
    const std::string mapPath = argc > 1 ? argv[1] : "assets/maps/town.map";
    TestStraightPath();
    TestDiagonalIsShorter();
    TestWallBlocksAndRoutesAround();
    TestNoCornerCutting();
    TestUnreachable();
    TestBlockedGoal();
    TestClickOnWallWalksNextToIt();
    TestClickInsideSealedRoomFails();
    TestDoorsAreWalkable();
    TestParse();
    TestRealTownMap(mapPath);
    if (g_failed == 0) std::printf("All core tests passed.\n");
    else std::printf("%d check(s) failed.\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
