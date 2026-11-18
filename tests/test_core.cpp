// Tests for the pure-C++ game core (no window, no GPU). Run with: ctest
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <algorithm>
#include <vector>

#include "game/Combat.h"
#include "game/Cooking.h"
#include "game/Equipment.h"
#include "game/Gear.h"
#include "game/Mining.h"
#include "game/People.h"
#include "game/Shops.h"
#include "game/Smithing.h"
#include "game/Fires.h"
#include "game/Fishing.h"
#include "game/Forest.h"
#include "game/Gameplay.h"
#include "game/GroundItems.h"
#include "game/Inventory.h"
#include "game/Npc.h"
#include "game/Player.h"
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
    for (int i = 0; i < kSkillCount; ++i) {
        const Skill sk = static_cast<Skill>(i);
        if (sk == Skill::Hitpoints) CHECK(s.Level(sk) == 10 && s.Xp(sk) == XpForLevel(10));   // hit points start at 10
        else CHECK(s.Level(sk) == 1 && s.Xp(sk) == 0);
    }
    CHECK(s.TotalLevel() == kSkillCount + 9);
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

static void TestMapExtras() {
    TileMap m;
    CHECK(m.ParseText("@size 4 2\n@spot 1.50 0.50 net\n@spot nonsense\n@rows\n~~..\n~~..\n"));
    CHECK(m.Spots().size() == 1 && m.Spots()[0].kind == "net" && m.Spots()[0].Tile() == TileCoord({1, 0}));
    CHECK(m.Walkable(2, 0));
    m.SetBlocked({2, 0}, true);                      // a campfire
    CHECK(!m.Walkable(2, 0) && m.IsBlocked({2, 0}) && m.Walkable(3, 0));
    CHECK(!FindPath(m, {3, 0}, {2, 0}).found);
    m.SetBlocked({2, 0}, false);
    CHECK(m.Walkable(2, 0));
    m.SetBlocked({-4, 9}, true);                     // out of range is ignored
    CHECK(!m.IsBlocked({-4, 9}));
}

static void TestItems() {
    Inventory bag;
    CHECK(GiveStarterKit(&bag) == 5);
    CHECK(bag.Has(Item::BronzeAxe) && bag.Has(Item::SmallNet) && bag.Has(Item::Tinderbox) && bag.Has(Item::BronzePickaxe) && bag.Has(Item::Hammer));
    CHECK(GiveStarterKit(&bag) == 0);                // never duplicates
    bag.Add(Item::Logs, 3);
    CHECK(bag.Count(Item::Logs) == 3 && bag.FindSlot(Item::Logs) == 5 && bag.FindSlot(Item::Shrimps) == -1);
    CHECK(bag.Remove(Item::Logs, 2) && bag.Count(Item::Logs) == 1);
    CHECK(!bag.Remove(Item::Logs, 5) && bag.Count(Item::Logs) == 1);   // too few: nothing changes
    CHECK(bag.Remove(Item::Logs) && !bag.Has(Item::Logs) && bag.FindSlot(Item::Logs) == -1);

    // Buttons shown under a selected item.
    auto has = [](const std::vector<ItemAction>& v, ItemAction a) { return std::find(v.begin(), v.end(), a) != v.end(); };
    bag.Add(Item::Logs);
    CHECK(has(ActionsFor(Item::Logs, bag), ItemAction::Light) && has(ActionsFor(Item::Logs, bag), ItemAction::Drop));
    CHECK(has(ActionsFor(Item::Tinderbox, bag), ItemAction::Light) && !has(ActionsFor(Item::Tinderbox, bag), ItemAction::Drop));
    CHECK(has(ActionsFor(Item::BronzeAxe, bag), ItemAction::Use) && !has(ActionsFor(Item::BronzeAxe, bag), ItemAction::Drop));   // tools: no Drop
    CHECK(has(ActionsFor(Item::SmallNet, bag), ItemAction::Examine) && !has(ActionsFor(Item::SmallNet, bag), ItemAction::Drop));
    CHECK(has(ActionsFor(Item::RawShrimps, bag), ItemAction::Cook) && !has(ActionsFor(Item::RawShrimps, bag), ItemAction::Eat));
    CHECK(has(ActionsFor(Item::Shrimps, bag), ItemAction::Eat) && !has(ActionsFor(Item::Shrimps, bag), ItemAction::Cook));
    CHECK(has(ActionsFor(Item::BurntFish, bag), ItemAction::Drop) && !has(ActionsFor(Item::BurntFish, bag), ItemAction::Eat));
    CHECK(has(ActionsFor(Item::Bones, bag), ItemAction::Bury) && ActionsFor(Item::Bones, bag)[0] == ItemAction::Bury);
    CHECK(has(ActionsFor(Item::RawChicken, bag), ItemAction::Cook) && has(ActionsFor(Item::Beef, bag), ItemAction::Eat));
    CHECK(has(ActionsFor(Item::Egg, bag), ItemAction::Eat) && !has(ActionsFor(Item::Cowhide, bag), ItemAction::Eat));
    CHECK(ActionsFor(Item::None, bag).empty());
    for (int i = 1; i < static_cast<int>(Item::Count); ++i) {   // every item has a name, an examine text and a ground model
        const ItemInfo& info = GetItemInfo(static_cast<Item>(i));
        CHECK(info.name && info.name[0] && info.examine && info.examine[0] && info.model && info.model[0]);
        CHECK(has(ActionsFor(static_cast<Item>(i), bag), ItemAction::Examine));
    }
    Inventory bare;
    bare.Add(Item::Logs);
    CHECK(!has(ActionsFor(Item::Logs, bare), ItemAction::Light));   // no tinderbox, no fire

    // Raw food is found for cooking; names all start with "Raw " (the chat messages rely on it).
    Inventory food;
    CHECK(food.FindRawFood() == -1);
    food.Add(Item::Logs);
    food.Add(Item::RawAnchovies);
    CHECK(food.FindRawFood() == 1);
    CHECK(std::string(GetItemInfo(Item::RawShrimps).name).rfind("Raw ", 0) == 0);
    CHECK(std::string(GetItemInfo(Item::RawAnchovies).name).rfind("Raw ", 0) == 0);
}

static void TestFishing() {
    CHECK(CatchChance(1) < CatchChance(50) && CatchChance(50) < CatchChance(99));
    CHECK(CatchChance(1) > 0.2f && CatchChance(99) < 1.0f);
    CHECK(RollCatch(1, 0.0) == Item::RawShrimps);                          // anchovies need level 15
    CHECK(RollCatch(kAnchovyLevel, 0.0) == Item::RawAnchovies && RollCatch(kAnchovyLevel, 0.9) == Item::RawShrimps);
    CHECK(FishXp(Item::RawAnchovies) > FishXp(Item::RawShrimps) && FishXp(Item::RawShrimps) > 0 && FishXp(Item::Logs) == 0);
    CHECK(FishLevelRequired(Item::RawShrimps) == 1 && FishLevelRequired(Item::RawAnchovies) == kAnchovyLevel);

    FishSession f;
    CHECK(!f.Active() && f.Update(5.0f, 1, [] { return 0.0; }) == FishEvent::None);
    f.Start(2);
    CHECK(f.Active() && f.Spot() == 2);
    CHECK(f.Update(0.5f, 1, [] { return 0.0; }) == FishEvent::None);       // still casting
    CHECK(f.Update(0.8f, 1, [] { return 0.99; }) == FishEvent::Cast);      // net came up empty
    CHECK(f.Update(kFishCastSeconds, 1, [] { return 0.0; }) == FishEvent::Caught);
    CHECK(f.Active());                                                      // keeps fishing after a catch
}

static void TestCooking() {
    const Recipe* shrimp = RecipeFor(Item::RawShrimps);
    CHECK(shrimp && shrimp->cooked == Item::Shrimps && shrimp->burnt == Item::BurntFish && shrimp->xp > 0);
    CHECK(RecipeFor(Item::Logs) == nullptr && RecipeFor(Item::Shrimps) == nullptr);
    CHECK(RecipeForCooked(Item::Anchovies) && RecipeForCooked(Item::Anchovies)->raw == Item::RawAnchovies);
    CHECK(BurnChance(*shrimp, 1) > 0.3f);                                   // beginners burn a lot of food
    CHECK(BurnChance(*shrimp, 1) > BurnChance(*shrimp, 20) && BurnChance(*shrimp, 20) > BurnChance(*shrimp, 33));
    CHECK(BurnChance(*shrimp, shrimp->burnStopLevel) == 0.0f && BurnChance(*shrimp, 99) == 0.0f);
    for (int l = 1; l <= 99; ++l) CHECK(BurnChance(*shrimp, l) >= 0.0f && BurnChance(*shrimp, l) <= 1.0f);
}

static void TestFires() {
    TileMap m = TileMap::FromRows({"....", ".#D.", "...."});
    Fires fires;
    CHECK(fires.CanLight(m, {0, 0}) && !fires.CanLight(m, {1, 1}) && !fires.CanLight(m, {2, 1}));   // not in a wall or doorway
    CHECK(!fires.CanLight(m, {9, 9}));
    LightSession l;
    CHECK(l.Update(9.0f, 1, [] { return 0.0; }) == LightEvent::None);
    l.Start();
    CHECK(l.Update(0.5f, 1, [] { return 0.0; }) == LightEvent::None);
    CHECK(l.Update(0.8f, 1, [] { return 0.99; }) == LightEvent::Failed);
    CHECK(l.Active());
    CHECK(l.Update(kLightAttemptSeconds, 1, [] { return 0.0; }) == LightEvent::Lit && !l.Active());
    CHECK(LightChance(1) < LightChance(99) && LightChance(99) < 1.0f);

    const int id = fires.Light(&m, {0, 0});
    CHECK(id > 0 && !m.Walkable(0, 0) && fires.BurningAt({0, 0}) != nullptr);
    CHECK(fires.Light(&m, {0, 0}) == -1);                                  // already burning there
    CHECK(fires.NearestBurning({3, 2}) && fires.NearestBurning({3, 2})->id == id);
    CHECK(fires.NearestBurning({3, 2}) == fires.Find(id));
    const int id2 = fires.Light(&m, {3, 0});
    CHECK(id2 > id && fires.NearestBurning({3, 2})->id == id2);            // the closer one wins
    fires.Update(kFireBurnSeconds - 1.0f, &m);
    CHECK(fires.BurningAt({0, 0}) && !m.Walkable(0, 0));
    fires.Update(2.0f, &m);                                                // burnt out: tile is free, ashes remain
    CHECK(!fires.BurningAt({0, 0}) && m.Walkable(0, 0) && fires.Find(id) && !fires.Find(id)->Burning());
    CHECK(fires.NearestBurning({0, 0}) == nullptr);
    CHECK(fires.CanLight(m, {0, 0}));                                      // can relight on the ashes
    fires.Update(kAshSeconds + 1.0f, &m);
    CHECK(fires.All().empty() && fires.Find(id) == nullptr);
}

// Records everything the game says, so a test can check the story.
struct RecordingEvents : GameEvents {
    std::vector<std::string> said;
    int xpDrops = 0, levelUps = 0, changes = 0, deaths = 0, makeMenus = 0, shopMenus = 0;
    MakeMenu lastMake;
    ShopMenu lastShop;
    std::vector<int> hitsOnPlayer, hitsOnNpc;   // damage numbers of every hitsplat
    void Say(const std::string& t, Tone) override { said.push_back(t); }
    void XpDrop(Skill, int) override { ++xpDrops; }
    void LevelUp(Skill, int) override { ++levelUps; }
    void Hit(bool onPlayer, int, int damage) override {
        (onPlayer ? hitsOnPlayer : hitsOnNpc).push_back(damage);
    }
    void PlayerDied() override { ++deaths; }
    void Changed() override { ++changes; }
    void OpenShop(const ShopMenu& m) override {
        ++shopMenus;
        lastShop = m;
    }
    void OpenMake(const MakeMenu& m) override {
        ++makeMenus;
        lastMake = m;
    }
    bool Said(const std::string& part) const {
        for (const std::string& s : said)
            if (s.find(part) != std::string::npos) return true;
        return false;
    }
};

// A small world: river on the left (with a fishing spot), a tree on the right, open grass between.
struct MiniWorld {
    TileMap map;
    Player player;
    Skills skills;
    Inventory bag;
    Equipment equip;
    Forest forest;
    Rocks rocks;
    Fires fires;
    Npcs npcs;
    GroundItems ground;
    Shops shops;
    People people;
    RecordingEvents ev;
    double rollValue = 0.0;   // 0.0 = every roll succeeds (and burns food); 0.99 = every roll fails
    Gameplay game;

    MiniWorld()
        : game(map, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, ev, [this] { return rollValue; }) {
        map.ParseText("@size 14 4\n@spawn 6 1\n@tree 8.5 2.5 round 1.0 0.0\n@spot 2.5 1.5 net\n"
                      "@pen yard 10 0 14 4\n@npc chicken 11.5 1.5 yard\n@npc cow 12.5 2.5 yard\n"
                      "@rock 5.5 0.5 copper 0\n@rock 7.5 0.5 iron 0\n@station furnace 4.5 3.5 0\n@station anvil 6.5 3.5 0\n"
                      "@person aubrey 8.5 3.5 180\n@person wilf 12.5 0.5 0\n@rows\n"
                      "~~~..o.o......\n"
                      "~~~...........\n"
                      "~~~.....T.....\n"
                      "~~~.o.o.o.....\n");
        player.Teleport({6, 1});
        forest.Init(map.Trees());
        rocks.Init(map.Rocks());
        npcs.Init(map);
        shops.Init();
        people.Init(map);
    }
    void Run(float seconds) {
        for (float t = 0; t < seconds; t += 0.05f) {
            player.Update(0.05f);
            forest.Update(0.05f);
            rocks.Update(0.05f);
            shops.Update(0.05f);
            fires.Update(0.05f, &map);
            ground.Update(0.05f);
            npcs.Update(0.05f, map, player.Tile(), [this] { return rollValue < 0.5 ? 0.37 : 0.9; });
            game.Update(0.05f);
        }
    }
};

static void TestGameplayChopping() {
    MiniWorld w;
    w.game.TapTree(0);                                   // no axe in the bag yet
    CHECK(w.ev.Said("need an axe") && w.game.Current() == Activity::None && !w.player.Moving());
    GiveStarterKit(&w.bag);
    w.game.TapTree(0);
    float mx = 0, mz = 0;
    CHECK(w.game.TakeMarker(&mx, &mz) && mx == 8.5f && mz == 2.5f && !w.game.TakeMarker(&mx, &mz));
    CHECK(w.player.Moving());                            // walking up to the tree
    w.Run(3.0f);
    CHECK(w.bag.Has(Item::Logs) && w.forest.Trees()[0].stump);
    CHECK(w.skills.Xp(Skill::Woodcutting) == kLogXp && w.ev.xpDrops == 1 && w.game.Current() == Activity::None);
    w.game.TapTree(0);                                   // the stump
    CHECK(w.ev.Said("grow back"));

    // Chopping chance: with bad luck nothing drops yet, but the swing animation runs.
    MiniWorld u;
    GiveStarterKit(&u.bag);
    u.rollValue = 0.99;
    u.game.TapTree(0);
    u.Run(4.0f);
    CHECK(!u.bag.Has(Item::Logs) && u.game.Current() == Activity::Chopping);
    u.game.Cancel();                                     // tapped elsewhere
    CHECK(u.game.Current() == Activity::None);
    u.Run(3.0f);
    CHECK(!u.bag.Has(Item::Logs) && !u.forest.Trees()[0].stump);

    // A full bag stops the chopping politely.
    MiniWorld f;
    GiveStarterKit(&f.bag);
    while (!f.bag.Full()) f.bag.Add(Item::BurntFish);
    f.game.TapTree(0);
    f.Run(3.0f);
    CHECK(f.ev.Said("too full") && !f.forest.Trees()[0].stump);
}

static void TestGameplayFishingAndCooking() {
    MiniWorld w;
    w.game.TapSpot(0);                                   // no net
    CHECK(w.ev.Said("need a small fishing net"));
    GiveStarterKit(&w.bag);
    w.game.TapSpot(0);
    CHECK(w.player.Moving());
    w.Run(3.0f);                                         // walks to the bank (x=3), starts fishing
    CHECK(w.player.Tile().x == 3);
    CHECK(w.game.Current() == Activity::Fishing || w.bag.Has(Item::RawShrimps));
    w.Run(4.0f);
    CHECK(w.bag.Count(Item::RawShrimps) >= 3);           // keeps casting: several shrimps
    CHECK(w.skills.Xp(Skill::Fishing) >= 30 && w.game.Current() == Activity::Fishing);
    CHECK(w.ev.Said("You catch some shrimps"));

    // Walking away ends it.
    w.game.Cancel();
    const int caught = w.bag.Count(Item::RawShrimps);
    w.Run(5.0f);
    CHECK(w.bag.Count(Item::RawShrimps) == caught);

    // Fish until the bag is full.
    MiniWorld full;
    GiveStarterKit(&full.bag);
    full.game.TapSpot(0);
    full.Run(60.0f);
    CHECK(full.bag.Full() && full.game.Current() == Activity::None && full.ev.Said("too full"));

    // Cooking: no fire yet.
    MiniWorld c;
    GiveStarterKit(&c.bag);
    c.bag.Add(Item::RawShrimps, 2);
    c.bag.Add(Item::Logs);
    c.game.DoItemAction(c.bag.FindSlot(Item::RawShrimps), ItemAction::Cook);
    CHECK(c.ev.Said("need a fire") && c.game.Current() == Activity::None);

    // Light a fire (the hero stands at 6,1), then step away; the tile is blocked.
    const TileCoord spot = c.player.Tile();
    c.game.DoItemAction(c.bag.FindSlot(Item::Logs), ItemAction::Light);
    CHECK(c.game.Current() == Activity::Lighting && c.ev.Said("attempt to light"));
    c.rollValue = 0.99;
    c.Run(4.0f);                                         // unlucky: still trying
    CHECK(c.game.Current() == Activity::Lighting && c.bag.Has(Item::Logs));
    c.rollValue = 0.0;
    c.Run(1.5f);
    CHECK(!c.bag.Has(Item::Logs) && c.fires.BurningAt(spot) && !c.map.Walkable(spot));
    CHECK(c.skills.Xp(Skill::Firemaking) == kLightXp && c.ev.Said("logs begin to burn"));
    c.Run(1.0f);
    CHECK(c.player.Tile() != spot);                      // stepped off the fire

    // Cook both shrimps. Level 1 burns food when the roll is low; a high roll always cooks.
    c.rollValue = 0.99;
    c.game.DoItemAction(c.bag.FindSlot(Item::RawShrimps), ItemAction::Cook);
    c.Run(1.0f);
    CHECK(c.game.Current() == Activity::Cooking || c.bag.Count(Item::Shrimps) > 0);
    c.Run(5.0f);
    CHECK(c.bag.Count(Item::Shrimps) == 2 && !c.bag.Has(Item::RawShrimps) && c.skills.Xp(Skill::Cooking) == 60);
    CHECK(c.game.Current() == Activity::None && c.ev.Said("nothing left to cook"));

    // Burning: bad luck at level 1 gives burnt fish and no XP.
    c.bag.Add(Item::RawShrimps);
    c.rollValue = 0.0;
    c.game.TapFire(c.fires.All()[0].id);
    c.Run(5.0f);
    CHECK(c.bag.Count(Item::BurntFish) == 1 && c.skills.Xp(Skill::Cooking) == 60 && c.ev.Said("accidentally burn"));

    // Eat and drop.
    c.game.DoItemAction(c.bag.FindSlot(Item::Shrimps), ItemAction::Eat);
    CHECK(c.bag.Count(Item::Shrimps) == 1 && c.ev.Said("You eat the shrimps"));
    c.game.DoItemAction(c.bag.FindSlot(Item::BurntFish), ItemAction::Drop);
    CHECK(!c.bag.Has(Item::BurntFish));
    c.game.DoItemAction(c.bag.FindSlot(Item::BronzeAxe), ItemAction::Drop);
    CHECK(c.bag.Has(Item::BronzeAxe) && c.ev.Said("keep your bronze axe"));      // tools stay

    // The fire burns out while we wait: tapping it says so.
    c.Run(kFireBurnSeconds + 1.0f);
    CHECK(c.map.Walkable(spot));
    c.bag.Add(Item::RawShrimps);
    c.game.TapFire(c.fires.All()[0].id);
    CHECK(c.ev.Said("burnt out") && c.game.Current() == Activity::None);
}

static void TestGameplayLightingRules() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    w.game.DoItemAction(w.bag.FindSlot(Item::Tinderbox), ItemAction::Light);
    CHECK(w.ev.Said("no logs"));
    w.bag.Add(Item::Logs, 2);
    w.game.DoItemAction(w.bag.FindSlot(Item::Logs), ItemAction::Light);
    w.Run(3.0f);
    CHECK(w.bag.Count(Item::Logs) == 1);
    // Two fires cannot share a tile: the hero stepped off, so move back onto the burning tile? Not possible - it is blocked.
    CHECK(!w.map.Walkable(6, 1) || !w.fires.CanLight(w.map, {6, 1}));
    // Walking cancels lighting.
    w.game.DoItemAction(w.bag.FindSlot(Item::Logs), ItemAction::Light);
    w.rollValue = 0.99;
    w.Run(2.0f);
    CHECK(w.game.Current() == Activity::Lighting);
    w.game.Cancel();
    w.Run(5.0f);
    CHECK(w.bag.Count(Item::Logs) == 1 && w.game.Current() == Activity::None);
}

static void TestHitpointsAndCombatLevel() {
    Skills s;
    CHECK(s.Hp() == 10 && s.MaxHp() == 10 && s.CombatLevel() == 3);   // 0.25*(1+10) + 0.325*(1+1) = 3.4
    s.Hurt(4);
    CHECK(s.Hp() == 6);
    CHECK(s.Heal(3) == 3 && s.Hp() == 9);
    CHECK(s.Heal(50) == 1 && s.Hp() == 10);                             // cannot heal past the maximum
    s.Hurt(99);
    CHECK(s.Hp() == 0);                                                  // never negative
    s.SetHp(5);
    s.AddXp(Skill::Hitpoints, XpForLevel(11) - XpForLevel(10));          // level 11: heals by the same one point
    CHECK(s.Level(Skill::Hitpoints) == 11 && s.MaxHp() == 11 && s.Hp() == 6);
    s.SetXp(Skill::Hitpoints, 0);                                        // hit points never go below the starting 10
    CHECK(s.Level(Skill::Hitpoints) == 10);
    Skills m;
    m.SetXp(Skill::Attack, XpForLevel(40));
    m.SetXp(Skill::Strength, XpForLevel(40));
    m.SetXp(Skill::Defence, XpForLevel(40));
    m.SetXp(Skill::Hitpoints, XpForLevel(40));
    m.SetXp(Skill::Prayer, XpForLevel(20));
    CHECK(m.CombatLevel() == static_cast<int>(0.25 * (40 + 40 + 10) + 0.325 * 80));   // 22.5 + 26 = 48
    // hit points survive a save, whatever order the file lists them in
    Inventory bag;
    Skills a;
    a.SetXp(Skill::Hitpoints, XpForLevel(30));
    a.SetHp(17);
    const std::string text = SerializeProgress(a, bag, CombatStyle::Defensive);
    Skills b;
    CombatStyle style = CombatStyle::Accurate;
    ParseProgress(text, &b, &bag, &style);
    CHECK(b.Level(Skill::Hitpoints) == 30 && b.Hp() == 17 && style == CombatStyle::Defensive);
    Skills c;
    ParseProgress("hp=25\nxp.hitpoints=" + std::to_string(XpForLevel(30)) + "\n", &c, &bag);   // hp line first
    CHECK(c.Hp() == 25);
    Skills d;
    ParseProgress("xp.attack=500\n", &d, &bag);                         // an old save with no hp line: full health
    CHECK(d.Hp() == 10 && d.Level(Skill::Attack) == LevelForXp(500));
    Skills e;
    ParseProgress("hp=999\nstyle=77\n", &e, &bag, &style);
    CHECK(e.Hp() == 10 && style == CombatStyle::Controlled);            // clamped, never out of range
}

static void TestCombatMath() {
    Skills s;
    const Fighter hero = PlayerFighter(s, CombatStyle::Accurate);
    Fighter chicken;
    chicken.creature = true;
    CHECK(MaxHit(hero) == 1 && MaxHit(chicken) == 1);                    // level 1 strength hits for 1
    Skills strong;
    strong.SetXp(Skill::Strength, XpForLevel(99));
    strong.SetXp(Skill::Attack, XpForLevel(99));
    CHECK(MaxHit(PlayerFighter(strong, CombatStyle::Aggressive)) >= 11);
    CHECK(MaxHit(PlayerFighter(strong, CombatStyle::Accurate)) <= MaxHit(PlayerFighter(strong, CombatStyle::Aggressive)));
    Skills mid;
    mid.SetXp(Skill::Strength, XpForLevel(20));
    CHECK(MaxHit(PlayerFighter(mid, CombatStyle::Accurate)) < MaxHit(PlayerFighter(mid, CombatStyle::Aggressive)) + 1);   // +3 strength never lowers it
    // The better your Attack against their Defence, the more often you hit; never 0 or 1.
    Fighter low, high, def;
    low.attack = 1;
    high.attack = 80;
    def.defence = 20;
    def.creature = true;
    CHECK(HitChance(low, def) < HitChance(high, def));
    for (const Fighter& f : {low, high, hero}) CHECK(HitChance(f, def) > 0.0 && HitChance(f, def) < 1.0);
    Fighter tough = def;
    tough.defence = 90;
    CHECK(HitChance(high, tough) < HitChance(high, def));               // more defence, fewer hits
    const double even = HitChance(PlayerFighter(s, CombatStyle::Accurate), chicken);
    CHECK(even > 0.4 && even < 0.7);                                     // a fresh hero vs a chicken: a bit better than a coin flip

    // Rolling: a low roll lands for 1..max; a high roll misses (a 0).
    const AttackResult hit = RollAttack(hero, chicken, [] { return 0.0; });
    CHECK(hit.landed && hit.damage == 1);
    const AttackResult miss = RollAttack(hero, chicken, [] { return 0.999; });
    CHECK(!miss.landed && miss.damage == 0);
    const Fighter big = PlayerFighter(strong, CombatStyle::Aggressive);
    int lo = 1000, hi = 0;
    unsigned seed = 7;
    auto roll = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) / 16777216.0;
    };
    int landed = 0;
    for (int i = 0; i < 4000; ++i) {
        const AttackResult r = RollAttack(big, chicken, roll);
        if (r.landed) {
            ++landed;
            lo = std::min(lo, r.damage);
            hi = std::max(hi, r.damage);
            CHECK(r.damage >= 1 && r.damage <= MaxHit(big));
        } else {
            CHECK(r.damage == 0);
        }
    }
    CHECK(lo == 1 && hi == MaxHit(big));                                 // the whole range 1..max shows up
    CHECK(landed > 3000);                                                // a level-99 striker rarely misses a chicken

    // XP: 4 per point to the trained skill, 1.33 to hit points; controlled shares it out.
    CombatXp x = XpForDamage(CombatStyle::Accurate, 3);
    CHECK(x.attack == 12 && x.strength == 0 && x.defence == 0 && x.hitpoints == 4);
    x = XpForDamage(CombatStyle::Aggressive, 2);
    CHECK(x.strength == 8 && x.attack == 0 && x.hitpoints == 3);
    x = XpForDamage(CombatStyle::Defensive, 1);
    CHECK(x.defence == 4 && x.hitpoints == 1);
    x = XpForDamage(CombatStyle::Controlled, 3);
    CHECK(x.attack == 4 && x.strength == 4 && x.defence == 4 && x.hitpoints == 4);
    x = XpForDamage(CombatStyle::Accurate, 0);
    CHECK(x.attack == 0 && x.hitpoints == 0);                            // a miss earns nothing
    CHECK(StyleFromIndex(-3) == CombatStyle::Accurate && StyleFromIndex(9) == CombatStyle::Controlled);

    // Level colours: green well below you, yellow level, red well above, blending between.
    Rgb c = LevelColor(30, 5);
    CHECK(c.r == 0 && c.g == 255);
    c = LevelColor(30, 30);
    CHECK(c.r == 255 && c.g == 255 && c.b == 0);
    c = LevelColor(3, 40);
    CHECK(c.r == 255 && c.g == 0);
    c = LevelColor(10, 13);                                              // a little above: orange-ish (less green)
    CHECK(c.r == 255 && c.g > 0 && c.g < 255);
    c = LevelColor(10, 7);                                               // a little below: yellow-green
    CHECK(c.g == 255 && c.r > 0 && c.r < 255);
    CHECK(LevelColor(10, 12).g > LevelColor(10, 18).g);                  // redder the higher they are
    CHECK(LevelColor(10, 8).r > LevelColor(10, 2).r);                    // greener the lower they are
}

static void TestNpcsAndLoot() {
    TileMap m;
    CHECK(m.ParseText("@size 12 6\n@pen yard 6 1 11 5\n@pen bogus 1\n@npc chicken 7.5 2.5 yard\n@npc cow 9.5 3.5 yard\n@npc dragon 8.5 2.5 yard\n"
                      "@npc calf 8.5 2.5 nowhere\n@rows\n"
                      "............\n.....#####..\n......#.....\n.....#......\n............\n............\n"));
    CHECK(m.Pens().size() == 1 && m.FindPen("yard") && m.FindPen("yard")->rect.Contains(10, 4) && !m.FindPen("yard")->rect.Contains(11, 4));
    CHECK(m.FindPen("nothing") == nullptr);
    CHECK(m.Npcs().size() == 4);                                         // (bad ones are dropped when the game spawns them)
    Npcs npcs;
    npcs.Init(m);
    CHECK(npcs.All().size() == 2);                                       // unknown species / unknown pen skipped
    CHECK(npcs.All()[0].species == Species::Chicken && npcs.All()[1].species == Species::Cow);
    CHECK(npcs.All()[0].id != npcs.All()[1].id && npcs.Find(npcs.All()[1].id) == &npcs.All()[1] && npcs.Find(99) == nullptr);
    CHECK(npcs.All()[0].hp == 3 && npcs.All()[1].hp == GetSpecies(Species::Cow).hitpoints);

    // species table sanity (the numbers the menus and the fights use)
    for (int i = 0; i < kSpeciesCount; ++i) {
        const SpeciesInfo& sp = GetSpecies(static_cast<Species>(i));
        CHECK(sp.level >= 1 && sp.hitpoints >= 1 && sp.speed > 0.0f && sp.respawnSeconds > 0.0f && sp.lootCount >= 1);
        Species back;
        CHECK(SpeciesFromKey(sp.key, &back) && back == static_cast<Species>(i));
        CHECK(sp.loot[0].item == Item::Bones && sp.loot[0].chance == 1.0f);   // everything leaves bones
    }
    Species dummy;
    CHECK(!SpeciesFromKey("dragon", &dummy));
    CHECK(GetSpecies(Species::Chicken).level < GetSpecies(Species::Cow).level);

    // They wander about, always inside their pen.
    TileMap open;
    open.ParseText("@size 12 6\n@pen yard 6 1 11 5\n@npc chicken 7.5 2.5 yard\n@npc chicken 9.5 3.5 yard\n@npc cow 8.5 1.5 yard\n@rows\n"
                   "............\n............\n............\n............\n............\n............\n");
    Npcs herd;
    herd.Init(open);
    unsigned seed = 99;
    auto roll = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) / 16777216.0;
    };
    float travelled = 0;
    float px = herd.All()[0].x, pz = herd.All()[0].z;
    bool leftPen = false;
    for (int i = 0; i < 4000; ++i) {   // 200 seconds
        herd.Update(0.05f, open, {0, 5}, roll);
        for (const Npc& n : herd.All())
            if (!open.FindPen("yard")->rect.Contains(n.Tile().x, n.Tile().z)) leftPen = true;
        travelled += std::fabs(herd.All()[0].x - px) + std::fabs(herd.All()[0].z - pz);
        px = herd.All()[0].x;
        pz = herd.All()[0].z;
    }
    CHECK(!leftPen);
    CHECK(travelled > 5.0f);                                             // it really does wander

    // Killing: damage, death, the loot waits, then it comes back.
    Npc& chicken = herd.All()[0];
    CHECK(!herd.Hurt(chicken.id, 1) && chicken.hp == 2 && chicken.hurtT == 0.0f);
    CHECK(herd.Hurt(chicken.id, 5) && chicken.dead && chicken.hp == 0 && chicken.lootPending);
    CHECK(!herd.Hurt(chicken.id, 5) && !herd.Hurt(777, 1));              // already dead / no such animal
    CHECK(herd.AliveAt(chicken.Tile()) != chicken.id);
    for (int i = 0; i < 40 * 20; ++i) herd.Update(0.05f, open, {0, 5}, roll);   // 40 s: past the 25 s respawn
    CHECK(!chicken.dead && chicken.hp == chicken.maxHp && chicken.spawnT < 20.0f);

    // Hunting: a hunting animal comes up to the hero, then stays next to him; and gives up when it cannot reach him.
    Npcs hunt;
    hunt.Init(open);
    hunt.StartHunt(hunt.All()[0].id);
    const TileCoord hero{10, 4};
    for (int i = 0; i < 20 * 8; ++i) hunt.Update(0.05f, open, hero, roll);
    CHECK(std::max(std::abs(hunt.All()[0].Tile().x - hero.x), std::abs(hunt.All()[0].Tile().z - hero.z)) <= 1);
    hunt.StopAllHunts();
    CHECK(!hunt.All()[0].hunting);
    hunt.StartHunt(hunt.All()[1].id);
    for (int i = 0; i < 20 * 15; ++i) hunt.Update(0.05f, open, {0, 0}, roll);    // hero is outside the pen, out of reach
    CHECK(!hunt.All()[1].hunting);                                        // it forgot about the fight

    // Loot tables: bones and meat always; the chicken's egg and the cow's beef only sometimes.
    std::vector<ItemStack> l = RollLoot(Species::Chicken, [] { return 0.0; });
    CHECK(l.size() == 3 && l[0].item == Item::Bones && l[1].item == Item::RawChicken && l[2].item == Item::Egg);
    l = RollLoot(Species::Chicken, [] { return 0.5; });
    CHECK(l.size() == 2 && l[0].item == Item::Bones && l[1].item == Item::RawChicken);          // 0.5 > 0.30: no egg
    l = RollLoot(Species::Cow, [] { return 0.0; });
    CHECK(l.size() == 3 && l[1].item == Item::Cowhide && l[2].item == Item::RawBeef);
    l = RollLoot(Species::Cow, [] { return 0.7; });
    CHECK(l.size() == 2 && l[0].item == Item::Bones && l[1].item == Item::Cowhide);             // hides always, beef only sometimes
    l = RollLoot(Species::Calf, [] { return 0.5; });
    CHECK(l.size() == 1 && l[0].item == Item::Bones);
}

static void TestGroundItems() {
    GroundItems g;
    const int a = g.Drop({3, 4}, {Item::Bones, 1});
    CHECK(a > 0 && g.All().size() == 1 && g.AtTile({3, 4}) && g.AtTile({3, 4})->id == a && g.AtTile({4, 4}) == nullptr);
    const int b = g.Drop({3, 4}, {Item::RawChicken, 1});
    CHECK(b == a && g.All().size() == 1 && g.All()[0].items.size() == 2);   // same tile: same pile
    CHECK(g.All()[0].items[0].item == Item::RawChicken);                      // the newest on top
    CHECK(g.Drop({3, 4}, {Item::None, 1}) == -1 && g.Drop({3, 4}, {Item::Bones, 0}) == -1);
    const int c = g.Drop({9, 9}, {Item::Logs, 1});
    CHECK(c != a && g.All().size() == 2);

    Inventory bag;
    ItemStack got;
    CHECK(g.TakeOne(a, 1, &bag, &got) && got.item == Item::Bones && bag.Has(Item::Bones));   // by index
    CHECK(g.Find(a) && g.Find(a)->items.size() == 1);
    CHECK(!g.TakeOne(a, 5, &bag) && !g.TakeOne(999, 0, &bag));
    const std::vector<ItemStack> all = g.TakeAll(a, &bag);
    CHECK(all.size() == 1 && all[0].item == Item::RawChicken && g.Find(a) == nullptr && g.All().size() == 1);   // empty piles vanish

    // A nearly full bag: what fits is taken, the rest stays.
    for (int i = 0; i < 3; ++i) g.Drop({5, 5}, {Item::Egg, 1});
    Inventory tight;
    while (tight.FreeSlots() > 2) tight.Add(Item::Logs);
    const int pid = g.AtTile({5, 5})->id;
    const std::vector<ItemStack> some = g.TakeAll(pid, &tight);
    CHECK(some.size() == 2 && tight.Full() && g.Find(pid) && g.Find(pid)->items.size() == 1);
    CHECK(!g.TakeOne(pid, 0, &tight));                                       // full: nothing more
    // Piles do not last forever.
    g.Update(GroundItems::kLifeSeconds - 1.0f);
    CHECK(g.All().size() == 2);
    g.Update(2.0f);
    CHECK(g.All().empty());
}

static void TestGameplayCombat() {
    // ---- a fight to the end: roll 0.0 = every strike lands for 1
    MiniWorld w;
    GiveStarterKit(&w.bag);
    const int chickenId = w.npcs.All()[0].id;
    w.game.TapNpc(chickenId);
    float mx = 0, mz = 0;
    bool red = false;
    CHECK(w.game.TakeMarker(&mx, &mz, &red) && red);
    CHECK(w.player.Moving());
    w.Run(14.0f);                                                       // gets there, fights, wins
    const Npc& chicken = *w.npcs.Find(chickenId);
    CHECK(chicken.dead);                                                // three hits of 1 kill a 3 HP chicken
    CHECK(w.ev.hitsOnNpc.size() >= 3 && w.ev.hitsOnNpc[0] == 1);
    CHECK(!w.ev.hitsOnPlayer.empty() && w.ev.hitsOnPlayer[0] == 1);     // it hit back (roll 0.0 lands)
    CHECK(w.skills.Xp(Skill::Attack) == 12 && w.skills.Xp(Skill::Hitpoints) == XpForLevel(10) + 4);
    CHECK(w.skills.Hp() < 10 && w.skills.Hp() >= 7);                     // lost a couple of points, not more
    CHECK(w.ev.Said("You have killed the chicken"));
    CHECK(w.game.Current() == Activity::None);
    // the loot pile appears where it fell, after the fall animation
    w.Run(Npcs::kDeathAnimSeconds + 0.3f);
    CHECK(!chicken.lootPending && w.ground.AtTile(chicken.Tile()) != nullptr);
    const Pile* pile = w.ground.AtTile(chicken.Tile());
    bool bones = false, meat = false;
    for (const ItemStack& st : pile->items) {
        if (st.item == Item::Bones) bones = true;
        if (st.item == Item::RawChicken) meat = true;
    }
    CHECK(bones && meat);

    // picking it up: tap the pile, walk onto it, everything goes in the bag
    w.game.TapPile(pile->id);
    w.Run(4.0f);
    CHECK(w.bag.Has(Item::Bones) && w.bag.Has(Item::RawChicken) && w.ground.All().empty());
    CHECK(w.ev.Said("You take the bones") && w.ev.Said("You take the raw chicken"));

    // burying the bones: takes a moment, then Prayer XP
    const int boneSlot = w.bag.FindSlot(Item::Bones);
    w.game.DoItemAction(boneSlot, ItemAction::Bury);
    CHECK(w.game.Current() == Activity::Burying && w.bag.Has(Item::Bones));
    w.Run(1.0f);
    CHECK(w.bag.Has(Item::Bones));
    w.Run(1.2f);
    CHECK(!w.bag.Has(Item::Bones) && w.skills.Xp(Skill::Prayer) == 5 && w.ev.Said("You bury the bones"));
    // moving away interrupts a burial
    w.bag.Add(Item::Bones);
    w.game.DoItemAction(w.bag.FindSlot(Item::Bones), ItemAction::Bury);
    w.game.Cancel();
    w.Run(3.0f);
    CHECK(w.bag.Has(Item::Bones) && w.skills.Xp(Skill::Prayer) == 5);

    // eating heals (3 from chicken), but not past the maximum
    w.skills.SetHp(4);
    w.bag.Add(Item::Chicken);
    w.game.DoItemAction(w.bag.FindSlot(Item::Chicken), ItemAction::Eat);
    CHECK(w.skills.Hp() == 7 && w.ev.Said("It heals 3 hitpoints"));
    w.skills.SetHp(10);
    w.bag.Add(Item::Chicken);
    w.game.DoItemAction(w.bag.FindSlot(Item::Chicken), ItemAction::Eat);
    CHECK(w.skills.Hp() == 10 && !w.bag.Has(Item::Chicken));

    // dropping puts the item on the ground under you, and you can pick it up again
    w.bag.Add(Item::Logs);
    w.game.DoItemAction(w.bag.FindSlot(Item::Logs), ItemAction::Drop);
    CHECK(!w.bag.Has(Item::Logs) && w.ground.AtTile(w.player.Tile()) != nullptr);
    w.game.TapPile(w.ground.AtTile(w.player.Tile())->id);
    CHECK(w.bag.Has(Item::Logs) && w.ground.All().empty());

    // ---- misses: a high roll never lands; the splats are all 0 (blue) and no XP is earned
    MiniWorld m;
    GiveStarterKit(&m.bag);
    m.rollValue = 0.99;
    m.game.TapNpc(m.npcs.All()[0].id);
    m.Run(20.0f);
    CHECK(!m.npcs.All()[0].dead && m.skills.Xp(Skill::Attack) == 0 && m.skills.Hp() == 10);
    CHECK(!m.ev.hitsOnNpc.empty() && !m.ev.hitsOnPlayer.empty());
    for (int d : m.ev.hitsOnNpc) CHECK(d == 0);
    for (int d : m.ev.hitsOnPlayer) CHECK(d == 0);
    // turns alternate: about the same number of strikes each way
    CHECK(std::abs(static_cast<int>(m.ev.hitsOnNpc.size()) - static_cast<int>(m.ev.hitsOnPlayer.size())) <= 1);
    CHECK(m.ev.hitsOnNpc.size() >= 4 && m.ev.hitsOnNpc.size() <= 9);     // one strike per 2.4 s each over ~17 s of fighting

    // walking away ends MY attacks, but it keeps chasing and hitting while it can
    m.game.Cancel();
    const size_t mine = m.ev.hitsOnNpc.size();
    m.Run(8.0f);
    CHECK(m.ev.hitsOnNpc.size() >= mine);
}

static void TestGameplayDeathAndAutoRetaliate() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    w.skills.SetHp(2);
    w.rollValue = 0.0;
    // the cow is not touched by the hero; he stands next to it and it is "hunting" him: he hits back by himself
    const int cowId = w.npcs.All()[1].id;
    w.player.Teleport({12, 3});
    w.npcs.StartHunt(cowId);
    w.Run(15.0f);
    CHECK(w.ev.deaths == 1 && w.ev.Said("Oh dear, you are dead"));
    CHECK(w.player.Tile() == w.map.Spawn() && w.skills.Hp() == w.skills.MaxHp());   // back at the spawn, healed
    CHECK(w.game.Current() == Activity::None);
    CHECK(!w.npcs.Find(cowId)->hunting);                                              // it forgot him
    CHECK(!w.ev.hitsOnNpc.empty());                                                   // he did fight back before dying

    // hit points creep back by themselves
    MiniWorld r;
    r.skills.SetHp(5);
    r.Run(31.0f);
    CHECK(r.skills.Hp() == 6);
    r.Run(61.0f);
    CHECK(r.skills.Hp() == 8);
}

static void TestMenusAndUse() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    // ground only: Walk here, then Cancel
    std::vector<MenuEntry> m = w.game.BuildMenu({}, true, 6.5f, 1.5f);
    CHECK(m.size() == 2 && m[0].Text() == "Walk here" && m[0].act == MenuAct::WalkHere && m.back().act == MenuAct::Cancel);
    m = w.game.BuildMenu({}, false, 0, 0);
    CHECK(m.size() == 1 && m[0].act == MenuAct::Cancel);

    // an animal: Attack first (with a coloured level), Examine, Walk here, Cancel
    const int chickenId = w.npcs.All()[0].id, cowId = w.npcs.All()[1].id;
    m = w.game.BuildMenu({{TargetKind::Npc, chickenId}}, true, 11.5f, 1.5f);
    CHECK(m.size() == 4 && m[0].act == MenuAct::Attack && m[0].Text() == "Attack Chicken (level-1)");
    CHECK(m[0].label.size() == 3 && m[0].label[1].color.r == 255 && m[0].label[1].color.g == 255 && m[0].label[1].color.b == 0);   // yellow name
    const Rgb want = LevelColor(w.skills.CombatLevel(), 1);
    CHECK(m[0].label[2].color.r == want.r && m[0].label[2].color.g == want.g);       // level coloured against ours (level 3 vs 1)
    CHECK(m[1].act == MenuAct::ExamineTarget && m[2].act == MenuAct::WalkHere);
    m = w.game.BuildMenu({{TargetKind::Npc, cowId}}, true, 12.5f, 2.5f);
    CHECK(m[0].Text() == "Attack Cow (level-3)");
    CHECK(LevelColor(40, 1).g == 255 && LevelColor(40, 1).r == 0);                    // a strong hero: green chicken
    CHECK(LevelColor(1, 60).r == 255 && LevelColor(1, 60).g == 0);                    // a weak one: red monster

    // a dead animal offers nothing
    w.npcs.Hurt(chickenId, 99);
    m = w.game.BuildMenu({{TargetKind::Npc, chickenId}}, true, 0, 0);
    CHECK(m.size() == 2);

    // trees, spots, fires and piles
    m = w.game.BuildMenu({{TargetKind::Tree, 0}}, false, 0, 0);
    CHECK(m[0].Text() == "Chop down Tree" && m[1].Text() == "Examine Tree" && m[0].label[1].color.b == 255 && m[0].label[1].color.r == 0);   // cyan object
    w.forest.Fell(0);
    m = w.game.BuildMenu({{TargetKind::Tree, 0}}, false, 0, 0);
    CHECK(m[0].Text() == "Examine Tree stump");
    m = w.game.BuildMenu({{TargetKind::Spot, 0}}, false, 0, 0);
    CHECK(m[0].Text() == "Net Fishing spot" && m[0].act == MenuAct::Fish);
    w.ground.Drop({6, 1}, {Item::Bones, 1});
    const int pileId = w.ground.AtTile({6, 1})->id;
    m = w.game.BuildMenu({{TargetKind::Pile, pileId}}, true, 6.5f, 1.5f);
    CHECK(m[0].Text() == "Take Bones" && m[0].label[1].color.r == 255 && m[0].label[1].color.g == 152);   // orange item
    w.ground.Drop({6, 1}, {Item::RawBeef, 1});
    m = w.game.BuildMenu({{TargetKind::Pile, pileId}}, false, 0, 0);
    CHECK(m[0].Text() == "Take all" && m[1].Text() == "Take Raw beef" && m[2].Text() == "Take Bones" && m[3].act == MenuAct::ExamineItem);
    // doing a menu entry does what it says
    w.game.Perform(m[2]);                                    // take the bones only
    w.Run(1.0f);
    CHECK(w.bag.Has(Item::Bones) && !w.bag.Has(Item::RawBeef) && w.ground.Find(pileId) != nullptr);
    w.game.Perform(w.game.BuildMenu({{TargetKind::Ground, -1}}, true, 8.5f, 1.5f)[0]);
    CHECK(w.player.Moving());                                // "Walk here"
    w.Run(2.0f);
    CHECK(w.player.Tile() == TileCoord({8, 1}));

    // the menu of a bag item: Eat / Bury / Cook / Light first, Use, Drop, Examine, Cancel
    MiniWorld b;
    GiveStarterKit(&b.bag);
    b.bag.Add(Item::Bones);
    const int bs = b.bag.FindSlot(Item::Bones);
    m = b.game.BuildItemMenu(bs);
    CHECK(m.size() == 5 && m[0].Text() == "Bury Bones" && m[1].Text() == "Use Bones" && m[2].Text() == "Drop Bones" &&
          m[3].Text() == "Examine Bones" && m[4].act == MenuAct::Cancel);
    b.game.Perform(m[3]);
    CHECK(b.ev.Said("Ew, a bone."));
    m = b.game.BuildItemMenu(b.bag.FindSlot(Item::BronzeAxe));
    CHECK(m.size() == 3 && m[0].Text() == "Use Bronze axe" && m[1].Text() == "Examine Bronze axe");   // tools cannot be dropped
    CHECK(b.game.BuildItemMenu(27).empty() && b.game.BuildItemMenu(-1).empty());

    // "Use": tinderbox on logs lights a fire; nothing happens with other combinations
    b.bag.Add(Item::Logs);
    b.game.DoItemAction(b.bag.FindSlot(Item::Tinderbox), ItemAction::Use);
    CHECK(b.game.UseSlot() == b.bag.FindSlot(Item::Tinderbox));
    m = b.game.BuildItemMenu(b.bag.FindSlot(Item::Logs));
    CHECK(m[0].Text() == "Use Tinderbox -> Logs" && m[0].act == MenuAct::UseOnItem);
    CHECK(b.game.UseItemOnSlot(b.bag.FindSlot(Item::Logs)));
    CHECK(b.game.UseSlot() == -1 && b.game.Current() == Activity::Lighting);
    b.game.Cancel();
    b.game.DoItemAction(b.bag.FindSlot(Item::Bones), ItemAction::Use);
    b.game.UseItemOnSlot(b.bag.FindSlot(Item::BronzeAxe));
    CHECK(b.ev.Said("Nothing interesting happens") && b.game.Current() == Activity::None);
    CHECK(!b.game.UseItemOnSlot(3));                         // nothing was being used
    b.game.DoItemAction(b.bag.FindSlot(Item::Bones), ItemAction::Use);
    b.game.Cancel();                                         // tapping away cancels it
    CHECK(b.game.UseSlot() == -1);

    // raw food used on a fire cooks it; the fire's menu offers that first
    MiniWorld f;
    GiveStarterKit(&f.bag);
    f.fires.Light(&f.map, {7, 1});
    f.bag.Add(Item::RawBeef);
    const int fid = f.fires.All()[0].id;
    f.game.DoItemAction(f.bag.FindSlot(Item::RawBeef), ItemAction::Use);
    m = f.game.BuildMenu({{TargetKind::Fire, fid}}, true, 7.5f, 1.5f);
    CHECK(m[0].Text() == "Use Raw beef -> Fire" && m[1].Text() == "Cook Fire");
    f.game.Perform(m[0]);
    f.rollValue = 0.99;
    f.Run(4.0f);
    CHECK(f.bag.Has(Item::Beef) && f.skills.Xp(Skill::Cooking) == 30);
    // and the other meats cook too
    f.bag.Add(Item::RawChicken);
    f.game.TapFire(fid);
    f.Run(4.0f);
    CHECK(f.bag.Has(Item::Chicken));
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
    CHECK(m.Width() == 64 && m.Height() == 176);
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
    // Fishing spots: on water, each reachable from the bank (a walkable neighbour the hero can walk to from spawn).
    CHECK(m.Spots().size() >= 5);
    for (size_t i = 0; i < m.Spots().size(); ++i) {
        const TileCoord t = m.Spots()[i].Tile();
        if (m.At(t.x, t.z) != '~') {
            std::printf("  FAIL spot %zu at %d,%d is not on water ('%c')\n", i, t.x, t.z, m.At(t.x, t.z));
            ++g_failed;
            continue;
        }
        bool reachable = false;
        for (int dz = -1; dz <= 1 && !reachable; ++dz)
            for (int dx = -1; dx <= 1 && !reachable; ++dx)
                if (m.Walkable(t.x + dx, t.z + dz) && FindPath(m, m.Spawn(), {t.x + dx, t.z + dz}).found) reachable = true;
        if (!reachable) {
            std::printf("  FAIL spot %zu at %d,%d has no reachable bank tile next to it\n", i, t.x, t.z);
            ++g_failed;
        }
    }
    // Animal pens: every animal starts on an open tile inside its pen, and the hero can walk in to it from the spawn.
    CHECK(m.Pens().size() == 3 && m.FindPen("coop") && m.FindPen("cowpen") && m.FindPen("goblincamp"));
    int chickens = 0, cows = 0, calves = 0, goblins = 0, warriors = 0;
    for (const NpcDef& d : m.Npcs()) {
        const TileCoord t{static_cast<int>(d.x), static_cast<int>(d.z)};
        const PenDef* pen = m.FindPen(d.pen);
        if (!pen || !pen->rect.Contains(t.x, t.z) || !m.Walkable(t) || !FindPath(m, m.Spawn(), t).found) {
            std::printf("  FAIL %s at %d,%d is not on open ground inside its pen (or cannot be reached)\n", d.species.c_str(), t.x, t.z);
            ++g_failed;
        }
        chickens += d.species == "chicken";
        cows += d.species == "cow";
        calves += d.species == "calf";
        goblins += d.species == "goblin";
        warriors += d.species == "goblin_warrior";
    }
    CHECK(chickens >= 5 && cows >= 2 && calves >= 2 && goblins >= 5 && warriors >= 2);
    for (const PenDef& pen : m.Pens()) {   // the pen's whole interior is reachable (nobody is walled in)
        int open = 0, reachable = 0;
        for (int z = pen.rect.z0; z < pen.rect.z1; ++z)
            for (int x = pen.rect.x0; x < pen.rect.x1; ++x)
                if (m.Walkable(x, z)) {
                    ++open;
                    if (FindPath(m, m.Spawn(), {x, z}).found) ++reachable;
                }
        CHECK(open > 30 && open == reachable);
    }
    Npcs herd;
    herd.Init(m);
    CHECK(herd.All().size() == m.Npcs().size());                          // every line of the map became an animal

    // The quarry and the smithy: every rock and workstation is on a blocked tile with an open, reachable neighbour,
    // all five metals can be mined, and there is a furnace and an anvil.
    CHECK(m.Rocks().size() >= 8 && m.Stations().size() >= 3);
    bool kinds[kRockKindCount] = {};
    for (size_t i = 0; i < m.Rocks().size(); ++i) {
        const TileCoord t = m.Rocks()[i].Tile();
        RockKind k;
        CHECK(RockKindFromKey(m.Rocks()[i].kind, &k));
        kinds[static_cast<int>(k)] = true;
        bool reachable = false;
        for (int dz = -1; dz <= 1 && !reachable; ++dz)
            for (int dx = -1; dx <= 1 && !reachable; ++dx)
                if ((dx || dz) && m.Walkable(t.x + dx, t.z + dz) && FindPath(m, m.Spawn(), {t.x + dx, t.z + dz}).found) reachable = true;
        if (m.Walkable(t) || !reachable) {
            std::printf("  FAIL rock %zu at %d,%d is walkable or has no reachable neighbour\n", i, t.x, t.z);
            ++g_failed;
        }
    }
    for (bool seen : kinds) CHECK(seen);
    int furnaces = 0, anvils = 0;
    for (size_t i = 0; i < m.Stations().size(); ++i) {
        const StationDef& st = m.Stations()[i];
        const TileCoord t = st.Tile();
        furnaces += st.kind == "furnace";
        anvils += st.kind == "anvil";
        bool reachable = false;
        for (int dz = -1; dz <= 1 && !reachable; ++dz)
            for (int dx = -1; dx <= 1 && !reachable; ++dx)
                if ((dx || dz) && m.Walkable(t.x + dx, t.z + dz) && FindPath(m, m.Spawn(), {t.x + dx, t.z + dz}).found) reachable = true;
        if (m.Walkable(t) || !reachable) {
            std::printf("  FAIL station %zu (%s) at %d,%d is walkable or has no reachable neighbour\n", i, st.kind.c_str(), t.x, t.z);
            ++g_failed;
        }
    }
    CHECK(furnaces >= 1 && anvils >= 2);
    CHECK(m.AreaAt(58, 53) == "Smithy" && m.AreaAt(58, 60) == "Quarry");
    // and the whole chain works on the real map: walk from the spawn to a copper rock and mine it
    {
        Player player;
        Skills skills;
        Inventory bag;
        Equipment equip;
        Forest forest;
        Rocks rocks;
        Fires fires;
        Npcs npcs;
        GroundItems ground;
        RecordingEvents ev;
        TileMap real = m;
        Shops shops;
        People people;
        shops.Init();
        people.Init(real);
        Gameplay game(real, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, ev, [] { return 0.0; });
        forest.Init(real.Trees());
        rocks.Init(real.Rocks());
        npcs.Init(real);
        GiveStarterKit(&bag);
        player.Teleport(real.Spawn());
        int copper = -1;
        for (size_t i = 0; i < rocks.All().size(); ++i)
            if (rocks.All()[i].kind == RockKind::Copper) {
                copper = static_cast<int>(i);
                break;
            }
        CHECK(copper >= 0);
        game.TapRock(copper);
        for (int i = 0; i < 20 * 120 && !bag.Has(Item::CopperOre); ++i) {   // up to two minutes of game time
            player.Update(0.05f);
            rocks.Update(0.05f);
            game.Update(0.05f);
        }
        CHECK(bag.Has(Item::CopperOre) && rocks.All()[copper].depleted);
    }

    // The road south and Millbrook: the map is long, and the village is really connected to the town by walking.
    CHECK(m.Height() >= 170 && m.Width() == 64);
    for (TileCoord goal : {TileCoord{32, 80}, TileCoord{32, 97}, TileCoord{32, 100}, TileCoord{32, 140}, TileCoord{32, 172}, TileCoord{50, 158}, TileCoord{10, 120}}) {
        const PathResult pr = FindPath(m, m.Spawn(), goal);
        if (!m.Walkable(goal) || !pr.found) {
            std::printf("  FAIL cannot walk from the spawn to %d,%d (walkable=%d)\n", goal.x, goal.z, (int)m.Walkable(goal));
            ++g_failed;
        }
    }
    CHECK(m.AreaAt(32, 80) == "Millbrook Road" && m.AreaAt(48, 80) == "Goblin Camp" && m.AreaAt(32, 100) == "Millbrook Village" && m.AreaAt(32, 136) == "Millbrook Square");
    // People: every one is a known character standing on a blocked tile, and the hero can get within talking distance of them.
    CHECK(m.Persons().size() >= 12);
    std::vector<std::string> shopsMet;
    for (const PersonDef& d : m.Persons()) {
        const PersonInfo* info = FindPersonInfo(d.key);
        if (!info) {
            std::printf("  FAIL unknown person '%s'\n", d.key.c_str());
            ++g_failed;
            continue;
        }
        const TileCoord t = d.Tile();
        bool reach = false;
        for (int dz = -info->reach; dz <= info->reach && !reach; ++dz)
            for (int dx = -info->reach; dx <= info->reach && !reach; ++dx)
                if ((dx || dz) && m.Walkable(t.x + dx, t.z + dz) && FindPath(m, m.Spawn(), {t.x + dx, t.z + dz}).found) reach = true;
        if (m.Walkable(t) || !reach) {
            std::printf("  FAIL person %s at %d,%d is on a walkable tile or cannot be reached\n", d.key.c_str(), t.x, t.z);
            ++g_failed;
        }
        if (info->shop[0]) shopsMet.push_back(info->shop);
    }
    for (const ShopDef& sd : AllShopDefs()) {   // every shop has someone to run it
        if (std::find(shopsMet.begin(), shopsMet.end(), sd.key) == shopsMet.end()) {
            std::printf("  FAIL shop %s has no shopkeeper on the map\n", sd.key.c_str());
            ++g_failed;
        }
    }
    {   // a whole shopping trip on the real map: walk from the spawn to the general store, buy, and the coins are spent
        Player player;
        Skills skills;
        Inventory bag;
        Equipment equip;
        Forest forest;
        Rocks rocks;
        Fires fires;
        Npcs npcs;
        GroundItems ground;
        Shops shops;
        People people;
        RecordingEvents ev;
        TileMap real = m;
        shops.Init();
        people.Init(real);
        forest.Init(real.Trees());
        rocks.Init(real.Rocks());
        Gameplay game(real, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, ev, [] { return 0.0; });
        player.Teleport(real.Spawn());
        bag.Add(Item::Coins, 100);
        int aubrey = -1;
        for (const Person& pr : people.All())
            if (std::string(pr.info->key) == "aubrey") aubrey = pr.id;
        CHECK(aubrey > 0);
        game.TapPerson(aubrey);
        for (int i = 0; i < 20 * 60 && ev.shopMenus == 0; ++i) {
            player.Update(0.05f);
            game.Update(0.05f);
        }
        CHECK(ev.shopMenus == 1 && !ev.lastShop.buy.empty());
        game.Buy(ev.lastShop.shop, ev.lastShop.buy[0].index, 2);
        CHECK(bag.Count(Item::Coins) < 100 && bag.Has(ev.lastShop.buy[0].item));
    }
    // Walls must actually block: the keep's side wall is not walkable.
    CHECK(!m.Walkable(24, 10));
    // But its door is.
    CHECK(m.Walkable(32, 15));
}


static bool BuildEquipMenuOk(MiniWorld& w) {
    w.bag.Add(GearItem(Tier::Bronze, GearType::Shield));
    w.game.DoItemAction(w.bag.FindSlot(GearItem(Tier::Bronze, GearType::Shield)), ItemAction::Wield);
    const std::vector<MenuEntry> e = w.game.BuildEquipMenu(EquipSlot::Shield);
    if (e.size() != 3 || e[0].act != MenuAct::RemoveWorn || e[1].act != MenuAct::ExamineWorn || e.back().act != MenuAct::Cancel) return false;
    if (!w.game.BuildEquipMenu(EquipSlot::Head).empty()) return false;
    w.game.Perform(e[1]);
    if (!w.ev.Said("kiteshield")) return false;
    w.game.Perform(e[0]);
    return !w.equip.HasShield() && w.bag.Has(GearItem(Tier::Bronze, GearType::Shield));
}

// ---------------------------------------------------------------------------------------------- gear, mining, smithing
static void TestGearTable() {
    // every (tier, type) is its own item, and GearOf turns it back
    std::vector<Item> seen;
    for (int t = 0; t < kTierCount; ++t)
        for (int g = 0; g < kGearTypeCount; ++g) {
            const Item it = GearItem(static_cast<Tier>(t), static_cast<GearType>(g));
            CHECK(it != Item::None && std::find(seen.begin(), seen.end(), it) == seen.end());
            seen.push_back(it);
            Tier t2;
            GearType g2;
            CHECK(GearOf(it, &t2, &g2) && t2 == static_cast<Tier>(t) && g2 == static_cast<GearType>(g));
            const std::string name = GetItemInfo(it).name;
            CHECK(name.find(TierName(static_cast<Tier>(t))) == 0);   // "Iron mace" starts with the metal
            CHECK(std::string(GetItemInfo(it).model).find(TierKey(static_cast<Tier>(t))) == 0);
        }
    CHECK(!GearOf(Item::Logs, nullptr, nullptr) && !GearOf(Item::Hammer, nullptr, nullptr) && !GearOf(Item::MithrilBar, nullptr, nullptr));
    CHECK(static_cast<int>(GearItem(Tier::Mithril, GearType::Body)) == static_cast<int>(Item::GearLast));
    CHECK(GetItemInfo(GearItem(Tier::Iron, GearType::Mace)).name == std::string("Iron mace"));
    CHECK(GetItemInfo(GearItem(Tier::Steel, GearType::Helm)).name == std::string("Steel medium helm"));

    // where things go
    EquipSlot slot;
    CHECK(Equippable(GearItem(Tier::Bronze, GearType::Dagger), &slot) && slot == EquipSlot::Weapon);
    CHECK(Equippable(GearItem(Tier::Bronze, GearType::Shield), &slot) && slot == EquipSlot::Shield);
    CHECK(Equippable(GearItem(Tier::Bronze, GearType::Helm), &slot) && slot == EquipSlot::Head);
    CHECK(Equippable(GearItem(Tier::Bronze, GearType::Body), &slot) && slot == EquipSlot::Body);
    CHECK(Equippable(GearItem(Tier::Bronze, GearType::Legs), &slot) && slot == EquipSlot::Legs);
    CHECK(!Equippable(GearItem(Tier::Bronze, GearType::Pickaxe), &slot) && !Equippable(Item::Logs, &slot));

    // better metal = better numbers; each weapon has its own character
    for (int g = 0; g < kWornTypeCount; ++g)
        for (int t = 1; t < kTierCount; ++t) {
            const GearStats a = GetGearStats(static_cast<Tier>(t - 1), static_cast<GearType>(g));
            const GearStats b = GetGearStats(static_cast<Tier>(t), static_cast<GearType>(g));
            CHECK(b.attack >= a.attack && b.strength >= a.strength && b.defence >= a.defence);
            CHECK(b.attack + b.strength + b.defence > a.attack + a.strength + a.defence);
        }
    const GearStats dag = GetGearStats(Tier::Iron, GearType::Dagger), axe = GetGearStats(Tier::Iron, GearType::Axe);
    CHECK(dag.attack > axe.attack && axe.strength > dag.strength);                 // dagger: accurate, battleaxe: heavy
    CHECK(GetGearStats(Tier::Iron, GearType::Body).defence > GetGearStats(Tier::Iron, GearType::Helm).defence);
    CHECK(GetGearStats(Tier::Iron, GearType::Dagger).defence == 0 && GetGearStats(Tier::Iron, GearType::Helm).attack == 0);
    CHECK(ItemStats(Item::Logs).attack == 0 && ItemStats(Item::None).defence == 0);

    CHECK(RequirementSkill(GearType::Mace) == Skill::Attack && RequirementSkill(GearType::Body) == Skill::Defence &&
          RequirementSkill(GearType::Pickaxe) == Skill::Mining);
    CHECK(TierRequirement(Tier::Bronze) == 1 && TierRequirement(Tier::Mithril) == 30);
    CHECK(PickaxeSpeed(Tier::Mithril) > PickaxeSpeed(Tier::Iron) && PickaxeSpeed(Tier::Iron) > PickaxeSpeed(Tier::Bronze));
    CHECK(AnimOf(GearType::Dagger) == WeaponAnim::Stab && AnimOf(GearType::Sword) == WeaponAnim::Slash &&
          AnimOf(GearType::Axe) == WeaponAnim::Chop && AnimOf(GearType::Mace) == WeaponAnim::Crush && AnimOf(GearType::Helm) == WeaponAnim::Unarmed);

    // starter tools can't be dropped, smithed pickaxes can (you still own the bronze one)
    CHECK(GetItemInfo(Item::Hammer).tool && GetItemInfo(Item::BronzePickaxe).tool && !GetItemInfo(Item::IronPickaxe).tool);
    Inventory bag;
    auto actions = ActionsFor(GearItem(Tier::Bronze, GearType::Sword), bag);
    CHECK(actions[0] == ItemAction::Wield);
    actions = ActionsFor(GearItem(Tier::Bronze, GearType::Helm), bag);
    CHECK(actions[0] == ItemAction::Wear);
    actions = ActionsFor(GearItem(Tier::Bronze, GearType::Shield), bag);
    CHECK(actions[0] == ItemAction::Wield);
    actions = ActionsFor(Item::IronPickaxe, bag);
    CHECK(std::find(actions.begin(), actions.end(), ItemAction::Wield) == actions.end());
}

static void TestEquipment() {
    Skills skills;
    Inventory bag;
    Equipment eq;
    CHECK(eq.Empty() && eq.Anim() == WeaponAnim::Unarmed && eq.Total().attack == 0 && !eq.HasShield());
    const Item bsword = GearItem(Tier::Bronze, GearType::Sword), isword = GearItem(Tier::Iron, GearType::Sword);
    bag.Add(Item::Logs);
    bag.Add(bsword);
    const int slot = bag.FindSlot(bsword);
    CHECK(EquipFromBag(&eq, &bag, slot, skills) == EquipResult::Done);
    CHECK(eq.Get(EquipSlot::Weapon) == bsword && !bag.Has(bsword) && eq.Anim() == WeaponAnim::Slash && eq.IsWorn(bsword));
    CHECK(eq.Total().attack == GetGearStats(Tier::Bronze, GearType::Sword).attack);

    // iron needs Attack 5: refused, and nothing moves
    bag.Add(isword);
    std::string need;
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(isword), skills, &need) == EquipResult::LevelTooLow);
    CHECK(need == "Attack level 5" && bag.Has(isword) && eq.Get(EquipSlot::Weapon) == bsword);
    skills.SetXp(Skill::Attack, XpForLevel(5));
    const int islot = bag.FindSlot(isword);
    CHECK(EquipFromBag(&eq, &bag, islot, skills) == EquipResult::Done);
    CHECK(eq.Get(EquipSlot::Weapon) == isword && bag.Has(bsword) && bag.FindSlot(bsword) == islot);   // swapped into the same slot

    // armour: its own slot, Defence level; two pieces add up
    const Item helm = GearItem(Tier::Steel, GearType::Helm), body = GearItem(Tier::Bronze, GearType::Body);
    bag.Add(helm);
    bag.Add(body);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(helm), skills, &need) == EquipResult::LevelTooLow && need == "Defence level 20");
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(body), skills) == EquipResult::Done);
    CHECK(eq.Total().defence == GetGearStats(Tier::Bronze, GearType::Body).defence);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::Logs), skills) == EquipResult::NotWearable);
    CHECK(EquipFromBag(&eq, &bag, 99, skills) == EquipResult::NotWearable);

    const Item shield = GearItem(Tier::Bronze, GearType::Shield);
    bag.Add(shield);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(shield), skills) == EquipResult::Done && eq.HasShield());

    // taking things off
    CHECK(Unequip(&eq, &bag, EquipSlot::Shield) == EquipResult::Done && !eq.HasShield() && bag.Has(shield));
    CHECK(Unequip(&eq, &bag, EquipSlot::Head) == EquipResult::NotWearable);
    while (!bag.Full()) bag.Add(Item::BurntFish);
    CHECK(Unequip(&eq, &bag, EquipSlot::Body) == EquipResult::BagFull && eq.Get(EquipSlot::Body) == body);   // no room: it stays on

    // worn gear is saved and loaded; a wrong item in a slot is ignored
    Equipment loaded;
    Skills s2;
    Inventory b2;
    ParseProgress(SerializeProgress(skills, bag, CombatStyle::Accurate, &eq), &s2, &b2, nullptr, &loaded);
    CHECK(loaded.Get(EquipSlot::Weapon) == isword && loaded.Get(EquipSlot::Body) == body && loaded.Get(EquipSlot::Shield) == Item::None);
    Equipment bad;
    ParseProgress("worn.0=" + std::to_string(static_cast<int>(isword)) + "\nworn.9=5\nworn.3=zzz\nworn.1=" + std::to_string(static_cast<int>(Item::IronPickaxe)) + "\n",
                  &s2, &b2, nullptr, &bad);
    CHECK(bad.Empty());
}

static void TestMiningRules() {
    for (int k = 0; k < kRockKindCount; ++k) {
        const RockInfo& r = GetRockInfo(static_cast<RockKind>(k));
        RockKind back;
        CHECK(RockKindFromKey(r.key, &back) && back == static_cast<RockKind>(k));
        CHECK(MineChance(static_cast<RockKind>(k), r.level) >= 0.2f && MineChance(static_cast<RockKind>(k), 99) <= 0.92f + 1e-5f);
        CHECK(MineChance(static_cast<RockKind>(k), r.level + 10) > MineChance(static_cast<RockKind>(k), r.level));
        CHECK(GetItemInfo(r.ore).name != std::string("nothing"));
    }
    RockKind dummy;
    CHECK(!RockKindFromKey("gold", &dummy));
    CHECK(GetRockInfo(RockKind::Mithril).level > GetRockInfo(RockKind::Coal).level && GetRockInfo(RockKind::Coal).level > GetRockInfo(RockKind::Iron).level);
    CHECK(GetRockInfo(RockKind::Mithril).xp > GetRockInfo(RockKind::Copper).xp && GetRockInfo(RockKind::Mithril).respawn > GetRockInfo(RockKind::Copper).respawn);

    // the best pickaxe the level allows
    Inventory bag;
    CHECK(BestPickaxe(bag, 99) == Item::None);
    bag.Add(Item::BronzePickaxe);
    CHECK(BestPickaxe(bag, 1) == Item::BronzePickaxe);
    bag.Add(Item::IronPickaxe);
    bag.Add(Item::MithrilPickaxe);
    CHECK(BestPickaxe(bag, 1) == Item::BronzePickaxe && BestPickaxe(bag, 5) == Item::IronPickaxe && BestPickaxe(bag, 30) == Item::MithrilPickaxe);

    // a swing takes 2.4 s; a faster pickaxe swings sooner
    auto swingsIn = [](float seconds, float speed) {
        MineSession m;
        m.Start(0);
        int n = 0;
        for (float t = 0; t < seconds; t += 0.05f)
            if (m.Update(0.05f, RockKind::Copper, 1, speed, [] { return 0.99; }) != MineEvent::None) ++n;
        return n;
    };
    CHECK(swingsIn(2.3f, 1.0f) == 0 && swingsIn(2.6f, 1.0f) == 1 && swingsIn(10.0f, 1.0f) == 4);
    CHECK(swingsIn(10.0f, 2.0f) >= 8);
    MineSession lucky;
    lucky.Start(3);
    MineEvent last = MineEvent::None;
    for (float t = 0; t < 3.0f; t += 0.05f) {
        const MineEvent e = lucky.Update(0.05f, RockKind::Copper, 1, 1.0f, [] { return 0.0; });
        if (e != MineEvent::None) last = e;
    }
    CHECK(last == MineEvent::GotOre && lucky.Rock() == 3);

    // rocks: one ore, then bare stone until they grow back
    TileMap m;
    CHECK(m.ParseText("@size 4 1\n@rock 1.5 0.5 copper 90\n@rock 2.5 0.5 gold 0\n@rock nonsense\n@station anvil 3.5 0.5 45\n@station bad\n@rows\n.oo.\n"));
    CHECK(m.Rocks().size() == 2 && m.Rocks()[0].kind == "copper" && m.Stations().size() == 1 && m.Stations()[0].kind == "anvil");
    Rocks rocks;
    rocks.Init(m.Rocks());
    CHECK(rocks.All().size() == 1 && rocks.IndexAtTile({1, 0}) == 0 && rocks.IndexAtTile({3, 0}) == -1);   // the unknown kind is skipped
    CHECK(rocks.Deplete(0) && !rocks.Deplete(0) && rocks.All()[0].depleted);
    CHECK(rocks.SecondsUntilRespawn(0) > 5.0f);
    rocks.Update(GetRockInfo(RockKind::Copper).respawn + 0.5f);
    CHECK(!rocks.All()[0].depleted && rocks.All()[0].growAnim < 1.0f);
    rocks.Update(kRockGrowSeconds + 0.1f);
    CHECK(rocks.All()[0].growAnim >= 1.0f);
}

static void TestSmithingRules() {
    // smelting
    const SmeltRecipe& bronze = GetSmeltRecipe(Tier::Bronze);
    CHECK(bronze.bar == Item::BronzeBar && bronze.ore[0] == Item::CopperOre && bronze.ore[1] == Item::TinOre && bronze.success == 1.0f);
    CHECK(GetSmeltRecipe(Tier::Iron).success < 1.0f && GetSmeltRecipe(Tier::Steel).count[1] == 2 && GetSmeltRecipe(Tier::Mithril).count[1] == 4);
    for (int t = 1; t < kTierCount; ++t) {   // each metal is harder to smelt and worth more
        CHECK(GetSmeltRecipe(static_cast<Tier>(t)).level > GetSmeltRecipe(static_cast<Tier>(t - 1)).level);
        CHECK(GetSmeltRecipe(static_cast<Tier>(t)).xp > GetSmeltRecipe(static_cast<Tier>(t - 1)).xp);
    }
    Tier t;
    CHECK(TierOfBar(Item::SteelBar, &t) && t == Tier::Steel && !TierOfBar(Item::Coal, &t) && BarFor(Tier::Iron) == Item::IronBar);
    Inventory bag;
    bag.Add(Item::CopperOre, 3);
    bag.Add(Item::TinOre, 2);
    CHECK(CanSmelt(bronze, bag) == 2);                              // limited by the tin
    bag.Add(Item::IronOre, 2);
    bag.Add(Item::Coal, 5);
    CHECK(CanSmelt(GetSmeltRecipe(Tier::Steel), bag) == 2);         // two iron, but only 5 coal = 2 lots of 2
    CHECK(CanSmelt(GetSmeltRecipe(Tier::Mithril), bag) == 0);

    // smithing: more bars and a higher level for the bigger pieces
    for (int ti = 0; ti < kTierCount; ++ti) {
        const SmithRecipe dag = GetSmithRecipe(static_cast<Tier>(ti), GearType::Dagger);
        const SmithRecipe body = GetSmithRecipe(static_cast<Tier>(ti), GearType::Body);
        CHECK(dag.bars == 1 && body.bars == 5 && body.level > dag.level && body.xp > dag.xp);
        CHECK(dag.product == GearItem(static_cast<Tier>(ti), GearType::Dagger));
        for (int g = 0; g < kGearTypeCount; ++g) {
            const SmithRecipe r = GetSmithRecipe(static_cast<Tier>(ti), static_cast<GearType>(g));
            CHECK(r.bars >= 1 && r.level >= 1 && r.xp == r.bars * (r.xp / r.bars) && r.product != Item::None);
        }
    }
    CHECK(GetSmithRecipe(Tier::Bronze, GearType::Dagger).level == 1 && GetSmithRecipe(Tier::Mithril, GearType::Body).level == 68);
    Inventory bars;
    bars.Add(Item::BronzeBar, 7);
    CHECK(CanSmith(GetSmithRecipe(Tier::Bronze, GearType::Body), bars) == 1 && CanSmith(GetSmithRecipe(Tier::Bronze, GearType::Dagger), bars) == 7);
    CHECK(CanSmith(GetSmithRecipe(Tier::Iron, GearType::Dagger), bars) == 0);
}

static void TestGameplayMining() {
    MiniWorld w;
    w.game.TapRock(0);                                          // pickaxe not in the bag yet
    CHECK(w.ev.Said("need a pickaxe") && !w.player.Moving());
    GiveStarterKit(&w.bag);
    w.player.Teleport({9, 2});                                  // a few steps from the rocks
    w.game.TapRock(1);                                          // iron needs level 15
    CHECK(w.ev.Said("Mining level of 15") && w.game.Current() == Activity::None);

    w.game.TapRock(0);
    float mx = 0, mz = 0;
    CHECK(w.game.TakeMarker(&mx, &mz) && mx == 5.5f && mz == 0.5f && w.player.Moving());
    w.Run(6.0f);                                                // walk over, swing once (2.4 s), copper at rollValue 0
    CHECK(w.bag.Count(Item::CopperOre) == 1 && w.rocks.All()[0].depleted);
    CHECK(w.skills.Xp(Skill::Mining) == GetRockInfo(RockKind::Copper).xp && w.game.Current() == Activity::None);
    CHECK(w.ev.Said("You manage to mine some copper"));
    w.game.TapRock(0);                                          // the bare rock: examine only
    CHECK(w.ev.Said("no ore left") && w.game.Current() == Activity::None);
    w.Run(GetRockInfo(RockKind::Copper).respawn + 2.0f);
    CHECK(!w.rocks.All()[0].depleted);                          // grown back

    // with bad luck the swings go on, the animation runs, and walking away stops it
    MiniWorld u;
    GiveStarterKit(&u.bag);
    u.rollValue = 0.99;
    u.game.TapRock(0);
    u.Run(8.0f);
    CHECK(u.bag.Count(Item::CopperOre) == 0 && u.game.Current() == Activity::Mining && !u.rocks.All()[0].depleted);
    float wx = 0, wz = 0;
    CHECK(u.game.WorkSpot(&wx, &wz) && wx == 5.5f && wz == 0.5f);
    const float ph = u.game.WorkPhase();
    CHECK(ph >= 0.0f && ph <= 1.0f);
    CHECK(u.rocks.All()[0].hitAge < 3.0f);                      // the rock shook at the last swing
    u.game.Cancel();
    CHECK(u.game.Current() == Activity::None && !u.game.WorkSpot(&wx, &wz));

    // a better pickaxe swings sooner: with every roll lucky, the mithril pickaxe has its ore before the bronze one does
    MiniWorld fast, slow;
    for (MiniWorld* m : {&fast, &slow}) {
        GiveStarterKit(&m->bag);
        m->skills.SetXp(Skill::Mining, XpForLevel(30));
    }
    fast.bag.Add(Item::MithrilPickaxe);
    fast.game.TapRock(0);                                       // (the hero stands next to the rock, so the swinging starts at once)
    slow.game.TapRock(0);
    fast.Run(1.6f);
    slow.Run(1.6f);
    CHECK(fast.bag.Has(Item::CopperOre) && !slow.bag.Has(Item::CopperOre));
    slow.Run(1.5f);
    CHECK(slow.bag.Has(Item::CopperOre));

    // full bag: refuses politely
    MiniWorld f;
    GiveStarterKit(&f.bag);
    while (!f.bag.Full()) f.bag.Add(Item::BurntFish);
    f.game.TapRock(0);
    CHECK(f.ev.Said("too full") && f.game.Current() == Activity::None);

    // iron at level 15
    MiniWorld ir;
    GiveStarterKit(&ir.bag);
    ir.skills.SetXp(Skill::Mining, XpForLevel(15));
    ir.game.TapRock(1);
    ir.Run(7.0f);
    CHECK(ir.bag.Count(Item::IronOre) == 1 && ir.skills.Xp(Skill::Mining) == XpForLevel(15) + GetRockInfo(RockKind::Iron).xp);
}

static void TestGameplaySmeltingAndSmithing() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    w.game.TapStation(0);                                       // the furnace, but no ore
    w.Run(3.0f);
    CHECK(w.ev.Said("no ore to smelt") && w.ev.makeMenus == 0);
    w.bag.Add(Item::CopperOre, 3);
    w.bag.Add(Item::TinOre, 2);
    w.game.TapStation(0);
    float mx = 0, mz = 0;
    CHECK(w.game.TakeMarker(&mx, &mz) && mx == 4.5f && mz == 3.5f);
    w.Run(4.0f);
    CHECK(w.ev.makeMenus == 1 && w.ev.lastMake.kind == MakeMenu::Kind::Smelt && w.ev.lastMake.options.size() == 4);
    CHECK(w.ev.lastMake.options[0].haveAll && w.ev.lastMake.options[0].canMake == 2 && w.ev.lastMake.options[0].levelOk);
    CHECK(!w.ev.lastMake.options[1].haveAll && !w.ev.lastMake.options[1].levelOk && w.ev.lastMake.options[2].level == 30);
    CHECK(w.ev.lastMake.options[0].detail == "1 copper ore + 1 tin ore");
    // asking for a bar you cannot make
    w.game.StartSmelt(Tier::Iron, 1);
    CHECK(w.ev.Said("Smithing level of 15") && w.game.Current() == Activity::None);
    w.game.StartSmelt(Tier::Bronze, 5);                         // wants 5, can make 2
    CHECK(w.game.Current() == Activity::Smelting);
    float fx = 0, fz = 0;
    CHECK(w.game.WorkSpot(&fx, &fz) && fx == 4.5f && fz == 3.5f);
    w.Run(kSmeltSeconds * 2 + 0.5f);
    CHECK(w.bag.Count(Item::BronzeBar) == 2 && w.bag.Count(Item::CopperOre) == 1 && !w.bag.Has(Item::TinOre));
    CHECK(w.skills.Xp(Skill::Smithing) == 2 * GetSmeltRecipe(Tier::Bronze).xp && w.game.Current() == Activity::None);
    CHECK(w.ev.Said("retrieve a bar of bronze"));

    // walking away from the furnace is not allowed to smelt remotely
    MiniWorld far;
    far.bag.Add(Item::CopperOre);
    far.bag.Add(Item::TinOre);
    far.game.StartSmelt(Tier::Bronze, 1);
    CHECK(far.ev.Said("next to the furnace") && far.game.Current() == Activity::None);

    // iron ore is unreliable: a failed roll loses the ore
    MiniWorld ir;
    GiveStarterKit(&ir.bag);
    ir.skills.SetXp(Skill::Smithing, XpForLevel(15));
    ir.bag.Add(Item::IronOre, 2);
    ir.game.TapStation(0);
    ir.Run(4.0f);
    ir.rollValue = 0.99;
    ir.game.StartSmelt(Tier::Iron, 2);
    ir.Run(kSmeltSeconds * 2 + 0.5f);
    CHECK(!ir.bag.Has(Item::IronOre) && !ir.bag.Has(Item::IronBar) && ir.ev.Said("too impure"));
    ir.bag.Add(Item::IronOre, 1);
    ir.rollValue = 0.0;
    ir.game.StartSmelt(Tier::Iron, 1);
    ir.Run(kSmeltSeconds + 0.5f);
    CHECK(ir.bag.Count(Item::IronBar) == 1);

    // steel: iron + coal
    MiniWorld st;
    GiveStarterKit(&st.bag);
    st.skills.SetXp(Skill::Smithing, XpForLevel(30));
    st.bag.Add(Item::IronOre);
    st.bag.Add(Item::Coal, 2);
    st.game.TapStation(0);
    st.Run(4.0f);
    st.game.StartSmelt(Tier::Steel, 1);
    st.Run(kSmeltSeconds + 0.5f);
    CHECK(st.bag.Count(Item::SteelBar) == 1 && !st.bag.Has(Item::Coal) && !st.bag.Has(Item::IronOre));

    // ---- the anvil
    MiniWorld a;
    a.bag.Add(Item::BronzeBar, 6);
    a.game.TapStation(1);
    a.Run(4.0f);
    CHECK(a.ev.Said("need a hammer") && a.ev.makeMenus == 0);
    GiveStarterKit(&a.bag);
    a.game.TapStation(1);
    a.Run(4.0f);
    CHECK(a.ev.makeMenus == 1 && a.ev.lastMake.kind == MakeMenu::Kind::Smith && a.ev.lastMake.options.size() == static_cast<size_t>(kGearTypeCount));
    CHECK(a.ev.lastMake.tier == static_cast<int>(Tier::Bronze) && a.ev.lastMake.options[0].canMake == 6 && a.ev.lastMake.options[0].detail == "1 bar");
    CHECK(!a.ev.lastMake.options[1].levelOk && a.ev.lastMake.options[1].level == 5);   // sword: level 5
    a.game.StartSmith(Tier::Bronze, GearType::Sword, 1);
    CHECK(a.ev.Said("Smithing level of 5") && a.game.Current() == Activity::None);
    a.game.StartSmith(Tier::Bronze, GearType::Dagger, 3);
    CHECK(a.game.Current() == Activity::Smithing);
    CHECK(a.game.WorkPhase() >= 0.0f);
    a.Run(kSmithSeconds * 3 + 0.5f);
    CHECK(a.bag.Count(GearItem(Tier::Bronze, GearType::Dagger)) == 3 && a.bag.Count(Item::BronzeBar) == 3);
    CHECK(a.skills.Xp(Skill::Smithing) == 3 * GetSmithRecipe(Tier::Bronze, GearType::Dagger).xp && a.game.Current() == Activity::None);
    CHECK(a.ev.Said("You make a bronze dagger"));
    // not enough bars for the big piece
    a.skills.SetXp(Skill::Smithing, XpForLevel(25));
    a.game.StartSmith(Tier::Bronze, GearType::Body, 1);
    CHECK(a.ev.Said("You need 5 bars of bronze to make a bronze platebody"));
    // "an iron ..."
    a.bag.Add(Item::IronBar, 1);
    a.skills.SetXp(Skill::Smithing, XpForLevel(20));
    a.game.StartSmith(Tier::Iron, GearType::Dagger, 1);
    a.Run(kSmithSeconds + 0.5f);
    CHECK(a.ev.Said("You make an iron dagger"));
    // a pickaxe
    a.bag.Add(Item::BronzeBar, 2);
    a.skills.SetXp(Skill::Smithing, XpForLevel(6));
    a.game.StartSmith(Tier::Bronze, GearType::Pickaxe, 1);
    a.Run(kSmithSeconds + 0.5f);
    CHECK(a.bag.Count(Item::BronzePickaxe) == 2);

    // using an item on a station
    MiniWorld use;
    GiveStarterKit(&use.bag);
    use.bag.Add(Item::TinOre);
    use.bag.Add(Item::BronzeBar);
    use.game.BeginUse(use.bag.FindSlot(Item::TinOre));
    use.game.UseItemOnStation(use.bag.FindSlot(Item::TinOre), 1);        // ore on an anvil: nothing
    CHECK(use.ev.Said("Nothing interesting"));
    use.game.UseItemOnStation(use.bag.FindSlot(Item::TinOre), 0);        // ore on the furnace: walks there and opens the menu
    use.Run(4.0f);
    CHECK(use.ev.makeMenus == 1);
    use.game.UseItemOnStation(use.bag.FindSlot(Item::BronzeBar), 1);
    use.Run(4.0f);
    CHECK(use.ev.makeMenus == 2 && use.ev.lastMake.kind == MakeMenu::Kind::Smith);
}

static void TestGameplayEquipAndCombat() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    const Item sword = GearItem(Tier::Bronze, GearType::Sword), isword = GearItem(Tier::Iron, GearType::Sword);
    w.bag.Add(sword);
    w.bag.Add(isword);
    w.game.DoItemAction(w.bag.FindSlot(isword), ItemAction::Wield);
    CHECK(w.ev.Said("need Attack level 5") && w.equip.Empty());
    const int before = w.ev.changes;
    w.game.DoItemAction(w.bag.FindSlot(sword), ItemAction::Wield);
    CHECK(w.equip.Get(EquipSlot::Weapon) == sword && w.ev.Said("You wield the bronze sword") && w.ev.changes > before);
    CHECK(w.game.Worn().Get(EquipSlot::Weapon) == sword);
    w.bag.Add(GearItem(Tier::Bronze, GearType::Helm));
    w.game.DoItemAction(w.bag.FindSlot(GearItem(Tier::Bronze, GearType::Helm)), ItemAction::Wear);
    CHECK(w.ev.Said("You put on the bronze medium helm"));
    w.game.Unequip(EquipSlot::Head);
    CHECK(w.ev.Said("You remove the bronze medium helm") && w.bag.Has(GearItem(Tier::Bronze, GearType::Helm)));
    CHECK(BuildEquipMenuOk(w));

    // gear changes the numbers the combat formulas see
    Skills s;
    const Fighter bare = PlayerFighter(s, CombatStyle::Accurate);
    const Fighter armed = PlayerFighter(s, CombatStyle::Accurate, 36, 32, 0);
    Fighter chicken;
    chicken.creature = true;
    CHECK(HitChance(armed, chicken) > HitChance(bare, chicken));
    Skills strong;
    strong.SetXp(Skill::Strength, XpForLevel(40));
    CHECK(MaxHit(PlayerFighter(strong, CombatStyle::Aggressive, 0, 32, 0)) > MaxHit(PlayerFighter(strong, CombatStyle::Aggressive)));
    Fighter beast = chicken;
    beast.attack = 10;
    beast.strength = 10;
    const Fighter naked = PlayerFighter(s, CombatStyle::Defensive);
    const Fighter armoured = PlayerFighter(s, CombatStyle::Defensive, 0, 0, 121);
    CHECK(HitChance(beast, armoured) < HitChance(beast, naked) * 0.6);

    // the same fight goes better with a good weapon: kill a chicken with fists vs a mithril battleaxe (rolls fixed)
    auto fightSeconds = [](Item weapon) {
        MiniWorld m;
        m.skills.SetXp(Skill::Attack, XpForLevel(30));
        if (weapon != Item::None) {
            m.bag.Add(weapon);
            m.game.DoItemAction(m.bag.FindSlot(weapon), ItemAction::Wield);
        }
        m.rollValue = 0.3;
        m.player.Teleport({9, 1});
        m.game.TapNpc(1);
        float t = 0;
        while (m.npcs.Find(1)->Alive() && t < 60.0f) {
            m.Run(0.5f);
            t += 0.5f;
        }
        return t;
    };
    CHECK(fightSeconds(GearItem(Tier::Mithril, GearType::Axe)) <= fightSeconds(Item::None));

    // a creature's strike makes the hero block (the animation clock), landed or not
    MiniWorld b;
    b.player.Teleport({11, 1});
    b.rollValue = 0.0;    // every roll succeeds: the creature hits
    b.game.TapNpc(1);
    b.Run(6.0f);
    CHECK(b.ev.hitsOnPlayer.size() >= 1);
    CHECK(b.game.BlockAge() < 6.0f);
}

static void TestMenusRocksAndStations() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    w.player.Teleport({9, 2});
    std::vector<MenuEntry> e = w.game.BuildMenu({{TargetKind::Rock, 0}}, true, 5.5f, 0.5f);
    CHECK(e.size() == 4 && e[0].act == MenuAct::Mine && e[0].Text() == "Mine Copper rocks" && e[1].act == MenuAct::ExamineTarget);
    CHECK(e[0].label[1].color.r == kMenuObject.r && e[0].label[1].color.g == kMenuObject.g);
    w.game.Perform(e[0]);
    CHECK(w.player.Moving());
    w.Run(7.0f);
    e = w.game.BuildMenu({{TargetKind::Rock, 0}}, false, 0, 0);
    CHECK(e.size() == 2 && e[0].Text() == "Examine Empty rocks");     // a mined-out rock can only be examined
    e = w.game.BuildMenu({{TargetKind::Station, 0}, {TargetKind::Station, 1}}, false, 0, 0);
    CHECK(e[0].Text() == "Smelt Furnace" && e[2].Text() == "Smith Anvil" && e[0].act == MenuAct::UseStation);
    w.bag.Add(Item::TinOre);
    w.game.BeginUse(w.bag.FindSlot(Item::TinOre));
    e = w.game.BuildMenu({{TargetKind::Station, 0}}, false, 0, 0);
    CHECK(e[0].act == MenuAct::UseOnStation && e[0].Text() == "Use Tin ore -> Furnace");
    // item menus of gear
    w.game.Cancel();
    w.bag.Add(GearItem(Tier::Steel, GearType::Mace));
    const std::vector<MenuEntry> im = w.game.BuildItemMenu(w.bag.FindSlot(GearItem(Tier::Steel, GearType::Mace)));
    CHECK(im[0].Text() == "Wield Steel mace" && im[0].itemAction == ItemAction::Wield);
}

// ---------------------------------------------------------------------------------------------- coins, shops, people, goblins
static void TestCoinsAndValues() {
    Inventory bag;
    CHECK(GetItemInfo(Item::Coins).stackable && !GetItemInfo(Item::Bread).stackable);
    CHECK(bag.Add(Item::Coins, 25) == 25 && bag.Add(Item::Coins, 10) == 10 && bag.Count(Item::Coins) == 35);
    int stacks = 0;
    for (int i = 0; i < Inventory::kSlots; ++i) stacks += bag.Slot(i).item == Item::Coins;
    CHECK(stacks == 1);                                                       // one slot, however many coins
    while (!bag.Full()) bag.Add(Item::BurntFish);
    CHECK(bag.Add(Item::Coins, 5) == 5 && bag.Count(Item::Coins) == 40);      // a full bag still takes coins you already carry
    CHECK(bag.Remove(Item::Coins, 15) && bag.Count(Item::Coins) == 25 && !bag.Remove(Item::Coins, 26));
    // piles merge coins; picking them up gives one stack
    GroundItems g;
    g.Drop({3, 3}, {Item::Coins, 5});
    g.Drop({3, 3}, {Item::Coins, 7});
    g.Drop({3, 3}, {Item::Bones, 1});
    CHECK(g.AtTile({3, 3})->items.size() == 2);
    // values
    CHECK(ItemValue(Item::Logs) > 0 && ItemValue(Item::BurntFish) == 0 && ItemValue(Item::Coins) == 1);
    CHECK(!ItemSellable(Item::Coins) && !ItemSellable(Item::Hammer) && !ItemSellable(Item::BurntFish) && ItemSellable(Item::Logs));
    for (int t = 1; t < kTierCount; ++t) {                                    // better metal, higher price; bigger piece, higher price
        CHECK(ItemValue(GearItem(static_cast<Tier>(t), GearType::Sword)) > ItemValue(GearItem(static_cast<Tier>(t - 1), GearType::Sword)));
        CHECK(ItemValue(static_cast<Item>(static_cast<int>(Item::BronzeBar) + t)) > ItemValue(static_cast<Item>(static_cast<int>(Item::BronzeBar) + t - 1)));
    }
    CHECK(ItemValue(GearItem(Tier::Iron, GearType::Body)) > ItemValue(GearItem(Tier::Iron, GearType::Helm)));
    for (int i = 1; i < static_cast<int>(Item::Count); ++i) {                 // every item has a name and a sensible (non-negative) value
        CHECK(ItemValue(static_cast<Item>(i)) >= 0);
        CHECK(std::string(GetItemInfo(static_cast<Item>(i)).name) != "nothing");
    }
    // smelting and smithing make money: a smithed bronze dagger is worth more than its bar
    CHECK(ItemValue(GearItem(Tier::Bronze, GearType::Dagger)) > ItemValue(Item::BronzeBar));
    CHECK(GetItemInfo(Item::Bread).food && GetItemInfo(Item::Bread).heals == 5);
}

static void TestShopsData() {
    Shops shops;
    shops.Init();
    CHECK(shops.Count() >= 6 && ShopIndex("village_weapons") >= 0 && ShopIndex("nope") == -1);
    const int general = ShopIndex("general_town"), weapons = ShopIndex("village_weapons"), ores = ShopIndex("village_ore"), cook = ShopIndex("village_cook");
    // buying costs more than the shop pays you, always
    for (int sh = 0; sh < shops.Count(); ++sh)
        for (const Shops::Line& l : shops.Lines(sh)) {
            CHECK(l.count == l.base && l.count > 0 && shops.BuyPrice(sh, l.item) > 0);
            if (shops.Buys(sh, l.item)) CHECK(shops.SellPrice(sh, l.item) < shops.BuyPrice(sh, l.item));   // no free money by buying and selling back
        }
    // what each shop buys
    CHECK(shops.Buys(general, Item::Logs) && shops.Buys(general, Item::BronzeBar) && !shops.Buys(general, Item::Coins) && !shops.Buys(general, Item::BurntFish));
    CHECK(shops.Buys(weapons, GearItem(Tier::Iron, GearType::Mace)) && !shops.Buys(weapons, GearItem(Tier::Iron, GearType::Helm)) && !shops.Buys(weapons, Item::Logs));
    CHECK(shops.Buys(ores, Item::Coal) && shops.Buys(ores, Item::SteelBar) && !shops.Buys(ores, Item::Bread));
    CHECK(shops.Buys(cook, Item::RawShrimps) && shops.Buys(cook, Item::Egg) && !shops.Buys(cook, Item::CopperOre));
    CHECK(shops.SellPrice(ores, Item::Logs) == 0);                              // not their kind of thing
    CHECK((CategoryOf(Item::IronPickaxe) & kCatTool) && (CategoryOf(GearItem(Tier::Steel, GearType::Shield)) & kCatArmour));

    // taking and restocking
    int line = -1;
    for (size_t i = 0; i < shops.Lines(ores).size(); ++i)
        if (shops.Lines(ores)[i].item == Item::MithrilOre) line = static_cast<int>(i);
    CHECK(line >= 0 && shops.Lines(ores)[line].count == 2);
    CHECK(shops.Take(ores, line, 5) == 2 && shops.Lines(ores)[line].count == 0 && shops.Lines(ores)[line].base == 2);   // a normal line stays, empty
    shops.Update(kRestockSeconds + 0.5f);
    CHECK(shops.Lines(ores)[line].count == 1);
    shops.Update(kRestockSeconds * 3);
    CHECK(shops.Lines(ores)[line].count == 2);                                  // back to normal, no more
    // things sold to the shop go on its shelves and fade away when it is not a normal stock line
    const size_t before = shops.Lines(ores).size();
    shops.Add(ores, Item::MithrilBar, 3);
    shops.Add(ores, Item::MithrilBar, 1);
    CHECK(shops.Lines(ores).size() == before);                                  // (mithril bar is a normal line: its count just went up)
    shops.Add(general, GearItem(Tier::Iron, GearType::Axe), 2);
    const size_t gl = shops.Lines(general).size();
    shops.Update(kExcessSeconds + 1.0f);
    shops.Update(kExcessSeconds + 1.0f);
    CHECK(shops.Lines(general).size() == gl - 1);                               // the extra line sold out of its own accord (count 2 -> 0)
}

static void TestPeopleAndShopping() {
    MiniWorld w;
    CHECK(w.people.All().size() == 2 && w.people.All()[0].info->key == std::string("aubrey") && w.people.AtTile({8, 3}) == 1);
    CHECK(FindPersonInfo("zzz") == nullptr && FindPersonInfo("marta") && FindPersonInfo("marta")->look.female);
    for (const PersonInfo& p : AllPersonInfos()) {                              // every shopkeeper's shop exists; everyone has lines to say
        CHECK(p.talkCount >= 1 && p.talk[0] && p.talk[0][0] && p.name[0] && p.examine[0]);
        if (p.shop[0]) CHECK(ShopIndex(p.shop) >= 0);
    }
    // the menu: shopkeepers are "Trade", others "Talk-to"
    std::vector<MenuEntry> e = w.game.BuildMenu({{TargetKind::Person, 1}}, false, 0, 0);
    CHECK(e.size() == 4 && e[0].act == MenuAct::Trade && e[0].Text() == "Trade Aubrey" && e[1].act == MenuAct::TalkTo && e[2].act == MenuAct::ExamineTarget);
    CHECK(e[0].label[1].color.r == kMenuNpc.r && e[0].label[1].color.b == kMenuNpc.b);
    e = w.game.BuildMenu({{TargetKind::Person, 2}}, false, 0, 0);
    CHECK(e[0].act == MenuAct::TalkTo && e[0].Text() == "Talk-to Wilf");

    // walking up and trading (a shopkeeper across a counter can be reached from two tiles away)
    GiveStarterKit(&w.bag);
    w.player.Teleport({3, 1});
    w.game.TapPerson(1);
    float mx = 0, mz = 0;
    CHECK(w.game.TakeMarker(&mx, &mz) && mx == 8.5f && mz == 3.5f && w.player.Moving());
    w.Run(5.0f);
    CHECK(w.ev.shopMenus == 1 && w.ev.Said("Aubrey: Welcome"));
    const TileCoord at = w.player.Tile();
    CHECK(std::max(std::abs(at.x - 8), std::abs(at.z - 3)) <= 2);
    CHECK(w.ev.lastShop.name == "Journey General Store" && w.ev.lastShop.keeper == "Aubrey" && w.ev.lastShop.coins == 0);
    CHECK(!w.ev.lastShop.buy.empty() && !w.ev.lastShop.buy[0].ok);              // no coins: nothing is affordable
    // the shopkeeper turned to face the hero
    const Person* p = w.people.Find(1);
    CHECK(std::fabs(p->facing - std::atan2(w.player.X() - p->x, w.player.Z() - p->z)) < 0.01f);

    // buying
    const int shop = ShopIndex("general_town");
    w.game.Buy(shop, 0, 1);
    CHECK(w.ev.Said("don't have enough coins") && !w.bag.Has(Item::Bread));
    w.bag.Add(Item::Coins, 100);
    int bread = -1;
    for (const Shops::Line& l : w.shops.Lines(shop)) bread = l.item == Item::Bread ? static_cast<int>(&l - &w.shops.Lines(shop)[0]) : bread;
    CHECK(bread >= 0);
    const int price = w.shops.BuyPrice(shop, Item::Bread);
    w.game.Buy(shop, bread, 3);
    CHECK(w.bag.Count(Item::Bread) == 3 && w.bag.Count(Item::Coins) == 100 - 3 * price && w.shops.Lines(shop)[bread].count == 12 - 3);
    CHECK(w.ev.Said("You buy 3 bread"));
    w.game.Buy(shop, bread, 1000);                                              // more than it has, more than he can afford: as many as possible
    CHECK(w.bag.Count(Item::Bread) <= 12 && w.bag.Count(Item::Coins) >= 0 && w.bag.Count(Item::Bread) == 3 + (100 - 3 * price) / price);
    // restocks
    const int stock = w.shops.Lines(shop)[bread].count;
    w.Run(kRestockSeconds + 1.0f);
    CHECK(w.shops.Lines(shop)[bread].count == std::min(12, stock + 1));
    // a full bag refunds
    MiniWorld full;
    full.player.Teleport({8, 1});
    full.bag.Add(Item::Coins, 50);
    while (!full.bag.Full()) full.bag.Add(Item::BurntFish);
    full.game.Buy(shop, bread, 2);
    CHECK(full.bag.Count(Item::Coins) == 50 && full.ev.Said("inventory space"));
    // too far from the shopkeeper
    MiniWorld far;
    far.bag.Add(Item::Coins, 50);
    far.player.Teleport({12, 3});
    far.game.Buy(shop, bread, 1);
    CHECK(far.ev.Said("too far") && !far.bag.Has(Item::Bread));

    // selling: the shop pays its rate and puts the goods on its shelves; worthless things and tools are refused
    MiniWorld sell;
    sell.player.Teleport({8, 1});
    GiveStarterKit(&sell.bag);
    sell.bag.Add(Item::Logs, 4);
    sell.bag.Add(Item::BurntFish);
    sell.game.Sell(shop, Item::BurntFish, 1);
    CHECK(sell.ev.Said("no use for that") && sell.bag.Has(Item::BurntFish));
    sell.game.Sell(shop, Item::Hammer, 1);
    CHECK(sell.bag.Has(Item::Hammer));
    const int sp = sell.shops.SellPrice(shop, Item::Logs);
    CHECK(sp > 0 && sp < sell.shops.BuyPrice(shop, Item::Logs));
    sell.game.Sell(shop, Item::Logs, 3);
    CHECK(sell.bag.Count(Item::Logs) == 1 && sell.bag.Count(Item::Coins) == 3 * sp && sell.ev.Said("You sell 3 logs"));
    sell.game.Sell(shop, Item::Logs, 99);
    CHECK(!sell.bag.Has(Item::Logs) && sell.bag.Count(Item::Coins) == 4 * sp);
    // the shop menu lists what you carry that it buys
    sell.bag.Add(Item::CopperOre, 2);
    const ShopMenu sm = sell.game.BuildShopMenu(shop);
    bool ore = false, hammer = false;
    for (const ShopLine& l : sm.sell) {
        ore = ore || (l.item == Item::CopperOre && l.stock == 2 && l.price == sell.shops.SellPrice(shop, Item::CopperOre));
        hammer = hammer || l.item == Item::Hammer || l.item == Item::BurntFish || l.item == Item::Coins;
    }
    CHECK(ore && !hammer && sm.coins == 4 * sp);

    // talking to a villager just talks, and cycles their lines
    MiniWorld talk;
    talk.player.Teleport({12, 2});
    talk.game.TapPerson(2, true);
    talk.Run(3.0f);
    CHECK(talk.ev.shopMenus == 0 && talk.ev.Said("Wilf:"));
    talk.game.TapPerson(2, true);
    talk.Run(1.0f);
    CHECK(talk.people.Find(2)->nextLine == 2);
}

static void TestGoblins() {
    Species gs, ws, cs;
    CHECK(SpeciesFromKey("goblin", &gs) && SpeciesFromKey("goblin_warrior", &ws) && SpeciesFromKey("cow", &cs));
    const SpeciesInfo& g = GetSpecies(gs);
    const SpeciesInfo& wr = GetSpecies(ws);
    CHECK(g.aggressive && wr.aggressive && !GetSpecies(cs).aggressive && g.level == 2 && wr.level > g.level && wr.hitpoints > g.hitpoints);
    CHECK(std::string(g.model) == "goblin" && std::string(wr.model) == "goblin" && g.colorway != wr.colorway && wr.scale > g.scale);
    // loot: bones and coins every time (coins within range), the odd weapon
    int withDagger = 0;
    double coinsTotal = 0;
    for (int i = 0; i < 400; ++i) {
        double r = (i + 0.5) / 400.0;
        int calls = 0;
        const std::vector<ItemStack> loot = RollLoot(gs, [&] { return calls++ % 2 == 0 ? 0.0 : r; });   // first roll of each entry succeeds
        bool bones = false;
        for (const ItemStack& s : loot) {
            bones = bones || s.item == Item::Bones;
            if (s.item == Item::Coins) {
                CHECK(s.count >= 3 && s.count <= 14);
                coinsTotal += s.count;
            }
            withDagger += s.item == GearItem(Tier::Bronze, GearType::Dagger);
        }
        CHECK(bones);
    }
    CHECK(coinsTotal / 400 > 5.0 && coinsTotal / 400 < 12.0);
    bool coins = false;
    for (const ItemStack& s : RollLoot(ws, [] { return 0.0; })) {
        if (s.item == Item::Coins) {
            coins = true;
            CHECK(s.count >= 10);
        }
    }
    CHECK(coins);
    // the loot actually drops as a coin pile where it dies
    MiniWorld w;
    w.map.ParseText("@size 14 4\n@spawn 6 1\n@pen camp 8 0 14 4\n@npc goblin 9.5 1.5 camp\n@rows\n..............\n..............\n..............\n..............\n");
    w.npcs.Init(w.map);
    CHECK(w.npcs.All().size() == 1 && w.npcs.All()[0].species == gs && w.npcs.All()[0].colorway == g.colorway);

    // aggression: it attacks a hero who comes within 3 tiles, but only a hero who is weak enough to be worth it
    auto step = [&](MiniWorld& m, float secs) { m.Run(secs); };
    {
        MiniWorld a;
        a.map.ParseText("@size 14 4\n@spawn 6 1\n@pen camp 8 0 14 4\n@npc goblin 10.5 1.5 camp\n@rows\n..............\n..............\n..............\n..............\n");
        a.npcs.Init(a.map);
        a.player.Teleport({1, 1});
        a.npcs.SetHero(a.skills.CombatLevel(), true);
        step(a, 2.0f);
        CHECK(!a.npcs.All()[0].hunting);                                         // far away: it minds its own business
        a.player.Teleport({7, 1});                                               // 3 tiles from it
        a.npcs.SetHero(a.skills.CombatLevel(), true);
        step(a, 2.0f);
        CHECK(a.npcs.All()[0].hunting);
        step(a, 8.0f);
        CHECK(!a.ev.hitsOnPlayer.empty() || !a.ev.hitsOnNpc.empty());            // and the fight is on (it strikes, he hits back)
    }
    {
        MiniWorld b;
        b.map.ParseText("@size 14 4\n@spawn 6 1\n@pen camp 8 0 14 4\n@npc goblin 10.5 1.5 camp\n@rows\n..............\n..............\n..............\n..............\n");
        b.npcs.Init(b.map);
        b.player.Teleport({8, 1});
        b.npcs.SetHero(60, true);                                                // a strong hero: ignored
        b.Run(3.0f);
        CHECK(!b.npcs.All()[0].hunting);
        b.npcs.SetHero(3, false);                                                // a dead hero: ignored
        b.Run(1.0f);
        CHECK(!b.npcs.All()[0].hunting);
    }
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
    TestMapExtras();
    TestItems();
    TestFishing();
    TestCooking();
    TestFires();
    TestGameplayChopping();
    TestGameplayFishingAndCooking();
    TestGameplayLightingRules();
    TestHitpointsAndCombatLevel();
    TestCombatMath();
    TestNpcsAndLoot();
    TestGroundItems();
    TestGameplayCombat();
    TestGameplayDeathAndAutoRetaliate();
    TestMenusAndUse();
    TestGearTable();
    TestEquipment();
    TestMiningRules();
    TestSmithingRules();
    TestGameplayMining();
    TestGameplaySmeltingAndSmithing();
    TestGameplayEquipAndCombat();
    TestMenusRocksAndStations();
    TestCoinsAndValues();
    TestShopsData();
    TestPeopleAndShopping();
    TestGoblins();
    TestRealTownMap(mapPath);
    if (g_failed == 0) std::printf("All core tests passed.\n");
    else std::printf("%d check(s) failed.\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
