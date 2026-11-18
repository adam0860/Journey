// Tests for the pure-C++ game core (no window, no GPU). Run with: ctest
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "game/Forest.h"
#include "game/Inventory.h"
#include "game/Progress.h"
#include "game/Skills.h"
#include "game/Woodcutting.h"
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

// ---- skills, bag, trees, chopping --------------------------------------------------------------
static void TestXpTable() {
    CHECK(XpForLevel(1) == 0);
    CHECK(XpForLevel(2) == 83);
    CHECK(XpForLevel(10) == 1154);
    CHECK(XpForLevel(50) == 101333);
    CHECK(XpForLevel(99) == 13034431);
    CHECK(LevelForXp(0) == 1);
    CHECK(LevelForXp(82) == 1);
    CHECK(LevelForXp(83) == 2);
    CHECK(LevelForXp(13034430) == 98);
    CHECK(LevelForXp(13034431) == 99);
    CHECK(LevelForXp(200000000) == 99);   // never above 99
    for (int l = 2; l <= 99; ++l) CHECK(XpForLevel(l) > XpForLevel(l - 1));
}

static void TestSkills() {
    Skills s;
    for (int i = 0; i < kSkillCount; ++i) CHECK(s.Level(static_cast<Skill>(i)) == 1 && s.Xp(static_cast<Skill>(i)) == 0);
    CHECK(s.TotalLevel() == kSkillCount);
    int lv = 0;
    CHECK(!s.AddXp(Skill::Woodcutting, 25, &lv));
    CHECK(s.Level(Skill::Woodcutting) == 1);
    CHECK(s.AddXp(Skill::Woodcutting, 58, &lv));   // 25 + 58 = 83 -> level 2
    CHECK(lv == 2 && s.Level(Skill::Woodcutting) == 2);
    CHECK(s.Level(Skill::Mining) == 1);             // other skills untouched
    int64_t into = 0, need = 0;
    s.Progress(Skill::Woodcutting, &into, &need);
    CHECK(into == 0 && need == XpForLevel(3) - XpForLevel(2));
    s.AddXp(Skill::Woodcutting, 1000000000);        // absurd amount: clamps to 200M / level 99
    CHECK(s.Xp(Skill::Woodcutting) == kMaxXp && s.Level(Skill::Woodcutting) == 99);
    s.Progress(Skill::Woodcutting, &into, &need);
    CHECK(into == need);
    CHECK(!s.AddXp(Skill::Woodcutting, 5));
    CHECK(!s.AddXp(Skill::Mining, -5));             // negative / zero XP is ignored
}

static void TestInventory() {
    Inventory bag;
    CHECK(bag.FreeSlots() == Inventory::kSlots && !bag.Full());
    CHECK(bag.Add(Item::Logs) == 1);
    CHECK(bag.Count(Item::Logs) == 1 && bag.Slot(0).item == Item::Logs);
    CHECK(bag.Add(Item::Logs, 27) == 27);
    CHECK(bag.Full() && bag.Count(Item::Logs) == 28);
    CHECK(bag.Add(Item::Logs) == 0);                // full bag takes nothing
    const ItemStack taken = bag.TakeSlot(3);
    CHECK(taken.item == Item::Logs && taken.count == 1);
    CHECK(bag.FreeSlots() == 1 && bag.Slot(3).item == Item::None);
    CHECK(bag.TakeSlot(3).item == Item::None);      // empty slot
    CHECK(bag.TakeSlot(-1).item == Item::None && bag.TakeSlot(99).item == Item::None);
    CHECK(bag.Add(Item::None) == 0);
}

static void TestProgressSave() {
    Skills s;
    Inventory bag;
    s.AddXp(Skill::Woodcutting, 275);
    s.AddXp(Skill::Fishing, 10);
    bag.Add(Item::Logs, 3);
    bag.TakeSlot(1);   // leaves a gap: slots 0 and 2
    const std::string text = SerializeProgress(s, bag);
    Skills s2;
    Inventory bag2;
    ParseProgress(text, &s2, &bag2);
    CHECK(s2.Xp(Skill::Woodcutting) == 275 && s2.Level(Skill::Woodcutting) == LevelForXp(275));
    CHECK(s2.Xp(Skill::Fishing) == 10 && s2.Level(Skill::Mining) == 1);
    CHECK(bag2.Slot(0).item == Item::Logs && bag2.Slot(1).item == Item::None && bag2.Slot(2).item == Item::Logs);
    // Garbage must not crash or invent items.
    Skills s3;
    Inventory bag3;
    ParseProgress("xp.woodcutting=abc\nslot.99=1:1\nslot.0=77:1\nslot.1=1:-4\nnonsense\nxp.bogus=5\n", &s3, &bag3);
    CHECK(s3.Level(Skill::Woodcutting) == 1 && bag3.FreeSlots() == Inventory::kSlots);
}

static void TestForest() {
    TreeDef a, b;
    a.x = 5.5f; a.z = 5.5f; a.kind = "round";
    b.x = 9.5f; b.z = 2.5f; b.kind = "pine";
    Forest f;
    f.Init({a, b});
    CHECK(f.Trees().size() == 2);
    CHECK(f.IndexAtTile({5, 5}) == 0 && f.IndexAtTile({9, 2}) == 1 && f.IndexAtTile({1, 1}) == -1);
    CHECK(!f.Trees()[0].stump);
    CHECK(f.Fell(0));
    CHECK(f.Trees()[0].stump && !f.Trees()[1].stump);
    CHECK(!f.Fell(0));                               // already a stump
    CHECK(!f.Fell(7));                               // no such tree
    CHECK(f.IndexAtTile({5, 5}) == 0);               // the stump still occupies its tile
    f.Update(Forest::kRegrowSeconds - 1.0f);
    CHECK(f.Trees()[0].stump);                       // not yet
    CHECK(f.SecondsUntilRegrow(0) > 0.5f && f.SecondsUntilRegrow(0) < 1.5f);
    f.Update(2.0f);
    CHECK(!f.Trees()[0].stump);                      // grown back after the regrow time
    CHECK(f.Trees()[0].growAnim < 1.0f);
    f.Update(Forest::kGrowAnimSeconds + 0.1f);
    CHECK(f.Trees()[0].growAnim == 1.0f);
    CHECK(f.Fell(0));                                // can be cut again
}

static void TestChopping() {
    CHECK(Forest::kRegrowSeconds >= 120.0f);         // "a few minutes"
    CHECK(ChopChance(1) < ChopChance(50) && ChopChance(50) < ChopChance(99));
    CHECK(ChopChance(1) > 0.2f && ChopChance(99) < 0.9f);
    CHECK(ChopChance(-5) == ChopChance(1) && ChopChance(500) == ChopChance(99));

    ChopSession c;
    CHECK(!c.Active());
    CHECK(c.Update(10.0f, 1, [] { return 0.0; }) == ChopEvent::None);   // nothing to chop
    c.Start(4);
    CHECK(c.Active() && c.Tree() == 4);
    CHECK(c.Update(0.3f, 1, [] { return 0.0; }) == ChopEvent::None);    // half a tick: no swing yet
    CHECK(c.Update(0.4f, 1, [] { return 0.99; }) == ChopEvent::Swing);  // tick passed, unlucky roll
    CHECK(c.Active());
    CHECK(c.Update(kChopTickSeconds, 1, [] { return 0.0; }) == ChopEvent::GotLog);   // lucky roll
    CHECK(!c.Active());                                                  // one log per tree: job done

    // Statistics: higher level gets logs faster.
    auto avgSwings = [](int level) {
        unsigned seed = 12345;
        auto roll = [&seed] {
            seed = seed * 1664525u + 1013904223u;
            return (seed >> 8) / 16777216.0;
        };
        long swings = 0;
        for (int i = 0; i < 2000; ++i) {
            ChopSession s;
            s.Start(0);
            while (s.Active()) {
                if (s.Update(kChopTickSeconds, level, roll) != ChopEvent::None) ++swings;
            }
        }
        return swings / 2000.0;
    };
    const double low = avgSwings(1), high = avgSwings(99);
    CHECK(low > 3.0 && low < 5.0);    // ~1/0.25 = 4 swings
    CHECK(high < 1.6);                // ~1/0.78 = 1.3 swings
}

static void TestTreesInMap() {
    TileMap m;
    CHECK(m.ParseText("@size 3 2\n@tree 1.50 0.50 round 1.10 45.0\n@tree bogus\n@rows\n...\n.T.\n"));
    CHECK(m.Trees().size() == 1);                    // the malformed @tree line is skipped
    CHECK(m.Trees()[0].kind == "round" && m.Trees()[0].Tile() == TileCoord({1, 0}));
    CHECK(m.Trees()[0].scale > 1.09f && m.Trees()[0].scale < 1.11f && m.Trees()[0].yawDegrees == 45.0f);
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
    // Every choppable tree sits on a blocked 'T' tile, none share a tile, and each has a walkable neighbour.
    CHECK(m.Trees().size() > 50);
    Forest forest;
    forest.Init(m.Trees());
    for (size_t i = 0; i < m.Trees().size(); ++i) {
        const TileCoord t = m.Trees()[i].Tile();
        if (m.At(t.x, t.z) != 'T' || forest.IndexAtTile(t) != static_cast<int>(i)) {
            std::printf("  FAIL tree %zu at %d,%d: tile is '%c' or shared\n", i, t.x, t.z, m.At(t.x, t.z));
            ++g_failed;
        }
        bool open = false;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (m.Walkable(t.x + dx, t.z + dz)) open = true;
        if (!open) std::printf("  note: tree %zu at %d,%d has no open neighbouring tile\n", i, t.x, t.z);
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
    TestXpTable();
    TestSkills();
    TestInventory();
    TestProgressSave();
    TestForest();
    TestChopping();
    TestTreesInMap();
    TestRealTownMap(mapPath);
    if (g_failed == 0) std::printf("All core tests passed.\n");
    else std::printf("%d check(s) failed.\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
