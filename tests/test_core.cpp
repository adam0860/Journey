// Tests for the pure-C++ game core (no window, no GPU). Run with: ctest
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <vector>

#include "game/Archery.h"
#include "game/Combat.h"
#include "game/Cooking.h"
#include "game/Crafting.h"
#include "game/Equipment.h"
#include "game/Gear.h"
#include "game/Mining.h"
#include "game/People.h"
#include "game/Shops.h"
#include "game/Smithing.h"
#include "game/Fires.h"
#include "game/Fishing.h"
#include "game/Forest.h"
#include "balance.h"
#include "game/Adventurers.h"
#include "game/Gameplay.h"
#include "game/GroundItems.h"
#include "game/Inventory.h"
#include "game/Loot.h"
#include "game/Npc.h"
#include "game/Player.h"
#include "game/Progress.h"
#include "game/Settings.h"
#include "game/Skilling.h"
#include "game/Skills.h"
#include "game/Synth.h"
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
    ParseProgress("xp.woodcutting=abc\nslot.99=1:1\nslot.0=9999:1\nslot.1=1:-4\nnonsense\nxp.bogus=5\n", &s3, &bag3);
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
    CHECK(has(ActionsFor(Item::Tinderbox, bag), ItemAction::Light) && has(ActionsFor(Item::Tinderbox, bag), ItemAction::Drop));
    CHECK(has(ActionsFor(Item::BronzeAxe, bag), ItemAction::Use) && has(ActionsFor(Item::BronzeAxe, bag), ItemAction::Drop));   // every item can be dropped
    CHECK(has(ActionsFor(Item::SmallNet, bag), ItemAction::Examine) && has(ActionsFor(Item::SmallNet, bag), ItemAction::Drop));
    for (int i = 1; i < static_cast<int>(Item::Count); ++i)   // ...really every item
        CHECK(has(ActionsFor(static_cast<Item>(i), bag), ItemAction::Drop));
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
    int xpDrops = 0, levelUps = 0, changes = 0, deaths = 0, makeMenus = 0, shopMenus = 0, bankOpens = 0;
    std::vector<ShotFx> shots;
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
    void OpenBank() override { ++bankOpens; }
    void Shoot(const ShotFx& fx) override { shots.push_back(fx); }
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
    Bank bank;
    RecordingEvents ev;
    double rollValue = 0.0;   // 0.0 = every roll succeeds (and burns food); 0.99 = every roll fails
    Gameplay game;

    MiniWorld()
        : game(map, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, bank, ev, [this] { return rollValue; }) {
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
    CHECK(!c.bag.Has(Item::BronzeAxe) && c.ev.Said("You drop the bronze axe"));      // tools can be dropped too

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
    CHECK(m.ParseText("@size 12 6\n@pen yard 6 1 11 5\n@pen bogus 1\n@npc chicken 7.5 2.5 yard\n@npc cow 9.5 3.5 yard\n@npc unicorn 8.5 2.5 yard\n"
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
        CHECK(sp.level >= 1 && sp.hitpoints >= 1 && sp.speed > 0.0f && sp.respawnSeconds > 0.0f);
        Species back;
        CHECK(SpeciesFromKey(sp.key, &back) && back == static_cast<Species>(i));
        const LootTable* table = FindLootTable(sp.lootKey);
        CHECK(table != nullptr);
        if (table) {   // everything leaves bones of some kind, every time
            bool bones = false;
            for (const LootDrop& d : table->drops)
                bones = bones || (d.tier == LootTier::Always && GetItemInfo(d.item).buryXp > 0);
            CHECK(bones);
        }
    }
    Species dummy;
    CHECK(!SpeciesFromKey("unicorn", &dummy));
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
    l = RollLoot(Species::Calf, [] { return 0.9; });
    CHECK(l.size() == 1 && l[0].item == Item::Bones);                                           // one extra pick, or nothing
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
    // Items do not last forever: 60 seconds after landing, each on its own clock.
    CHECK(GroundItems::kLifeSeconds == 60.0f);
    g.Update(GroundItems::kLifeSeconds - 1.0f);
    CHECK(g.All().size() == 2);
    g.Update(2.0f);
    CHECK(g.All().empty());
    g.Drop({7, 7}, {Item::Bones, 1});
    g.Update(40.0f);
    g.Drop({7, 7}, {Item::Logs, 1});          // a new drop on an old pile does not keep the old item alive
    g.Update(25.0f);
    CHECK(g.All().size() == 1 && g.All()[0].items.size() == 1 && g.All()[0].items[0].item == Item::Logs);
    g.Update(40.0f);
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

// Dying in a danger zone (the Abyss) costs everything but the three most valuable items; what falls lies there for five minutes.
static void TestDeathInTheAbyss() {
    MiniWorld w;
    w.map.ParseText("@size 14 4\n@spawn 6 1\n@area 0 0 14 4 The Abyss\n@pen yard 10 0 14 4\n@npc cow 12.5 2.5 yard\n@rows\n"
                    "~~~..o.o......\n~~~...........\n~~~...........\n~~~.....o.....\n");
    w.npcs.Init(w.map);
    w.forest.Init(w.map.Trees());
    w.rocks.Init(w.map.Rocks());
    w.people.Init(w.map);
    CHECK(IsDangerousArea(w.map.AreaAt(5, 1)) && !IsDangerousArea("Millbrook Village"));
    CHECK(ItemValue(Item::MithrilBar) > ItemValue(Item::SteelBar) && ItemValue(Item::SteelBar) > ItemValue(Item::IronBar));
    CHECK(ItemValue(Item::IronBar) > ItemValue(GearItem(Tier::Bronze, GearType::Helm)) && ItemValue(GearItem(Tier::Bronze, GearType::Helm)) > ItemValue(Item::Logs));
    w.bag.Add(Item::MithrilBar);
    w.bag.Add(Item::SteelBar);
    w.bag.Add(Item::IronBar);
    w.bag.Add(Item::Logs, 5);
    w.bag.Add(Item::Coins, 500);
    w.equip.Set(EquipSlot::Head, GearItem(Tier::Bronze, GearType::Helm));
    w.skills.SetHp(2);
    w.rollValue = 0.0;
    const int cowId = w.npcs.All()[0].id;
    w.player.Teleport({12, 3});
    w.npcs.StartHunt(cowId);
    w.Run(15.0f);
    CHECK(w.ev.deaths == 1 && w.player.Tile() == w.map.Spawn());
    CHECK(w.bag.Has(Item::MithrilBar) && w.bag.Has(Item::SteelBar) && w.bag.Has(Item::IronBar));   // the three best are kept
    CHECK(!w.bag.Has(Item::Logs) && !w.bag.Has(Item::Coins) && w.equip.Get(EquipSlot::Head) == Item::None);
    const Pile* pile = w.ground.AtTile({12, 3});
    CHECK(pile != nullptr);
    if (pile) {
        int coins = 0, logs = 0, helms = 0;
        for (const ItemStack& s : pile->items) {
            coins += s.item == Item::Coins ? s.count : 0;
            logs += s.item == Item::Logs ? s.count : 0;
            helms += s.item == GearItem(Tier::Bronze, GearType::Helm) ? s.count : 0;
        }
        CHECK(coins == 500 && logs == 5 && helms == 1);
    }
    CHECK(w.ev.Said("most valuable items"));
    w.Run(290.0f);                                                  // still there after nearly five minutes...
    CHECK(w.ground.AtTile({12, 3}) != nullptr);
    w.Run(15.0f);                                                   // ...and gone after that
    CHECK(w.ground.AtTile({12, 3}) == nullptr);
}

// The procedural audio and the volume settings.
static void TestSynthAndSettings() {
    for (int m = 0; m < kMoodCount; ++m) {
        const std::vector<int16_t> a = RenderMusic(static_cast<Mood>(m));
        CHECK(a.size() > static_cast<size_t>(kSampleRate) * 15 && a.size() < static_cast<size_t>(kSampleRate) * 60);   // a loop of 15-60 seconds
        int peak = 0;
        for (int16_t v : a) peak = std::max(peak, std::abs(static_cast<int>(v)));
        CHECK(peak > 5000 && peak <= 30000);                                          // audible, and never clipping
        CHECK(a == RenderMusic(static_cast<Mood>(m)));                                // the same every time
        CHECK(std::abs(static_cast<int>(a.front()) - static_cast<int>(a.back())) < 15000);   // no big jump where the loop restarts
        CHECK(std::string(MoodName(static_cast<Mood>(m))) != "?");
    }
    for (int s = 0; s < kSfxCount; ++s) {
        const std::vector<int16_t> a = RenderSfx(static_cast<Sfx>(s));
        CHECK(a.size() > 100 && a.size() < static_cast<size_t>(kSampleRate) * 2);     // a short sound
        int peak = 0;
        for (int16_t v : a) peak = std::max(peak, std::abs(static_cast<int>(v)));
        CHECK(peak > 1500 && peak <= 30000);
        CHECK(a == RenderSfx(static_cast<Sfx>(s)));
    }
    CHECK(MoodForArea("The Abyss") == Mood::Abyss && MoodForArea("Mining Caves") == Mood::Cave && MoodForArea("Castle Crypt") == Mood::Cave);
    CHECK(MoodForArea("Desert Ruins") == Mood::Desert && MoodForArea("Millbrook Square") == Mood::Town && MoodForArea("Castle Town") == Mood::Town);
    CHECK(MoodForArea("") == Mood::Field && MoodForArea("Lakeside") == Mood::Field && MoodForArea("Wolf Wood") == Mood::Field);
    // settings: they survive a round trip, are clamped, and bad lines are ignored
    Settings s;
    s.music = 0.25f;
    s.sfx = 1.0f;
    const Settings r = ParseSettings(SerializeSettings(s));
    CHECK(std::fabs(r.music - 0.25f) < 1e-4f && std::fabs(r.sfx - 1.0f) < 1e-4f);
    const Settings d = ParseSettings("");
    CHECK(std::fabs(d.music - 0.5f) < 1e-4f && std::fabs(d.sfx - 0.8f) < 1e-4f);
    const Settings g = ParseSettings("music=7\nsfx=-3\nnonsense\nvolume=abc\nsfx=x\n");
    CHECK(g.music == 1.0f && g.sfx == 0.0f);
}

// Rings and amulets: worn in their own slots, each with a small bonus from its gem, and saved with the rest of the worn gear.
static void TestJewellery() {
    Equipment eq;
    Inventory bag;
    Skills skills;
    bag.Add(Item::RubyRing);
    bag.Add(Item::SapphireRing);
    bag.Add(Item::EmeraldAmulet);
    bag.Add(Item::GoldBar);
    EquipSlot slot;
    CHECK(Equippable(Item::RubyRing, &slot) && slot == EquipSlot::Ring);
    CHECK(Equippable(Item::EmeraldAmulet, &slot) && slot == EquipSlot::Amulet);
    CHECK(!Equippable(Item::GoldBar, &slot) && !Equippable(Item::Pot, &slot));
    CHECK(std::string(SlotName(EquipSlot::Ring)) == "Ring" && kEquipSlotCount == 11);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::RubyRing), skills) == EquipResult::Done);
    CHECK(eq.Total().strength == 4 && eq.Total().defence == 1 && !bag.Has(Item::RubyRing));
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::SapphireRing), skills) == EquipResult::Done);   // a second ring swaps with the first
    CHECK(eq.Get(EquipSlot::Ring) == Item::SapphireRing && bag.Has(Item::RubyRing) && eq.Total().magic == 4 && eq.Total().strength == 0);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::EmeraldAmulet), skills) == EquipResult::Done);
    CHECK(eq.Total().ranged == 7 && eq.Total().defence == 3);
    // the worn jewellery survives a save
    const std::string text = SerializeProgress(skills, bag, CombatStyle::Accurate, &eq);
    Equipment eq2;
    Skills s2;
    Inventory bag2;
    CombatStyle style;
    ParseProgress(text, &s2, &bag2, &style, &eq2);
    CHECK(eq2.Get(EquipSlot::Ring) == Item::SapphireRing && eq2.Get(EquipSlot::Amulet) == Item::EmeraldAmulet);
    // and it can be taken off again
    CHECK(Unequip(&eq, &bag, EquipSlot::Amulet) == EquipResult::Done && eq.Get(EquipSlot::Amulet) == Item::None && bag.Has(Item::EmeraldAmulet));
}

// A cooking range: raw food goes on it (menu, tap or the Cook button) and it burns less often than a fire.
static void TestCookingRange() {
    MiniWorld w;
    w.map.ParseText("@size 12 5\n@spawn 1 1\n@station range 5.5 2.5 0\n@rows\n............\n............\n.....o......\n............\n............\n");
    w.npcs.Init(w.map);
    w.forest.Init(w.map.Trees());
    w.rocks.Init(w.map.Rocks());
    w.people.Init(w.map);
    w.player.Teleport({1, 1});
    const Recipe* shrimp = RecipeFor(Item::RawShrimps);
    CHECK(shrimp && BurnChance(*shrimp, 1 + kRangeLevelBonus) < BurnChance(*shrimp, 1));
    w.bag.Add(Item::RawShrimps, 3);
    w.rollValue = 0.40;                       // a fire would burn this (45% at level 1); the range (about 34%) does not
    w.game.DoItemAction(w.bag.FindSlot(Item::RawShrimps), ItemAction::Cook);   // no fire anywhere: the range is the only place
    w.Run(20.0f);
    CHECK(w.bag.Count(Item::Shrimps) == 3 && w.bag.Count(Item::BurntFish) == 0);
    CHECK(w.skills.Xp(Skill::Cooking) > 0);
    // the menu and "use" know the range
    const std::vector<MenuEntry> menu = w.game.BuildMenu({{TargetKind::Station, 0}}, false, 0.0f, 0.0f);
    bool cookEntry = false;
    for (const MenuEntry& e : menu) cookEntry = cookEntry || e.act == MenuAct::UseStation;
    CHECK(cookEntry);
    w.bag.Add(Item::Logs);
    w.game.UseItemOnStation(w.bag.FindSlot(Item::Logs), 0);              // logs do nothing there
    CHECK(w.ev.Said("Nothing interesting"));
    // without a range or a fire the Cook button explains
    MiniWorld none;
    none.bag.Add(Item::RawShrimps);
    none.game.DoItemAction(none.bag.FindSlot(Item::RawShrimps), ItemAction::Cook);
    CHECK(none.ev.Said("fire or a cooking range"));
}

// Teleport scrolls: Read uses one up and takes you to the middle of the named place, but not out of a danger zone.
static void TestTeleportScrolls() {
    MiniWorld w;
    w.map.ParseText("@size 14 4\n@spawn 1 1\n@area 0 0 14 4 The Abyss\n@area 8 0 14 4 Market Square\n@rows\n"
                    "..............\n..............\n..............\n..............\n");
    w.npcs.Init(w.map);
    w.forest.Init(w.map.Trees());
    w.rocks.Init(w.map.Rocks());
    w.people.Init(w.map);
    const std::vector<ItemAction> acts = ActionsFor(Item::CastleScroll, w.bag);
    CHECK(!acts.empty() && acts[0] == ItemAction::Read && std::string(ActionName(ItemAction::Read)) == "Read");
    CHECK(GetItemInfo(Item::CastleScroll).stackable && ItemValue(Item::LakesideScroll) > 0);
    w.bag.Add(Item::CastleScroll, 2);
    w.player.Teleport({2, 1});                                      // inside the Abyss: the magic fails
    w.game.DoItemAction(w.bag.FindSlot(Item::CastleScroll), ItemAction::Read);
    CHECK(w.player.Tile() == TileCoord({2, 1}) && w.bag.Count(Item::CastleScroll) == 2 && w.ev.Said("strange force"));
    w.player.Teleport({8, 0});                                      // in the market square, but he has never been there: it does not work
    w.game.DoItemAction(w.bag.FindSlot(Item::CastleScroll), ItemAction::Read);
    CHECK(w.player.Tile() == TileCoord({8, 0}) && w.bag.Count(Item::CastleScroll) == 2 && w.ev.Said("never been"));
    w.game.Update(0.1f);                                            // standing in a place counts as having been there
    CHECK(w.game.Visited().count("Market Square") == 1);
    w.game.DoItemAction(w.bag.FindSlot(Item::CastleScroll), ItemAction::Read);
    CHECK(w.player.Tile() == TileCoord({11, 2}) && w.bag.Count(Item::CastleScroll) == 1 && w.ev.Said("whisked"));
    // no such place on this map: nothing happens and the scroll is kept
    w.bag.Add(Item::LakesideScroll);
    w.game.MarkVisited("Lakeside");
    w.game.DoItemAction(w.bag.FindSlot(Item::LakesideScroll), ItemAction::Read);
    CHECK(w.bag.Has(Item::LakesideScroll) && w.player.Tile() == TileCoord({11, 2}));
    // the visited places are saved (with a version number), and an old file without either still loads
    const std::string saved = SerializeProgress(w.skills, w.bag, CombatStyle::Accurate, nullptr, nullptr, 0, nullptr, &w.game.Visited());
    CHECK(SaveVersion(saved) == kSaveVersion && saved.find("visited=Market Square") != std::string::npos);
    std::set<std::string> back;
    Skills s2;
    Inventory b2;
    ParseProgress(saved, &s2, &b2, nullptr, nullptr, nullptr, nullptr, nullptr, &back);
    CHECK(back.count("Market Square") == 1 && back.count("Lakeside") == 1 && back.size() == 2);
    CHECK(SaveVersion("hp=5") == 1);
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
    CHECK(w.bag.Has(Item::BronzeAxe) && w.bag.Has(Item::Tinderbox));                  // (a safe place: he lost nothing)

    // hit points creep back by themselves: one a minute (for everybody)
    MiniWorld r;
    r.skills.SetHp(5);
    r.Run(59.0f);
    CHECK(r.skills.Hp() == 5);
    r.Run(2.0f);
    CHECK(r.skills.Hp() == 6);
    r.Run(120.0f);
    CHECK(r.skills.Hp() == 8);
    // ...and a hurt creature mends the same way
    MiniWorld c;
    Npc& cow = c.npcs.All()[1];
    CHECK(cow.maxHp >= 3);
    c.npcs.Hurt(cow.id, 2);
    const int hurt = cow.hp;
    c.Run(30.0f);
    CHECK(c.npcs.Find(cow.id)->hp == hurt);
    c.Run(31.0f);
    CHECK(c.npcs.Find(cow.id)->hp == hurt + 1);
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
    CHECK(m.size() == 4 && m[0].Text() == "Use Bronze axe" && m[1].Text() == "Drop Bronze axe" && m[2].Text() == "Examine Bronze axe");   // tools can be dropped
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

// The hand-built strip (Castle Town down to the Abyss) stands inside the wider world at this offset: tools/worldmap.py (OX, OZ).
static constexpr int kOX = 64, kOZ = 48;
static constexpr int kWorldW = 192, kWorldH = 400;
static TileCoord Core(int x, int z) { return {x + kOX, z + kOZ}; }   // a tile of the old strip, in world tiles

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
    CHECK(m.Width() == kWorldW && m.Height() == kWorldH);
    CHECK(m.Walkable(m.Spawn()));

    struct Goal { const char* name; TileCoord t; };
    const Goal goals[] = {
        {"castle gate", Core(32, 24)},        {"throne room carpet", Core(32, 10)}, {"inside the bank", Core(45, 33)},
        {"inside the inn", Core(45, 48)},     {"market square", Core(30, 38)},      {"river pier end", Core(2, 33)},
        {"east road exit", Core(63, 40)},     {"south road exit", Core(32, 63)},    {"castle courtyard", Core(22, 10)},
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
    CHECK(m.Pens().size() >= 3 && m.FindPen("coop") && m.FindPen("cowpen") && m.FindPen("goblincamp"));
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
    for (int k = 0; k < kRockKindCount; ++k) CHECK(kinds[k]);   // every kind of rock grows somewhere on the map
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
    CHECK(m.AreaAt(58 + kOX, 53 + kOZ) == "Smithy" && m.AreaAt(58 + kOX, 60 + kOZ) == "Quarry");
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
        Bank bank;
        shops.Init();
        people.Init(real);
        Gameplay game(real, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, bank, ev, [] { return 0.0; });
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
    CHECK(m.Height() == kWorldH && m.Width() == kWorldW);
    for (TileCoord goal : {Core(32, 80), Core(32, 97), Core(32, 100), Core(32, 140), Core(32, 172), Core(50, 158), Core(10, 120)}) {
        const PathResult pr = FindPath(m, m.Spawn(), goal);
        if (!m.Walkable(goal) || !pr.found) {
            std::printf("  FAIL cannot walk from the spawn to %d,%d (walkable=%d)\n", goal.x, goal.z, (int)m.Walkable(goal));
            ++g_failed;
        }
    }
    CHECK(m.AreaAt(Core(32, 80).x, Core(32, 80).z) == "Millbrook Road" && m.AreaAt(Core(48, 80).x, Core(48, 80).z) == "Goblin Camp" &&
          m.AreaAt(Core(32, 100).x, Core(32, 100).z) == "Millbrook Village" && m.AreaAt(Core(32, 136).x, Core(32, 136).z) == "Millbrook Square");
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
        Bank bank;
        RecordingEvents ev;
        TileMap real = m;
        shops.Init();
        people.Init(real);
        forest.Init(real.Trees());
        rocks.Init(real.Rocks());
        Gameplay game(real, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, bank, ev, [] { return 0.0; });
        player.Teleport(real.Spawn());
        bag.Add(Item::Coins, 100);
        int aubrey = -1;
        for (const Person& pr : people.All())
            if (std::string(pr.info->key) == "aubrey") aubrey = pr.id;
        CHECK(aubrey > 0);
        game.TapPerson(aubrey);
        for (int i = 0; i < 20 * 60 && !game.Dialogue().open && ev.shopMenus == 0; ++i) {
            player.Update(0.05f);
            game.Update(0.05f);
        }
        CHECK(game.Dialogue().open);               // Aubrey has a quest for him first: say "not now", and the shop opens
        game.DialogueChoose(1);
        CHECK(ev.shopMenus == 1 && !ev.lastShop.buy.empty());
        game.Buy(ev.lastShop.shop, ev.lastShop.buy[0].index, 2);
        CHECK(bag.Count(Item::Coins) < 100 && bag.Has(ev.lastShop.buy[0].item));
    }
    // Walls must actually block: the keep's side wall is not walkable.
    CHECK(!m.Walkable(Core(24, 10).x, Core(24, 10).z));
    // But its door is.
    CHECK(m.Walkable(Core(32, 15).x, Core(32, 15).z));
}


// The far south of the real map: the lake, the wolf wood, the troll mountains, the caves, the desert and the Abyss are all
// connected to the spawn by walking, every monster species and fishing spot is known, and the ferry between the docks works.
static TileCoord NearestWalkable(const TileMap& m, TileCoord t, int radius) {
    for (int r = 0; r <= radius; ++r)
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx)
                if (std::max(std::abs(dx), std::abs(dz)) == r && m.Walkable(t.x + dx, t.z + dz)) return {t.x + dx, t.z + dz};
    return {-1, -1};
}

static void TestFarSouthMap(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping far-south test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    std::string err;
    CHECK(m.ParseText(ss.str(), &err));
    CHECK(m.Width() == kWorldW && m.Height() == kWorldH);

    struct Goal { const char* area; TileCoord t; };
    const Goal goals[] = {
        {"Lakeside", Core(35, 190)},          {"Wolf Wood", Core(50, 190)},          {"Troll Mountains", Core(32, 225)},       {"Mining Caves", Core(32, 255)},
        {"Desert Ruins", Core(32, 285)},      {"The Dragon's Temple", Core(52, 297)}, {"The Abyss", Core(32, 320)},            {"Castle Crypt", Core(16, 58)},
        {"Millbrook Crafters' Yard", Core(55, 104)},
    };
    for (const Goal& g : goals) {
        const TileCoord t = NearestWalkable(m, g.t, 3);
        const PathResult pr = FindPath(m, m.Spawn(), t);
        if (t.x < 0 || !pr.found) {
            std::printf("  FAIL cannot walk from the spawn to %s (%d,%d)\n", g.area, g.t.x, g.t.z);
            ++g_failed;
        }
        if (m.AreaAt(g.t.x, g.t.z) != g.area) {
            std::printf("  FAIL the area at %d,%d is '%s', not '%s'\n", g.t.x, g.t.z, m.AreaAt(g.t.x, g.t.z).c_str(), g.area);
            ++g_failed;
        }
    }

    // all nine kinds of rock are somewhere, and every kind of tree and fishing spot is one the game knows
    bool kinds[kRockKindCount] = {};
    for (const RockDef& r : m.Rocks()) {
        RockKind k;
        CHECK(RockKindFromKey(r.kind, &k));
        kinds[static_cast<int>(k)] = true;
    }
    for (int k = 0; k < kRockKindCount; ++k) CHECK(kinds[k]);
    bool oak = false, willow = false, yew = false;
    for (const TreeDef& t : m.Trees()) {
        oak = oak || t.kind == "oak";
        willow = willow || t.kind == "willow";
        yew = yew || t.kind == "yew";
        CHECK(std::string(GetTreeInfo(t.kind).kind) == t.kind || t.kind == "round" || t.kind == "pine" || t.kind == "tall");
    }
    CHECK(oak && willow && yew);
    bool rod = false, harpoon = false, cage = false;
    for (const SpotDef& s : m.Spots()) {
        const std::string kind = s.kind;
        CHECK(std::string(GetSpotInfo(kind).kind) == kind);
        rod = rod || kind == "rod";
        harpoon = harpoon || kind == "harpoon";
        cage = cage || kind == "cage";
    }
    CHECK(rod && harpoon && cage);

    // every monster species is known, and the map has a whole bestiary
    std::vector<std::string> species;
    for (const NpcDef& n : m.Npcs()) {
        Species sp;
        if (!SpeciesFromKey(n.species, &sp)) {
            std::printf("  FAIL unknown species '%s' in the map\n", n.species.c_str());
            ++g_failed;
        }
        if (std::find(species.begin(), species.end(), n.species) == species.end()) species.push_back(n.species);
    }
    for (const char* want : {"wolf", "dark_wolf", "giant_spider", "giant_rat", "skeleton", "zombie", "mountain_troll", "desert_mummy", "dragon",
                             "abyss_warrior", "abyss_archer", "abyss_mage", "abyss_champion"})
        if (std::find(species.begin(), species.end(), want) == species.end()) {
            std::printf("  FAIL no %s anywhere on the map\n", want);
            ++g_failed;
        }

    // the stations of the crafters' yard, and the people of the far south are all reachable
    int tanner = 0, wheel = 0, jeweller = 0;
    for (const StationDef& st : m.Stations()) {
        tanner += st.kind == "tanner";
        wheel += st.kind == "wheel";
        jeweller += st.kind == "jeweller";
    }
    CHECK(tanner >= 1 && wheel >= 1 && jeweller >= 1);
    for (const char* key : {"king", "priest", "bram", "silas", "barnaby", "ellis", "gerrant"}) {
        bool found = false;
        for (const PersonDef& d : m.Persons()) found = found || d.key == key;
        if (!found) {
            std::printf("  FAIL person %s is not on the map\n", key);
            ++g_failed;
        }
    }

    // the ferry: Barnaby takes the hero to Lakeside for 20 coins, and Silas takes him back; with no coins nothing happens
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
    Bank bank;
    RecordingEvents ev;
    TileMap real = m;
    shops.Init();
    people.Init(real);
    forest.Init(real.Trees());
    rocks.Init(real.Rocks());
    npcs.Init(real);
    Gameplay game(real, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, bank, ev, [] { return 0.0; });
    auto personId = [&](const char* key) {
        for (const Person& p : people.All())
            if (std::string(p.info->key) == key) return p.id;
        return -1;
    };
    const int barnaby = personId("barnaby"), silas = personId("silas");
    CHECK(barnaby > 0 && silas > 0);
    auto talkTo = [&](int id) {   // tap the person and let the hero walk up to them until the conversation opens
        game.TapPerson(id);
        for (int i = 0; i < 20 * 30 && !game.Dialogue().open; ++i) {
            player.Update(0.05f);
            game.Update(0.05f);
        }
    };
    auto sail = [&]() {   // a ferryman may have a quest to talk about first: read through those pages, then take the ferry (the first option)
        for (int i = 0; i < 4 && game.Dialogue().open && game.Dialogue().options.empty(); ++i) game.DialogueChoose(0);
        game.DialogueChoose(0);
    };
    auto cheb = [](TileCoord a, TileCoord b) { return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z)); };
    player.Teleport(NearestWalkable(real, people.Find(barnaby)->Tile(), 1));
    talkTo(barnaby);
    CHECK(game.Dialogue().open && game.Dialogue().options.size() == 2);
    sail();                                                             // no coins: still on the castle side
    CHECK(player.Tile().z < kOZ + 100);
    talkTo(barnaby);
    bag.Add(Item::Coins, 25);
    sail();                                                             // 20 coins: sailing
    CHECK(player.Tile().z > kOZ + 176 && cheb(player.Tile(), people.Find(silas)->Tile()) <= 3);
    CHECK(bag.Count(Item::Coins) == 5);
    CHECK(!game.Dialogue().open);
    talkTo(silas);
    sail();                                                             // only 5 coins: he cannot pay for the way back
    CHECK(player.Tile().z > kOZ + 176);
    bag.Add(Item::Coins, 15);
    talkTo(silas);
    sail();
    if (!(player.Tile().z < kOZ + 100 && bag.Count(Item::Coins) == 0))
        std::printf("  ferry back: hero at %d,%d with %d coins, dialogue open=%d\n", player.Tile().x, player.Tile().z, bag.Count(Item::Coins), (int)game.Dialogue().open);
    CHECK(player.Tile().z < kOZ + 100 && bag.Count(Item::Coins) == 0);
}


// ---------------------------------------------------------------------------------------------- round 12: the wider world
// The map is 192 x 400 now: the old strip stands in the middle and there is country on every side of it. This checks that the new places
// can be walked to from the spawn, that the people and creatures of the wings are there, and that no wing is a wasteland.
static void TestTheWiderWorld(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping wider-world test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    std::string err;
    CHECK(m.ParseText(ss.str(), &err));
    CHECK(m.Width() == kWorldW && m.Height() == kWorldH);

    // every new place: it exists, and the hero can walk to the middle of it
    for (const char* name : {"Elmswick", "Saltmere", "Northwatch Tower", "Sunfield Farm", "Bandits' Hollow", "The Standing Stones", "The Deep Mine", "The Witch's Hut",
                             "Woodcutters' Camp", "Whispering Pines", "Elmwood", "Greenmarch", "Mirewood Swamp", "Barrow Downs", "Eastwatch Downs", "The Sunfields",
                             "Darkwood", "Giant's Rise", "The Iron Range", "The Dune Sea", "The Northern Reach", "Northwatch Moor"}) {
        const AreaDef* area = nullptr;
        for (const AreaDef& a : m.Areas())
            if (a.name == name) area = &a;
        if (!area) {
            std::printf("  FAIL no area called %s\n", name);
            ++g_failed;
            continue;
        }
        const TileCoord centre{(area->rect.x0 + area->rect.x1) / 2, (area->rect.z0 + area->rect.z1) / 2};
        const TileCoord t = NearestWalkable(m, centre, 14);
        if (t.x < 0 || !FindPath(m, m.Spawn(), t).found) {
            std::printf("  FAIL cannot walk from the spawn to %s\n", name);
            ++g_failed;
        }
    }
    // the high mountains and the sea are not places to walk: the Frostpeak Range is mostly rock and the Eastern Sea is water
    {
        int rock = 0, sea = 0;
        for (int z = 0; z < 12; ++z)
            for (int x = 0; x < m.Width(); ++x) rock += m.Walkable(x, z) ? 0 : 1;
        for (int z = 60; z < 240; ++z)
            for (int x = 184; x < m.Width(); ++x) sea += m.At(x, z) == '~' ? 1 : 0;
        CHECK(rock > 12 * m.Width() / 2);
        CHECK(sea > 900);
    }
    // each side of the old strip has real country: lots of open ground, creatures, trees and rocks to work
    struct Wing { const char* name; int x0, z0, x1, z1; int minOpen, minCreatures, minTrees; };
    const Wing wings[] = {
        {"the west wing", 0, 48, 64, 384, 9000, 40, 250},
        {"the east wing", 128, 48, 192, 384, 8000, 40, 120},
        {"the far north", 0, 0, 192, 48, 3000, 12, 60},
        {"the dune sea", 128, 324, 192, 372, 1500, 8, 0},
    };
    for (const Wing& w : wings) {
        int open = 0, creatures = 0, trees = 0;
        for (int z = w.z0; z < w.z1; ++z)
            for (int x = w.x0; x < w.x1; ++x) open += m.Walkable(x, z) ? 1 : 0;
        for (const NpcDef& n : m.Npcs()) creatures += (n.x >= w.x0 && n.x < w.x1 && n.z >= w.z0 && n.z < w.z1) ? 1 : 0;
        for (const TreeDef& t : m.Trees()) trees += (t.x >= w.x0 && t.x < w.x1 && t.z >= w.z0 && t.z < w.z1) ? 1 : 0;
        if (open < w.minOpen || creatures < w.minCreatures || trees < w.minTrees) {
            std::printf("  FAIL %s is too empty: %d open tiles (want %d), %d creatures (%d), %d trees (%d)\n", w.name, open, w.minOpen, creatures, w.minCreatures, trees, w.minTrees);
            ++g_failed;
        }
    }
    // the new people stand where they should and can be talked to; the new shops and the new teleport towns have them
    for (const char* key : {"perrin", "hobb", "brine", "captain", "ada", "watchman", "maggie", "druid"}) {
        const PersonDef* found = nullptr;
        for (const PersonDef& d : m.Persons())
            if (d.key == key) found = &d;
        if (!found) {
            std::printf("  FAIL person %s is not on the map\n", key);
            ++g_failed;
            continue;
        }
        const TileCoord at = found->Tile();
        const bool west = std::string(key) == "perrin" || std::string(key) == "hobb" || std::string(key) == "maggie";
        const bool eastSide = std::string(key) == "brine" || std::string(key) == "captain" || std::string(key) == "ada" || std::string(key) == "druid";
        CHECK(!west || at.x < 64);
        CHECK(!eastSide || at.x >= 128);
    }
    // species of every kind live somewhere in the wings, and none of them stands on a blocked tile
    std::vector<std::string> wild;
    for (const NpcDef& n : m.Npcs())
        if (n.x < kOX || n.x >= kOX + 64) {
            if (std::find(wild.begin(), wild.end(), n.species) == wild.end()) wild.push_back(n.species);
            CHECK(m.Walkable(static_cast<int>(n.x), static_cast<int>(n.z)));
        }
    CHECK(wild.size() >= 18);
    // the three bridges over the river: Castle Town's side to the west wing, and two more
    int crossings = 0;
    for (int z = 60; z < 330; ++z) {
        bool open = true;
        for (int x = 55; x < 70; ++x) open = open && (m.Walkable(x, z) || m.At(x, z) == 'T' || m.At(x, z) == 'o' || m.At(x, z) == '#');
        crossings += open ? 1 : 0;
    }
    CHECK(crossings >= 9);   // three bridges, three tiles wide
    // a lap: from the spawn to a place in each wing and back is the same walk (the roads are two-way and nothing is one-way)
    for (const char* name : {"Elmswick", "Saltmere", "Sunfield Farm", "Northwatch Tower"}) {
        const AreaDef* area = nullptr;
        for (const AreaDef& a : m.Areas())
            if (a.name == name) area = &a;
        if (!area) continue;
        const TileCoord t = NearestWalkable(m, {(area->rect.x0 + area->rect.x1) / 2, (area->rect.z0 + area->rect.z1) / 2}, 14);
        const PathResult there = FindPath(m, m.Spawn(), t), back = FindPath(m, t, m.Spawn());
        CHECK(there.found && back.found && there.tiles.size() > 20);
    }
}

// Elmswick and Saltmere are towns a hero can wake up in, once he has been there.
static void TestNewTownsAreRespawnPoints() {
    std::string text = "@size 60 60\n@spawn 6 1\n@area 0 0 14 6 Market Square\n@area 40 40 54 46 Saltmere\n@area 0 50 14 56 Elmswick\n"
                       "@pen yard 40 20 54 30\n@npc cow 44.5 22.5 yard\n@rows\n";
    for (int z = 0; z < 60; ++z) text += std::string(60, '.') + "\n";
    {   // he has been to Saltmere: dying beside it wakes him there
        MiniWorld w;
        w.map.ParseText(text);
        w.npcs.Init(w.map);
        w.forest.Init(w.map.Trees());
        w.rocks.Init(w.map.Rocks());
        w.people.Init(w.map);
        w.player.Teleport({44, 24});
        w.skills.SetHp(2);
        w.rollValue = 0.0;
        w.game.MarkVisited("Saltmere");
        w.npcs.StartHunt(w.npcs.All()[0].id);
        w.Run(15.0f);
        CHECK(w.ev.deaths == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == "Saltmere" && w.ev.Said("You wake up in Saltmere"));
    }
    {   // a place he has never been to is not a town he can wake up in
        MiniWorld w;
        w.map.ParseText(text);
        w.npcs.Init(w.map);
        w.forest.Init(w.map.Trees());
        w.rocks.Init(w.map.Rocks());
        w.people.Init(w.map);
        w.player.Teleport({44, 24});
        w.skills.SetHp(2);
        w.rollValue = 0.0;
        w.npcs.StartHunt(w.npcs.All()[0].id);
        w.Run(15.0f);
        CHECK(w.ev.deaths == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == "Market Square");
    }
}

// The scrolls for the two towns of the wild country: they read like the others, are sold in the towns, and the real map has their places.
static void TestWiderScrolls(const std::string& path) {
    for (Item s : {Item::ElmswickScroll, Item::SaltmereScroll}) {
        CHECK(IsScroll(s) && GetItemInfo(s).stackable && ItemValue(s) > 0 && ItemSellable(s));
        MiniWorld w;
        w.bag.Add(s, 1);
        const std::vector<ItemAction> acts = ActionsFor(s, w.bag);
        CHECK(!acts.empty() && acts[0] == ItemAction::Read);
        bool sold = false;
        for (const ShopDef& d : AllShopDefs())
            for (const ShopStockDef& st : d.stock) sold = sold || st.item == s;
        CHECK(sold);
    }
    CHECK(!IsScroll(Item::Logs) && IsScroll(Item::CastleScroll));
    // on the real map: having stood in Elmswick, a scroll takes him there from the far end of the world (and uses itself up)
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping wider-scrolls test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string mapText = ss.str();
    for (const char* place : {"Elmswick", "Saltmere"}) {
        MiniWorld w;
        CHECK(w.map.ParseText(mapText));
        w.npcs.Init(w.map);
        w.forest.Init(w.map.Trees());
        w.rocks.Init(w.map.Rocks());
        w.people.Init(w.map);
        const Item scroll = std::string(place) == "Elmswick" ? Item::ElmswickScroll : Item::SaltmereScroll;
        w.bag.Add(scroll, 2);
        w.player.Teleport(w.map.Spawn());
        w.game.DoItemAction(w.bag.FindSlot(scroll), ItemAction::Read);
        CHECK(w.bag.Count(scroll) == 2 && w.ev.Said("never been"));   // (not yet visited)
        w.game.MarkVisited(place);
        w.game.DoItemAction(w.bag.FindSlot(scroll), ItemAction::Read);
        CHECK(w.bag.Count(scroll) == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == place && w.ev.Said("whisked"));
    }
}

// The map's quest markers: a person with a quest to give shows hollow; once he is on it, the next step shows (the creature, the person, the place).
static void TestQuestMarkers() {
    MiniWorld w;
    const int chicken = FindQuest("chicken_run");   // Aubrey's: kill 3 chickens, then bring him 3 raw chicken
    CHECK(chicken >= 0);
    auto find = [&](const std::string& quest, bool offer, QuestMarker* out) {
        for (const QuestMarker& m : w.game.QuestMarkers())
            if (m.quest == quest && m.offer == offer) {
                *out = m;
                return true;
            }
        return false;
    };
    QuestMarker m;
    CHECK(find("Chicken Run", true, &m) && std::fabs(m.x - 8.5f) < 0.01f && std::fabs(m.z - 3.5f) < 0.01f);   // Aubrey stands at (8.5, 3.5)
    CHECK(!find("Chicken Run", false, &m));
    w.game.Quests().Start(chicken);                                                                              // on it: the hunt is the marker, and the offer is gone
    CHECK(!find("Chicken Run", true, &m) && find("Chicken Run", false, &m) && m.what.find("chickens") != std::string::npos);
    CHECK(m.x > 10.0f && m.x < 14.0f);                                                                            // the chickens are in the pen on the right
    for (int i = 0; i < 3; ++i) w.game.Quests().OnKill(Species::Chicken, AttackKind::Melee);
    CHECK(find("Chicken Run", false, &m) && std::fabs(m.x - 8.5f) < 0.01f);                                       // bring the meat to Aubrey
    w.game.Quests().Finish(chicken);
    CHECK(!find("Chicken Run", false, &m) && !find("Chicken Run", true, &m));                                     // finished: nothing to show
}

// On the real map every step of every quest has a place to show: its person, a creature of its kind, or its named place.
static void TestQuestMarkersOnTheRealMap(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping quest-marker test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    MiniWorld w;
    CHECK(w.map.ParseText(ss.str()));
    w.npcs.Init(w.map);
    w.forest.Init(w.map.Trees());
    w.rocks.Init(w.map.Rocks());
    w.people.Init(w.map);
    w.player.Teleport(w.map.Spawn());
    const std::vector<QuestInfo>& all = AllQuests();
    for (size_t qi = 0; qi < all.size(); ++qi) {
        // every quest is offered by someone standing on the map...
        bool offered = false;
        for (const Person& p : w.people.All()) offered = offered || (p.info && all[qi].giver == p.info->key);
        CHECK(offered);
        for (size_t si = 0; si < all[qi].steps.size(); ++si) {   // ...and every step of it points somewhere
            w.game.Quests() = QuestLog();
            w.game.Quests().ParseLine(all[qi].key, std::to_string(si) + ":0:0");
            const std::vector<QuestMarker> marks = w.game.QuestMarkers();
            bool got = false;
            for (const QuestMarker& m : marks) got = got || (m.quest == all[qi].name && !m.offer);
            if (!got) {
                std::printf("  FAIL quest \"%s\" step %d has no place on the map\n", all[qi].key.c_str(), static_cast<int>(si) + 1);
                ++g_failed;
            }
        }
    }
}

// The wider world's new shops: each has stock, a keeper, and sells what its name says.
static void TestWiderWorldShops() {
    for (const char* key : {"elm_general", "elm_woodcraft", "salt_market", "sunfield_farm", "swamp_witch"}) {
        const int i = ShopIndex(key);
        CHECK(i >= 0);
        if (i < 0) continue;
        const ShopDef& d = AllShopDefs()[i];
        CHECK(d.stock.size() >= 8 && d.stock.size() <= static_cast<size_t>(kMaxShopLines));
        bool keeper = false;
        for (const PersonInfo& p : AllPersonInfos()) keeper = keeper || std::string(p.shop) == key;
        CHECK(keeper);
    }
    const ShopDef& fish = AllShopDefs()[ShopIndex("salt_market")];
    bool harpoon = false, cod = false;
    for (const ShopStockDef& s : fish.stock) {
        harpoon = harpoon || s.item == Item::Harpoon;
        cod = cod || s.item == Item::RawCod;
    }
    CHECK(harpoon && cod);
}


// ---------------------------------------------------------------------------------------------- round 13: the other adventurers
// A world to run the adventurers in: the real map, with its trees, rocks, creatures and people, and nobody else.
struct AdvRig {
    TileMap map;
    Forest forest;
    Rocks rocks;
    Fires fires;
    Npcs npcs;
    People people;
    Adventurers advs;
    AdvWorld world;
    unsigned rng = 77;
    bool ok = false;
    bool Load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::stringstream ss;
        ss << f.rdbuf();
        if (!map.ParseText(ss.str())) return false;
        forest.Init(map.Trees());
        rocks.Init(map.Rocks());
        npcs.Init(map);
        people.Init(map);
        world.map = &map;
        world.forest = &forest;
        world.rocks = &rocks;
        world.fires = &fires;
        world.npcs = &npcs;
        world.people = &people;
        world.roll = [this]() {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            return (rng & 0xFFFFFF) / 16777216.0;
        };
        world.hero = {-500, -500};
        advs.Init(world);
        ok = true;
        return true;
    }
    void Run(float seconds, float step = 0.2f) {
        for (float t = 0; t < seconds; t += step) {
            forest.Update(step);
            rocks.Update(step);
            fires.Update(step, &map);
            npcs.Update(step, map, {-500, -500}, world.roll);
            advs.Update(step, world);
        }
    }
};

static void TestAdventurersStartLikeTheHero(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping adventurers test: %s not found)\n", path.c_str());
        return;
    }
    int real = 0, fillers = 0;
    for (const Adventurer& a : r.advs.All()) {
        if (a.kind == AdvKind::Real) {
            ++real;
            // exactly the hero's start: every skill 1, hit points 10, the starter kit and nothing else
            for (int s = 0; s < kSkillCount; ++s) {
                const int want = static_cast<Skill>(s) == Skill::Hitpoints ? kStartHitpoints : 1;
                CHECK(a.skills.Level(static_cast<Skill>(s)) == want);
            }
            CHECK(a.skills.Hp() == 10 && a.equip.Empty() && a.bag.Has(Item::BronzeAxe) && a.bag.Has(Item::SmallNet) && a.bag.Has(Item::Hammer));
            CHECK(!a.bag.Has(Item::Coins));
            CHECK(r.map.Walkable(a.Tile()));
        } else {
            ++fillers;
        }
        CHECK(!a.name.empty());
    }
    CHECK(real == kAdvRealCount && fillers >= 50);
    std::string why;
    CHECK(r.advs.Honest(&why));
    // names are unique, and the fillers are spread over the levels (never everyone alike)
    std::vector<std::string> names;
    int minLevel = 99, maxLevel = 0;
    std::vector<int> combat;
    for (const Adventurer& a : r.advs.All()) {
        CHECK(std::find(names.begin(), names.end(), a.name) == names.end());
        names.push_back(a.name);
        if (a.kind == AdvKind::Filler) {
            minLevel = std::min(minLevel, a.level);
            maxLevel = std::max(maxLevel, a.level);
            combat.push_back(a.Combat());
        }
    }
    CHECK(minLevel <= 8 && maxLevel >= 85);
    std::sort(combat.begin(), combat.end());
    combat.erase(std::unique(combat.begin(), combat.end()), combat.end());
    CHECK(combat.size() >= 25);
}

static void TestFillersNeverChange(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    struct Snap {
        std::string name;
        int total;
        int combat;
        std::vector<int> worn;
        bool dead = false;   // (an Abyss filler killed in a fight is away for a while with nothing on: that is not a change of gear)
    };
    auto snap = [&]() {
        std::vector<Snap> v;
        for (const Adventurer& a : r.advs.All())
            if (a.kind == AdvKind::Filler) {
                Snap s{a.name, a.skills.TotalLevel(), a.Combat(), {}};
                for (int i = 0; i < kEquipSlotCount; ++i) s.worn.push_back(static_cast<int>(a.equip.Get(static_cast<EquipSlot>(i))));
                s.dead = a.pvp.dead;
                v.push_back(s);
            }
        return v;
    };
    const std::vector<Snap> before = snap();
    std::vector<TileCoord> start;
    for (const Adventurer& a : r.advs.All()) start.push_back(a.Tile());
    r.Run(600.0f);
    const std::vector<Snap> after = snap();
    CHECK(before.size() == after.size());
    for (size_t i = 0; i < before.size() && i < after.size(); ++i)
        CHECK(before[i].total == after[i].total && before[i].combat == after[i].combat && (after[i].dead || before[i].worn == after[i].worn));
    // they do move about, and do their trades
    int moved = 0;
    size_t k = 0;
    int trades = 0;
    for (const Adventurer& a : r.advs.All()) {
        if (a.kind == AdvKind::Filler && (a.Tile().x != start[k].x || a.Tile().z != start[k].z)) ++moved;
        if (a.kind == AdvKind::Filler && (a.act == AdvAct::Chop || a.act == AdvAct::Fish || a.act == AdvAct::Mine || a.act == AdvAct::Smith || a.act == AdvAct::Fight)) ++trades;
        ++k;
    }
    CHECK(moved >= 30);
    CHECK(trades >= 8);
}

static void TestRealAdventurersPlayTheGame(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    const float hours = 1.5f;
    r.Run(hours * 3600.0f, 0.25f);
    int train = 0, quests = 0, upgraded = 0, coins = 0, deaths = 0;
    std::vector<std::string> doing;
    for (const Adventurer& a : r.advs.All()) {
        if (a.kind != AdvKind::Real) continue;
        const int total = a.skills.TotalLevel();
        std::printf("  %-8s total %3d combat %2d quests %d coins %5d deaths %d  now: %s\n", a.name.c_str(), total, a.Combat(), a.quests.Points(), a.bag.Count(Item::Coins),
                    a.mind.deaths, a.mind.doing.c_str());
        train += total > kSkillCount + 8 ? 1 : 0;
        quests += a.quests.Points();
        coins += a.bag.Count(Item::Coins);
        deaths += a.mind.deaths;
        for (int s = 0; s < kEquipSlotCount; ++s) upgraded += a.equip.Get(static_cast<EquipSlot>(s)) != Item::None ? 1 : 0;
    }
    std::printf("  after %.1f hours: %d of %d trained, %d quests, %d worn pieces, %d coins, %d deaths\n", hours, train, kAdvRealCount, quests, upgraded, coins, deaths);
    CHECK(train >= kAdvRealCount - 1);          // everyone has learned something
    std::string why;
    const bool honest = r.advs.Honest(&why);
    if (!honest) std::printf("  FAIL %s\n", why.c_str());
    CHECK(honest);                              // and everything they own came by skilling, drops, quests or the shops
    CHECK(deaths < kAdvRealCount * 6);          // they do not suicide all day
}

static void TestAdventurersSaveAndLoad(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    r.Run(900.0f);
    const std::string text = r.advs.Save();
    AdvRig r2;
    r2.Load(path);
    CHECK(r2.advs.Load(r2.world, text));
    for (const Adventurer& a : r.advs.All()) {
        if (a.kind != AdvKind::Real) continue;
        const Adventurer* b = nullptr;
        for (const Adventurer& x : r2.advs.All())
            if (x.name == a.name) b = &x;
        CHECK(b != nullptr);
        if (!b) continue;
        CHECK(b->skills.TotalXp() == a.skills.TotalXp() && b->quests.Points() == a.quests.Points());
        for (int s = 0; s < kEquipSlotCount; ++s) CHECK(b->equip.Get(static_cast<EquipSlot>(s)) == a.equip.Get(static_cast<EquipSlot>(s)));
        CHECK(b->bag.Count(Item::Coins) == a.bag.Count(Item::Coins));
    }
    std::string why;
    CHECK(r2.advs.Honest(&why));
    CHECK(!r2.advs.Load(r2.world, "not a save"));   // (and a bad file just gives fresh ones)
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
            if (static_cast<GearType>(g) == GearType::Arrows) continue;   // (arrows are not worn gear: see Archery.h)
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

    // everything can be dropped and sold, starter tools and pickaxes included
    CHECK(ItemSellable(Item::Hammer) && ItemSellable(Item::BronzeAxe) && ItemSellable(Item::BronzePickaxe) && !ItemSellable(Item::Coins));
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
    CHECK(!RockKindFromKey("unobtainium", &dummy));
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
    CHECK(m.ParseText("@size 4 1\n@rock 1.5 0.5 copper 90\n@rock 2.5 0.5 unobtainium 0\n@rock nonsense\n@station anvil 3.5 0.5 45\n@station bad\n@rows\n.oo.\n"));
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
    CHECK(w.ev.makeMenus == 1 && w.ev.lastMake.kind == MakeMenu::Kind::Smelt && w.ev.lastMake.options.size() == static_cast<size_t>(kSmeltCount));
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
    CHECK(w.ev.Said("retrieve a bronze bar"));

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
        m.skills.SetXp(Skill::Strength, XpForLevel(30));   // (with Strength 1 nothing hits for more than 1 or 2, and a slow weapon only loses time)
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
    CHECK(!ItemSellable(Item::Coins) && ItemSellable(Item::Hammer) && !ItemSellable(Item::BurntFish) && ItemSellable(Item::Logs));
    for (int t = 1; t < kTierCount; ++t) {                                    // better metal, higher price; bigger piece, higher price
        CHECK(ItemValue(GearItem(static_cast<Tier>(t), GearType::Sword)) > ItemValue(GearItem(static_cast<Tier>(t - 1), GearType::Sword)));
        CHECK(ItemValue(BarFor(static_cast<Tier>(t))) > ItemValue(BarFor(static_cast<Tier>(t - 1))));
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
            if (shops.Buys(sh, l.item)) {   // no free money by buying and selling back (a 1-coin item sells for its 1 coin: nothing to gain)
                CHECK(shops.SellPrice(sh, l.item) <= shops.BuyPrice(sh, l.item));
                if (shops.BuyPrice(sh, l.item) >= 3) CHECK(shops.SellPrice(sh, l.item) < shops.BuyPrice(sh, l.item));
            }
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
        if (p.shop[0] && std::string(p.shop) != "bank") CHECK(ShopIndex(p.shop) >= 0);
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
    CHECK(w.game.Dialogue().open && w.ev.shopMenus == 0);   // she has a quest to offer first...
    w.game.DialogueChoose(1);                               // ...not now: and the shop opens
    CHECK(w.ev.shopMenus == 1 && w.ev.Said("Aubrey:"));
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
    sell.game.Sell(shop, Item::BronzeAxe, 1);      // the starting axe sells like anything else
    CHECK(!sell.bag.Has(Item::BronzeAxe) && sell.bag.Count(Item::Coins) > 0);
    sell.bag.Remove(Item::Coins, sell.bag.Count(Item::Coins));
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
        hammer = hammer || l.item == Item::BurntFish || l.item == Item::Coins;
    }
    CHECK(ore && !hammer && sm.coins == 4 * sp);

    // talking to a villager just talks, and cycles their lines
    MiniWorld talk;
    talk.game.Quests().Finish(FindQuest("ore_for_the_smith"));   // (no quest to offer: an ordinary chat)
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
    // loot: bones and coins every time (coins within range), now and then a weapon, runes or arrows
    unsigned lseed = 7;
    auto lr = [&lseed] {
        lseed = lseed * 1664525u + 1013904223u;
        return (lseed >> 8) / 16777216.0;
    };
    int withDagger = 0, withRunes = 0, withArrows = 0;
    double coinsTotal = 0;
    const int kills = 4000;
    for (int i = 0; i < kills; ++i) {
        const std::vector<ItemStack> loot = RollLoot(gs, lr);
        bool bones = false;
        int extras = 0;
        for (const ItemStack& s : loot) {
            bones = bones || s.item == Item::Bones;
            if (s.item == Item::Coins) {
                CHECK(s.count >= 3 && s.count <= 14);
                coinsTotal += s.count;
            } else if (s.item != Item::Bones) {
                ++extras;   // everything else comes from the ONE weighted pick
            }
            withDagger += s.item == GearItem(Tier::Bronze, GearType::Dagger);
            withRunes += s.item == Item::AirRune || s.item == Item::MindRune;
            withArrows += s.item == Item::BronzeArrows;
        }
        CHECK(bones && extras <= 1);
    }
    CHECK(coinsTotal / kills > 7.5 && coinsTotal / kills < 9.5);
    CHECK(withDagger > kills * 0.03 && withDagger < kills * 0.10 && withRunes > kills * 0.25 && withRunes < kills * 0.45 && withArrows > kills * 0.06);
    CHECK(std::fabs(DropChance(*FindLootTable("goblin"), GearItem(Tier::Bronze, GearType::Dagger)) - 0.06) < 1e-9 &&
          DropChance(*FindLootTable("goblin"), Item::Coins) == 1.0 && DropChance(*FindLootTable("goblin"), Item::MithrilBar) == 0.0);
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

// ---------------------------------------------------------------------------------------------- monsters

static void TestMonsterData() {
    // every species has a loot table, a sensible size and (for people-shaped ones) things to wear that really are wearable
    for (int i = 0; i < kSpeciesCount; ++i) {
        const SpeciesInfo& sp = GetSpecies(static_cast<Species>(i));
        const LootTable* t = FindLootTable(sp.lootKey);
        CHECK(t && !t->drops.empty() && sp.height > 0.3f && sp.scale > 0.0f);
        if (sp.attackKind != AttackKind::Melee) CHECK(sp.attackRange > 1);
        if (sp.rig == Rig::Human) {
            for (Item it : {sp.weapon, sp.shield, sp.helm, sp.body, sp.legs}) {
                EquipSlot slot;
                CHECK(it == Item::None || Equippable(it, &slot));
            }
            if (IsBow(sp.weapon)) CHECK(sp.attackKind == AttackKind::Ranged);
            if (IsStaff(sp.weapon)) CHECK(sp.attackKind == AttackKind::Magic);
        }
    }
    // a ladder of danger: stronger monsters have more hit points and hit harder
    Species s;
    CHECK(SpeciesFromKey("wolf", &s) && SpeciesFromKey("giant_spider", &s) && SpeciesFromKey("dragon", &s) && SpeciesFromKey("abyss_mage", &s));
    CHECK(GetSpecies(Species::Wolf).level < GetSpecies(Species::Skeleton).level && GetSpecies(Species::Skeleton).level < GetSpecies(Species::MountainTroll).level &&
          GetSpecies(Species::MountainTroll).level < GetSpecies(Species::Dragon).level);
    CHECK(GetSpecies(Species::Dragon).hitpoints > 200 && GetSpecies(Species::Dragon).attackKind == AttackKind::Magic);
    CHECK(GetSpecies(Species::GiantSpider).poison > 0 && GetSpecies(Species::Wolf).pack > 0 && GetSpecies(Species::DesertMummy).attackKind == AttackKind::Ranged);

    // loot tables: Always drops always come, then ONE weighted pick (or nothing)
    LootTable t;
    t.key = "test";
    t.nothing = 50;
    t.drops = {{LootTier::Always, Item::Bones, 1, 1, 0}, {LootTier::Common, Item::Coins, 5, 5, 30}, {LootTier::Rare, Item::MithrilBar, 1, 1, 20}};
    auto rollWith = [&t](double pick) {
        int calls = 0;
        return RollLootTable(t, [&] { return calls++ < 1 ? 0.0 : pick; });   // (the first roll is the Always count)
    };
    std::vector<ItemStack> l = rollWith(0.1);   // 0.1 * 100 = 10 -> the coins (weight 30)
    CHECK(l.size() == 2 && l[0].item == Item::Bones && l[1].item == Item::Coins && l[1].count == 5);
    l = rollWith(0.35);                         // 35 -> past the coins (30), inside the mithril bar (30..50)
    CHECK(l.size() == 2 && l[1].item == Item::MithrilBar);
    l = rollWith(0.9);                          // 90 -> the "nothing" share
    CHECK(l.size() == 1 && l[0].item == Item::Bones);
    CHECK(std::fabs(DropChance(t, Item::Coins) - 0.30) < 1e-9 && std::fabs(DropChance(t, Item::MithrilBar) - 0.20) < 1e-9 && DropChance(t, Item::Bones) == 1.0);
    CHECK(FindLootTable("nope") == nullptr && std::string(LootTierName(LootTier::Rare)) == "Rare");
    // the bigger the monster, the better the loot: the dragon always drops dragon bones and a pile of coins
    const LootTable* dragon = FindLootTable("dragon");
    CHECK(dragon && DropChance(*dragon, Item::DragonBones) == 1.0 && DropChance(*dragon, Item::Coins) == 1.0 && GetItemInfo(Item::DragonBones).buryXp == 72);
    // simulated kills match the table (the GDD's "10,000 kills match the drop rates" gate, in miniature)
    unsigned seed = 5;
    auto r = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) / 16777216.0;
    };
    const LootTable* skel = FindLootTable("skeleton");
    int sword = 0;
    const int n = 20000;
    for (int i = 0; i < n; ++i)
        for (const ItemStack& s2 : RollLootTable(*skel, r)) sword += s2.item == GearItem(Tier::Iron, GearType::Sword);
    const double expect = DropChance(*skel, GearItem(Tier::Iron, GearType::Sword));
    CHECK(expect > 0.05 && std::fabs(static_cast<double>(sword) / n - expect) < 0.01);
}

// A strip of open ground with a pen the whole length, for monster tests.
static void MonsterStrip(MiniWorld* w, const char* npcs, int width = 20, int penFrom = 0, int penTo = -1) {
    std::string text = "@size " + std::to_string(width) + " 3\n@spawn 1 1\n@pen lair " + std::to_string(penFrom) + " 0 " +
                       std::to_string(penTo < 0 ? width : penTo) + " 3\n" + npcs + "@rows\n";
    for (int z = 0; z < 3; ++z) text += std::string(static_cast<size_t>(width), '.') + "\n";
    w->map.ParseText(text);
    w->npcs.Init(w->map);
    w->forest.Init(w->map.Trees());
    w->rocks.Init(w->map.Rocks());
    w->people.Init(w->map);
}

static void TestGameplayMonsters() {
    // ---- a spider's bite poisons: damage over time that weakens and wears off
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc giant_spider 5.5 1.5 lair\n");
        w.skills.SetXp(Skill::Hitpoints, XpForLevel(60));
        w.skills.SetHp(w.skills.MaxHp());
        w.player.Teleport({3, 1});
        w.npcs.SetHero(w.skills.CombatLevel(), true);
        const int full = w.skills.Hp();
        w.Run(12.0f);
        CHECK(w.ev.Said("You have been poisoned") && w.game.Poisoned());
        const int hurt = full - w.skills.Hp();
        CHECK(hurt >= 3);
        w.game.Cancel();                 // he walks away and the spider forgets him: nothing bites again, the poison runs its course
        w.npcs.StopAllHunts();
        w.player.Teleport({19, 1});
        w.Run(60.0f);
        CHECK(!w.game.Poisoned() && w.ev.Said("poison has worn off"));
    }
    // ---- a pack: attack one wolf and the wolves near it join in; a far one does not
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc wolf 6.5 1.5 lair\n@npc wolf 9.5 1.5 lair\n@npc wolf 19.5 1.5 lair\n");
        w.npcs.StartHunt(w.npcs.All()[0].id);
        CHECK(w.npcs.All()[0].hunting && w.npcs.All()[1].hunting && !w.npcs.All()[2].hunting);
    }
    // ---- an aggressive wolf bites a weak hero, and leaves a strong one alone (but never one far away)
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc wolf 6.5 1.5 lair\n", 20, 6, 8);   // a small den: it cannot wander into range
        w.player.Teleport({1, 1});
        w.npcs.SetHero(3, true);
        w.Run(4.0f);
        CHECK(!w.npcs.All()[0].hunting);   // 5 tiles away: out of its range
        w.player.Teleport({4, 1});
        w.Run(3.0f);
        CHECK(w.npcs.All()[0].hunting);
        MiniWorld s;
        MonsterStrip(&s, "@npc wolf 6.5 1.5 lair\n", 20, 6, 8);
        s.player.Teleport({4, 1});
        s.npcs.SetHero(60, true);   // far stronger than a level 11 wolf: it ignores him
        s.Run(4.0f);
        CHECK(!s.npcs.All()[0].hunting);
    }
    // ---- an archer shoots from a distance and the arrow hits the hero
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc abyss_archer 10.5 1.5 lair\n");
        w.skills.SetXp(Skill::Hitpoints, XpForLevel(60));
        w.skills.SetHp(w.skills.MaxHp());
        w.player.Teleport({5, 1});
        w.npcs.SetHero(w.skills.CombatLevel(), true);
        const int full = w.skills.Hp();
        w.Run(10.0f);
        bool shotAtHero = false;
        for (const ShotFx& fx : w.ev.shots) shotAtHero = shotAtHero || (fx.toHero && !fx.magic && fx.npcId == w.npcs.All()[0].id);
        CHECK(shotAtHero && w.skills.Hp() < full);
        CHECK(w.ev.hitsOnPlayer.size() >= 1);
    }
    // ---- a wall between them: nothing flies, nothing hits
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc abyss_mage 10.5 1.5 lair\n");
        for (int z = 0; z < 3; ++z) w.map.ParseText("@size 1 1\n@rows\n.\n");   // (placeholder: rebuilt below)
        w.map.ParseText("@size 20 3\n@spawn 1 1\n@pen lair 0 0 20 3\n@npc abyss_mage 10.5 1.5 lair\n@rows\n"
                        ".......#............\n.......#............\n.......#............\n");
        w.npcs.Init(w.map);
        w.people.Init(w.map);
        w.player.Teleport({5, 1});
        w.npcs.SetHero(w.skills.CombatLevel(), true);
        w.Run(8.0f);
        CHECK(w.ev.shots.empty() && w.skills.Hp() == w.skills.MaxHp());
    }
    // ---- a magic attacker casts a coloured spell
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc dragon 10.5 1.5 lair\n");
        w.skills.SetXp(Skill::Hitpoints, XpForLevel(80));
        w.skills.SetHp(w.skills.MaxHp());
        w.player.Teleport({5, 1});
        w.npcs.SetHero(w.skills.CombatLevel(), true);
        w.Run(8.0f);
        bool fire = false;
        for (const ShotFx& fx : w.ev.shots) fire = fire || (fx.toHero && fx.magic && fx.color.r > 200 && fx.color.b < 100);
        CHECK(fire);
    }
    // ---- big kills: the dragon's bones and coins land in a pile when it dies
    {
        MiniWorld w;
        MonsterStrip(&w, "@npc skeleton 10.5 1.5 lair\n");
        w.npcs.Hurt(w.npcs.All()[0].id, 1000);
        w.Run(Npcs::kDeathAnimSeconds + 1.0f);
        bool bones = false, coins = false;
        for (const Pile& p : w.ground.All())
            for (const ItemStack& s : p.items) {
                bones = bones || s.item == Item::Bones;
                coins = coins || s.item == Item::Coins;
            }
        CHECK(bones && coins);
    }
}

// ---------------------------------------------------------------------------------------------- quests and dialogue

static void TestQuestData() {
    const std::vector<QuestInfo>& all = AllQuests();
    CHECK(all.size() >= 12);
    for (size_t i = 0; i < all.size(); ++i) {
        const QuestInfo& q = all[i];
        CHECK(!q.key.empty() && !q.name.empty() && !q.intro.empty() && !q.steps.empty() && FindQuest(q.key) == static_cast<int>(i));
        CHECK(FindPersonInfo(q.giver) != nullptr);                       // the giver is somebody who exists
        CHECK(q.after.empty() || FindQuest(q.after) >= 0);
        CHECK(q.reward.coins > 0 || !q.reward.items.empty() || !q.reward.xp.empty());
        const StepKind last = q.steps.back().kind;
        CHECK(last == StepKind::Have || last == StepKind::Talk);        // a quest ends with somebody thanking you
        for (const QuestStep& st : q.steps) {
            CHECK(!st.objective.empty());
            if (st.kind == StepKind::Have || st.kind == StepKind::Talk) CHECK(FindPersonInfo(st.person) != nullptr && !st.say.empty());
            if (st.kind == StepKind::Have) CHECK(st.item != Item::None && st.count >= 1);
            if (st.kind == StepKind::Kill) CHECK(st.count >= 1);
            if (st.kind == StepKind::Reach) CHECK(!st.area.empty());
        }
        for (const QuestInfo& other : all) CHECK(&other == &q || other.key != q.key);   // unique keys
    }
    CHECK(FindQuest("nope") == -1);
}

static void TestQuestLog() {
    QuestLog log;
    Skills sk;
    Inventory bag;
    const int goblin = FindQuest("goblin_trouble"), chief = FindQuest("goblin_chief"), chicken = FindQuest("chicken_run");
    CHECK(goblin >= 0 && chief >= 0 && chicken >= 0 && log.Points() == 0 && log.Count() == static_cast<int>(AllQuests().size()));
    CHECK(log.CanStart(chicken, sk) && log.CanStart(goblin, sk));
    CHECK(!log.CanStart(chief, sk) && log.Missing(chief, sk).find("Goblin Trouble") != std::string::npos);   // needs the first quest
    CHECK(log.Missing(FindQuest("the_dragon"), sk).find("combat level 40") != std::string::npos);
    CHECK(log.Missing(FindQuest("a_sharp_order"), sk).find("Smithing level") != std::string::npos);
    CHECK(log.Missing(99, sk) != "" && !log.CanStart(-1, sk));

    log.Start(chicken);
    CHECK(!log.CanStart(chicken, sk) && log.Current(chicken) && log.Current(chicken)->kind == StepKind::Kill && log.Current(goblin) == nullptr);
    CHECK(log.OnKill(Species::Cow, AttackKind::Melee) == -1);                      // the wrong creature
    CHECK(log.OnKill(Species::Chicken, AttackKind::Melee) == -1 && log.State(chicken).count == 1);
    CHECK(log.ObjectiveText(chicken, bag) == "Kill 3 chickens (1/3)");
    CHECK(log.OnKill(Species::Chicken, AttackKind::Ranged) == -1 && log.OnKill(Species::Chicken, AttackKind::Magic) == chicken);   // the third kill finishes the step
    CHECK(log.Current(chicken)->kind == StepKind::Have && log.State(chicken).count == 0 && log.State(chicken).step == 1);
    CHECK(!log.StepReady(chicken, "aubrey", bag));                                 // no chicken to hand over
    bag.Add(Item::RawChicken, 3);
    CHECK(!log.StepReady(chicken, "wilf", bag) && log.StepReady(chicken, "aubrey", bag));
    CHECK(log.ObjectiveText(chicken, bag).find("you have 3") != std::string::npos);
    CHECK(log.CompleteStep(chicken, &bag) && !bag.Has(Item::RawChicken));          // the last step: the meat is taken
    log.Finish(chicken);
    CHECK(log.State(chicken).done && log.Points() == 1 && log.Current(chicken) == nullptr && !log.CanStart(chicken, sk));
    CHECK(log.OnKill(Species::Chicken, AttackKind::Melee) == -1);                  // done quests do not count kills

    // kills that need a weapon: only a bow counts
    const int bow = FindQuest("bowyers_trial");
    log.Start(bow);
    CHECK(log.OnKill(Species::Goblin, AttackKind::Melee) == -1 && log.State(bow).count == 0);
    for (int i = 0; i < 4; ++i) log.OnKill(Species::Goblin, AttackKind::Ranged);
    CHECK(log.State(bow).step == 1 && log.Current(bow)->kind == StepKind::Talk && log.StepReady(bow, "larson", bag));
    // reaching a place
    const int crypt = FindQuest("bones_of_the_crypt");
    log.Start(crypt);
    CHECK(log.OnReach("Castle Crypt") == crypt && log.State(crypt).step == 1 && log.OnReach("Castle Crypt") == -1);
    CHECK(log.OnReach("Somewhere else") == -1);
    // quest points open doors
    QuestLog many;
    for (const char* k : {"welcome", "chicken_run", "hildas_order", "lumberjacks_lament", "fishermans_friend", "ore_for_the_smith"}) many.Finish(FindQuest(k));
    Skills strong;
    for (int i = 0; i < kSkillCount; ++i) strong.SetXp(static_cast<Skill>(i), XpForLevel(50));
    CHECK(many.Points() == 6 && many.CanStart(FindQuest("the_dragon"), strong));

    // saved and loaded
    const std::string text = log.Serialize();
    QuestLog back;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (line.rfind("quest.", 0) == 0 && eq != std::string::npos) back.ParseLine(line.substr(6, eq - 6), line.substr(eq + 1));
    }
    CHECK(back.State(chicken).done && back.State(bow).step == 1 && back.State(crypt).step == 1 && back.State(goblin).started == false && back.Points() == 1);
    back.ParseLine("nonsense", "1:2:3");
    back.ParseLine("welcome", "garbage");
    back.ParseLine("welcome", "99:-5:0");   // out-of-range numbers are clamped
    CHECK(back.State(FindQuest("welcome")).step == static_cast<int>(AllQuests()[static_cast<size_t>(FindQuest("welcome"))].steps.size()) &&
          back.State(FindQuest("welcome")).count == 0);
    // through the progress file
    Skills s;
    Inventory b;
    Equipment e;
    const std::string file = SerializeProgress(s, b, CombatStyle::Accurate, &e, nullptr, 0, &log);
    QuestLog loaded;
    ParseProgress(file, &s, &b, nullptr, &e, nullptr, nullptr, &loaded);
    CHECK(loaded.State(chicken).done && loaded.State(bow).started && loaded.Points() == 1);
}

static void TestGameplayDialogue() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    const int chicken = FindQuest("chicken_run");
    // aubrey stands behind her counter (tile 8,3). Talking to her makes an offer, which stays on the screen until it is answered.
    w.game.TapPerson(1);
    w.Run(4.0f);
    DialogueView d = w.game.Dialogue();
    CHECK(d.open && d.speaker == "Aubrey" && d.options.size() == 2 && d.text.find("poultry") != std::string::npos);
    CHECK(w.ev.shopMenus == 0 && w.ev.Said("Aubrey:"));                          // her shop waits until the talking is over
    w.game.DialogueChoose(-1);                                                   // a choice must be made: nothing happens
    CHECK(w.game.Dialogue().open && !w.game.Quests().State(chicken).started);
    w.game.DialogueChoose(0);                                                    // accept
    CHECK(w.game.Quests().State(chicken).started && w.ev.Said("Quest started: Chicken Run"));
    d = w.game.Dialogue();
    CHECK(d.open && d.options.empty() && d.text.find("Kill 3 chickens") != std::string::npos);
    w.game.DialogueChoose(-1);                                                   // continue: the conversation ends and the shop opens
    CHECK(!w.game.Dialogue().open && w.ev.shopMenus == 1);

    // talking again, with the quest unfinished: a reminder, then the shop
    w.game.TapPerson(1);
    w.Run(4.0f);
    d = w.game.Dialogue();
    CHECK(d.open && d.text.find("Kill 3 chickens (0/3)") != std::string::npos);
    w.game.DialogueChoose(-1);
    CHECK(w.ev.shopMenus == 2);

    // a real kill counts (rolls of 0.0: every strike lands)
    w.game.Quests().OnKill(Species::Chicken, AttackKind::Melee);
    w.game.TapNpc(w.npcs.All()[0].id);
    w.Run(12.0f);
    CHECK(w.npcs.All()[0].dead && w.game.Quests().State(chicken).count == 2);
    w.game.Quests().OnKill(Species::Chicken, AttackKind::Melee);                // the third
    CHECK(w.game.Quests().Current(chicken)->kind == StepKind::Have);
    // hand in: the meat is taken, the reward given, the quest is done
    w.bag.Add(Item::RawChicken, 3);
    const int coins = w.bag.Count(Item::Coins);
    const int64_t xp = w.skills.Xp(Skill::Attack);
    w.game.TapPerson(1);
    w.Run(4.0f);
    d = w.game.Dialogue();
    CHECK(d.open && d.text.find("fresh") != std::string::npos);
    CHECK(w.game.Quests().State(chicken).done && !w.bag.Has(Item::RawChicken) && w.bag.Count(Item::Coins) == coins + 60 && w.skills.Xp(Skill::Attack) >= xp + 120);
    CHECK(w.ev.Said("Quest complete: Chicken Run") && w.game.Quests().Points() == 1);
    w.game.DialogueChoose(-1);
    d = w.game.Dialogue();
    CHECK(d.open && d.speaker.empty() && d.text.find("Quest complete") != std::string::npos && d.text.find("60 coins") != std::string::npos);
    w.game.DialogueChoose(-1);
    d = w.game.Dialogue();
    CHECK(d.open && d.speaker == "Aubrey" && d.text.find("sell more") != std::string::npos);
    w.game.DialogueChoose(-1);
    CHECK(!w.game.Dialogue().open && w.ev.shopMenus == 3);
    // nothing more to say: an ordinary chat in the chat box, and the shop
    w.game.TapPerson(1);
    w.Run(4.0f);
    CHECK(!w.game.Dialogue().open && w.ev.shopMenus == 4);

    // declining an offer ends it (and the quest stays available)
    MiniWorld g;
    g.map.ParseText("@size 8 3\n@spawn 1 1\n@person guard_village 5.5 1.5 90\n@rows\n........\n.....o..\n........\n");
    g.npcs.Init(g.map);
    g.forest.Init(g.map.Trees());
    g.rocks.Init(g.map.Rocks());
    g.people.Init(g.map);
    g.player.Teleport({1, 1});
    g.game.TapPerson(1);
    g.Run(4.0f);
    d = g.game.Dialogue();
    CHECK(d.open && d.options.size() == 2 && d.text.find("goblins") != std::string::npos);
    g.game.DialogueChoose(1);
    CHECK(!g.game.Dialogue().open && !g.game.Quests().State(FindQuest("goblin_trouble")).started);
    // the follow-up quest is not offered until the first is done, and he says why
    g.game.TapPerson(1);
    g.game.DialogueChoose(0);   // accept goblin trouble
    g.game.DialogueChoose(-1);
    CHECK(!g.game.Dialogue().open && g.game.Quests().State(FindQuest("goblin_trouble")).started);
    g.game.TapPerson(1);
    d = g.game.Dialogue();
    CHECK(d.open && d.text.find("Kill 6 goblins") != std::string::npos);        // a reminder of the one in progress
    // walking away closes the conversation
    g.game.DialogueClose();
    CHECK(!g.game.Dialogue().open);
    // a steps that needs a place: walking in counts
    MiniWorld a;
    a.map.ParseText("@size 8 3\n@spawn 1 1\n@area 5 0 8 3 Castle Crypt\n@rows\n........\n........\n........\n");
    a.npcs.Init(a.map);
    a.people.Init(a.map);
    a.player.Teleport({1, 1});
    a.game.Quests().Start(FindQuest("bones_of_the_crypt"));
    a.Run(0.5f);
    CHECK(a.game.Quests().State(FindQuest("bones_of_the_crypt")).step == 0);
    a.player.Teleport({6, 1});
    a.Run(0.5f);
    CHECK(a.game.Quests().State(FindQuest("bones_of_the_crypt")).step == 1 && a.ev.Said("Quest updated"));
}

// ---------------------------------------------------------------------------------------------- the gathering skills, more of them

// A world with the given extras on a small field (the hero starts at 1,1).
static void SkillWorld(MiniWorld* w, const char* extras) {
    std::string text = std::string("@size 12 5\n@spawn 1 1\n") + extras + "@rows\n";
    for (int z = 0; z < 5; ++z) text += "............\n";
    w->map.ParseText(text);
    w->npcs.Init(w->map);
    w->forest.Init(w->map.Trees());
    w->rocks.Init(w->map.Rocks());
    w->people.Init(w->map);
    w->player.Teleport({1, 1});
}

static void TestSkillingData() {
    // trees: ordinary ones, then oak, willow and yew (each a step up in level, XP and time to regrow)
    const TreeInfo& plain = GetTreeInfo("round");
    CHECK(&GetTreeInfo("pine") != &GetTreeInfo("oak") && &GetTreeInfo("nonsense") == &GetTreeInfo("round") && plain.level == 1 && plain.logs == Item::Logs);
    CHECK(GetTreeInfo("oak").level == 15 && GetTreeInfo("willow").level == 30 && GetTreeInfo("yew").level == 60);
    CHECK(GetTreeInfo("oak").xp > plain.xp && GetTreeInfo("willow").xp > GetTreeInfo("oak").xp && GetTreeInfo("yew").xp > GetTreeInfo("willow").xp);
    CHECK(GetTreeInfo("yew").regrow > GetTreeInfo("oak").regrow && GetTreeInfo("oak").logs == Item::OakLogs && plain.regrow == Forest::kRegrowSeconds);
    // axes: the starter bronze axe and three better ones
    Inventory bag;
    CHECK(BestHatchet(bag, 99) == Item::None);
    bag.Add(Item::BronzeAxe);
    bag.Add(Item::SteelAxe);
    CHECK(BestHatchet(bag, 1) == Item::BronzeAxe && BestHatchet(bag, 20) == Item::BronzeAxe && BestHatchet(bag, 21) == Item::SteelAxe);
    CHECK(HatchetSpeed(Item::MithrilAxe) > HatchetSpeed(Item::SteelAxe) && HatchetSpeed(Item::SteelAxe) > HatchetSpeed(Item::IronAxe) && HatchetSpeed(Item::IronAxe) > HatchetSpeed(Item::BronzeAxe));
    CHECK(IsHatchet(Item::IronAxe) && !IsHatchet(GearItem(Tier::Iron, GearType::Axe)) && HatchetLevel(Item::MithrilAxe) == 31);
    // the axes are also gear tables: the smith makes them, and they are tools, not worn
    CHECK(GearItem(Tier::Bronze, GearType::Hatchet) == Item::BronzeAxe && GearItem(Tier::Iron, GearType::Hatchet) == Item::IronAxe);
    Tier t;
    GearType g;
    EquipSlot slot;
    CHECK(GearOf(Item::SteelAxe, &t, &g) && t == Tier::Steel && g == GearType::Hatchet && !Equippable(Item::SteelAxe, &slot));
    CHECK(GetSmithRecipe(Tier::Iron, GearType::Hatchet).bars == 1 && GetSmithRecipe(Tier::Iron, GearType::Hatchet).product == Item::IronAxe);
    CHECK(GetSmithRecipe(Tier::Steel, GearType::Arrows).yield == 15 && GetSmithRecipe(Tier::Steel, GearType::Arrows).product == Item::SteelArrows);
    // chopping chance: better with the level, the axe, and an easier tree
    CHECK(TreeChopChance(plain, 10, 1.0f) < TreeChopChance(plain, 40, 1.0f) && TreeChopChance(plain, 10, 1.0f) < TreeChopChance(plain, 10, 1.9f));
    CHECK(TreeChopChance(GetTreeInfo("oak"), 15, 1.0f) < TreeChopChance(plain, 15, 1.0f) && TreeChopChance(GetTreeInfo("yew"), 99, 1.9f) <= 0.95f);
    // logs and firemaking
    CHECK(IsLog(Item::YewLogs) && !IsLog(Item::Coins) && GetLogInfo(Item::OakLogs)->fmLevel == 15 && GetLogInfo(Item::YewLogs)->fmXp > GetLogInfo(Item::Logs)->fmXp);
    Inventory logs;
    logs.Add(Item::OakLogs);
    CHECK(BurnableLog(logs, 14) == Item::None && AnyLog(logs) == Item::OakLogs && BurnableLog(logs, 15) == Item::OakLogs);
    logs.Add(Item::Logs);
    CHECK(BurnableLog(logs, 1) == Item::Logs);   // the plain ones first
    // fishing spots and fish
    CHECK(&GetSpotInfo("zzz") == &GetSpotInfo("net") && GetSpotInfo("rod").bait == Item::Feathers && GetSpotInfo("net").bait == Item::None);
    CHECK(GetSpotInfo("harpoon").tool == Item::Harpoon && GetSpotInfo("cage").tool == Item::LobsterPot);
    const SpotInfo& net = GetSpotInfo("net");
    CHECK(RollSpotCatch(net, 1, 0.99) == Item::RawShrimps && RollSpotCatch(net, 15, 0.0) == Item::RawShrimps && RollSpotCatch(net, 15, 0.95) == Item::RawAnchovies);
    CHECK(RollSpotCatch(GetSpotInfo("rod"), 10, 0.5) == Item::None && RollSpotCatch(GetSpotInfo("rod"), 20, 0.99) == Item::RawTrout && RollSpotCatch(GetSpotInfo("rod"), 30, 0.99) == Item::RawSalmon);
    CHECK(RollSpotCatch(GetSpotInfo("harpoon"), 50, 0.99) == Item::RawSwordfish && RollSpotCatch(GetSpotInfo("cage"), 40, 0.5) == Item::RawLobster);
    CHECK(FishXp(Item::RawSwordfish) > FishXp(Item::RawTuna) && FishLevelRequired(Item::RawLobster) == 40 && FindFish(Item::Bones) == nullptr);
    // cooking them: recipes for every fish, with their own levels, and every cooked fish heals more than the last
    for (Item raw : {Item::RawTrout, Item::RawSalmon, Item::RawTuna, Item::RawLobster, Item::RawSwordfish}) {
        const Recipe* r = RecipeFor(raw);
        CHECK(r && GetItemInfo(raw).raw && GetItemInfo(r->cooked).food && r->level >= 15 && r->xp > 60);
    }
    CHECK(GetItemInfo(Item::Swordfish).heals > GetItemInfo(Item::Lobster).heals && GetItemInfo(Item::Lobster).heals > GetItemInfo(Item::Trout).heals);
    // rocks: clay, silver, gold, gems; gem rocks give one of three gems
    RockKind k;
    CHECK(RockKindFromKey("gems", &k) && k == RockKind::Gems && RockKindFromKey("gold", &k) && GetRockInfo(RockKind::Gold).ore == Item::GoldOre);
    CHECK(RockYield(RockKind::Gems, 0.0) == Item::UncutOpal && RockYield(RockKind::Gems, 0.3) == Item::UncutJade && RockYield(RockKind::Gems, 0.45) == Item::UncutRedTopaz);
    CHECK(RockYield(RockKind::Gems, 0.6) == Item::UncutSapphire && RockYield(RockKind::Gems, 0.8) == Item::UncutEmerald && RockYield(RockKind::Gems, 0.9) == Item::UncutRuby && RockYield(RockKind::Gems, 0.99) == Item::UncutDiamond);
    CHECK(RockYield(RockKind::Clay, 0.5) == Item::Clay && GetRockInfo(RockKind::Gold).level == 40 && GetRockInfo(RockKind::Clay).level == 1);
    // smelting silver and gold
    CHECK(GetSmeltRecipe(static_cast<Tier>(5)).bar == Item::SilverBar && GetSmeltRecipe(static_cast<Tier>(6)).bar == Item::GoldBar && GetSmeltRecipe(static_cast<Tier>(6)).level == 40);
    CHECK(BarFor(static_cast<Tier>(6)) == Item::GoldBar && BarFor(Tier::Adamant) == Item::AdamantBar && kSmeltCount == 7);
    // every new item is a real, named, priced thing
    for (int i = static_cast<int>(Item::OakLogs); i < static_cast<int>(Item::Count); ++i) {
        const Item it = static_cast<Item>(i);
        const bool worthless = it == Item::BurntPie || it == Item::RustyKey || it == Item::CaptainsLog;   // (burnt food and quest items are priceless, in the other sense)
        CHECK(std::string(GetItemInfo(it).name) != "nothing" && (ItemValue(it) > 0 || worthless) && GetItemInfo(it).examine[0] && GetItemInfo(it).model[0]);
    }
}

static void TestGameplayWoodcutting() {
    // an oak needs level 15
    MiniWorld w;
    SkillWorld(&w, "@tree 3.5 1.5 oak 1.0 0\n@tree 5.5 1.5 round 1.0 0\n");
    GiveStarterKit(&w.bag);
    w.game.TapTree(0);
    CHECK(w.ev.Said("Woodcutting level of 15") && w.game.Current() == Activity::None);
    w.skills.SetXp(Skill::Woodcutting, XpForLevel(15));
    w.game.TapTree(0);
    w.Run(10.0f);
    CHECK(w.bag.Count(Item::OakLogs) == 1 && w.skills.Xp(Skill::Woodcutting) == XpForLevel(15) + 38 && w.forest.Trees()[0].stump);
    CHECK(w.forest.SecondsUntilRegrow(0) > 200.0f && w.forest.SecondsUntilRegrow(0) <= 240.0f);          // an oak takes longer to grow back
    CHECK(w.ev.Said("You get some oak logs"));
    // an ordinary tree gives plain logs and 25 XP
    MiniWorld p;
    SkillWorld(&p, "@tree 3.5 1.5 round 1.0 0\n");
    GiveStarterKit(&p.bag);
    p.game.TapTree(0);
    p.Run(10.0f);
    CHECK(p.bag.Count(Item::Logs) == 1 && p.skills.Xp(Skill::Woodcutting) == 25 && p.forest.SecondsUntilRegrow(0) > 170.0f);
    // the menu names the tree; a better axe is used by itself
    const std::vector<MenuEntry> menu = w.game.BuildMenu({{TargetKind::Tree, 0}}, false, 0, 0);
    CHECK(menu[0].Text() == "Chop down Oak tree" || menu.size() >= 2);
    MiniWorld a;
    SkillWorld(&a, "@tree 3.5 1.5 round 1.0 0\n");
    a.bag.Add(Item::BronzeAxe);
    a.bag.Add(Item::IronAxe);
    CHECK(a.game.AxeInUse() == Item::BronzeAxe);   // (level 1: the iron axe needs 6)
    a.skills.SetXp(Skill::Woodcutting, XpForLevel(6));
    CHECK(a.game.AxeInUse() == Item::IronAxe);
    // no axe at all
    MiniWorld n;
    SkillWorld(&n, "@tree 3.5 1.5 round 1.0 0\n");
    n.game.TapTree(0);
    CHECK(n.ev.Said("You need an axe"));
    // lighting: oak logs need Firemaking 15, and give more XP
    MiniWorld f;
    SkillWorld(&f, "");
    f.bag.Add(Item::Tinderbox);
    f.bag.Add(Item::OakLogs);
    f.game.DoItemAction(f.bag.FindSlot(Item::OakLogs), ItemAction::Light);
    CHECK(f.ev.Said("Firemaking level of 15") && f.game.Current() == Activity::None);
    f.skills.SetXp(Skill::Firemaking, XpForLevel(15));
    f.game.DoItemAction(f.bag.FindSlot(Item::OakLogs), ItemAction::Light);
    f.Run(15.0f);
    CHECK(!f.bag.Has(Item::OakLogs) && f.fires.All().size() == 1 && f.skills.Xp(Skill::Firemaking) == XpForLevel(15) + 60);
    // the bag buttons offer "Light" for every kind of log
    Inventory b;
    b.Add(Item::Tinderbox);
    b.Add(Item::WillowLogs);
    const std::vector<ItemAction> acts = ActionsFor(Item::WillowLogs, b);
    CHECK(std::find(acts.begin(), acts.end(), ItemAction::Light) != acts.end());
}

static void TestGameplayFishingMore() {
    // a rod needs feathers and Fishing 20; every cast uses a feather; trout come out
    MiniWorld w;
    SkillWorld(&w, "@spot 3.5 1.5 rod\n");   // (an open tile is not water, but the rules do not care)
    w.game.TapSpot(0);
    CHECK(w.ev.Said("fly fishing rod") && w.game.Current() == Activity::None);
    w.bag.Add(Item::FlyFishingRod);
    w.game.TapSpot(0);
    CHECK(w.ev.Said("feathers") && w.game.Current() == Activity::None);
    w.bag.Add(Item::Feathers, 5);
    w.game.TapSpot(0);
    CHECK(w.ev.Said("Fishing level of 20"));
    w.skills.SetXp(Skill::Fishing, XpForLevel(20));
    w.game.TapSpot(0);
    w.Run(20.0f);
    CHECK(w.bag.Count(Item::RawTrout) >= 1 && w.bag.Count(Item::Feathers) < 5 && w.skills.Xp(Skill::Fishing) >= XpForLevel(20) + 50);
    CHECK(w.bag.Count(Item::Feathers) + w.bag.Count(Item::RawTrout) <= 5);   // a feather is used on every cast
    // running out of feathers stops the fishing
    MiniWorld e;
    SkillWorld(&e, "@spot 3.5 1.5 rod\n");
    e.bag.Add(Item::FlyFishingRod);
    e.bag.Add(Item::Feathers, 1);
    e.skills.SetXp(Skill::Fishing, XpForLevel(20));
    e.rollValue = 0.99;   // nothing bites
    e.game.TapSpot(0);
    e.Run(10.0f);
    CHECK(e.ev.Said("run out of feathers") && e.game.Current() == Activity::None);
    // a harpoon for tuna (35), a cage for lobster (40)
    MiniWorld h;
    SkillWorld(&h, "@spot 3.5 1.5 harpoon\n@spot 3.5 3.5 cage\n");
    h.bag.Add(Item::Harpoon);
    h.bag.Add(Item::LobsterPot);
    h.game.TapSpot(0);
    CHECK(h.ev.Said("Fishing level of 35"));
    h.skills.SetXp(Skill::Fishing, XpForLevel(40));
    h.game.TapSpot(0);
    h.Run(8.0f);
    CHECK(h.bag.Has(Item::RawTuna) || h.bag.Has(Item::RawSwordfish));
    h.game.TapSpot(1);
    h.Run(8.0f);
    CHECK(h.bag.Has(Item::RawLobster));
    // the menu says what to do at each kind of spot
    const std::vector<MenuEntry> menu = h.game.BuildMenu({{TargetKind::Spot, 0}}, false, 0, 0);
    CHECK(menu[0].Text() == "Harpoon Harpoon fishing spot");
    CHECK(h.game.BuildMenu({{TargetKind::Spot, 1}}, false, 0, 0)[0].Text() == "Cage Cage fishing spot");
}

static void TestGameplayCookingMore() {
    // cooking a trout needs Cooking 15; the level-1 food next to it is cooked first
    MiniWorld w;
    SkillWorld(&w, "");
    w.rollValue = 0.99;   // (nothing burns)
    w.fires.Light(&w.map, {3, 1});
    w.bag.Add(Item::RawTrout);
    w.bag.Add(Item::RawShrimps);
    w.game.TapFire(w.fires.All()[0].id);
    w.Run(10.0f);
    CHECK(w.bag.Has(Item::Shrimps) && w.bag.Has(Item::RawTrout) && w.ev.Said("Cooking level of 15"));
    w.skills.SetXp(Skill::Cooking, XpForLevel(15));
    w.game.TapFire(w.fires.All()[0].id);
    w.Run(10.0f);
    CHECK(w.bag.Has(Item::Trout) && w.skills.Xp(Skill::Cooking) >= XpForLevel(15) + 70);
    // eating the new fish heals more
    w.skills.SetXp(Skill::Hitpoints, XpForLevel(40));
    w.skills.SetHp(1);
    w.game.DoItemAction(w.bag.FindSlot(Item::Trout), ItemAction::Eat);
    CHECK(w.skills.Hp() == 1 + GetItemInfo(Item::Trout).heals);
}

static void TestGameplayMineralsAndSmithing() {
    // gem rocks give gems, gold needs 40 Mining, clay is easy
    MiniWorld w;
    SkillWorld(&w, "@rock 3.5 0.5 gems 0\n@rock 5.5 0.5 gold 0\n@rock 7.5 0.5 clay 0\n");
    GiveStarterKit(&w.bag);
    w.game.TapRock(2);
    w.Run(10.0f);
    CHECK(w.bag.Has(Item::Clay) && w.skills.Xp(Skill::Mining) == 5);
    w.game.TapRock(1);
    CHECK(w.ev.Said("Mining level of 40"));
    w.skills.SetXp(Skill::Mining, XpForLevel(40));
    w.game.TapRock(0);
    w.Run(12.0f);
    CHECK((w.bag.Has(Item::UncutOpal) || w.bag.Has(Item::UncutJade) || w.bag.Has(Item::UncutRedTopaz) || w.bag.Has(Item::UncutSapphire) ||
           w.bag.Has(Item::UncutEmerald) || w.bag.Has(Item::UncutRuby) || w.bag.Has(Item::UncutDiamond)) && w.ev.Said("You find"));
    w.game.TapRock(1);
    w.Run(12.0f);
    CHECK(w.bag.Has(Item::GoldOre));

    // the furnace: gold ore -> a gold bar (Smithing 40)
    MiniWorld f;
    SkillWorld(&f, "@station furnace 3.5 2.5 0\n@station anvil 5.5 2.5 0\n");
    GiveStarterKit(&f.bag);
    f.bag.Add(Item::GoldOre, 2);
    f.bag.Add(Item::SilverOre, 1);
    f.game.TapStation(0);
    f.Run(6.0f);
    CHECK(f.ev.makeMenus == 1 && f.ev.lastMake.options.size() == 7 && f.ev.lastMake.options[6].product == Item::GoldBar && f.ev.lastMake.options[5].product == Item::SilverBar);
    f.game.StartSmelt(static_cast<Tier>(6), 2);
    CHECK(f.ev.Said("Smithing level of 40"));
    f.skills.SetXp(Skill::Smithing, XpForLevel(40));
    f.game.StartSmelt(static_cast<Tier>(6), 2);
    f.Run(8.0f);
    CHECK(f.bag.Count(Item::GoldBar) == 2 && !f.bag.Has(Item::GoldOre) && f.ev.Said("retrieve a gold bar"));
    // the anvil: an iron axe from one bar, and a stack of steel arrows from one bar
    f.skills.SetXp(Skill::Smithing, XpForLevel(60));
    f.bag.Add(Item::IronBar, 2);
    f.bag.Add(Item::SteelBar, 1);
    f.game.TapStation(1);
    f.Run(6.0f);
    CHECK(f.ev.lastMake.kind == MakeMenu::Kind::Smith);
    f.game.StartSmith(Tier::Iron, GearType::Hatchet, 1);
    f.Run(5.0f);
    CHECK(f.bag.Has(Item::IronAxe) && f.bag.Count(Item::IronBar) == 1);
    f.game.StartSmith(Tier::Steel, GearType::Arrows, 1);
    f.Run(5.0f);
    CHECK(f.bag.Count(Item::SteelArrows) == 15 && !f.bag.Has(Item::SteelBar) && f.ev.Said("You make 15 steel arrows"));
}

static void TestCrafting() {
    // data
    const std::vector<CraftRecipe>& all = AllCraftRecipes();
    CHECK(all.size() >= 15);
    for (const CraftRecipe& r : all) {
        CHECK(r.product != Item::None && r.yield >= 1 && r.level >= 1 && GetItemInfo(r.product).name[0] != 'n');
        CHECK(r.input[0] != Item::None && r.count[0] >= 1 && !IngredientText(r).empty());
    }
    CHECK(std::string(CraftStation(CraftKind::Pottery)) == "wheel" && std::string(CraftStation(CraftKind::Leather)).empty() && std::string(CraftTitle(CraftKind::Tan)) == "Tanner");
    Inventory bag;
    const CraftRecipe* cap = nullptr;
    for (const CraftRecipe& r : all)
        if (r.product == Item::LeatherCap) cap = &r;
    CHECK(cap && CanCraft(*cap, bag) == 0 && IngredientText(*cap) == "1 leather + 1 thread");
    bag.Add(Item::Leather, 3);
    bag.Add(Item::Thread, 5);
    CHECK(CanCraft(*cap, bag) == 0);            // no needle: a tool is needed
    bag.Add(Item::Needle);
    CHECK(CanCraft(*cap, bag) == 3);
    // leather armour is worn like any armour (head, body, legs) with a little ranged help
    EquipSlot slot;
    CHECK(Equippable(Item::LeatherCap, &slot) && slot == EquipSlot::Head && Equippable(Item::LeatherBody, &slot) && slot == EquipSlot::Body && Equippable(Item::LeatherChaps, &slot) && slot == EquipSlot::Legs);
    CHECK(ItemStats(Item::LeatherBody).defence > ItemStats(Item::LeatherCap).defence && ItemStats(Item::LeatherBody).ranged > 0 && FindLeather(Item::Leather) == nullptr);

    // the tanner: hide and two coins -> leather
    MiniWorld w;
    SkillWorld(&w, "@station tanner 3.5 2.5 0\n@station jeweller 6.5 2.5 0\n@station wheel 9.5 2.5 0\n");
    w.bag.Add(Item::Cowhide, 2);
    w.bag.Add(Item::Coins, 3);
    w.game.TapStation(0);
    w.Run(6.0f);
    CHECK(w.ev.makeMenus == 1 && w.ev.lastMake.kind == MakeMenu::Kind::Craft && w.ev.lastMake.title == "Tanner" && w.ev.lastMake.options.size() == 2);   // (leather, and hard leather)
    CHECK(w.ev.lastMake.options[0].canMake == 1 && w.ev.lastMake.options[0].haveAll);          // only enough coins for one
    w.game.StartCraft(w.ev.lastMake.options[0].index, 5);
    w.Run(8.0f);
    CHECK(w.bag.Count(Item::Leather) == 1 && w.bag.Count(Item::Cowhide) == 1 && w.bag.Count(Item::Coins) == 1 && w.ev.Said("leather"));
    // sewing: use the needle on leather (anywhere), needs level 7 for a cap
    w.bag.Add(Item::Needle);
    w.bag.Add(Item::Thread, 4);
    w.bag.Add(Item::Leather, 2);
    w.game.BeginUse(w.bag.FindSlot(Item::Needle));
    CHECK(w.game.UseItemOnSlot(w.bag.FindSlot(Item::Leather)) && w.ev.makeMenus == 2 && w.ev.lastMake.title == "Leatherwork");
    int capIndex = -1;
    for (const MakeOption& o : w.ev.lastMake.options)
        if (o.product == Item::LeatherCap) capIndex = o.index;
    CHECK(capIndex >= 0);
    w.game.StartCraft(capIndex, 1);
    CHECK(w.ev.Said("Crafting level of 7") && w.game.Current() == Activity::None);
    w.skills.SetXp(Skill::Crafting, XpForLevel(7));
    w.game.StartCraft(capIndex, 1);
    w.Run(4.0f);
    CHECK(w.bag.Has(Item::LeatherCap) && w.skills.Xp(Skill::Crafting) == XpForLevel(7) + 25 && w.bag.Count(Item::Thread) == 3);
    // gems: the chisel on an uncut gem
    w.bag.Add(Item::Chisel);
    w.bag.Add(Item::UncutSapphire);
    w.skills.SetXp(Skill::Crafting, XpForLevel(20));
    w.game.BeginUse(w.bag.FindSlot(Item::Chisel));
    CHECK(w.game.UseItemOnSlot(w.bag.FindSlot(Item::UncutSapphire)) && w.ev.lastMake.title == "Gem cutting");
    for (const MakeOption& o : w.ev.lastMake.options)
        if (o.product == Item::Sapphire) w.game.StartCraft(o.index, 1);
    w.Run(4.0f);
    CHECK(w.bag.Has(Item::Sapphire) && !w.bag.Has(Item::UncutSapphire));
    // jewellery: at the bench, with a mould, a gold bar and the gem
    w.bag.Add(Item::RingMould);
    w.bag.Add(Item::GoldBar);
    w.player.Teleport({6, 1});
    w.game.TapStation(1);
    w.Run(5.0f);
    CHECK(w.ev.lastMake.title == "Jeweller's bench");
    for (const MakeOption& o : w.ev.lastMake.options)
        if (o.product == Item::SapphireRing) w.game.StartCraft(o.index, 1);
    w.Run(4.0f);
    CHECK(w.bag.Has(Item::SapphireRing) && !w.bag.Has(Item::GoldBar) && !w.bag.Has(Item::Sapphire) && w.bag.Has(Item::RingMould));   // (the mould is not used up)
    // crafting at a bench you are not at
    w.player.Teleport({1, 4});
    w.bag.Add(Item::Clay, 2);
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].product == Item::Pot) w.game.StartCraft(static_cast<int>(i), 1);
    CHECK(w.ev.Said("next to the potter's wheel"));
    // pottery
    w.game.TapStation(2);
    w.Run(10.0f);
    for (const MakeOption& o : w.ev.lastMake.options)
        if (o.product == Item::Pot) w.game.StartCraft(o.index, 2);
    w.Run(8.0f);
    CHECK(w.bag.Count(Item::Pot) == 2 && !w.bag.Has(Item::Clay));
    // using a gold bar on the bench opens it
    w.bag.Add(Item::GoldBar);
    w.player.Teleport({6, 1});
    w.game.BeginUse(w.bag.FindSlot(Item::GoldBar));
    w.game.UseItemOnStation(w.bag.FindSlot(Item::GoldBar), 1);
    w.Run(3.0f);
    CHECK(w.ev.makeMenus >= 5);
}

// ---------------------------------------------------------------------------------------------- bank, archery, magic

static void TestBankData() {
    Bank b;
    CHECK(Bank::kSlots == 200 && b.Empty() && b.Deposit(Item::Logs, 5) == 5 && b.Count(Item::Logs) == 5 && b.Used() == 1);
    CHECK(b.Deposit(Item::Logs, 3) == 3 && b.Count(Item::Logs) == 8 && b.Used() == 1);   // one slot per KIND of item
    CHECK(b.Deposit(Item::None, 1) == 0 && b.Deposit(Item::Logs, 0) == 0 && b.Deposit(Item::Logs, -2) == 0);
    CHECK(b.Withdraw(Item::Logs, 3) == 3 && b.Count(Item::Logs) == 5);
    CHECK(b.Withdraw(Item::Logs, 99) == 5 && !b.Has(Item::Logs) && b.Empty());           // emptied: the slot is free again
    CHECK(b.Withdraw(Item::Logs, 1) == 0 && b.Withdraw(Item::Coins, 1) == 0);
    b.Deposit(Item::Coins, Bank::kMaxStack - 5);
    CHECK(b.Deposit(Item::Coins, 100) == 5 && b.Count(Item::Coins) == Bank::kMaxStack);   // a stack has a limit
    // the order kinds were first put in is kept, and loading a save sets counts directly
    Bank c;
    c.Deposit(Item::Bones, 1);
    c.Deposit(Item::Coins, 7);
    c.SetSlot(Item::Logs, 4);
    c.SetSlot(Item::Bones, 0);
    CHECK(c.Slots().size() == 2 && c.Slots()[0].item == Item::Coins && c.Slots()[1].item == Item::Logs && c.Count(Item::Logs) == 4);

    // saved with the rest of the progress
    Skills s;
    Inventory bag;
    Equipment eq;
    Bank bank;
    bank.Deposit(Item::IronOre, 40);
    bank.Deposit(Item::Coins, 12345);
    eq.Set(EquipSlot::Weapon, Item::Shortbow);
    eq.SetAmmo(Item::IronArrows, 77);
    const std::string text = SerializeProgress(s, bag, CombatStyle::Accurate, &eq, &bank, FindSpell("fire_strike"));
    Skills s2;
    Inventory bag2;
    Equipment eq2;
    Bank bank2;
    int spell = 0;
    CombatStyle st = CombatStyle::Accurate;
    ParseProgress(text, &s2, &bag2, &st, &eq2, &bank2, &spell);
    CHECK(bank2.Count(Item::IronOre) == 40 && bank2.Count(Item::Coins) == 12345 && bank2.Used() == 2);
    CHECK(eq2.Get(EquipSlot::Weapon) == Item::Shortbow && eq2.Get(EquipSlot::Ammo) == Item::IronArrows && eq2.AmmoCount() == 77);
    CHECK(spell == FindSpell("fire_strike") && spell > 0);
    // damaged lines are ignored
    Bank bank3;
    ParseProgress("bank.0=9999:5\nbank.1=1:-4\nbank.2=nonsense\nbank.3=1:6\n", &s2, &bag2, &st, &eq2, &bank3);
    CHECK(bank3.Used() == 1 && bank3.Count(Item::Logs) == 6);
}

// A small world for the bank: one banker standing behind a counter tile.
static void BankWorld(MiniWorld* w) {
    w->map.ParseText("@size 8 3\n@spawn 1 1\n@person oswin 5.5 1.5 90\n@rows\n........\n.....o..\n........\n");
    w->npcs.Init(w->map);
    w->forest.Init(w->map.Trees());
    w->rocks.Init(w->map.Rocks());
    w->people.Init(w->map);
    w->player.Teleport({1, 1});
}

static void TestGameplayBank() {
    MiniWorld w;
    BankWorld(&w);
    w.bag.Add(Item::Logs, 4);
    w.bag.Add(Item::Coins, 100);
    w.bag.Add(Item::Bones, 1);
    w.game.BankDeposit(Item::Logs, 2);                                     // too far away
    CHECK(w.ev.Said("too far") && w.bag.Count(Item::Logs) == 4 && w.bank.Empty());
    // a banker's menu says Bank, and a tap walks up and opens the window
    const std::vector<MenuEntry> menu = w.game.BuildMenu({{TargetKind::Person, 1}}, false, 0, 0);
    CHECK(menu.size() == 4 && menu[0].Text() == "Bank Oswin" && menu[1].Text() == "Talk-to Oswin");
    w.game.TapPerson(1, true);                                             // talk only: no bank window
    w.Run(4.0f);
    CHECK(w.ev.bankOpens == 0 && w.game.NearBanker());
    w.game.TapPerson(1);
    w.Run(1.0f);
    CHECK(w.ev.bankOpens == 1);

    w.game.BankDeposit(Item::Logs, 3);
    CHECK(w.bag.Count(Item::Logs) == 1 && w.bank.Count(Item::Logs) == 3);
    w.game.BankDeposit(Item::Coins, 1000);                                 // more than you carry: all of it
    CHECK(!w.bag.Has(Item::Coins) && w.bank.Count(Item::Coins) == 100);
    w.game.BankDeposit(Item::Shrimps, 5);                                  // not carried: nothing happens
    CHECK(w.bank.Count(Item::Shrimps) == 0);
    w.game.BankWithdraw(Item::Coins, 40);
    CHECK(w.bag.Count(Item::Coins) == 40 && w.bank.Count(Item::Coins) == 60 && w.bag.FindSlot(Item::Coins) >= 0);
    w.game.BankWithdraw(Item::Logs, 2);                                    // logs do not stack: 2 slots
    CHECK(w.bag.Count(Item::Logs) == 3 && w.bank.Count(Item::Logs) == 1);
    // everything at once
    w.game.BankDepositAll();
    CHECK(w.bag.FreeSlots() == Inventory::kSlots && w.bank.Count(Item::Bones) == 1 && w.bank.Count(Item::Logs) == 4 && w.bank.Count(Item::Coins) == 100);
    // a full bag: only what fits comes out
    for (int i = 0; i < Inventory::kSlots - 2; ++i) w.bag.Add(Item::Egg);
    w.game.BankWithdraw(Item::Logs, 4);
    CHECK(w.bag.Count(Item::Logs) == 2 && w.bank.Count(Item::Logs) == 2 && w.ev.Said("inventory is full"));
    w.bag.Add(Item::Egg, 2);                                               // no free slot left at all
    w.game.BankWithdraw(Item::Bones, 1);
    CHECK(!w.bag.Has(Item::Bones) && w.bank.Count(Item::Bones) == 1);
    w.game.BankWithdraw(Item::Coins, 10);                                  // coins have no slot to go in either
    CHECK(w.bank.Count(Item::Coins) == 100);

    // walking away: the banker is out of reach again
    w.player.Teleport({0, 0});
    w.game.BankWithdraw(Item::Logs, 1);
    CHECK(w.bank.Count(Item::Logs) == 2 && w.ev.Said("too far"));
}

static void TestArcheryData() {
    CHECK(IsBow(Item::Shortbow) && IsBow(Item::YewLongbow) && !IsBow(Item::Staff) && IsArrow(Item::MithrilArrows) && !IsArrow(Item::Bones));
    for (int i = 0; i < kBowCount; ++i) {
        const BowInfo& b = GetBow(i);
        CHECK(FindBow(b.item) == &b && b.range >= 7 && b.accuracy > 0 && b.level >= 1 && b.value > 0);
        EquipSlot slot;
        CHECK(Equippable(b.item, &slot) && slot == EquipSlot::Weapon && GetItemInfo(b.item).name == std::string(b.name));
        CHECK(!GetItemInfo(b.item).stackable && ItemStats(b.item).ranged == b.accuracy);
    }
    CHECK(GetBow(1).range > GetBow(0).range && GetBow(1).level > GetBow(0).level);   // longbows reach further
    for (int i = 0; i < kArrowKinds; ++i) {
        const ArrowInfo& a = GetArrow(i);
        EquipSlot slot;
        CHECK(Equippable(a.item, &slot) && slot == EquipSlot::Ammo && GetItemInfo(a.item).stackable && ItemStats(a.item).rangedStrength == a.strength);
        if (i > 0) CHECK(a.strength > GetArrow(i - 1).strength && a.level >= GetArrow(i - 1).level && a.value > GetArrow(i - 1).value);
    }
    // wielding: levels are checked, the quiver takes the whole stack, and arrows of the same kind join it
    Skills sk;
    Inventory bag;
    Equipment eq;
    bag.Add(Item::Longbow);
    std::string need;
    CHECK(EquipFromBag(&eq, &bag, 0, sk, &need) == EquipResult::LevelTooLow && need == "Ranged level 5" && bag.Has(Item::Longbow));
    bag.Add(Item::Shortbow);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::Shortbow), sk) == EquipResult::Done && eq.Get(EquipSlot::Weapon) == Item::Shortbow);
    CHECK(eq.Anim() == WeaponAnim::Shoot && eq.Total().ranged == GetBow(0).accuracy);
    bag.Add(Item::BronzeArrows, 40);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::BronzeArrows), sk) == EquipResult::Done);
    CHECK(eq.Get(EquipSlot::Ammo) == Item::BronzeArrows && eq.AmmoCount() == 40 && !bag.Has(Item::BronzeArrows) && eq.Total().rangedStrength == 7);
    bag.Add(Item::BronzeArrows, 10);
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::BronzeArrows), sk) == EquipResult::Done && eq.AmmoCount() == 50);
    bag.Add(Item::IronArrows, 5);
    need.clear();
    CHECK(EquipFromBag(&eq, &bag, bag.FindSlot(Item::IronArrows), sk, &need) == EquipResult::LevelTooLow && need == "Ranged level 5");
    CHECK(eq.UseAmmo(3) == 3 && eq.AmmoCount() == 47 && eq.UseAmmo(100) == 47 && eq.Get(EquipSlot::Ammo) == Item::None && eq.AmmoCount() == 0);
    // taking the quiver off puts the arrows back in the bag
    eq.SetAmmo(Item::BronzeArrows, 12);
    CHECK(Unequip(&eq, &bag, EquipSlot::Ammo) == EquipResult::Done && bag.Count(Item::BronzeArrows) == 12 && eq.AmmoCount() == 0);
    CHECK(Unequip(&eq, &bag, EquipSlot::Ammo) == EquipResult::NotWearable);

    // the maths: the bow adds accuracy, the arrows add max hit
    Skills ranged;
    ranged.SetXp(Skill::Ranged, XpForLevel(30));
    const Fighter weak = RangedFighter(Skills(), CombatStyle::Accurate, 8, 7, 0);
    const Fighter strong = RangedFighter(ranged, CombatStyle::Accurate, 46, 22, 0);
    CHECK(MaxHit(strong) > MaxHit(weak) && MaxHit(weak) >= 1);
    Fighter cow;
    cow.attack = 3, cow.strength = 3, cow.defence = 3, cow.creature = true;
    CHECK(HitChance(strong, cow) > HitChance(weak, cow));
    CHECK(RangedFighter(Skills(), CombatStyle::Accurate, 0, 0, 0).styleAttack == 3 && RangedFighter(Skills(), CombatStyle::Aggressive, 0, 0, 0).styleStrength == 3);
    CHECK(XpForRangedDamage(CombatStyle::Accurate, 3).ranged == 12 && XpForRangedDamage(CombatStyle::Accurate, 3).hitpoints == 4);
    CHECK(XpForRangedDamage(CombatStyle::Defensive, 3).ranged == 6 && XpForRangedDamage(CombatStyle::Defensive, 3).defence == 6);
    CHECK(XpForRangedDamage(CombatStyle::Accurate, 0).ranged == 0);
}

static void TestFencesAndShots() {
    // low things stop a walker but not an arrow: fences, water. Walls and trees stop both.
    TileMap m;
    m.ParseText("@size 9 5\n@rows\n...f.....\n...#.....\n...~.....\n...T.....\n.........\n");
    CHECK(!m.Walkable(3, 0) && !m.Walkable(3, 2) && m.ShotPasses(3, 0) && m.ShotPasses(3, 2) && !m.ShotPasses(3, 1) && !m.ShotPasses(3, 3));
    CHECK(m.ProjectileClear({0, 0}, {6, 0}));    // over the fence
    CHECK(!m.ProjectileClear({0, 1}, {6, 1}));   // through a wall: no
    CHECK(m.ProjectileClear({0, 2}, {6, 2}));    // across water
    CHECK(!m.ProjectileClear({0, 3}, {6, 3}));   // through a tree: no
    CHECK(m.ProjectileClear({0, 4}, {6, 4}) && m.ProjectileClear({2, 1}, {3, 1}) && m.ProjectileClear({2, 1}, {2, 1}));   // open ground, or the next tile
    CHECK(!m.ProjectileClear({0, 0}, {6, 1}) || m.ProjectileClear({0, 0}, {6, 1}));   // (a slanting line is just not a crash)
    const TileMap real = [] {
        TileMap r;
        r.ParseText("@size 4 1\n@rows\n.f..\n");
        return r;
    }();
    CHECK(!real.Walkable(1, 0) && real.At(1, 0) == 'f');
}

// A pen behind a fence, with a cow in it.
static void FencedCowWorld(MiniWorld* w) {
    w->map.ParseText("@size 14 3\n@spawn 2 1\n@pen yard 8 0 14 3\n@npc cow 11.5 1.5 yard\n@rows\n.......f......\n.......f......\n.......f......\n");
    w->npcs.Init(w->map);
    w->forest.Init(w->map.Trees());
    w->rocks.Init(w->map.Rocks());
    w->people.Init(w->map);
    w->player.Teleport({2, 1});
    GiveStarterKit(&w->bag);
}

static void TestGameplayArchery() {
    MiniWorld w;
    FencedCowWorld(&w);
    const int cow = w.npcs.All()[0].id;
    // no bow: it is a punch, and nothing flies
    w.bag.Add(Item::Shortbow);
    w.bag.Add(Item::BronzeArrows, 30);
    w.game.DoItemAction(w.bag.FindSlot(Item::Shortbow), ItemAction::Wield);
    // a bow with an empty quiver will not start a fight
    w.game.TapNpc(cow);
    CHECK(w.ev.Said("no arrows") && w.game.Current() == Activity::None && w.ev.shots.empty());
    w.game.DoItemAction(w.bag.FindSlot(Item::BronzeArrows), ItemAction::Wear);
    CHECK(w.equip.AmmoCount() == 30 && w.ev.Said("quiver") && w.game.Kind() == AttackKind::Ranged && w.game.AttackReach() == 7);
    // the menu says Shoot
    const std::vector<MenuEntry> menu = w.game.BuildMenu({{TargetKind::Npc, cow}}, false, 0, 0);
    CHECK(menu[0].Text().rfind("Shoot Cow", 0) == 0);

    w.game.TapNpc(cow);
    w.Run(1.5f);
    CHECK(w.player.Tile().x >= 4 && w.player.Tile().x <= 5);                   // walked until the cow was in range (9 tiles -> 7) and not a step more
    w.Run(20.0f);                                                              // the hero strikes every other turn: 2.4 s
    const int fired = static_cast<int>(w.ev.shots.size());
    CHECK(fired >= 1 && w.equip.AmmoCount() == 30 - fired);                    // one arrow per shot
    CHECK(!w.ev.shots[0].magic && w.ev.shots[0].fromX < w.ev.shots[0].toX && w.ev.shots[0].flight > 0.0f);
    CHECK(w.skills.Xp(Skill::Ranged) > 0 && w.skills.Xp(Skill::Attack) == 0 && w.skills.Xp(Skill::Hitpoints) > 0);
    CHECK(w.skills.Hp() == w.skills.MaxHp());                                  // the cow could not get past the fence
    CHECK(w.player.Tile().x < 7);                                              // he never crossed it
    // the cow is dead (6 hits at 1 damage), and arrows that did not break lie where it stood
    CHECK(w.npcs.All()[0].dead && w.ev.Said("killed the cow"));
    CHECK(fired == 6 && w.ground.All().size() >= 1);
    bool arrows = false;
    for (const Pile& p : w.ground.All())
        for (const ItemStack& s : p.items) arrows = arrows || s.item == Item::BronzeArrows;
    CHECK(arrows);

    // the quiver runs dry
    MiniWorld d;
    FencedCowWorld(&d);
    d.bag.Add(Item::Shortbow);
    d.game.DoItemAction(d.bag.FindSlot(Item::Shortbow), ItemAction::Wield);
    d.equip.SetAmmo(Item::BronzeArrows, 2);
    d.rollValue = 0.99;   // every shot misses
    d.game.TapNpc(d.npcs.All()[0].id);
    d.Run(12.0f);
    CHECK(d.ev.shots.size() == 2 && d.equip.AmmoCount() == 0 && d.ev.Said("no arrows") && d.game.Current() == Activity::None);

    // arrows too good for his level are refused
    MiniWorld l;
    FencedCowWorld(&l);
    l.bag.Add(Item::Shortbow);
    l.game.DoItemAction(l.bag.FindSlot(Item::Shortbow), ItemAction::Wield);
    l.equip.SetAmmo(Item::SteelArrows, 10);
    l.game.TapNpc(l.npcs.All()[0].id);
    CHECK(l.ev.Said("Ranged level 20") && l.ev.shots.empty());
}

static void TestMagicData() {
    CHECK(SpellCount() == 16 && FindSpell("fire_strike") >= 0 && FindSpell("zzz") == -1);
    const SpellInfo& fs = GetSpell(FindSpell("fire_strike"));
    const SpellInfo& ws = GetSpell(FindSpell("wind_strike"));
    CHECK(fs.level == 7 && std::string(fs.name) == "Fire Strike" && fs.maxHit == 8 && ws.level == 1 && ws.maxHit == 2);
    for (int i = 0; i < SpellCount(); ++i) {   // data sanity: a ladder of level, damage, XP
        const SpellInfo& s = GetSpell(i);
        CHECK(s.costCount >= 2 && s.maxHit >= 1 && s.xp > 0);
        if (i > 0) CHECK(s.level > GetSpell(i - 1).level && s.maxHit > GetSpell(i - 1).maxHit);
    }
    for (int i = 0; i < kRuneCount; ++i) {
        Rune r;
        CHECK(IsRune(RuneItem(static_cast<Rune>(i)), &r) && r == static_cast<Rune>(i) && GetItemInfo(RuneItem(r)).stackable && ItemValue(RuneItem(r)) > 0);
    }
    CHECK(!IsRune(Item::Bones) && IsStaff(Item::FireStaff) && !IsStaff(Item::Shortbow) && FindRobe(Item::WizardHat) && !FindRobe(Item::Staff));
    // casting needs level and runes; an elemental staff pays for its own element
    Inventory bag;
    CHECK(CheckCast(fs, bag, Item::None, 10) == CastCheck::NoRunes);
    bag.Add(Item::FireRune);
    bag.Add(Item::AirRune);
    bag.Add(Item::MindRune);
    CHECK(CheckCast(fs, bag, Item::None, 6) == CastCheck::LevelTooLow && CheckCast(fs, bag, Item::None, 7) == CastCheck::Ok);
    PayRunes(fs, &bag, Item::None);
    CHECK(bag.FreeSlots() == Inventory::kSlots);
    bag.Add(Item::AirRune);
    bag.Add(Item::MindRune);
    CHECK(CheckCast(fs, bag, Item::None, 7) == CastCheck::NoRunes && CheckCast(fs, bag, Item::FireStaff, 7) == CastCheck::Ok);
    PayRunes(fs, &bag, Item::FireStaff);
    CHECK(bag.FreeSlots() == Inventory::kSlots);
    CHECK(RunesNeeded(fs, Rune::Fire, Item::AirStaff) == 1 && RunesNeeded(fs, Rune::Air, Item::AirStaff) == 0 && RunesNeeded(fs, Rune::Fire, Item::FireStaff) == 0);
    const SpellInfo& fb = GetSpell(FindSpell("fire_bolt"));   // the bolts take a lot of one element
    bag.Add(Item::FireRune, 3);
    bag.Add(Item::AirRune, 3);
    bag.Add(Item::ChaosRune, 1);
    CHECK(CheckCast(fb, bag, Item::None, 15) == CastCheck::NoRunes);
    bag.Add(Item::FireRune, 1);
    CHECK(CheckCast(fb, bag, Item::None, 15) == CastCheck::Ok);
    PayRunes(fb, &bag, Item::None);
    CHECK(bag.FreeSlots() == Inventory::kSlots);
    // worn: staffs and robes help casting, nothing needs a level
    Equipment eq;
    eq.Set(EquipSlot::Weapon, Item::FireStaff);
    eq.Set(EquipSlot::Head, Item::WizardHat);
    eq.Set(EquipSlot::Body, Item::WizardRobe);
    eq.Set(EquipSlot::Legs, Item::WizardSkirt);
    CHECK(eq.Anim() == WeaponAnim::Cast && eq.Total().magic == GetStaff(4).accuracy + 4 + 8 + 5 && eq.Total().defence == 1 + 3 + 2);
    Inventory b2;
    b2.Add(Item::WizardRobe);
    Equipment e2;
    CHECK(EquipFromBag(&e2, &b2, 0, Skills()) == EquipResult::Done && e2.Get(EquipSlot::Body) == Item::WizardRobe);
    // the maths: only the Magic level and bonuses matter for the hit; the spell sets the damage
    Skills mage;
    mage.SetXp(Skill::Magic, XpForLevel(20));
    Fighter cow;
    cow.attack = 3, cow.strength = 3, cow.defence = 3, cow.creature = true;
    CHECK(HitChance(MagicFighter(mage, 8, 0), cow) > HitChance(MagicFighter(Skills(), 0, 0), cow));
    const AttackResult hit = RollSpell(MagicFighter(mage, 8, 0), 8, cow, [] { return 0.0; });
    const AttackResult top = RollSpell(MagicFighter(mage, 8, 0), 8, cow, [] { return 0.999; });
    CHECK(hit.landed && hit.damage == 1 && !top.landed && top.damage == 0);
    int roll = 0;
    const AttackResult big = RollSpell(MagicFighter(mage, 8, 0), 8, cow, [&roll] { return roll++ == 0 ? 0.0 : 0.999; });
    CHECK(big.landed && big.damage == 8);
    CHECK(XpForSpellDamage(5).magic == 10 && XpForSpellDamage(5).hitpoints == 7 && XpForSpellDamage(0).magic == 0);
}

static void TestGameplayMagic() {
    MiniWorld w;
    GiveStarterKit(&w.bag);
    w.skills.SetXp(Skill::Magic, XpForLevel(7));
    const int chicken = w.npcs.All()[0].id;
    w.bag.Add(Item::FireStaff);
    w.game.DoItemAction(w.bag.FindSlot(Item::FireStaff), ItemAction::Wield);
    CHECK(w.game.Kind() == AttackKind::Magic && w.game.AttackReach() == 8);
    w.game.SetSpell(FindSpell("fire_strike"));
    CHECK(w.game.SpellIndex() == FindSpell("fire_strike"));
    // no runes: the cast is refused before the hero moves
    w.game.TapNpc(chicken);
    CHECK(w.ev.Said("not have enough runes") && w.game.Current() == Activity::None && w.ev.shots.empty());
    w.bag.Add(Item::AirRune, 5);
    w.bag.Add(Item::MindRune, 5);                       // the staff of fire supplies the fire runes
    const std::vector<MenuEntry> menu = w.game.BuildMenu({{TargetKind::Npc, chicken}}, false, 0, 0);
    CHECK(menu[0].Text().rfind("Cast Fire Strike on Chicken", 0) == 0);
    w.game.TapNpc(chicken);
    w.Run(8.0f);
    CHECK(w.npcs.All()[0].dead && w.ev.Said("killed the chicken"));
    CHECK(w.ev.shots.size() == 3 && w.ev.shots[0].magic && w.ev.shots[0].color.r > w.ev.shots[0].color.b);   // a red-orange fire ball
    CHECK(w.bag.Count(Item::AirRune) == 2 && w.bag.Count(Item::MindRune) == 2);                                   // 3 casts, 1 air + 1 mind each
    // cast XP (12 each) plus 2 per point of damage
    CHECK(w.skills.Xp(Skill::Magic) == XpForLevel(7) + 3 * 12 + 2 * 3 && w.skills.Xp(Skill::Attack) == 0 && w.skills.Xp(Skill::Ranged) == 0);
    CHECK(w.skills.Xp(Skill::Hitpoints) > 0);

    // a spell above his level is refused
    MiniWorld l;
    l.bag.Add(Item::Staff);
    l.game.DoItemAction(l.bag.FindSlot(Item::Staff), ItemAction::Wield);
    l.game.SetSpell(FindSpell("fire_strike"));
    l.bag.Add(Item::FireRune, 3);
    l.bag.Add(Item::AirRune, 3);
    l.bag.Add(Item::MindRune, 3);
    l.game.TapNpc(l.npcs.All()[0].id);
    CHECK(l.ev.Said("Magic level 7") && l.ev.shots.empty());
    l.game.SetSpell(0);                                  // wind strike: level 1
    l.game.TapNpc(l.npcs.All()[0].id);
    l.Run(6.0f);
    CHECK(!l.ev.shots.empty() && l.bag.Count(Item::FireRune) == 3);   // used air + mind only
    l.game.SetSpell(99);
    CHECK(l.game.SpellIndex() == 0);                     // a bad index is ignored
}

static void TestArcheryAndMagicShops() {
    Shops shops;
    shops.Init();
    const int general = ShopIndex("general_town"), bows = ShopIndex("village_archery"), magic = ShopIndex("village_magic");
    CHECK(general >= 0 && bows >= 0 && magic >= 0);
    CHECK(shops.Buys(bows, Item::Shortbow) && shops.Buys(bows, Item::IronArrows) && !shops.Buys(bows, Item::FireRune));
    CHECK(shops.Buys(magic, Item::FireRune) && shops.Buys(magic, Item::WizardHat) && shops.Buys(magic, Item::Staff) && !shops.Buys(magic, Item::Shortbow));
    CHECK(shops.Buys(general, Item::Shortbow) && shops.Buys(general, Item::AirRune));
    CHECK(CategoryOf(Item::Shortbow) == kCatRanged && CategoryOf(Item::AirRune) == kCatMagic && CategoryOf(Item::WizardRobe) == kCatMagic);
    // big shelves refill in bigger steps (a hundred arrows must not take twenty minutes)
    int line = -1;
    for (size_t i = 0; i < shops.Lines(bows).size(); ++i)
        if (shops.Lines(bows)[i].item == Item::BronzeArrows) line = static_cast<int>(i);
    CHECK(line >= 0 && shops.Take(bows, line, 200) == 200);
    for (int i = 0; i < 120; ++i) shops.Update(kRestockSeconds);
    CHECK(shops.Lines(bows)[line].count == 300);
    shops.Take(bows, line, 300);
    shops.Update(kRestockSeconds);
    CHECK(shops.Lines(bows)[line].count == 300 / 25);
}

// ---------------------------------------------------------------------------------------------- round 9: adamant, fish, gems, monsters

// The fifth metal: its own run of item ids, and every table that has one row per metal.
static void TestAdamant() {
    CHECK(kTierCount == 5 && std::string(TierName(Tier::Adamant)) == "Adamant" && std::string(TierKey(Tier::Adamant)) == "adamant");
    CHECK(GearItem(Tier::Adamant, GearType::Dagger) == Item::AdamantFirst && GearItem(Tier::Adamant, GearType::Arrows) == Item::AdamantArrows);
    CHECK(GearItem(Tier::Adamant, GearType::Pickaxe) == Item::AdamantPickaxe && GearItem(Tier::Adamant, GearType::Hatchet) == Item::AdamantAxe);
    for (int g = 0; g < static_cast<int>(GearType::Arrows); ++g) {
        const GearType gt = static_cast<GearType>(g);
        const Item it = GearItem(Tier::Adamant, gt);
        Tier t;
        GearType back;
        CHECK(GearOf(it, &t, &back) && t == Tier::Adamant && back == gt);
        CHECK(std::string(GetItemInfo(it).name).rfind("Adamant", 0) == 0 && std::string(GetItemInfo(it).examine).size() > 5);
        CHECK(ItemValue(it) > ItemValue(GearItem(Tier::Mithril, gt)));   // every piece is worth more than the mithril one
        if (g < kWornTypeCount) {                                        // ...and hits or protects at least as well, and better overall
            const GearStats a = GetGearStats(Tier::Adamant, gt), m = GetGearStats(Tier::Mithril, gt);
            CHECK(a.attack >= m.attack && a.strength >= m.strength && a.defence >= m.defence);
            CHECK(a.attack + a.strength + a.defence > m.attack + m.strength + m.defence);
        }
    }
    CHECK(!GearOf(Item::AdamantArrows, nullptr, nullptr));   // arrows are not gear, as with the other metals
    CHECK(IsArrow(Item::AdamantArrows) && GetArrow(4).item == Item::AdamantArrows && GetArrow(4).strength > GetArrow(3).strength);
    CHECK(GetArrow(4).level == 40 && GetItemInfo(Item::AdamantArrows).stackable);
    Skill sk;
    int lv;
    CHECK(ItemRequirement(GearItem(Tier::Adamant, GearType::Body), &sk, &lv) && sk == Skill::Defence && lv == 40);
    CHECK(ItemRequirement(GearItem(Tier::Adamant, GearType::Sword), &sk, &lv) && sk == Skill::Attack && lv == 40);
    CHECK(ItemRequirement(Item::AdamantArrows, &sk, &lv) && sk == Skill::Ranged && lv == 40);
    CHECK(ItemRequirement(Item::AdamantAxe, &sk, &lv) && sk == Skill::Woodcutting && lv == 41);
    EquipSlot es;
    CHECK(Equippable(GearItem(Tier::Adamant, GearType::Sword), &es) && es == EquipSlot::Weapon);
    CHECK(Equippable(Item::AdamantArrows, &es) && es == EquipSlot::Ammo && !Equippable(Item::AdamantPickaxe, &es) && !Equippable(Item::AdamantAxe, &es));
    // the furnace and the anvil
    const SmeltRecipe& sm = GetSmeltRecipe(Tier::Adamant);
    CHECK(sm.bar == Item::AdamantBar && sm.ore[0] == Item::AdamantOre && sm.ore[1] == Item::Coal && sm.count[1] == 6 && sm.level == 70);
    CHECK(sm.level > GetSmeltRecipe(Tier::Mithril).level && sm.xp > GetSmeltRecipe(Tier::Mithril).xp);
    CHECK(BarFor(Tier::Adamant) == Item::AdamantBar && BarFor(Tier::Mithril) == Item::MithrilBar);
    Tier bt;
    CHECK(TierOfBar(Item::AdamantBar, &bt) && bt == Tier::Adamant);
    CHECK(GetSmithRecipe(Tier::Adamant, GearType::Dagger).level == 70 && GetSmithRecipe(Tier::Adamant, GearType::Body).level == 88);
    CHECK(GetSmithRecipe(Tier::Adamant, GearType::Arrows).yield == 15 && GetSmithRecipe(Tier::Adamant, GearType::Body).product == GearItem(Tier::Adamant, GearType::Body));
    Inventory ore;
    ore.Add(Item::AdamantOre, 2);
    ore.Add(Item::Coal, 11);
    CHECK(CanSmelt(sm, ore) == 1);   // 11 coal is one lot of six (two ore, but only one lot of coal... and a second needs 12)
    // tools: the best one you are allowed to use
    Inventory bag;
    bag.Add(Item::AdamantPickaxe);
    bag.Add(Item::MithrilPickaxe);
    CHECK(BestPickaxe(bag, 39) == Item::MithrilPickaxe && BestPickaxe(bag, 40) == Item::AdamantPickaxe);
    CHECK(PickaxeSpeed(Tier::Adamant) > PickaxeSpeed(Tier::Mithril));
    bag.Add(Item::AdamantAxe);
    bag.Add(Item::MithrilAxe);
    CHECK(BestHatchet(bag, 40) == Item::MithrilAxe && BestHatchet(bag, 41) == Item::AdamantAxe);
    CHECK(HatchetSpeed(Item::AdamantAxe) > HatchetSpeed(Item::MithrilAxe) && IsHatchet(Item::AdamantAxe) && HatchetLevel(Item::AdamantAxe) == 41);
    CHECK(CategoryOf(Item::AdamantOre) == kCatOre && CategoryOf(Item::AdamantBar) == kCatBar && CategoryOf(Item::AdamantAxe) == kCatTool);
    // the rock it comes from
    RockKind rk;
    CHECK(RockKindFromKey("adamant", &rk) && GetRockInfo(rk).ore == Item::AdamantOre && GetRockInfo(rk).level == 70 && GetRockInfo(rk).respawn > GetRockInfo(RockKind::Mithril).respawn);
    // a saved bag, bank and worn set keep the new ids
    Skills sks;
    Inventory inv;
    Equipment worn;
    Bank bank;
    inv.Add(Item::AdamantBar, 3);
    worn.Set(EquipSlot::Body, GearItem(Tier::Adamant, GearType::Body));
    bank.Deposit(Item::AdamantOre, 7);
    const std::string text = SerializeProgress(sks, inv, CombatStyle::Accurate, &worn, &bank);
    Skills sks2;
    Inventory inv2;
    Equipment worn2;
    Bank bank2;
    ParseProgress(text, &sks2, &inv2, nullptr, &worn2, &bank2);
    CHECK(inv2.Count(Item::AdamantBar) == 3 && worn2.Get(EquipSlot::Body) == GearItem(Tier::Adamant, GearType::Body) && bank2.Count(Item::AdamantOre) == 7);
}

// Five more fish: where they bite, when they can be cooked, how much they heal.
static void TestMoreFish() {
    struct F {
        Item raw, cooked;
        const char* spot;
        int fishLevel, cookLevel;
    };
    const F fish[] = {
        {Item::RawSardine, Item::Sardine, "net", 5, 5},     {Item::RawHerring, Item::Herring, "net", 10, 10},
        {Item::RawPike, Item::Pike, "rod", 25, 20},         {Item::RawBass, Item::Bass, "cage", 46, 43},
        {Item::RawShark, Item::Shark, "harpoon", 76, 80},
    };
    for (const F& f : fish) {
        const SpotInfo& sp = GetSpotInfo(f.spot);
        const FishDef* d = FindFish(f.raw);
        CHECK(d != nullptr && d->level == f.fishLevel && d->xp > 0);
        bool here = false;
        for (const FishDef& x : sp.fish) here = here || x.raw == f.raw;
        CHECK(here);
        bool caught99 = false, caughtBelow = false;
        for (int i = 0; i < 1000; ++i) {
            const double roll = i / 1000.0;
            caught99 = caught99 || RollSpotCatch(sp, 99, roll) == f.raw;
            caughtBelow = caughtBelow || RollSpotCatch(sp, f.fishLevel - 1, roll) == f.raw;
        }
        CHECK(caught99 && !caughtBelow);
        const Recipe* r = RecipeFor(f.raw);
        CHECK(r != nullptr && r->cooked == f.cooked && r->level == f.cookLevel && r->xp > 0);
        CHECK(GetItemInfo(f.raw).raw && !GetItemInfo(f.raw).food && GetItemInfo(f.cooked).food && GetItemInfo(f.cooked).heals > 0);
        CHECK(ItemValue(f.cooked) > ItemValue(f.raw) && ItemValue(f.raw) > 0);
        CHECK(BurnChance(*r, r->level) > 0.0f && BurnChance(*r, r->burnStopLevel) == 0.0f);
    }
    CHECK(GetItemInfo(Item::Shark).heals > GetItemInfo(Item::Swordfish).heals && GetItemInfo(Item::Pike).heals > GetItemInfo(Item::Herring).heals);
    // the shark needs a harpoon, a high Fishing level, and is rare: with the lower fish it is the smallest share
    int weightShark = 0, total = 0;
    for (const FishDef& x : GetSpotInfo("harpoon").fish) {
        total += x.weight;
        if (x.raw == Item::RawShark) weightShark = x.weight;
    }
    CHECK(weightShark > 0 && weightShark * 4 < total);
}

// Diamonds and dragonstones: cut, set in gold, and worn. And the unique jewellery.
static void TestRareGemsAndUniqueJewellery() {
    auto find = [](Item product) -> const CraftRecipe* {
        for (const CraftRecipe& r : AllCraftRecipes())
            if (r.product == product) return &r;
        return nullptr;
    };
    const CraftRecipe* cutD = find(Item::Diamond);
    const CraftRecipe* cutS = find(Item::Dragonstone);
    CHECK(cutD && cutD->kind == CraftKind::Gem && cutD->input[0] == Item::UncutDiamond && cutD->tool == Item::Chisel && cutD->level > find(Item::Ruby)->level);
    CHECK(cutS && cutS->input[0] == Item::UncutDragonstone && cutS->level > cutD->level);
    const CraftRecipe* ring = find(Item::DiamondRing);
    const CraftRecipe* amu = find(Item::DiamondAmulet);
    const CraftRecipe* dsa = find(Item::DragonstoneAmulet);
    CHECK(ring && ring->kind == CraftKind::Jewellery && ring->input[0] == Item::GoldBar && ring->input[1] == Item::Diamond && ring->tool == Item::RingMould);
    CHECK(amu && amu->tool == Item::AmuletMould && amu->input[1] == Item::Diamond);
    CHECK(dsa && dsa->tool == Item::AmuletMould && dsa->input[1] == Item::Dragonstone && dsa->level > amu->level);
    Inventory bag;
    bag.Add(Item::GoldBar);
    bag.Add(Item::Diamond);
    CHECK(CanCraft(*ring, bag) == 0);   // (no mould yet)
    bag.Add(Item::RingMould);
    CHECK(CanCraft(*ring, bag) == 1);
    // worn: the right slot, and each one is better than the gems below it
    EquipSlot es;
    CHECK(Equippable(Item::DiamondRing, &es) && es == EquipSlot::Ring && Equippable(Item::AbyssalRing, &es) && es == EquipSlot::Ring);
    for (Item a : {Item::DiamondAmulet, Item::DragonstoneAmulet, Item::MummysAmulet, Item::TrollToothAmulet})
        CHECK(Equippable(a, &es) && es == EquipSlot::Amulet);
    auto total = [](Item i) {
        const GearStats s = ItemStats(i);
        return s.attack + s.strength + s.defence + s.ranged + s.magic;
    };
    CHECK(total(Item::DragonstoneAmulet) > total(Item::DiamondAmulet) && total(Item::DiamondAmulet) > total(Item::RubyAmulet));
    CHECK(total(Item::DiamondRing) > total(Item::RubyRing) && total(Item::AbyssalRing) > total(Item::DiamondRing) - 4);
    CHECK(ItemStats(Item::MummysAmulet).strength > ItemStats(Item::RubyAmulet).strength && ItemStats(Item::TrollToothAmulet).defence > ItemStats(Item::RubyAmulet).defence);
    // the gems come from gem rocks and from the big monsters
    CHECK(RockYield(RockKind::Gems, 0.999) == Item::UncutDiamond);
    CHECK(DropChance(*FindLootTable("black_dragon"), Item::UncutDragonstone) > 0.0 && DropChance(*FindLootTable("dragon"), Item::UncutDragonstone) > 0.0);
    CHECK(DropChance(*FindLootTable("desert_mummy"), Item::MummysAmulet) > 0.0 && DropChance(*FindLootTable("ogre"), Item::TrollToothAmulet) > 0.0);
    CHECK(DropChance(*FindLootTable("hellhound"), Item::AbyssalRing) > 0.0);
    for (Item i : {Item::UncutDiamond, Item::Diamond, Item::DiamondRing, Item::DiamondAmulet, Item::UncutDragonstone, Item::Dragonstone, Item::DragonstoneAmulet,
                   Item::MummysAmulet, Item::TrollToothAmulet, Item::AbyssalRing})
        CHECK(ItemValue(i) > 500 && std::string(GetItemInfo(i).name).size() > 3 && std::string(GetItemInfo(i).model).size() > 2);
    // chiselling a diamond with the use-on rule opens the gem menu
    MiniWorld w;
    w.bag.Add(Item::Chisel);
    w.bag.Add(Item::UncutDiamond);
    w.game.BeginUse(w.bag.FindSlot(Item::Chisel));
    w.game.UseItemOnSlot(w.bag.FindSlot(Item::UncutDiamond));
    CHECK(w.ev.makeMenus == 1 && w.ev.lastMake.kind == MakeMenu::Kind::Craft);
}

// Blasts and Waves: the spellbook grows to sixteen spells, in order of level.
static void TestMoreSpells() {
    CHECK(SpellCount() == 16 && FindSpell("fire_blast") == 11 && FindSpell("fire_wave") == 15 && FindSpell("nope") == -1);
    for (int i = 1; i < SpellCount(); ++i) {
        const SpellInfo &a = GetSpell(i - 1), &b = GetSpell(i);
        CHECK(b.level > a.level && b.maxHit >= a.maxHit && b.xp > a.xp);
        CHECK(b.costCount >= 2 && b.cost[0].count >= 1);
    }
    int chaos[3] = {0, 0, 0};   // chaos runes in a bolt, a blast and a wave
    for (int i : {FindSpell("fire_bolt"), FindSpell("fire_blast"), FindSpell("fire_wave")}) {
        const SpellInfo& sp = GetSpell(i);
        for (int k = 0; k < sp.costCount; ++k)
            if (sp.cost[k].rune == Rune::Chaos) chaos[i == FindSpell("fire_bolt") ? 0 : (i == FindSpell("fire_blast") ? 1 : 2)] += sp.cost[k].count;
    }
    CHECK(chaos[0] == 1 && chaos[1] == 2 && chaos[2] == 4);
    Inventory bag;
    bag.Add(Item::FireRune, 5);
    bag.Add(Item::AirRune, 4);
    bag.Add(Item::ChaosRune, 2);
    const SpellInfo& fb = GetSpell(FindSpell("fire_blast"));
    CHECK(CheckCast(fb, bag, Item::Staff, 22) == CastCheck::LevelTooLow && CheckCast(fb, bag, Item::Staff, 23) == CastCheck::Ok);
    CHECK(CheckCast(fb, bag, Item::FireStaff, 23) == CastCheck::Ok);
    bag.Remove(Item::ChaosRune, 1);
    CHECK(CheckCast(fb, bag, Item::Staff, 23) == CastCheck::NoRunes);
    // the save file names spells, so the order may grow without breaking old saves
    CHECK(FindSpell("fire_strike") == 3 && FindSpell("fire_bolt") == 7);
}

// The numbers the design document promises: how many monsters, how many drop rows, how many quests.
static void TestGddTargets() {
    int rows = 0;
    for (const LootTable& t : AllLootTables()) rows += static_cast<int>(t.drops.size());
    CHECK(kSpeciesCount >= 34);                       // 30 monsters + the 4 roaming fighters of the Abyss
    CHECK(rows >= 200);                               // "about 200 drop rows across all monster tables"
    CHECK(AllQuests().size() >= 20);                  // "20 quests"
    std::printf("  content: %d species, %d drop rows, %zu quests, %d items (the design document aims for about 300)\n", kSpeciesCount, rows,
                AllQuests().size(), static_cast<int>(Item::Count));
    CHECK(static_cast<int>(Item::Count) >= 170);
    // every species has a loot table, and no two share a key or a name
    for (int i = 0; i < kSpeciesCount; ++i)
        for (int j = i + 1; j < kSpeciesCount; ++j) {
            CHECK(std::string(GetSpecies(static_cast<Species>(i)).key) != GetSpecies(static_cast<Species>(j)).key);
            CHECK(std::string(GetSpecies(static_cast<Species>(i)).name) != GetSpecies(static_cast<Species>(j)).name);
        }
}

static bool g_obtainableBuilt = false;
static std::vector<char> g_obtainable;
static void BuildObtainable() {
    if (g_obtainableBuilt) return;
    g_obtainableBuilt = true;
    g_obtainable.assign(static_cast<size_t>(Item::Count), 0);
    auto mark = [](Item i) {
        if (i != Item::None) g_obtainable[static_cast<size_t>(i)] = 1;
    };
    for (const ShopDef& s : AllShopDefs())
        for (const ShopStockDef& st : s.stock) mark(st.item);
    for (const LootTable& t : AllLootTables())
        for (const LootDrop& d : t.drops) mark(d.item);
    for (const char* kind : {"net", "rod", "cage", "harpoon"})
        for (const FishDef& f : GetSpotInfo(kind).fish) mark(f.raw);
    for (int k = 0; k < kRockKindCount; ++k) {
        mark(GetRockInfo(static_cast<RockKind>(k)).ore);
        if (static_cast<RockKind>(k) == RockKind::Gems)
            for (double r : {0.0, 0.3, 0.45, 0.6, 0.8, 0.9, 0.99}) mark(RockYield(RockKind::Gems, r));
    }
    for (const char* kind : {"round", "oak", "willow", "teak", "maple", "mahogany", "yew", "magic"}) mark(GetTreeInfo(kind).logs);
    for (const CraftRecipe& r : AllCraftRecipes()) mark(r.product);
    for (const QuestInfo& q : AllQuests())
        for (const QuestStep& st : q.steps) mark(st.give);   // (what a spade turns up)
    for (int t = 0; t < kTierCount; ++t)
        for (int g = 0; g < kGearTypeCount; ++g) mark(GearItem(static_cast<Tier>(t), static_cast<GearType>(g)));
    for (int i = 0; i < kSmeltCount; ++i) mark(GetSmeltRecipe(static_cast<Tier>(i)).bar);
    for (int i = 1; i < static_cast<int>(Item::Count); ++i) {
        const Item it = static_cast<Item>(i);
        if (const Recipe* r = RecipeFor(it)) {
            mark(r->cooked);
            mark(r->burnt);
        }
    }
    Inventory kit;
    GiveStarterKit(&kit);
    for (int i = 0; i < Inventory::kSlots; ++i) mark(kit.Slot(i).item);
    mark(Item::Coins);
}

// The gate "a new character can finish every quest": on the real map, every quest's people, places and creatures are
// there, and every item it asks for can be got somewhere (a shop, a monster, a rock, a tree, a fishing spot, a workshop).
static void TestEveryQuestCanBeFinished(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping quest-reach test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    CHECK(m.ParseText(ss.str(), nullptr));
    BuildObtainable();
    std::vector<std::string> people, areas, species;
    for (const PersonDef& p : m.Persons()) people.push_back(p.key);
    for (const AreaDef& a : m.Areas()) areas.push_back(a.name);
    for (const NpcDef& n : m.Npcs()) species.push_back(n.species);
    auto has = [](const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); };
    const std::vector<QuestInfo>& all = AllQuests();
    for (size_t qi = 0; qi < all.size(); ++qi) {
        const QuestInfo& q = all[qi];
        if (!has(people, q.giver)) {
            std::printf("  FAIL quest %s: its giver %s is not on the map\n", q.key.c_str(), q.giver.c_str());
            ++g_failed;
        }
        // the chain of "after" quests ends (no cycle) and each link is an earlier quest
        int hops = 0;
        for (const QuestInfo* cur = &q; !cur->after.empty() && hops < 100; ++hops) cur = &all[static_cast<size_t>(FindQuest(cur->after))];
        CHECK(hops < 100);
        for (const QuestStep& st : q.steps) {
            switch (st.kind) {
                case StepKind::Kill:
                    if (!has(species, GetSpecies(st.species).key)) {
                        std::printf("  FAIL quest %s: no %s on the map\n", q.key.c_str(), GetSpecies(st.species).key);
                        ++g_failed;
                    }
                    break;
                case StepKind::Use:
                    if (!g_obtainable[static_cast<size_t>(st.item)] || (st.tool != Item::None && !g_obtainable[static_cast<size_t>(st.tool)])) {
                        std::printf("  FAIL quest %s: nobody sells, drops or makes %s\n", q.key.c_str(), GetItemInfo(st.item).name);
                        ++g_failed;
                    }
                    [[fallthrough]];
                case StepKind::Reach:
                    if (!has(areas, st.area)) {
                        std::printf("  FAIL quest %s: no place called %s on the map\n", q.key.c_str(), st.area.c_str());
                        ++g_failed;
                    }
                    break;
                case StepKind::Have:
                    if (!g_obtainable[static_cast<size_t>(st.item)]) {
                        std::printf("  FAIL quest %s: nobody sells, drops or makes %s\n", q.key.c_str(), GetItemInfo(st.item).name);
                        ++g_failed;
                    }
                    [[fallthrough]];
                case StepKind::Talk:
                    if (!has(people, st.person)) {
                        std::printf("  FAIL quest %s: %s is not on the map\n", q.key.c_str(), st.person.c_str());
                        ++g_failed;
                    }
                    break;
            }
        }
    }
    // and every kind of creature lives somewhere on the map (the design document's 30 + 4)
    for (int i = 0; i < kSpeciesCount; ++i)
        if (!has(species, GetSpecies(static_cast<Species>(i)).key)) {
            std::printf("  FAIL no %s anywhere on the map\n", GetSpecies(static_cast<Species>(i)).key);
            ++g_failed;
        }
    // every item the monsters drop is a real item with a name (a typo would be Item::None)
    for (const LootTable& t : AllLootTables())
        for (const LootDrop& d : t.drops) CHECK(d.item != Item::None && d.minCount >= 1 && d.maxCount >= d.minCount);
}


// ---------------------------------------------------------------------------------------------- round 10: the workshop round

static void TestMoreSmithedTypes() {
    // every (metal, type) is its own item and goes back to where it came from
    std::set<int> seen;
    for (int t = 0; t < kTierCount; ++t)
        for (int g = 0; g < kGearTypeCount; ++g) {
            const Tier tier = static_cast<Tier>(t);
            const GearType type = static_cast<GearType>(g);
            const Item it = GearItem(tier, type);
            CHECK(it != Item::None && seen.insert(static_cast<int>(it)).second);
            if (type == GearType::Arrows) continue;   // (arrows are ammunition, not gear)
            Tier t2;
            GearType g2;
            CHECK(GearOf(it, &t2, &g2) && t2 == tier && g2 == type);
        }
    CHECK(static_cast<int>(seen.size()) == kTierCount * kGearTypeCount);
    // names, slots and swings
    CHECK(std::string(GetItemInfo(GearItem(Tier::Steel, GearType::Scimitar)).name) == "Steel scimitar");
    CHECK(std::string(GetItemInfo(GearItem(Tier::Mithril, GearType::SquareShield)).name) == "Mithril square shield");
    CHECK(std::string(GetItemInfo(GearItem(Tier::Iron, GearType::Chainbody)).name) == "Iron chainbody");
    CHECK(std::string(GetItemInfo(GearItem(Tier::Adamant, GearType::Warhammer)).model) == "adamant_warhammer");
    EquipSlot slot;
    CHECK(Equippable(GearItem(Tier::Iron, GearType::Fullhelm), &slot) && slot == EquipSlot::Head);
    CHECK(Equippable(GearItem(Tier::Iron, GearType::Chainbody), &slot) && slot == EquipSlot::Body);
    CHECK(Equippable(GearItem(Tier::Iron, GearType::SquareShield), &slot) && slot == EquipSlot::Shield);
    for (GearType g : {GearType::Scimitar, GearType::Longsword, GearType::Spear, GearType::Warhammer})
        CHECK(Equippable(GearItem(Tier::Bronze, g), &slot) && slot == EquipSlot::Weapon);
    CHECK(AnimOf(GearType::Scimitar) == WeaponAnim::Slash && AnimOf(GearType::Longsword) == WeaponAnim::Slash);
    CHECK(AnimOf(GearType::Spear) == WeaponAnim::Stab && AnimOf(GearType::Warhammer) == WeaponAnim::Crush);
    // each piece keeps its character, in every metal
    for (int t = 0; t < kTierCount; ++t) {
        const Tier tier = static_cast<Tier>(t);
        auto st = [&](GearType g) { return GetGearStats(tier, g); };
        CHECK(st(GearType::Scimitar).attack > st(GearType::Sword).attack && st(GearType::Scimitar).strength < st(GearType::Sword).strength);
        CHECK(st(GearType::Longsword).strength > st(GearType::Sword).strength);
        CHECK(st(GearType::Warhammer).strength > st(GearType::Axe).strength && st(GearType::Warhammer).attack < st(GearType::Mace).attack);
        CHECK(st(GearType::Spear).attack > 0 && st(GearType::Spear).strength > 0);
        CHECK(st(GearType::Fullhelm).defence > st(GearType::Helm).defence);
        CHECK(st(GearType::SquareShield).defence > st(GearType::Shield).defence);
        CHECK(st(GearType::Chainbody).defence < st(GearType::Body).defence && st(GearType::Chainbody).defence > st(GearType::Legs).defence);
        if (t > 0) {
            const Tier lower = static_cast<Tier>(t - 1);
            for (int g : {11, 12, 13, 14}) CHECK(GetGearStats(tier, static_cast<GearType>(g)).strength > GetGearStats(lower, static_cast<GearType>(g)).strength);
            for (int g : {15, 16, 17}) CHECK(GetGearStats(tier, static_cast<GearType>(g)).defence > GetGearStats(lower, static_cast<GearType>(g)).defence);
        }
    }
    // the smith: bars, levels (the metal's first level + an offset) and a price that follows the bars
    const SmithRecipe sc = GetSmithRecipe(Tier::Steel, GearType::Scimitar);
    CHECK(sc.bars == 2 && sc.level == 36 && sc.product == GearItem(Tier::Steel, GearType::Scimitar) && sc.xp == 2 * 38);
    CHECK(GetSmithRecipe(Tier::Adamant, GearType::Chainbody).level == 83 && GetSmithRecipe(Tier::Bronze, GearType::Warhammer).bars == 4);
    CHECK(ItemValue(GearItem(Tier::Steel, GearType::Warhammer)) == static_cast<int>(4 * 130 * 1.15 + 0.5));
    Skill sk;
    int lv = 0;
    CHECK(ItemRequirement(GearItem(Tier::Steel, GearType::Spear), &sk, &lv) && sk == Skill::Attack && lv == 20);
    CHECK(ItemRequirement(GearItem(Tier::Mithril, GearType::Fullhelm), &sk, &lv) && sk == Skill::Defence && lv == 30);
    MiniWorld w;
    CHECK(w.game.BuildSmithMenu(Tier::Iron).options.size() == static_cast<size_t>(kGearTypeCount));
    // wearing one: it needs the level, then it goes in its slot and feeds the combat numbers
    w.bag.Add(GearItem(Tier::Steel, GearType::Chainbody));
    w.game.DoItemAction(w.bag.FindSlot(GearItem(Tier::Steel, GearType::Chainbody)), ItemAction::Wear);
    CHECK(w.equip.Get(EquipSlot::Body) == Item::None && w.ev.Said("Defence level 20"));
    w.skills.SetXp(Skill::Defence, XpForLevel(20));
    w.game.DoItemAction(w.bag.FindSlot(GearItem(Tier::Steel, GearType::Chainbody)), ItemAction::Wear);
    CHECK(w.equip.Get(EquipSlot::Body) == GearItem(Tier::Steel, GearType::Chainbody));
    // the shops of Millbrook sell the new common pieces
    int stocked = 0;
    for (const ShopDef& s : AllShopDefs())
        for (const ShopStockDef& d : s.stock) {
            Tier t3;
            GearType g3;
            if (GearOf(d.item, &t3, &g3) && static_cast<int>(g3) >= kExtraTypeFirst) ++stocked;
        }
    CHECK(stocked >= 18);   // (scimitar, longsword, spear, full helm, square shield, chainbody x bronze, iron, steel)
}

static void TestKitchenAndWoodwork() {
    MiniWorld w;
    SkillWorld(&w, "");
    w.bag.Add(Item::Flour, 3);
    w.bag.Add(Item::JugOfWater, 2);
    // flour used with water opens the kitchen
    w.game.DoItemAction(w.bag.FindSlot(Item::Flour), ItemAction::Use);
    CHECK(w.game.UseSlot() >= 0);
    w.game.UseItemOnSlot(w.bag.FindSlot(Item::JugOfWater));
    CHECK(w.ev.makeMenus == 1 && w.ev.lastMake.title == "Kitchen" && w.ev.lastMake.options.size() == 11);
    auto optionFor = [](const MakeMenu& m, Item product) {
        for (const MakeOption& o : m.options)
            if (o.product == product) return o.index;
        return -1;
    };
    const int dough = optionFor(w.ev.lastMake, Item::BreadDough);
    CHECK(dough >= 0);
    w.game.StartCraft(dough, 1);
    w.Run(4.0f);
    CHECK(w.bag.Count(Item::BreadDough) == 1 && w.bag.Count(Item::Flour) == 2 && w.bag.Count(Item::JugOfWater) == 1);
    CHECK(w.skills.Xp(Skill::Cooking) == 4 && w.skills.Xp(Skill::Crafting) == 0);   // (cooking trains Cooking, not Crafting)
    // dough bakes into bread, like the shop's loaf
    CHECK(GetItemInfo(Item::BreadDough).raw && RecipeFor(Item::BreadDough) && RecipeFor(Item::BreadDough)->cooked == Item::Bread);
    // a pie needs the level and a pie dish (which is kept)
    const int apple = optionFor(w.ev.lastMake, Item::RawApplePie);
    w.bag.Add(Item::PastryDough);
    w.bag.Add(Item::CookingApple);
    w.game.StartCraft(apple, 1);
    CHECK(w.ev.Said("Cooking level of 30") && w.game.Current() == Activity::None);
    w.skills.SetXp(Skill::Cooking, XpForLevel(30));
    w.game.StartCraft(apple, 1);
    CHECK(w.ev.Said("pie dish") && w.game.Current() == Activity::None);
    w.bag.Add(Item::PieDish);
    w.game.StartCraft(apple, 1);
    w.Run(4.0f);
    CHECK(w.bag.Has(Item::RawApplePie) && w.bag.Has(Item::PieDish) && !w.bag.Has(Item::PastryDough) && !w.bag.Has(Item::CookingApple));
    // every dish has a baking recipe: a raw thing, a cooked food that heals, and something burnt
    for (Item raw : {Item::RawMeatPie, Item::RawApplePie, Item::RawRedberryPie, Item::RawGardenPie, Item::RawFishPie, Item::UncookedStew, Item::UncookedCake,
                     Item::RawPizza, Item::BreadDough, Item::RawRatMeat, Item::RawWolfMeat}) {
        const Recipe* r = RecipeFor(raw);
        CHECK(r && GetItemInfo(raw).raw && GetItemInfo(r->cooked).food && GetItemInfo(r->cooked).heals > 0 && r->burnt != Item::None);
    }
    CHECK(GetItemInfo(Item::Cake).heals > GetItemInfo(Item::MeatPie).heals && GetItemInfo(Item::FishPie).heals > GetItemInfo(Item::ApplePie).heals);

    // woodwork: a knife carves logs into an unstrung bow (Woodcutting level and XP), a bowstring finishes it
    MiniWorld f;
    SkillWorld(&f, "");
    f.bag.Add(Item::Knife);
    f.bag.Add(Item::Logs, 4);
    f.game.DoItemAction(f.bag.FindSlot(Item::Knife), ItemAction::Use);
    f.game.UseItemOnSlot(f.bag.FindSlot(Item::Logs));
    CHECK(f.ev.makeMenus == 1 && f.ev.lastMake.title == "Woodwork" && f.ev.lastMake.options.size() == 12);
    const int longbow = optionFor(f.ev.lastMake, Item::UnstrungLongbow);
    f.game.StartCraft(longbow, 1);
    CHECK(f.ev.Said("Woodcutting level of 10") && f.bag.Count(Item::Logs) == 4);
    f.skills.SetXp(Skill::Woodcutting, XpForLevel(10));
    const int64_t before = f.skills.Xp(Skill::Woodcutting);
    f.game.StartCraft(longbow, 1);
    f.Run(4.0f);
    CHECK(f.bag.Has(Item::UnstrungLongbow) && f.bag.Count(Item::Logs) == 2 && f.bag.Has(Item::Knife) && f.skills.Xp(Skill::Woodcutting) == before + 15);
    f.bag.Add(Item::Bowstring);
    f.game.DoItemAction(f.bag.FindSlot(Item::Bowstring), ItemAction::Use);
    f.game.UseItemOnSlot(f.bag.FindSlot(Item::UnstrungLongbow));
    CHECK(f.ev.makeMenus == 2 && f.ev.lastMake.title == "Woodwork");
    f.game.StartCraft(optionFor(f.ev.lastMake, Item::Longbow), 1);
    f.Run(4.0f);
    CHECK(f.bag.Has(Item::Longbow) && !f.bag.Has(Item::UnstrungLongbow) && !f.bag.Has(Item::Bowstring));
    // all six bows can be made, level by level
    for (Item bow : {Item::Shortbow, Item::Longbow, Item::OakShortbow, Item::OakLongbow, Item::YewShortbow, Item::YewLongbow}) {
        int n = 0;
        for (const CraftRecipe& r : AllCraftRecipes())
            if (r.product == bow && r.kind == CraftKind::Fletching && r.skill == Skill::Woodcutting) ++n;
        CHECK(n == 1);
    }
}

static void TestRoundTenSkillingData() {
    // more trees: each a step up in level, XP, time to regrow; their logs burn at the same level
    const char* kinds[] = {"round", "oak", "willow", "teak", "maple", "mahogany", "yew", "magic"};
    int lastLevel = 0;
    for (const char* k : kinds) {
        const TreeInfo& t = GetTreeInfo(k);
        CHECK(std::string(t.kind) == k && t.level >= lastLevel && IsLog(t.logs) && GetLogInfo(t.logs)->fmLevel == t.level);
        lastLevel = t.level;
    }
    CHECK(GetTreeInfo("magic").level == 75 && GetTreeInfo("magic").logs == Item::MagicLogs && GetTreeInfo("maple").xp > GetTreeInfo("willow").xp);
    CHECK(GetTreeInfo("magic").xp == 250 && GetLogInfo(Item::MagicLogs)->fmXp > GetLogInfo(Item::YewLogs)->fmXp);
    // more fish: where, from what level, and what they are cooked into
    CHECK(FindFish(Item::RawMantaRay) && FindFish(Item::RawMantaRay)->level == 81 && FindFish(Item::RawSeaTurtle)->level == 79);
    CHECK(FindFish(Item::RawMackerel)->level == 16 && FindFish(Item::RawCod)->level == 23 && FindFish(Item::RawEel)->level == 28);
    for (Item raw : {Item::RawMackerel, Item::RawCod, Item::RawEel, Item::RawMantaRay, Item::RawSeaTurtle}) {
        const Recipe* r = RecipeFor(raw);
        CHECK(r && GetItemInfo(raw).raw && GetItemInfo(r->cooked).food && r->level >= 16);
    }
    CHECK(GetItemInfo(Item::MantaRay).heals > GetItemInfo(Item::Shark).heals && GetItemInfo(Item::SeaTurtle).heals > GetItemInfo(Item::Swordfish).heals);
    // gem rocks give the plainer stones too, and a chisel cuts each from its own level
    for (Item uncut : {Item::UncutOpal, Item::UncutJade, Item::UncutRedTopaz}) {
        int n = 0;
        for (const CraftRecipe& r : AllCraftRecipes())
            if (r.kind == CraftKind::Gem && r.input[0] == uncut && r.tool == Item::Chisel) ++n;
        CHECK(n == 1);
    }
    // hard and studded leather: worn in the same slots, better each step, made at the tanner and with a needle
    EquipSlot slot;
    CHECK(Equippable(Item::HardLeatherBody, &slot) && slot == EquipSlot::Body && Equippable(Item::StuddedChaps, &slot) && slot == EquipSlot::Legs);
    CHECK(Equippable(Item::StuddedCap, &slot) && slot == EquipSlot::Head);
    CHECK(ItemStats(Item::HardLeatherBody).defence > ItemStats(Item::LeatherBody).defence && ItemStats(Item::StuddedBody).defence > ItemStats(Item::HardLeatherBody).defence);
    CHECK(ItemStats(Item::StuddedBody).ranged > ItemStats(Item::HardLeatherBody).ranged && std::string(FindLeather(Item::StuddedBody)->model) == "gear_studded_body");
    int tanned = 0;
    for (const CraftRecipe& r : AllCraftRecipes())
        if (r.kind == CraftKind::Tan) ++tanned;
    CHECK(tanned == 2);
    // the gate "each skill has at least 15 gather, make or train actions", counted from the data: what each skill can do
    std::set<int> woodcutting, mining, fishing;
    for (const char* k : kinds) woodcutting.insert(static_cast<int>(GetTreeInfo(k).logs));
    int carving = 0, cooking = 0, crafting = 0;
    for (const CraftRecipe& r : AllCraftRecipes()) {
        if (r.skill == Skill::Woodcutting) ++carving;
        if (r.skill == Skill::Cooking) ++cooking;
        if (r.skill == Skill::Crafting) ++crafting;
    }
    for (int i = 1; i < static_cast<int>(Item::Count); ++i)
        if (RecipeFor(static_cast<Item>(i))) ++cooking;
    for (int k = 0; k < kRockKindCount; ++k) {
        const RockKind rk = static_cast<RockKind>(k);
        if (rk == RockKind::Gems) {
            for (double r = 0.0; r < 1.0; r += 0.01) mining.insert(static_cast<int>(RockYield(rk, r)));
        } else {
            mining.insert(static_cast<int>(GetRockInfo(rk).ore));
        }
    }
    for (const char* kind : {"net", "rod", "cage", "harpoon"})
        for (const FishDef& f : GetSpotInfo(kind).fish) fishing.insert(static_cast<int>(f.raw));
    // (Firemaking: every kind of log can be lit as a campfire or, four at a time, as a bonfire)
    const int actions[] = {static_cast<int>(woodcutting.size()) + carving, static_cast<int>(mining.size()), static_cast<int>(fishing.size()), cooking, crafting,
                           kSmeltCount + kGearTypeCount * kTierCount, static_cast<int>(woodcutting.size()) * 2};
    const char* names[] = {"Woodcutting", "Mining", "Fishing", "Cooking", "Crafting", "Smithing", "Firemaking"};
    std::printf("  skill actions:");
    for (int i = 0; i < 7; ++i) std::printf(" %s %d%s", names[i], actions[i], i < 6 ? "," : "\n");
    for (int i = 0; i < 7; ++i) CHECK(actions[i] >= 15);   // the design document's gate, for every skill
    CHECK(kSpeciesCount >= 15 && SpellCount() >= 15);
}

static bool HasAction(const std::vector<ItemAction>& v, ItemAction a) { return std::find(v.begin(), v.end(), a) != v.end(); }

static void TestBonfires() {
    MiniWorld w;
    SkillWorld(&w, "");
    w.bag.Add(Item::Tinderbox);
    w.bag.Add(Item::Logs, 3);
    CHECK(!HasAction(ActionsFor(Item::Logs, w.bag), ItemAction::Bonfire));      // three logs make no bonfire
    w.bag.Add(Item::Logs, 1);
    CHECK(HasAction(ActionsFor(Item::Logs, w.bag), ItemAction::Bonfire) && std::string(ActionName(ItemAction::Bonfire)) == "Bonfire");
    const TileCoord spot = w.player.Tile();
    w.rollValue = 0.0;
    w.game.DoItemAction(w.bag.FindSlot(Item::Logs), ItemAction::Bonfire);
    CHECK(w.game.Current() == Activity::Lighting && w.ev.Said("bonfire"));
    w.Run(3.0f);
    const Fire* f = w.fires.BurningAt(spot);
    CHECK(f && f->big && f->burnLeft > kFireBurnSeconds * 3.0f);
    CHECK(w.bag.Count(Item::Logs) == 0);                                       // all four went on the fire
    CHECK(w.skills.Xp(Skill::Firemaking) == static_cast<int64_t>(kLightXp * kBonfireLogs * kBonfireXpFactor) && w.ev.Said("bonfire roars"));
    // not enough logs: nothing happens
    MiniWorld v;
    SkillWorld(&v, "");
    v.bag.Add(Item::Tinderbox);
    v.bag.Add(Item::Logs, 2);
    v.game.DoItemAction(v.bag.FindSlot(Item::Logs), ItemAction::Bonfire);
    CHECK(v.ev.Said("needs 4 logs") && v.game.Current() == Activity::None && v.bag.Count(Item::Logs) == 2);
    // a bonfire cooks like a range: food that a campfire would burn is safe on it
    Fires fires;
    TileMap map = TileMap::FromRows({"....", "....", "...."});
    const int camp = fires.Light(&map, {0, 0}, false), big = fires.Light(&map, {2, 0}, true);
    CHECK(camp >= 0 && big >= 0 && !fires.Find(camp)->big && fires.Find(big)->big && fires.Find(big)->burnLeft == kBonfireBurnSeconds);
}

static void TestUseItemQuests() {
    const int chest = FindQuest("the_buried_chest");
    CHECK(chest >= 0 && AllQuests()[static_cast<size_t>(chest)].steps[0].kind == StepKind::Use);
    CHECK(AllQuests().size() >= 36);
    // a quest never ends on a "use" step (someone always thanks you), and every Use step names an item and a place
    for (const QuestInfo& q : AllQuests()) {
        CHECK(q.steps.back().kind == StepKind::Have || q.steps.back().kind == StepKind::Talk);
        for (const QuestStep& st : q.steps)
            if (st.kind == StepKind::Use) CHECK(st.item != Item::None && !st.area.empty());
    }
    // the log
    QuestLog log;
    log.Start(chest);
    CHECK(log.WantsItem(Item::Spade) && !log.WantsItem(Item::RustyKey) && log.FindUse(Item::Spade, "Lakeside") == chest);
    CHECK(log.FindUse(Item::Spade, "Castle Crypt") == -1 && log.FindUse(Item::Candle, "Lakeside") == -1);
    // in the game: a tap on the ground uses the item while a quest is waiting for it
    MiniWorld w;
    SkillWorld(&w, "@area 0 0 12 5 Lakeside\n");
    w.game.Quests().Start(chest);
    w.bag.Add(Item::Spade);
    w.game.DoItemAction(w.bag.FindSlot(Item::Spade), ItemAction::Use);
    std::vector<MenuEntry> menu = w.game.BuildMenu({}, true, 3.5f, 2.5f);
    CHECK(menu.size() >= 2 && menu[0].act == MenuAct::UseHere && menu[0].Text().find("Use Spade here") == 0);
    w.game.Perform(menu[0]);
    CHECK(w.bag.Has(Item::RustyKey) && w.bag.Has(Item::Spade) && w.game.Quests().State(chest).step == 1);
    CHECK(w.ev.Said("You find a rusty key") && w.ev.Said("You dig"));
    // the key belongs in the crypt: here it does nothing, and stays in the bag
    w.game.DoItemAction(w.bag.FindSlot(Item::RustyKey), ItemAction::Use);
    menu = w.game.BuildMenu({}, true, 3.5f, 2.5f);
    CHECK(menu[0].act == MenuAct::UseHere);
    w.game.Perform(menu[0]);
    CHECK(w.bag.Has(Item::RustyKey) && w.game.Quests().State(chest).step == 1 && w.ev.Said("not the right place"));
    // an item no quest wants is just an item: the ground tap walks
    w.bag.Add(Item::Knife);
    w.game.DoItemAction(w.bag.FindSlot(Item::Knife), ItemAction::Use);
    menu = w.game.BuildMenu({}, true, 3.5f, 2.5f);
    CHECK(menu[0].act == MenuAct::WalkHere);
    w.game.Perform(menu[0]);   // (walks off: that cancels the use)
    CHECK(w.game.UseSlot() < 0);
    // the second step, in the crypt: the key is used up and the log is found
    MiniWorld c;
    SkillWorld(&c, "@area 0 0 12 5 Castle Crypt\n");
    c.game.Quests().Start(chest);
    c.game.Quests().CompleteStep(chest, &c.bag);   // (the dig)
    c.bag.Add(Item::RustyKey);
    c.game.DoItemAction(c.bag.FindSlot(Item::RustyKey), ItemAction::Use);
    c.game.Perform(c.game.BuildMenu({}, true, 3.5f, 2.5f)[0]);
    CHECK(!c.bag.Has(Item::RustyKey) && c.bag.Has(Item::CaptainsLog) && c.game.Quests().Current(chest)->kind == StepKind::Have);
    // light the way: a candle is used up, and it needs a tinderbox
    MiniWorld l;
    SkillWorld(&l, "@area 0 0 12 5 Millbrook Chapel\n");
    const int way = FindQuest("light_the_way");
    l.game.Quests().Start(way);
    l.bag.Add(Item::Candle, 3);
    l.game.DoItemAction(l.bag.FindSlot(Item::Candle), ItemAction::Use);
    l.game.Perform(l.game.BuildMenu({}, true, 3.5f, 2.5f)[0]);
    CHECK(l.bag.Count(Item::Candle) == 3 && l.ev.Said("tinderbox") && l.game.Quests().State(way).step == 0);
    l.bag.Add(Item::Tinderbox);
    l.game.DoItemAction(l.bag.FindSlot(Item::Candle), ItemAction::Use);
    l.game.Perform(l.game.BuildMenu({}, true, 3.5f, 2.5f)[0]);
    CHECK(l.bag.Count(Item::Candle) == 2 && l.bag.Has(Item::Tinderbox) && l.game.Quests().State(way).step == 1);
}

// The gate "a simulated 10,000 kills match the drop rates in the tables": roll every table 10,000 times and compare with the
// numbers the table promises (Always drops every time; the others by weight; the chance of getting nothing).
static void TestLootRatesMatchTables() {
    std::mt19937 rng(20261119);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    auto roll = [&]() { return dist(rng); };
    int checkedItems = 0;
    for (const LootTable& t : AllLootTables()) {
        constexpr int kKills = 10000;
        std::map<int, int> got;
        for (int k = 0; k < kKills; ++k) {
            std::set<int> thisKill;
            for (const ItemStack& s : RollLootTable(t, roll)) thisKill.insert(static_cast<int>(s.item));
            for (int it : thisKill) ++got[it];
        }
        std::set<int> items;
        for (const LootDrop& d : t.drops) items.insert(static_cast<int>(d.item));
        for (int it : items) {
            const double want = DropChance(t, static_cast<Item>(it));
            const double have = static_cast<double>(got[it]) / kKills;
            // five standard deviations of a coin flip with that chance, plus a little for rounding
            const double slack = 5.0 * std::sqrt(want * (1.0 - want) / kKills) + 0.003;
            if (std::fabs(have - want) > slack) {
                std::printf("  FAIL loot %s: %s drops %.4f of the time, the table says %.4f\n", t.key.c_str(), GetItemInfo(static_cast<Item>(it)).name, have, want);
                ++g_failed;
            }
            ++checkedItems;
        }
        // the Always tier really is always, and the weights add up to the table's chance of nothing
        int always = 0, weights = 0;
        for (const LootDrop& d : t.drops) {
            if (d.tier == LootTier::Always) {
                ++always;
                CHECK(DropChance(t, d.item) >= 0.999);
            } else {
                weights += d.weight;
                CHECK(d.weight > 0);
            }
        }
        CHECK(weights + t.nothing > 0 || always > 0);
    }
    CHECK(checkedItems >= 200);
    // a rare drop is rarer than a common one in the same table (weights 2 vs 60)
    const LootTable* gob = FindLootTable("goblin");
    CHECK(gob && DropChance(*gob, Item::FireRune) < DropChance(*gob, Item::AirRune));
}


// Every item has a model to lie on the ground as (the ground pile draws drop_<model>.glb: a missing one would be an invisible item),
// and the worn / held models of every piece of gear exist.
static void TestEveryItemHasAModel(const std::string& mapPath) {
    const std::string root = mapPath.substr(0, mapPath.find("maps")) + "models/";
    auto exists = [](const std::string& file) {
        std::ifstream f(file, std::ios::binary);
        return static_cast<bool>(f);
    };
    if (!exists(root + "props/drop_bones.glb")) {
        std::printf("  (skipping model test: %s not found)\n", root.c_str());
        return;
    }
    std::set<std::string> missing;
    for (int i = 1; i < static_cast<int>(Item::Count); ++i) {
        const Item it = static_cast<Item>(i);
        const std::string drop = std::string("props/drop_") + GetItemInfo(it).model + ".glb";
        if (!exists(root + drop)) missing.insert(drop);
        Tier t;
        GearType g;
        if (GearOf(it, &t, &g) && g != GearType::Pickaxe && g != GearType::Hatchet) {
            const std::string base = std::string(TierKey(t)) + "_" + GearTypeKey(g);
            const bool weapon = SlotOf(g) == EquipSlot::Weapon, shield = SlotOf(g) == EquipSlot::Shield;
            if (weapon || shield) {
                if (!exists(root + "character/held_" + base + ".glb")) missing.insert("character/held_" + base + ".glb");
            } else if (SlotOf(g) == EquipSlot::Head) {
                if (!exists(root + "character/gear_" + base + ".glb")) missing.insert("character/gear_" + base + ".glb");
            } else {
                std::vector<const char*> parts;
                switch (SlotOf(g)) {
                    case EquipSlot::Body: parts = {"_torso_m", "_arm_l", "_arm_r"}; break;
                    case EquipSlot::Feet: parts = {"_leg_l", "_leg_r"}; break;
                    case EquipSlot::Hands: parts = {"_arm_l", "_arm_r"}; break;
                    default: parts = {"_hips_m", "_leg_l", "_leg_r"}; break;
                }
                for (const char* part : parts)
                    if (!exists(root + "character/gear_" + base + part + ".glb")) missing.insert("character/gear_" + base + part + ".glb");
            }
        }
        if (const LeatherInfo* l = FindLeather(it)) {
            const std::string base = l->model;
            std::vector<const char*> parts;
            switch (static_cast<EquipSlot>(l->slot)) {
                case EquipSlot::Head: parts = {""}; break;
                case EquipSlot::Body: parts = {"_torso_m", "_arm_l", "_arm_r"}; break;
                case EquipSlot::Feet: parts = {"_leg_l", "_leg_r"}; break;
                case EquipSlot::Hands: parts = {"_arm_l", "_arm_r"}; break;
                default: parts = {"_hips_m", "_leg_l", "_leg_r"}; break;
            }
            for (const char* part : parts)
                if (!exists(root + "character/" + base + part + ".glb")) missing.insert("character/" + base + part + ".glb");
        }
        if (const CapeInfo* c = FindCape(it))
            if (!exists(root + "character/gear_cape_" + c->key + ".glb")) missing.insert(std::string("character/gear_cape_") + c->key + ".glb");
    }
    for (const std::string& m : missing) std::printf("  FAIL missing model: %s\n", m.c_str());
    g_failed += static_cast<int>(missing.size());
}


// ---------------------------------------------------------------------------------------------- the skills gate: level 10 in a test run
// "Skills: all 12 skills, the XP table, and every gather and make action. Gate: each skill reaches level 10 in a test run."
// A hero with the starter kit trains each skill the way a player would (tap the next thing when idle, wait), in a small world, with
// the dice really rolled. It must get to level 10 within a fair stretch of game time; the minutes are printed so the balance can be seen.

static void ClearBag(Inventory* bag, std::initializer_list<Item> junk) {
    for (Item it : junk)
        if (bag->Count(it) > 0) bag->Remove(it, bag->Count(it));
}

// A little world for the combat skills: a pen with a flock of chickens.
static void CombatWorld(MiniWorld* w) {
    std::string text = "@size 14 4\n@spawn 2 1\n@pen yard 6 0 14 4\n";
    for (int i = 0; i < 6; ++i) text += "@npc chicken " + std::to_string(7 + i) + ".5 " + std::to_string(i % 3 + 1) + ".5 yard\n";
    text += "@rows\n";
    for (int z = 0; z < 4; ++z) text += "..............\n";
    w->map.ParseText(text);
    w->npcs.Init(w->map);
    w->player.Teleport({2, 1});
}

// Plays for at most `capSeconds` of game time, calling `step` once a second; returns the seconds it took to reach level 10 (or -1).
static double TrainToTen(MiniWorld* w, Skill skill, double capSeconds, const std::function<void()>& step) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dice(0.0, 1.0);
    for (double t = 0; t < capSeconds; t += 1.0) {
        if (w->skills.Level(skill) >= 10) return t;
        w->rollValue = dice(rng);
        step();
        w->Run(1.0f);
    }
    if (w->skills.Level(skill) >= 10) return capSeconds;
    std::printf("    (%s ended at level %d with %lld xp)\n", SkillName(skill), w->skills.Level(skill), static_cast<long long>(w->skills.Xp(skill)));
    return -1.0;
}

static void TestEverySkillReachesLevelTen() {
    struct Result {
        const char* name;
        double seconds;
    };
    std::vector<Result> results;
    auto report = [&](const char* name, double seconds) {
        results.push_back({name, seconds});
        if (seconds < 0) {
            std::printf("  FAIL %s did not reach level 10 in the time allowed\n", name);
            ++g_failed;
        }
    };
    const double kHour = 3600.0;

    {   // woodcutting: six trees, a bronze axe
        MiniWorld w;
        SkillWorld(&w, "@tree 3.5 1.5 round 1.0 0\n@tree 5.5 1.5 round 1.0 0\n@tree 7.5 1.5 round 1.0 0\n@tree 9.5 1.5 round 1.0 0\n@tree 3.5 3.5 round 1.0 0\n@tree 5.5 3.5 round 1.0 0\n");
        GiveStarterKit(&w.bag);
        report("Woodcutting", TrainToTen(&w, Skill::Woodcutting, 2 * kHour, [&] {
            if (w.bag.Full()) ClearBag(&w.bag, {Item::Logs});
            if (w.game.Current() != Activity::None) return;
            for (size_t i = 0; i < w.forest.Trees().size(); ++i)
                if (!w.forest.Trees()[i].stump) {
                    w.game.TapTree(static_cast<int>(i));
                    return;
                }
        }));
    }
    {   // fishing: a net
        MiniWorld w;
        SkillWorld(&w, "@spot 3.5 1.5 net\n");
        GiveStarterKit(&w.bag);
        report("Fishing", TrainToTen(&w, Skill::Fishing, 2 * kHour, [&] {
            if (w.bag.FreeSlots() < 4) ClearBag(&w.bag, {Item::RawShrimps, Item::RawSardine, Item::RawHerring, Item::RawAnchovies});
            if (w.game.Current() == Activity::None) w.game.TapSpot(0);
        }));
    }
    {   // mining: copper and tin rocks that grow back
        MiniWorld w;
        SkillWorld(&w, "@rock 3.5 1.5 copper 0\n@rock 5.5 1.5 tin 0\n@rock 7.5 1.5 copper 0\n@rock 9.5 1.5 tin 0\n");
        GiveStarterKit(&w.bag);
        report("Mining", TrainToTen(&w, Skill::Mining, 2 * kHour, [&] {
            if (w.bag.Full()) ClearBag(&w.bag, {Item::CopperOre, Item::TinOre});
            if (w.game.Current() != Activity::None) return;
            for (size_t i = 0; i < w.rocks.All().size(); ++i)
                if (!w.rocks.All()[i].depleted) {
                    w.game.TapRock(static_cast<int>(i));
                    return;
                }
        }));
    }
    {   // firemaking: logs and a tinderbox
        MiniWorld w;
        SkillWorld(&w, "");
        GiveStarterKit(&w.bag);
        report("Firemaking", TrainToTen(&w, Skill::Firemaking, 2 * kHour, [&] {
            if (w.game.Current() != Activity::None) return;
            if (!w.bag.Has(Item::Logs)) w.bag.Add(Item::Logs, 10);
            w.game.DoItemAction(w.bag.FindSlot(Item::Logs), ItemAction::Light);
        }));
    }
    {   // cooking: shrimps on a fire
        MiniWorld w;
        SkillWorld(&w, "");
        GiveStarterKit(&w.bag);
        report("Cooking", TrainToTen(&w, Skill::Cooking, 2 * kHour, [&] {
            ClearBag(&w.bag, {Item::Shrimps, Item::BurntFish});
            if (w.game.Current() != Activity::None) return;
            if (!w.bag.Has(Item::RawShrimps)) w.bag.Add(Item::RawShrimps, 10);
            const Fire* f = w.fires.NearestBurning({3, 1});
            if (!f) {
                w.fires.Light(&w.map, {3, 1});
                f = w.fires.NearestBurning({3, 1});
            }
            if (f) w.game.TapFire(f->id);
        }));
    }
    {   // smithing: smelt bronze bars, hammer them into daggers
        MiniWorld w;
        SkillWorld(&w, "@station furnace 4.5 3.5 0\n@station anvil 7.5 3.5 0\n");
        GiveStarterKit(&w.bag);
        report("Smithing", TrainToTen(&w, Skill::Smithing, 2 * kHour, [&] {
            ClearBag(&w.bag, {GearItem(Tier::Bronze, GearType::Dagger)});
            if (w.game.Current() != Activity::None) return;
            if (w.bag.Has(Item::BronzeBar)) {
                w.player.Teleport({7, 2});
                w.game.StartSmith(Tier::Bronze, GearType::Dagger, 99);
            } else {
                ClearBag(&w.bag, {Item::CopperOre, Item::TinOre});   // (what is left over would fill the bag with one kind)
                w.bag.Add(Item::CopperOre, 11);
                w.bag.Add(Item::TinOre, 11);
                w.player.Teleport({4, 2});
                w.game.StartSmelt(Tier::Bronze, 99);
            }
        }));
    }
    {   // crafting: pots at the potter's wheel
        MiniWorld w;
        SkillWorld(&w, "@station wheel 4.5 3.5 0\n");
        GiveStarterKit(&w.bag);
        int pot = -1;
        for (size_t i = 0; i < AllCraftRecipes().size(); ++i)
            if (AllCraftRecipes()[i].product == Item::Pot) pot = static_cast<int>(i);
        report("Crafting", TrainToTen(&w, Skill::Crafting, 2 * kHour, [&] {
            ClearBag(&w.bag, {Item::Pot});
            if (w.game.Current() != Activity::None) return;
            if (!w.bag.Has(Item::Clay)) w.bag.Add(Item::Clay, 20);
            w.player.Teleport({4, 2});
            w.game.StartCraft(pot, 99);
        }));
    }
    {   // prayer: burying bones
        MiniWorld w;
        SkillWorld(&w, "");
        report("Prayer", TrainToTen(&w, Skill::Prayer, 2 * kHour, [&] {
            if (w.game.Current() != Activity::None) return;
            if (!w.bag.Has(Item::Bones)) w.bag.Add(Item::Bones, 20);
            w.game.DoItemAction(w.bag.FindSlot(Item::Bones), ItemAction::Bury);
        }));
    }
    // the combat skills, each with its own fighting style, against a flock of chickens
    struct Fight {
        const char* name;
        Skill skill;
        CombatStyle style;
        int weapon;   // 0 fists, 1 bow, 2 staff
    };
    const Fight fights[] = {{"Attack", Skill::Attack, CombatStyle::Accurate, 0},    {"Strength", Skill::Strength, CombatStyle::Aggressive, 0},
                            {"Defence", Skill::Defence, CombatStyle::Defensive, 0}, {"Ranged", Skill::Ranged, CombatStyle::Accurate, 1},
                            {"Magic", Skill::Magic, CombatStyle::Accurate, 2}};
    for (const Fight& f : fights) {
        MiniWorld w;
        CombatWorld(&w);
        w.game.SetStyle(f.style);
        w.bag.Add(Item::Bread, 8);
        if (f.weapon == 1) {
            w.equip.Set(EquipSlot::Weapon, Item::Shortbow);
            w.equip.SetAmmo(Item::BronzeArrows, 5000);
        } else if (f.weapon == 2) {
            w.equip.Set(EquipSlot::Weapon, Item::Staff);
            w.bag.Add(Item::AirRune, 5000);
            w.bag.Add(Item::MindRune, 5000);
            w.game.SetSpell(0);
        }
        report(f.name, TrainToTen(&w, f.skill, 6 * kHour, [&] {
            if (w.skills.Hp() <= 4 && w.bag.Has(Item::Bread)) w.game.DoItemAction(w.bag.FindSlot(Item::Bread), ItemAction::Eat);
            if (!w.bag.Has(Item::Bread)) w.bag.Add(Item::Bread, 8);
            if (w.game.Current() == Activity::Fighting || w.game.FightTarget() >= 0) return;
            for (const Npc& n : w.npcs.All())
                if (!n.dead) {
                    w.game.TapNpc(n.id);
                    return;
                }
        }));
    }
    {   // (hitpoints start at level 10 and grow with every fight)
        Skills fresh;
        CHECK(fresh.Level(Skill::Hitpoints) >= 10);
    }
    std::printf("  game minutes to level 10 (one player, the dice really rolled):");
    for (const Result& r : results) std::printf(" %s %.0f%s", r.name, r.seconds / 60.0, &r == &results.back() ? "\n" : ",");
}


// The round-10 trees are on the real map, in groves a hero can walk up to, and each kind is a tree the rules know.
static void TestNewTreesInTheMap(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping tree test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    CHECK(m.ParseText(ss.str(), nullptr));
    std::map<std::string, int> count;
    for (const TreeDef& t : m.Trees()) ++count[t.kind];
    CHECK(count["maple"] >= 5 && count["teak"] >= 5 && count["mahogany"] >= 4 && count["magic"] >= 4);
    for (const char* kind : {"maple", "teak", "mahogany", "magic"}) {
        CHECK(std::string(GetTreeInfo(kind).kind) == kind);
        // at least one tree of the kind has a walkable tile beside it
        bool reachable = false;
        for (const TreeDef& t : m.Trees()) {
            if (t.kind != kind) continue;
            const TileCoord c = t.Tile();
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    if ((dx || dz) && m.Walkable({c.x + dx, c.z + dz})) reachable = true;
        }
        CHECK(reachable);
    }
}


// ---------------------------------------------------------------------------------------------- round 11: nearest town, quest shelves

static void TestWakeUpInTheNearestTown() {
    auto world = [](MiniWorld* w) {
        std::string text = "@size 14 90\n@spawn 6 1\n@area 0 0 14 6 Market Square\n@area 0 40 14 46 Millbrook Square\n@area 0 80 14 86 Lakeside\n"
                           "@pen yard 10 60 14 70\n@npc cow 12.5 62.5 yard\n@rows\n";
        for (int z = 0; z < 90; ++z) text += "..............\n";
        w->map.ParseText(text);
        w->npcs.Init(w->map);
        w->forest.Init(w->map.Trees());
        w->rocks.Init(w->map.Rocks());
        w->people.Init(w->map);
        w->player.Teleport({12, 62});
        w->skills.SetHp(2);
        w->rollValue = 0.0;
        w->npcs.StartHunt(w->npcs.All()[0].id);
    };
    {   // he has been to Millbrook and Lakeside: dying at z 62 (a little nearer Millbrook) wakes him in Millbrook
        MiniWorld w;
        world(&w);
        w.game.MarkVisited("Millbrook Square");
        w.game.MarkVisited("Lakeside");
        w.Run(15.0f);
        CHECK(w.ev.deaths == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == "Millbrook Square" && w.ev.Said("You wake up in Millbrook"));
    }
    {   // he has never been to Millbrook: the nearest town he knows is Lakeside
        MiniWorld w;
        world(&w);
        w.game.MarkVisited("Lakeside");
        w.Run(15.0f);
        CHECK(w.ev.deaths == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == "Lakeside");
    }
    {   // a stranger wakes up in Castle Town
        MiniWorld w;
        world(&w);
        w.Run(15.0f);
        CHECK(w.ev.deaths == 1 && w.map.AreaAt(w.player.Tile().x, w.player.Tile().z) == "Market Square");
        CHECK(!w.ev.Said("You wake up in"));
    }
}

static void TestQuestsOpenShelves() {
    const int archery = ShopIndex("village_archery");
    MiniWorld w;
    auto listed = [&](Item item) {
        for (const ShopLine& l : w.game.BuildShopMenu(archery).buy)
            if (l.item == item) return true;
        return false;
    };
    CHECK(listed(Item::Shortbow) && !listed(Item::YewLongbow) && !listed(Item::MithrilArrows));
    w.game.Quests().Finish(FindQuest("bowyers_trial"));
    CHECK(listed(Item::YewLongbow) && listed(Item::YewShortbow) && !listed(Item::MithrilArrows));
    w.game.Quests().Finish(FindQuest("bowyers_pride"));
    CHECK(listed(Item::MithrilArrows));
    // buying a gated line before its quest is refused
    MiniWorld b;
    b.bag.Add(Item::Coins, 100000);
    const int magic = ShopIndex("village_magic");
    b.player.Teleport({12, 0});   // beside wilf's tile: not a shop, so only the rule matters
    int staffLine = -1;
    const std::vector<Shops::Line>& lines = b.shops.Lines(magic);
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].item == Item::FireStaff) staffLine = static_cast<int>(i);
    CHECK(staffLine >= 0 && !b.game.ShopLineUnlocked(magic, Item::FireStaff));
    b.game.Buy(magic, staffLine, 1);
    CHECK(!b.bag.Has(Item::FireStaff));
    b.game.Quests().Finish(FindQuest("wizards_test"));
    CHECK(b.game.ShopLineUnlocked(magic, Item::FireStaff) && b.game.ShopLineUnlocked(magic, Item::Staff));
    // every gate names a real quest, and the gated things are real shop stock
    for (const ShopDef& s : AllShopDefs())
        for (const ShopStockDef& d : s.stock)
            if (d.quest[0]) CHECK(FindQuest(d.quest) >= 0);
}


// The design document: "each attack takes a set number of ticks, based on the weapon" (a tick is 0.6 s).
static void TestDarts() {
    // five metals, in the quiver slot, stackable, with levels and strengths that climb
    for (int i = 0; i < kDartKinds; ++i) {
        const ArrowInfo& d = GetDart(i);
        CHECK(d.thrown && IsThrown(d.item) && IsArrow(d.item) && !IsBow(d.item) && GetItemInfo(d.item).stackable && ItemValue(d.item) > 0);
        EquipSlot slot;
        CHECK(Equippable(d.item, &slot) && slot == EquipSlot::Ammo);
        if (i > 0) CHECK(d.strength > GetDart(i - 1).strength && d.level >= GetDart(i - 1).level && d.value > GetDart(i - 1).value);
        CHECK(ItemStats(d.item).ranged > 0 && ItemStats(d.item).rangedStrength == d.strength);
    }
    CHECK(!IsThrown(Item::BronzeArrows) && !IsThrown(Item::Bones));
    // with an empty hand and darts in the quiver he throws them; with a sword in his hand he fights in melee; with a bow the darts are no use
    MiniWorld w;
    w.equip.SetAmmo(Item::IronDarts, 40);
    CHECK(w.game.Kind() == AttackKind::Ranged && w.game.AttackReach() == kThrowRange);
    CHECK(std::fabs(w.game.HeroSwingSeconds() - kThrowTicks * kTickSeconds) < 1e-4f);
    w.equip.Set(EquipSlot::Weapon, GearItem(Tier::Bronze, GearType::Sword));
    CHECK(w.game.Kind() == AttackKind::Melee && w.game.AttackReach() == 1);
    w.equip.Set(EquipSlot::Weapon, Item::Shortbow);
    CHECK(w.game.Kind() == AttackKind::Ranged && w.game.AttackReach() == GetBow(0).range);
    w.equip.Set(EquipSlot::Weapon, Item::None);
    w.equip.SetAmmo(Item::None, 0);
    CHECK(w.game.Kind() == AttackKind::Melee);
    // the Ranged level decides which darts he may use; Larson sells them; goblins drop them
    MiniWorld v;
    v.bag.Add(Item::SteelDarts, 20);
    v.game.DoItemAction(v.bag.FindSlot(Item::SteelDarts), ItemAction::Wield);
    CHECK(v.equip.Get(EquipSlot::Ammo) == Item::None && v.ev.Said("Ranged level 20"));
    v.skills.SetXp(Skill::Ranged, XpForLevel(20));
    v.game.DoItemAction(v.bag.FindSlot(Item::SteelDarts), ItemAction::Wield);
    CHECK(v.equip.Get(EquipSlot::Ammo) == Item::SteelDarts && v.equip.AmmoCount() == 20 && v.ev.Said("You throw them when you wield no weapon"));
    bool sold = false;
    for (const ShopStockDef& s : AllShopDefs()[static_cast<size_t>(ShopIndex("village_archery"))].stock) sold = sold || s.item == Item::BronzeDarts;
    CHECK(sold);
    int seen = 0;
    for (int i = 0; i < 4000; ++i) {
        double r = (i + 0.5) / 4000.0;
        for (const ItemStack& st : RollLoot(Species::Goblin, [&]() { r = std::fmod(r * 7.31 + 0.137, 1.0); return r; })) seen += st.item == Item::BronzeDarts ? 1 : 0;
    }
    CHECK(seen > 0);
}

static void TestBowSpeeds() {
    // a shortbow shoots quickly (3 ticks), a longbow draws slowly (5), a staff casts at the usual 4; the slower weapon hits harder per shot
    CHECK(WeaponTicks(Item::Shortbow) == 3 && WeaponTicks(Item::OakShortbow) == 3 && WeaponTicks(Item::YewShortbow) == 3);
    CHECK(WeaponTicks(Item::Longbow) == 5 && WeaponTicks(Item::OakLongbow) == 5 && WeaponTicks(Item::YewLongbow) == 5);
    CHECK(WeaponTicks(Item::Staff) == kDefaultSwingTicks);
    Skills sk;
    sk.SetXp(Skill::Ranged, XpForLevel(40));
    Fighter quick = RangedFighter(sk, CombatStyle::Accurate, 20, 30, 0), slow = quick;
    quick.swingScale = 3.0f / 4.0f;
    slow.swingScale = 5.0f / 4.0f;
    CHECK(MaxHit(quick) < MaxHit(slow) && MaxHit(quick) >= 1);
    // in a real fight the swing timer follows the bow
    MiniWorld w;
    w.equip.Set(EquipSlot::Weapon, Item::Longbow);
    CHECK(std::fabs(w.game.HeroSwingSeconds() - 5 * kTickSeconds) < 1e-4f);
    w.equip.Set(EquipSlot::Weapon, Item::Shortbow);
    CHECK(std::fabs(w.game.HeroSwingSeconds() - 3 * kTickSeconds) < 1e-4f);
    // the design document: a strike never lands less than 5% or more than 95% of the time
    Fighter giant, mouse;
    giant.attack = 99;
    giant.attackBonus = 400;
    giant.defence = 99;
    giant.defenceBonus = 400;
    mouse.attack = 1;
    mouse.defence = 1;
    CHECK(HitChance(giant, mouse) <= 0.95 + 1e-9 && HitChance(giant, mouse) > 0.9);
    CHECK(HitChance(mouse, giant) >= 0.05 - 1e-9 && HitChance(mouse, giant) < 0.1);
}

static void TestWeaponSpeeds() {
    CHECK(WeaponTicks(Item::None) == 4 && WeaponTicks(Item::Staff) == 4);
    CHECK(WeaponTicks(GearItem(Tier::Iron, GearType::Dagger)) == 3 && WeaponTicks(GearItem(Tier::Iron, GearType::Sword)) == 4);
    CHECK(WeaponTicks(GearItem(Tier::Iron, GearType::Scimitar)) == 4 && WeaponTicks(GearItem(Tier::Iron, GearType::Mace)) == 4);
    CHECK(WeaponTicks(GearItem(Tier::Iron, GearType::Longsword)) == 5 && WeaponTicks(GearItem(Tier::Iron, GearType::Axe)) == 5);
    CHECK(WeaponTicks(GearItem(Tier::Iron, GearType::Warhammer)) == 6);
    // the hero swings that often, and a slow weapon hits harder per swing (the same tier, level and style)
    auto swings = [](Item weapon, int* maxHit, int* ticks) {
        MiniWorld w;
        w.skills.SetXp(Skill::Strength, XpForLevel(40));
        if (weapon != Item::None) {
            w.bag.Add(weapon);
            w.game.DoItemAction(w.bag.FindSlot(weapon), ItemAction::Wield);
        }
        *ticks = w.game.HeroSwingTicks();
        *maxHit = w.game.HeroMaxHit();
        w.rollValue = 0.99;   // every strike misses, so the fight lasts and the swings can be counted
        w.player.Teleport({11, 1});
        w.game.TapNpc(1);
        w.Run(30.0f);
        return static_cast<int>(w.ev.hitsOnNpc.size());
    };
    int fistHit, fistTicks, daggerHit, daggerTicks, hammerHit, hammerTicks;
    const int fists = swings(Item::None, &fistHit, &fistTicks);
    const int dagger = swings(GearItem(Tier::Bronze, GearType::Dagger), &daggerHit, &daggerTicks);
    const int hammer = swings(GearItem(Tier::Bronze, GearType::Warhammer), &hammerHit, &hammerTicks);
    CHECK(fistTicks == 4 && daggerTicks == 3 && hammerTicks == 6);
    CHECK(fists >= 12 && fists <= 14 && dagger >= 16 && dagger <= 18 && hammer >= 8 && hammer <= 10);   // 2.4 s, 1.8 s and 3.6 s a swing
    CHECK(hammerHit > fistHit && fistHit > daggerHit);
    // creatures keep their own pace: about one strike every 2.4 s whatever the hero wields
    MiniWorld c;
    c.skills.SetXp(Skill::Hitpoints, XpForLevel(99));
    c.skills.SetHp(c.skills.MaxHp());
    c.bag.Add(GearItem(Tier::Bronze, GearType::Warhammer));
    c.game.DoItemAction(c.bag.FindSlot(GearItem(Tier::Bronze, GearType::Warhammer)), ItemAction::Wield);
    c.rollValue = 0.99;   // (every strike misses, on both sides: the fight goes on, and every strike shows up as a hit splat)
    c.player.Teleport({11, 2});
    c.npcs.StartHunt(c.npcs.All()[1].id);   // (the cow comes for him; he hits back, but misses)
    c.Run(30.0f);
    CHECK(c.ev.hitsOnPlayer.size() >= 11 && c.ev.hitsOnPlayer.size() <= 13);
}


// ---------------------------------------------------------------------------------------------- round 11: boots, gloves, capes
static void TestBootsGlovesAndCapes() {
    EquipSlot slot;
    for (int t = 0; t < kTierCount; ++t) {
        const Tier tier = static_cast<Tier>(t);
        CHECK(Equippable(GearItem(tier, GearType::Boots), &slot) && slot == EquipSlot::Feet);
        CHECK(Equippable(GearItem(tier, GearType::Gauntlets), &slot) && slot == EquipSlot::Hands);
        CHECK(GetGearStats(tier, GearType::Boots).defence > 0 && GetGearStats(tier, GearType::Gauntlets).defence > 0 && GetGearStats(tier, GearType::Gauntlets).strength > 0);
        if (t > 0) CHECK(GetGearStats(tier, GearType::Boots).defence > GetGearStats(static_cast<Tier>(t - 1), GearType::Boots).defence);
    }
    CHECK(std::string(GetItemInfo(GearItem(Tier::Mithril, GearType::Boots)).name) == "Mithril boots");
    CHECK(std::string(GetItemInfo(GearItem(Tier::Iron, GearType::Gauntlets)).model) == "iron_gauntlets");
    CHECK(GetSmithRecipe(Tier::Steel, GearType::Boots).bars == 1 && GetSmithRecipe(Tier::Steel, GearType::Boots).level == 35);
    CHECK(Equippable(Item::LeatherBoots, &slot) && slot == EquipSlot::Feet && Equippable(Item::StuddedBoots, &slot) && slot == EquipSlot::Feet);
    CHECK(Equippable(Item::LeatherGloves, &slot) && slot == EquipSlot::Hands && Equippable(Item::HardLeatherGloves, &slot) && slot == EquipSlot::Hands);
    CHECK(ItemStats(Item::StuddedBoots).defence > ItemStats(Item::HardLeatherBoots).defence && ItemStats(Item::HardLeatherBoots).defence > ItemStats(Item::LeatherBoots).defence);
    // the leather boots and gloves are sewn (and the studded ones need the hard leather ones and studs)
    int sewn = 0;
    for (const CraftRecipe& r : AllCraftRecipes())
        if (r.kind == CraftKind::Leather && (r.product == Item::LeatherBoots || r.product == Item::LeatherGloves || r.product == Item::HardLeatherBoots ||
                                             r.product == Item::HardLeatherGloves || r.product == Item::StuddedBoots))
            ++sewn;
    CHECK(sewn == 5);
    // fifteen capes: one per skill and one for the quests, all worn on the back
    CHECK(CapeCount() == 15);
    std::set<std::string> keys;
    for (int i = 0; i < CapeCount(); ++i) {
        const CapeInfo& c = GetCape(i);
        CHECK(Equippable(c.item, &slot) && slot == EquipSlot::Cape && keys.insert(c.key).second);
        CHECK(ItemValue(c.item) == kCapePrice && ItemStats(c.item).defence >= 2 && !ItemSellable(c.item) == false);
        CHECK(std::string(GetItemInfo(c.item).model) == std::string("cape_") + c.key);
    }
    CHECK(GetItemInfo(Item::MiningCape).examine[0] && std::string(GetItemInfo(Item::MiningCape).examine).find("Mining") != std::string::npos);
    CHECK(ItemStats(Item::MagicCape).magic >= 8 && ItemStats(Item::RangedCape).ranged >= 6 && ItemStats(Item::AttackCape).attack >= 5 && ItemStats(Item::StrengthCape).strength >= 5);
    // wearing all three adds up, and the save keeps them
    MiniWorld w;
    for (Item it : {Item::MagicCape, GearItem(Tier::Steel, GearType::Boots), Item::HardLeatherGloves}) {
        w.bag.Add(it);
        if (it == GearItem(Tier::Steel, GearType::Boots)) w.skills.SetXp(Skill::Defence, XpForLevel(20));
        w.game.DoItemAction(w.bag.FindSlot(it), ItemAction::Wear);
    }
    CHECK(w.equip.Get(EquipSlot::Cape) == Item::MagicCape && w.equip.Get(EquipSlot::Feet) == GearItem(Tier::Steel, GearType::Boots) && w.equip.Get(EquipSlot::Hands) == Item::HardLeatherGloves);
    CHECK(w.equip.Total().magic == 8 && w.equip.Total().defence == ItemStats(Item::MagicCape).defence + GetGearStats(Tier::Steel, GearType::Boots).defence + ItemStats(Item::HardLeatherGloves).defence);
    const std::string text = SerializeProgress(w.skills, w.bag, CombatStyle::Accurate, &w.equip);
    Skills s2;
    Inventory bag2;
    Equipment eq2;
    ParseProgress(text, &s2, &bag2, nullptr, &eq2);
    CHECK(eq2.Get(EquipSlot::Cape) == Item::MagicCape && eq2.Get(EquipSlot::Feet) == GearItem(Tier::Steel, GearType::Boots) && eq2.Get(EquipSlot::Hands) == Item::HardLeatherGloves);
    // an old save (eight slots) still loads: the new ones are simply empty
    Equipment eq3;
    ParseProgress("worn.5=" + std::to_string(static_cast<int>(Item::BronzeArrows)) + ":10\nworn.7=" + std::to_string(static_cast<int>(Item::GoldAmulet)) + "\n", &s2, &bag2, nullptr, &eq3);
    CHECK(eq3.Get(EquipSlot::Cape) == Item::None && eq3.Get(EquipSlot::Feet) == Item::None);
}

static void TestTheGuildOfMasters() {
    const int guild = ShopIndex("masters");
    CHECK(guild >= 0 && AllShopDefs()[static_cast<size_t>(guild)].stock.size() == 15);
    MiniWorld w;
    auto listed = [&](Item item) {
        for (const ShopLine& l : w.game.BuildShopMenu(guild).buy)
            if (l.item == item) return l.price == kCapePrice;
        return false;
    };
    CHECK(!listed(Item::MiningCape) && !listed(Item::QuestCape) && w.game.BuildShopMenu(guild).buy.empty());
    w.skills.SetXp(Skill::Mining, XpForLevel(98));
    CHECK(!listed(Item::MiningCape));
    w.skills.SetXp(Skill::Mining, XpForLevel(99));
    CHECK(listed(Item::MiningCape) && !listed(Item::CookingCape));
    // the quest cape: every quest, not just most of them
    for (size_t i = 0; i + 1 < AllQuests().size(); ++i) w.game.Quests().Finish(static_cast<int>(i));
    CHECK(!listed(Item::QuestCape));
    w.game.Quests().Finish(static_cast<int>(AllQuests().size()) - 1);
    CHECK(listed(Item::QuestCape));
    // the king is the one who sells them, and sells nothing else; the guild buys nothing
    const PersonInfo* king = FindPersonInfo("king");
    CHECK(king && std::string(king->shop) == "masters");
    CHECK(!w.shops.Buys(guild, Item::Coins) && !w.shops.Buys(guild, Item::MiningCape) && !w.shops.Buys(guild, Item::Logs));
}

// The balance pass: guard rails over the damage per second of every weapon (tests/balance.h builds the table; run build/Release/balance_report.exe to read it).
// New gear, arrows or spells that break one of these are out of line with the rest of the game.
static void TestWeaponBalance() {
    const std::vector<balance::Row> rows = balance::WeaponTable();
    CHECK(rows.size() >= 80);
    for (const balance::Row& r : rows) CHECK(r.dps > 0.0 && r.hit >= 0.05 && r.hit <= 0.95 && r.seconds > 0.0);

    // in one metal no melee weapon is far better or far worse than the others (a dagger swings fast, a warhammer hits hard: the speed rule)
    for (int t = 0; t < kTierCount; ++t) {
        std::vector<double> d;
        for (const balance::Row& r : rows)
            if (r.cls == "melee" && r.tier == t) d.push_back(r.dps);
        std::sort(d.begin(), d.end());
        CHECK(d.size() == 8);
        if (d.size() != 8) continue;
        const double median = (d[3] + d[4]) / 2.0;
        CHECK(d.front() >= 0.55 * median && d.back() <= 1.45 * median);
    }
    // every kind of melee weapon gets better with the metal; so do the bows' arrows, the darts and the spells
    for (GearType g : {GearType::Dagger, GearType::Sword, GearType::Scimitar, GearType::Longsword, GearType::Spear, GearType::Mace, GearType::Axe, GearType::Warhammer}) {
        double prev = 0.0;
        for (int t = 0; t < kTierCount; ++t)
            for (const balance::Row& r : rows)
                if (r.cls == "melee" && r.type == static_cast<int>(g) && r.tier == t) {
                    CHECK(r.dps > prev);
                    prev = r.dps;
                }
    }
    for (int b = 0; b < kBowCount; ++b) {
        double prev = 0.0;
        for (int k = 0; k < kArrowKinds; ++k) {   // (a hero of level 45 can use everything)
            const double d = balance::BowDps(b, k, 45);
            CHECK(d >= prev);
            prev = d;
        }
        CHECK(balance::BowDps(b, kArrowKinds - 1, 45) > 1.15 * balance::BowDps(b, 0, 45));   // the best arrows are worth having
    }
    double prevDart = 0.0, prevSpell = 0.0;
    for (const balance::Row& r : rows) {
        if (r.cls == "dart") {
            CHECK(r.dps >= 0.95 * prevDart);   // (the first two are equal: a max hit of 1 either way at level 5)
            prevDart = r.dps;
        }
    }
    for (const SpellInfo& sp : AllSpells())
        for (const balance::Row& r : rows)
            if (r.cls == "spell" && r.name == sp.name) {
                CHECK(r.dps >= prevSpell);
                prevSpell = r.dps;
            }
    // the longer draw of a longbow pays: it hits harder than the same shortbow per shot
    CHECK(GetBow(1).accuracy >= 0 && WeaponTicks(GetBow(1).item) > WeaponTicks(GetBow(0).item));

    // across the weapon types, at every level: shooting is not a poor second to swinging, and the spells, which cost runes, lead by a bounded amount
    for (int level : {1, 5, 20, 30, 40}) {
        const double melee = balance::BestDps(rows, "melee", level);
        const double ranged = std::max(balance::BestDps(rows, "bow", level), balance::BestDps(rows, "dart", level));
        const double magic = balance::BestDps(rows, "spell", level);
        CHECK(melee > 0.0 && ranged >= 0.6 * melee && ranged <= 1.3 * melee);
        CHECK(magic >= 0.8 * melee && magic <= 3.0 * melee);
    }
    // power costs coins: every spell eats runes (from level 9 on, a lot), and a dart costs more than an arrow of the same metal
    for (const balance::Row& r : rows) {
        if (r.cls == "spell") CHECK(r.CoinsPerMinute() >= 40.0 && (r.level < 9 || r.CoinsPerMinute() >= 150.0));
        if (r.cls == "dart")
            for (const balance::Row& a : rows)
                if (a.cls == "bow" && a.tier == r.tier && a.type == 0) CHECK(r.CoinsPerMinute() > a.CoinsPerMinute());
    }
}

// ---------------------------------------------------------------------------------------------- round 13: the Abyss, skulls and other players
namespace {
// One stand-in opponent for the hero's own combat code.
struct FakeFoes : FoeSource {
    FoeView v;
    int hits = 0;
    bool killed = false;
    int killedHeroBy = 0;
    void HeroKilledBy(int id) override { killedHeroBy = id; }
    bool Get(int id, FoeView* out) const override {
        if (id != v.id) return false;
        *out = v;
        return true;
    }
    bool HeroHits(int id, int damage) override {
        if (id != v.id || !v.alive) return false;
        ++hits;
        v.hp -= damage;
        if (v.hp > 0) return false;
        v.alive = false;
        killed = true;
        return true;
    }
};

int OnTheGround(const GroundItems& g, Item item) {
    int n = 0;
    for (const Pile& p : g.All())
        for (const ItemStack& s : p.items)
            if (s.item == item) n += s.count;
    return n;
}

void MakeAbyssWorld(MiniWorld* w, bool abyss) {
    w->map.ParseText(std::string("@size 14 4\n@spawn 1 1\n") + (abyss ? "@area 0 0 14 4 The Abyss\n" : "@area 0 0 14 4 Meadow\n") +
                     "@rows\n..............\n..............\n..............\n..............\n");
    w->npcs.Init(w->map);
    w->forest.Init(w->map.Trees());
    w->rocks.Init(w->map.Rocks());
    w->people.Init(w->map);
    w->player.Teleport({6, 1});
}

FoeView MakeFoe(bool skulled, int combat = 3) {
    FoeView v;
    v.id = kFoeIdBase + 1;
    v.name = "Vex";
    v.x = 7.5f;
    v.z = 1.5f;
    v.hp = v.maxHp = 400;
    v.combat = combat;
    v.skulled = skulled;
    v.alive = true;
    Skills s;
    v.fighter = PlayerFighter(s, CombatStyle::Accurate, 0, 0, 0);
    return v;
}
}  // namespace

// The skull: attacking a player who is not skulled and did not start it skulls you for ten minutes; a skull makes a death in the Abyss cost everything.
static void TestSkullRules() {
    {   // the first blow at an unskulled player: a skull for ten minutes, which fades
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        FakeFoes foes;
        foes.v = MakeFoe(false);
        w.game.SetFoes(&foes);
        w.game.TapFoe(foes.v.id);
        w.Run(6.0f);
        CHECK(foes.hits > 0 && w.game.Skulled() && w.game.SkullLeft() > kSkullSeconds - 8.0f && w.ev.Said("skull appears"));
        foes.v.skulled = true;   // (he is skulled himself now: no new skull for the hero, and the old one runs out)
        w.Run(kSkullSeconds + 5.0f);
        CHECK(!w.game.Skulled() && w.ev.Said("skull fades"));
    }
    {   // a player who is already skulled is fair game: no skull for the hero
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        FakeFoes foes;
        foes.v = MakeFoe(true);
        w.game.SetFoes(&foes);
        w.game.TapFoe(foes.v.id);
        w.Run(6.0f);
        CHECK(foes.hits > 0 && !w.game.Skulled());
    }
    {   // so is somebody who attacked him first: hitting back earns no skull
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        FakeFoes foes;
        foes.v = MakeFoe(false);
        w.game.SetFoes(&foes);
        w.game.StrikeHero(foes.v.id, 0);
        w.Run(8.0f);   // (he was standing about, so he hits back at once)
        CHECK(foes.hits > 0 && !w.game.Skulled());
    }
    {   // outside the Abyss nobody fights anybody
        MiniWorld w;
        MakeAbyssWorld(&w, false);
        FakeFoes foes;
        foes.v = MakeFoe(false);
        w.game.SetFoes(&foes);
        w.game.TapFoe(foes.v.id);
        w.Run(4.0f);
        CHECK(foes.hits == 0 && !w.game.Skulled() && w.ev.Said("only fight other adventurers in the Abyss"));
    }
    {   // the gap of levels: here (Abyss level 20) a player 20 levels above may be fought, one 21 above may not
        MiniWorld probe;
        const int mine = probe.skills.CombatLevel();
        for (int gap : {20, 21}) {
            MiniWorld w;
            MakeAbyssWorld(&w, true);
            FakeFoes foes;
            foes.v = MakeFoe(false, mine + gap);
            w.game.SetFoes(&foes);
            w.game.TapFoe(foes.v.id);
            w.Run(5.0f);
            CHECK(w.game.AbyssLevel() == 20);
            if (gap == 20) CHECK(foes.hits > 0);
            else CHECK(foes.hits == 0 && !w.game.Skulled() && w.ev.Said("cannot fight Vex here"));
        }
    }
    {   // killing one: XP, a message, and the fight is over
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        FakeFoes foes;
        foes.v = MakeFoe(false);
        foes.v.hp = 1;
        w.game.SetFoes(&foes);
        w.rollValue = 0.0;
        w.game.TapFoe(foes.v.id);
        w.Run(8.0f);
        CHECK(foes.killed && w.ev.Said("defeated Vex") && w.game.FightTarget() < 0 && w.skills.Xp(Skill::Attack) > 0);
    }
}

static void TestSkulledDeathLosesEverything() {
    for (int skulled = 0; skulled < 2; ++skulled) {
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        w.bag.Add(Item::Coins, 5000);
        w.bag.Add(Item::Logs, 10);
        w.bag.Add(Item::Shark, 3);
        w.bag.Add(Item::AdamantBar, 3);
        w.bag.Add(Item::Bones, 2);
        w.equip.Set(EquipSlot::Weapon, GearItem(Tier::Bronze, GearType::Sword));
        if (skulled) w.game.SetSkull(300.0f);
        w.skills.SetHp(1);
        w.game.StrikeHero(kFoeIdBase + 9, 50);
        w.Run(2.0f);
        CHECK(w.ev.deaths == 1 && !w.game.Skulled());
        if (skulled) {   // everything, worn things too, lies where he fell
            CHECK(w.bag.Count(Item::Coins) == 0 && w.bag.Count(Item::Logs) == 0 && w.bag.Count(Item::Bones) == 0 && w.equip.Get(EquipSlot::Weapon) == Item::None);
            CHECK(OnTheGround(w.ground, Item::Coins) == 5000 && OnTheGround(w.ground, Item::Logs) == 10 && OnTheGround(w.ground, Item::Shark) == 3 &&
                  OnTheGround(w.ground, Item::AdamantBar) == 3 && OnTheGround(w.ground, GearItem(Tier::Bronze, GearType::Sword)) == 1);
            CHECK(w.ev.Said("EVERYTHING"));
        } else {         // the three most valuable things are kept (the coins, the adamant, the sharks), the rest falls
            CHECK(w.bag.Count(Item::AdamantBar) == 3 && w.bag.Count(Item::Logs) == 0 && OnTheGround(w.ground, Item::Logs) == 10);
            CHECK(w.ev.Said("three most valuable"));
        }
    }
}

// The Abyss people on the real map: hunters who pick fights with the hero and are skulled for it, whose things fall when they die and come back with
// the same gear, and who take what the dead leave lying about.
static void TestAbyssHunters(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping Abyss test: %s not found)\n", path.c_str());
        return;
    }
    GroundItems ground;
    r.world.ground = &ground;
    int hunters = 0, workers = 0;
    int first = -1;
    for (const Adventurer& a : r.advs.All()) {
        if (!a.filler.bound) continue;
        CHECK(IsDangerousArea(r.map.AreaAt(a.Tile().x, a.Tile().z)) && a.Combat() >= 20);
        if (a.filler.job == FillerJob::Hunt) {
            ++hunters;
            if (first < 0) first = a.id;
        } else {
            ++workers;
        }
    }
    CHECK(hunters >= 5 && workers >= 3 && first >= 0);
    // the hero stands three tiles from a hunter (inside the Abyss), not skulled: the hunter comes, hits him, and is skulled for it
    const Adventurer* h = r.advs.Find(first);
    TileCoord spot = h->Tile();
    for (const Adventurer& cand : r.advs.All()) {
        if (cand.filler.job != FillerJob::Hunt || !(spot == h->Tile()) && cand.id != first) continue;
        for (int d = 3; d <= 4; ++d)
            for (int dz = -d; dz <= d; ++dz)
                for (int dx = -d; dx <= d; ++dx) {
                    const int x = cand.Tile().x + dx, z = cand.Tile().z + dz;
                    if (spot == h->Tile() && std::max(std::abs(dx), std::abs(dz)) == d && r.map.Walkable(x, z) && IsDangerousArea(r.map.AreaAt(x, z)) && cand.Tile().z > 358) {
                        spot = {x, z};
                        first = cand.id;
                    }
                }
    }
    h = r.advs.Find(first);
    CHECK(!(spot == h->Tile()));
    int blows = 0;
    r.world.heroX = spot.x + 0.5f;
    r.world.heroZ = spot.z + 0.5f;
    r.world.heroAlive = r.world.heroInDanger = true;
    r.world.heroSkulled = false;
    Skills weak;
    r.world.heroFighter = PlayerFighter(weak, CombatStyle::Accurate, 0, 0, 0);
    r.world.hitHero = [&](int foeId, int) { CHECK(IsFoeId(foeId)); ++blows; };
    r.world.heroCombat = h->Combat() + 70;   // far beyond any gap the Abyss allows: the hunter leaves him alone
    r.Run(2.0f);
    CHECK(blows == 0 && !r.advs.Find(first)->pvp.fightingHero);
    {   // he has moved a little: the hero stands three tiles from where he is now
        const TileCoord ht = r.advs.Find(first)->Tile();
        bool placed = false;
        for (int dz = -3; dz <= 3 && !placed; ++dz)
            for (int dx = -3; dx <= 3 && !placed; ++dx)
                if (std::max(std::abs(dx), std::abs(dz)) == 3 && r.map.Walkable(ht.x + dx, ht.z + dz) && IsDangerousArea(r.map.AreaAt(ht.x + dx, ht.z + dz))) {
                    r.world.heroX = ht.x + dx + 0.5f;
                    r.world.heroZ = ht.z + dz + 0.5f;
                    placed = true;
                }
        CHECK(placed);
    }
    r.world.heroCombat = h->Combat();          // the same level: fair game at any depth
    r.Run(15.0f);
    h = r.advs.Find(first);
    CHECK(blows > 0 && h->pvp.skull > 500.0f && h->pvp.fightingHero);
    // out of the Abyss he stops
    r.world.heroInDanger = false;
    r.world.heroX = r.world.heroZ = 5.5f;   // (he has walked out of the Abyss)
    r.Run(4.0f);
    CHECK(!r.advs.Find(first)->pvp.fightingHero);
    // killed while skulled: everything he carries is on the ground, he is gone, and after four minutes he is back with the same kit
    const Equipment kit = h->kitEquip;
    const int coins = h->kitBag.Count(Item::Coins);
    CHECK(coins > 0);
    FoeView v;
    CHECK(r.advs.Get(kFoeIdBase + first, &v) && v.alive && v.skulled && v.combat >= 20);
    CHECK(r.advs.HeroHits(kFoeIdBase + first, 100000));
    CHECK(r.advs.Get(kFoeIdBase + first, &v) && !v.alive);
    CHECK(OnTheGround(ground, Item::Coins) >= coins);
    for (int s = 0; s < kEquipSlotCount; ++s) {
        const Item worn = kit.Get(static_cast<EquipSlot>(s));
        if (worn != Item::None && static_cast<EquipSlot>(s) != EquipSlot::Ammo) CHECK(OnTheGround(ground, worn) >= 1);
    }
    r.world.heroAlive = false;
    for (int i = 0; i < 2000 && r.advs.Find(first)->pvp.dead; ++i) r.Run(0.2f);
    h = r.advs.Find(first);
    CHECK(!h->pvp.dead && h->skills.Hp() == h->skills.MaxHp());
    for (int s = 0; s < kEquipSlotCount; ++s) CHECK(h->equip.Get(static_cast<EquipSlot>(s)) == kit.Get(static_cast<EquipSlot>(s)));
    CHECK(h->bag.Count(Item::Coins) == coins);
}

static void TestHuntersTakeWhatTheDeadLeave(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    GroundItems ground;
    r.world.ground = &ground;
    const Adventurer* h = nullptr;
    for (const Adventurer& a : r.advs.All())
        if (a.filler.job == FillerJob::Hunt && !h) h = &a;
    CHECK(h != nullptr);
    if (!h) return;
    TileCoord at = h->Tile();
    for (int dx = 4; dx <= 7 && at == h->Tile(); ++dx)
        if (r.map.Walkable(h->Tile().x + dx, h->Tile().z) && IsDangerousArea(r.map.AreaAt(h->Tile().x + dx, h->Tile().z))) at = {h->Tile().x + dx, h->Tile().z};
    CHECK(!(at == h->Tile()));
    ground.Drop(at, {Item::AdamantBar, 5}, GroundItems::kDeathLifeSeconds);   // what a dying hero leaves
    ground.Drop({at.x, at.z + 1}, {Item::Logs, 5});                               // an ordinary drop (a minute): nobody takes those
    r.world.heroAlive = false;
    for (int i = 0; i < 100 && OnTheGround(ground, Item::AdamantBar) > 0; ++i) {
        r.Run(0.4f);
        ground.Update(0.4f);
    }
    int carried = 0;
    for (const Adventurer& a : r.advs.All()) carried += a.bag.Count(Item::AdamantBar);
    CHECK(OnTheGround(ground, Item::AdamantBar) == 0 && carried >= 5 && OnTheGround(ground, Item::Logs) == 5);
}

// The Abyss holds the best resources in the game, all within reach of its gate.
static void TestAbyssRiches(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping Abyss riches test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    CHECK(m.ParseText(ss.str(), nullptr));
    const AreaDef* abyss = nullptr;
    for (const AreaDef& a : m.Areas())
        if (a.name == "The Abyss") abyss = &a;
    CHECK(abyss != nullptr);
    if (!abyss) return;
    int adamant = 0, gems = 0, mithril = 0, magic = 0, yew = 0;
    std::vector<TileCoord> where;
    for (const RockDef& r : m.Rocks()) {
        if (!abyss->rect.Contains(static_cast<int>(r.x), static_cast<int>(r.z))) continue;
        adamant += r.kind == "adamant";
        gems += r.kind == "gems";
        mithril += r.kind == "mithril";
        where.push_back({static_cast<int>(r.x), static_cast<int>(r.z)});
    }
    for (const TreeDef& t : m.Trees()) {
        if (!abyss->rect.Contains(static_cast<int>(t.x), static_cast<int>(t.z))) continue;
        magic += t.kind == "magic";
        yew += t.kind == "yew";
        where.push_back({static_cast<int>(t.x), static_cast<int>(t.z)});
    }
    CHECK(adamant >= 16 && gems >= 6 && mithril >= 6 && magic >= 8 && yew >= 8);
    const TileCoord gate = NearestWalkable(m, {(abyss->rect.x0 + abyss->rect.x1) / 2, abyss->rect.z0 + 3}, 8);
    CHECK(gate.x >= 0 && FindPath(m, m.Spawn(), gate).found);
    for (const TileCoord& t : where) {
        const TileCoord next = NearestWalkable(m, t, 2);
        CHECK(next.x >= 0 && FindPath(m, gate, next).found);
    }
}

// The late-game errands that send the hero into the Abyss.
static void TestAbyssQuests() {
    int n = 0;
    for (const char* key : {"riches_of_the_abyss", "heart_of_the_dark", "dark_pilgrims", "gems_for_the_crown", "the_black_dragon"}) {
        const int qi = FindQuest(key);
        CHECK(qi >= 0);
        if (qi < 0) continue;
        ++n;
        const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
        CHECK(q.minCombat >= 55 && !q.steps.empty());
    }
    CHECK(n == 5);
}

// The Abyss level: 1 at the gate, rising with depth to kAbyssMaxLevel, and the gap of combat levels it allows.
static void TestAbyssLevels(const std::string& path) {
    CHECK(CanFightPlayers(30, 31, 1, 1) && !CanFightPlayers(30, 32, 1, 1));
    CHECK(CanFightPlayers(20, 70, 50, 50) && !CanFightPlayers(20, 80, 50, 50));
    CHECK(!CanFightPlayers(30, 30, 0, 40) && !CanFightPlayers(30, 30, 40, 0));
    CHECK(!CanFightPlayers(30, 60, 10, 50) && CanFightPlayers(30, 40, 10, 50));   // the shallower of the two decides
    std::ifstream f(path);
    if (!f) {
        std::printf("  (skipping Abyss level test: %s not found)\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    TileMap m;
    CHECK(m.ParseText(ss.str(), nullptr));
    const AreaDef* abyss = nullptr;
    for (const AreaDef& a : m.Areas())
        if (a.name == "The Abyss") abyss = &a;
    CHECK(abyss != nullptr);
    if (!abyss) return;
    const int x = (abyss->rect.x0 + abyss->rect.x1) / 2;
    CHECK(AbyssLevelAt(m, x, abyss->rect.z0 - 3) == 0 && AbyssLevelAt(m, 5, 5) == 0);
    int prev = 0;
    for (int z = abyss->rect.z0; z < abyss->rect.z1; ++z) {
        const int lv = AbyssLevelAt(m, x, z);
        CHECK(lv >= prev && lv >= 1 && lv <= kAbyssMaxLevel);
        prev = lv;
    }
    CHECK(AbyssLevelAt(m, x, abyss->rect.z0) == 1 && AbyssLevelAt(m, x, abyss->rect.z1 - 1) == kAbyssMaxLevel);
    CHECK(AbyssLevelAt(m, x, abyss->rect.z1 - 2) >= 55 && kAbyssMaxLevel >= 40);
}

// Real adventurers sometimes go to the Abyss, and fight each other there by the same rules as everybody.
static void TestRealAdventurersVentureIntoTheAbyss(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping Abyss expedition test: %s not found)\n", path.c_str());
        return;
    }
    GroundItems ground;
    r.world.ground = &ground;
    r.world.hero = {-500, -500};
    // strong and fed: the ones who might set out
    for (const Adventurer& c : r.advs.All()) {
        if (c.kind != AdvKind::Real) continue;
        Adventurer& a = const_cast<Adventurer&>(c);
        for (Skill s : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Hitpoints}) a.skills.SetXp(s, XpForLevel(30));
        a.skills.SetHp(a.skills.MaxHp());
        a.bag.Add(Item::Bread, 6);
        a.source[Item::Bread] = kSrcShop;
        a.mind.nextAbyss = 0.0f;
    }
    int ventured = 0;
    std::set<int> inside;
    for (int t = 0; t < 2400; ++t) {
        r.Run(1.0f);
        for (const Adventurer& a : r.advs.All())
            if (a.kind == AdvKind::Real && AbyssLevelAt(r.map, a.Tile().x, a.Tile().z) > 0) inside.insert(a.id);
    }
    for (const Adventurer& a : r.advs.All())
        if (a.kind == AdvKind::Real) ventured += a.mind.abyssTrips;
    std::printf("  Abyss expeditions in 40 minutes: %d trips, %d adventurers set foot inside\n", ventured, static_cast<int>(inside.size()));
    CHECK(ventured >= 2 && inside.size() >= 1);
    std::string why;
    CHECK(r.advs.Honest(&why));
}

static void TestRealAdventurersFightEachOther(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    GroundItems ground;
    r.world.ground = &ground;
    r.world.hero = {-500, -500};
    const AreaDef* abyss = nullptr;
    for (const AreaDef& a : r.map.Areas())
        if (a.name == "The Abyss") abyss = &a;
    CHECK(abyss != nullptr);
    if (!abyss) return;
    TileCoord spot{-1, -1};   // a walkable tile deep in the Abyss, with room beside it
    for (int z = abyss->rect.z1 - 4; z > abyss->rect.z0 && spot.x < 0; --z)
        for (int x = abyss->rect.x0 + 10; x < abyss->rect.x1 - 10 && spot.x < 0; ++x)
            if (r.map.Walkable(x, z) && r.map.Walkable(x + 1, z) && r.map.Walkable(x, z + 1)) spot = {x, z};
    CHECK(spot.x >= 0 && AbyssLevelAt(r.map, spot.x, spot.z) >= 40);
    std::vector<int> ids;
    for (const Adventurer& c : r.advs.All())
        if (c.kind == AdvKind::Real && ids.size() < 2) ids.push_back(c.id);
    CHECK(ids.size() == 2);
    Adventurer& A = const_cast<Adventurer&>(*r.advs.Find(ids[0]));
    Adventurer& B = const_cast<Adventurer&>(*r.advs.Find(ids[1]));
    int k = 0;
    for (Adventurer* p : {&A, &B}) {
        for (Skill s : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Hitpoints}) p->skills.SetXp(s, XpForLevel(30));
        p->skills.SetHp(p->skills.MaxHp());
        p->bag.Add(Item::Coins, 100);
        p->source[Item::Coins] = kSrcShop;
        p->x = spot.x + 0.5f + 0.9f * k;
        p->z = spot.z + 0.5f;
        p->path.clear();
        ++k;
    }
    CHECK(std::abs(A.Combat() - B.Combat()) <= 2);
    A.pvp.pk = true;   // one of them picks fights
    r.world.heroAlive = false;
    bool aSkulled = false, bHurt = false;
    for (int t = 0; t < 90; ++t) {
        r.Run(0.5f);
        aSkulled = aSkulled || A.pvp.skull > 0.0f || A.mind.deaths > 0;
        bHurt = bHurt || B.skills.Hp() < B.skills.MaxHp() || B.mind.deaths > 0 || A.skills.Hp() < A.skills.MaxHp();
    }
    CHECK(aSkulled && bHurt);   // he started it (a skull), and blows were traded
    std::string why;
    CHECK(r.advs.Honest(&why));
}

// A skull is part of the save (the hero's, and a real adventurer's): quitting does not take it off.
static void TestSkullsAreSaved(const std::string& path) {
    Skills s;
    Inventory bag;
    const std::string text = SerializeProgress(s, bag, CombatStyle::Accurate, nullptr, nullptr, 0, nullptr, nullptr, 312.5f);
    float skull = 0.0f;
    Skills s2;
    Inventory b2;
    ParseProgress(text, &s2, &b2, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &skull);
    CHECK(std::fabs(skull - 312.5f) < 0.01f);
    CHECK(SerializeProgress(s, bag).find("skull=") == std::string::npos);   // (nothing written when he has none)
    float big = 0.0f;
    ParseProgress("skull=99999\n", &s2, &b2, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &big);
    CHECK(big <= kSkullSeconds && big > 0.0f);
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping skull save test: %s not found)\n", path.c_str());
        return;
    }
    std::string name;
    for (const Adventurer& c : r.advs.All())
        if (c.kind == AdvKind::Real && name.empty()) {
            name = c.name;
            const_cast<Adventurer&>(c).pvp.skull = 123.0f;
        }
    const std::string saved = r.advs.Save();
    AdvRig r2;
    CHECK(r2.Load(path) && r2.advs.Load(r2.world, saved));
    bool found = false;
    for (const Adventurer& c : r2.advs.All())
        if (c.name == name) {
            found = true;
            CHECK(std::fabs(c.pvp.skull - 123.0f) < 0.5f);
        }
    CHECK(found);
}

namespace {
// The weakest of the creatures that attack on sight in the Abyss, and a free tile at this distance from a tile inside it.
const Npc* WeakestAbyssBeast(const AdvRig& r) {
    const Npc* best = nullptr;
    for (const Npc& n : r.npcs.All())
        if (n.Info().aggressive && !n.dead && AbyssLevelAt(r.map, n.Tile().x, n.Tile().z) > 0 && (!best || n.Info().level < best->Info().level)) best = &n;
    return best;
}
bool TileAround(const AdvRig& r, TileCoord c, int d, TileCoord* out) {
    for (int dz = -d; dz <= d; ++dz)
        for (int dx = -d; dx <= d; ++dx)
            if (std::max(std::abs(dx), std::abs(dz)) == d && r.map.Walkable(c.x + dx, c.z + dz) && AbyssLevelAt(r.map, c.x + dx, c.z + dz) > 0) {
                *out = {c.x + dx, c.z + dz};
                return true;
            }
    return false;
}
}  // namespace

// The Abyss creatures go for the other players as they do for the hero, and the players hit back.
static void TestAbyssCreaturesAttackPlayers(const std::string& path) {
    for (int kind = 0; kind < 2; ++kind) {   // 0 = a real adventurer, 1 = one of the Abyss hunters
        AdvRig r;
        if (!r.Load(path)) {
            std::printf("  (skipping Abyss creature test: %s not found)\n", path.c_str());
            return;
        }
        GroundItems ground;
        r.world.ground = &ground;
        r.world.hero = {-500, -500};
        r.world.heroAlive = false;
        const Npc* beast = WeakestAbyssBeast(r);
        CHECK(beast != nullptr);
        if (!beast) return;
        const int beastId = beast->id;
        Adventurer* ad = nullptr;
        for (const Adventurer& c : r.advs.All())
            if (!ad && ((kind == 0 && c.kind == AdvKind::Real) || (kind == 1 && c.filler.job == FillerJob::Hunt))) ad = const_cast<Adventurer*>(&c);
        CHECK(ad != nullptr);
        if (!ad) return;
        if (kind == 0) {   // a level-25 fighter with food
            for (Skill s : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Hitpoints}) ad->skills.SetXp(s, XpForLevel(25));
            ad->skills.SetHp(ad->skills.MaxHp());
            ad->bag.Add(Item::Bread, 6);
            ad->source[Item::Bread] = kSrcShop;
        }
        TileCoord at;
        CHECK(TileAround(r, beast->Tile(), 2, &at));
        ad->x = at.x + 0.5f;
        ad->z = at.z + 0.5f;
        ad->path.clear();
        ad->pvp.cooldown = 0.0f;
        const int maxHp = ad->skills.MaxHp();
        bool adHurt = false, beastHurt = false;
        for (int t = 0; t < 200; ++t) {
            r.Run(0.2f);
            const Npc* n = r.npcs.Find(beastId);
            adHurt = adHurt || ad->skills.Hp() < maxHp || ad->mind.deaths > 0 || ad->pvp.dead;
            beastHurt = beastHurt || (n && (n->hp < n->maxHp || n->dead));
        }
        CHECK(adHurt && beastHurt);   // it went for him, and he fought back
        std::string why;
        CHECK(r.advs.Honest(&why));
    }
}

// A real adventurer who is out hunting in the Abyss picks a fight with a creature he can beat.
static void TestRealAdventurersFightAbyssCreatures(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    GroundItems ground;
    r.world.ground = &ground;
    r.world.hero = {-500, -500};
    r.world.heroAlive = false;
    const Npc* beast = WeakestAbyssBeast(r);
    CHECK(beast != nullptr);
    if (!beast) return;
    const int beastId = beast->id;
    Adventurer* ad = nullptr;
    for (const Adventurer& c : r.advs.All())
        if (!ad && c.kind == AdvKind::Real) ad = const_cast<Adventurer*>(&c);
    for (Skill s : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Hitpoints}) ad->skills.SetXp(s, XpForLevel(60));
    ad->skills.SetHp(ad->skills.MaxHp());
    ad->bag.Add(Item::Bread, 6);
    ad->source[Item::Bread] = kSrcShop;
    TileCoord at;
    CHECK(TileAround(r, beast->Tile(), 8, &at));
    ad->x = at.x + 0.5f;
    ad->z = at.z + 0.5f;
    ad->path.clear();
    ad->mind.hasGoal = true;   // out hunting in the Abyss
    ad->mind.goal = AdvGoal{};
    ad->mind.goal.task = AdvTask::Abyss;
    ad->mind.goal.a = 30;
    ad->mind.goal.left = 600.0f;
    ad->mind.phase = AdvPhase::Work;
    bool beastHurt = false;
    for (int t = 0; t < 300; ++t) {
        r.Run(0.2f);
        const Npc* n = r.npcs.Find(beastId);
        beastHurt = beastHurt || (n && (n->hp < n->maxHp || n->dead));
    }
    CHECK(beastHurt);
    std::string why;
    CHECK(r.advs.Honest(&why));
}

// With the creatures attacking them too, the Abyss people still live there: some fall now and then, and most are about at any time.
static void TestAbyssPeopleSurviveTheCreatures(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) return;
    GroundItems ground;
    r.world.ground = &ground;
    r.world.hero = {-500, -500};
    r.world.heroAlive = false;
    int fillers = 0;
    for (const Adventurer& a : r.advs.All()) fillers += a.filler.bound ? 1 : 0;
    int fewest = fillers, sampled = 0;
    for (int minute = 0; minute < 30; ++minute) {
        r.Run(60.0f);
        int alive = 0;
        for (const Adventurer& a : r.advs.All()) alive += (a.filler.bound && !a.pvp.dead) ? 1 : 0;
        fewest = std::min(fewest, alive);
        ++sampled;
    }
    std::printf("  Abyss people alive: at least %d of %d over %d minutes\n", fewest, fillers, sampled);
    CHECK(fillers == 10 && fewest >= 3);   // (with health back at a hit point a minute, the wounded ones fall more often)
}

// The rarest drop of the Abyss: an amulet better than any other, from the Abyss's champions, knights and dragon.
static void TestAbyssalAmulet() {
    EquipSlot slot;
    CHECK(Equippable(Item::AbyssalAmulet, &slot) && slot == EquipSlot::Amulet && !GetItemInfo(Item::AbyssalAmulet).stackable);
    const GearStats st = ItemStats(Item::AbyssalAmulet);
    for (Item other : {Item::DiamondAmulet, Item::DragonstoneAmulet, Item::MummysAmulet, Item::TrollToothAmulet}) {
        const GearStats o = ItemStats(other);
        CHECK(st.attack >= o.attack && st.strength >= o.strength && st.defence >= o.defence && st.ranged >= o.ranged && st.magic >= o.magic);
    }
    CHECK(ItemValue(Item::AbyssalAmulet) > ItemValue(Item::DragonstoneAmulet));
    for (const char* table : {"abyss_champion", "black_knight", "black_dragon"}) {
        const LootTable* t = FindLootTable(table);
        CHECK(t != nullptr && DropChance(*t, Item::AbyssalAmulet) > 0.0);
        if (t) CHECK(DropChance(*t, Item::AbyssalAmulet) < 0.12);   // (rare)
    }
    CHECK(DropChance(*FindLootTable("black_dragon"), Item::AbyssalAmulet) > DropChance(*FindLootTable("black_knight"), Item::AbyssalAmulet));
}

// The record against other players: kills and deaths for the hero and the adventurers, saved, and ranked.
static void TestPvpRecord(const std::string& path) {
    {   // the hero's kill and the hero's death
        MiniWorld w;
        MakeAbyssWorld(&w, true);
        FakeFoes foes;
        foes.v = MakeFoe(false);
        foes.v.hp = 1;
        w.game.SetFoes(&foes);
        w.rollValue = 0.0;
        w.game.TapFoe(foes.v.id);
        w.Run(8.0f);
        CHECK(foes.killed && w.game.PvpKills() == 1 && w.game.PvpDeaths() == 0 && w.ev.Said("Players killed: 1"));
        w.skills.SetHp(1);
        w.game.StrikeHero(foes.v.id, 50);
        w.Run(2.0f);
        CHECK(w.ev.deaths == 1 && w.game.PvpDeaths() == 1 && foes.killedHeroBy == foes.v.id);
        w.game.SetPvpRecord(7, 3);   // (the saved record comes back)
        CHECK(w.game.PvpKills() == 7 && w.game.PvpDeaths() == 3);
    }
    {   // the save file
        Skills s;
        Inventory bag;
        const std::string text = SerializeProgress(s, bag, CombatStyle::Accurate, nullptr, nullptr, 0, nullptr, nullptr, 0.0f, 4, 2);
        int k = 0, d = 0;
        Skills s2;
        Inventory b2;
        ParseProgress(text, &s2, &b2, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &k, &d);
        CHECK(k == 4 && d == 2);
        CHECK(SerializeProgress(s, bag).find("pk=") == std::string::npos);
    }
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping PvP record test: %s not found)\n", path.c_str());
        return;
    }
    CHECK(r.advs.PkRanking("Hero", 0, 0).empty());
    std::vector<int> ids;
    for (const Adventurer& c : r.advs.All())
        if (c.kind == AdvKind::Real && ids.size() < 2) ids.push_back(c.id);
    CHECK(ids.size() == 2);
    r.advs.HeroKilledBy(kFoeIdBase + ids[0]);   // the hero fell to the first one, twice
    r.advs.HeroKilledBy(kFoeIdBase + ids[0]);
    Adventurer& second = const_cast<Adventurer&>(*r.advs.Find(ids[1]));
    second.pkKills = 1;
    second.pkDeaths = 5;
    const std::vector<PkRank> rank = r.advs.PkRanking("Hero", 1, 0);
    CHECK(rank.size() == 3 && rank[0].kills == 2 && rank[0].name == r.advs.Find(ids[0])->name && rank[1].kills == 1 && rank[1].deaths == 0 && rank[1].hero &&
          rank[2].deaths == 5);
    // a real adventurer's record is saved with him
    const std::string saved = r.advs.Save();
    AdvRig r2;
    CHECK(r2.Load(path) && r2.advs.Load(r2.world, saved));
    const Adventurer* back = r2.advs.Find(ids[1]);
    CHECK(back && back->pkKills == 1 && back->pkDeaths == 5);
    const Adventurer* first = r2.advs.Find(ids[0]);
    CHECK(first && first->pkKills == 2);
}

// Two real adventurers who are free and fit set out for the Abyss together: they follow each other about and never fight each other.
static void TestAbyssPairs(const std::string& path) {
    AdvRig r;
    if (!r.Load(path)) {
        std::printf("  (skipping Abyss pair test: %s not found)\n", path.c_str());
        return;
    }
    GroundItems ground;
    r.world.ground = &ground;
    r.world.hero = {-500, -500};
    r.world.heroAlive = false;
    // a quiet Abyss: nobody else about (the pair is what is being watched)
    for (Npc& n : r.npcs.All())
        if (AbyssLevelAt(r.map, n.Tile().x, n.Tile().z) > 0) {
            n.dead = true;
            n.deathT = 99.0f;
            n.lootPending = false;
            n.respawnLeft = 1e9f;
        }
    for (const Adventurer& c : r.advs.All())
        if (c.filler.bound) {
            const_cast<Adventurer&>(c).pvp.dead = true;
            const_cast<Adventurer&>(c).pvp.respawn = 1e9f;
        }
    std::vector<int> ids;
    for (const Adventurer& c : r.advs.All()) {
        if (c.kind != AdvKind::Real) continue;
        Adventurer& a = const_cast<Adventurer&>(c);
        a.mind.nextAbyss = 1e9f;   // (the others stay at home)
        if (ids.size() < 2) {
            ids.push_back(a.id);
            for (Skill s : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Hitpoints}) a.skills.SetXp(s, XpForLevel(40));
            a.skills.SetHp(a.skills.MaxHp());
            a.bag.Add(Item::Bread, 8);
            a.source[Item::Bread] = kSrcShop;
            a.mind.nextAbyss = 0.0f;
            a.mind.abyssEager = true;
            a.mind.queue.clear();
            a.mind.hasGoal = false;
            a.mind.phase = AdvPhase::Plan;
            a.mind.thinkLeft = 0.0f;
        }
    }
    CHECK(ids.size() == 2);
    Adventurer& A = const_cast<Adventurer&>(*r.advs.Find(ids[0]));
    Adventurer& B = const_cast<Adventurer&>(*r.advs.Find(ids[1]));
    bool paired = false;
    int together = 0, both = 0;
    for (int t = 0; t < 1500; ++t) {
        r.Run(1.0f);
        if (A.mind.partner == B.id && B.mind.partner == A.id) paired = true;
        if (A.mind.partner != 0 || B.mind.partner != 0) {
            const bool inA = AbyssLevelAt(r.map, A.Tile().x, A.Tile().z) > 0, inB = AbyssLevelAt(r.map, B.Tile().x, B.Tile().z) > 0;
            if (inA && inB && A.mind.hasGoal && A.mind.goal.task == AdvTask::Abyss && B.mind.hasGoal && B.mind.goal.task == AdvTask::Abyss) {
                ++both;
                const int d = std::max(std::abs(A.Tile().x - B.Tile().x), std::abs(A.Tile().z - B.Tile().z));
                together += d <= 14 ? 1 : 0;
            }
        }
        CHECK(A.pvp.foe != B.id && B.pvp.foe != A.id);   // (never each other)
    }
    std::printf("  Abyss pair: paired=%d, %d seconds roaming together, %d of them within 14 tiles\n", paired ? 1 : 0, both, together);
    CHECK(paired && both >= 20 && together * 10 >= both * 7);
    std::string why;
    CHECK(r.advs.Honest(&why));
}

// Set JOURNEY_TEST_TRACE=1 to print the name of every test as it starts (to find the one that crashes).
static bool g_trace = false;
#define RUN(t)                                \
    do {                                      \
        if (g_trace) {                        \
            std::printf("%s\n", #t);          \
            std::fflush(stdout);              \
        }                                     \
        t();                                  \
    } while (0)
#define RUN_ARG(t, a)                         \
    do {                                      \
        if (g_trace) {                        \
            std::printf("%s\n", #t);          \
            std::fflush(stdout);              \
        }                                     \
        t(a);                                 \
    } while (0)

int main(int argc, char** argv) {
    g_trace = std::getenv("JOURNEY_TEST_TRACE") != nullptr;
    const std::string mapPath = argc > 1 ? argv[1] : "assets/maps/town.map";
    RUN(TestStraightPath);
    RUN(TestDiagonalIsShorter);
    RUN(TestWallBlocksAndRoutesAround);
    RUN(TestNoCornerCutting);
    RUN(TestUnreachable);
    RUN(TestBlockedGoal);
    RUN(TestClickOnWallWalksNextToIt);
    RUN(TestClickInsideSealedRoomFails);
    RUN(TestDoorsAreWalkable);
    RUN(TestParse);
    RUN(TestXpTable);
    RUN(TestSkills);
    RUN(TestInventory);
    RUN(TestProgressSave);
    RUN(TestForest);
    RUN(TestChopping);
    RUN(TestTreesInMap);
    RUN(TestMapExtras);
    RUN(TestItems);
    RUN(TestFishing);
    RUN(TestCooking);
    RUN(TestFires);
    RUN(TestGameplayChopping);
    RUN(TestGameplayFishingAndCooking);
    RUN(TestGameplayLightingRules);
    RUN(TestHitpointsAndCombatLevel);
    RUN(TestCombatMath);
    RUN(TestNpcsAndLoot);
    RUN(TestGroundItems);
    RUN(TestGameplayCombat);
    RUN(TestGameplayDeathAndAutoRetaliate);
    RUN(TestMenusAndUse);
    RUN(TestGearTable);
    RUN(TestEquipment);
    RUN(TestMiningRules);
    RUN(TestSmithingRules);
    RUN(TestGameplayMining);
    RUN(TestGameplaySmeltingAndSmithing);
    RUN(TestGameplayEquipAndCombat);
    RUN(TestMenusRocksAndStations);
    RUN(TestCoinsAndValues);
    RUN(TestShopsData);
    RUN(TestPeopleAndShopping);
    RUN(TestGoblins);
    RUN(TestMonsterData);
    RUN(TestGameplayMonsters);
    RUN(TestDeathInTheAbyss);
    RUN(TestTeleportScrolls);
    RUN(TestQuestMarkers);
    RUN_ARG(TestQuestMarkersOnTheRealMap, mapPath);
    RUN_ARG(TestWiderScrolls, mapPath);
    RUN(TestCookingRange);
    RUN(TestSynthAndSettings);
    RUN(TestJewellery);
    RUN(TestQuestData);
    RUN(TestQuestLog);
    RUN(TestGameplayDialogue);
    RUN(TestSkillingData);
    RUN(TestGameplayWoodcutting);
    RUN(TestGameplayFishingMore);
    RUN(TestGameplayCookingMore);
    RUN(TestGameplayMineralsAndSmithing);
    RUN(TestCrafting);
    RUN(TestBankData);
    RUN(TestGameplayBank);
    RUN(TestArcheryData);
    RUN(TestFencesAndShots);
    RUN(TestGameplayArchery);
    RUN(TestMagicData);
    RUN(TestGameplayMagic);
    RUN(TestArcheryAndMagicShops);
    RUN_ARG(TestRealTownMap, mapPath);
    RUN_ARG(TestFarSouthMap, mapPath);
    RUN_ARG(TestTheWiderWorld, mapPath);
    RUN(TestNewTownsAreRespawnPoints);
    RUN(TestWiderWorldShops);
    RUN(TestBonfires);
    RUN_ARG(TestAdventurersStartLikeTheHero, mapPath);
    RUN_ARG(TestFillersNeverChange, mapPath);
    RUN_ARG(TestRealAdventurersPlayTheGame, mapPath);
    RUN_ARG(TestAdventurersSaveAndLoad, mapPath);
    RUN(TestAdamant);
    RUN(TestMoreFish);
    RUN(TestRareGemsAndUniqueJewellery);
    RUN(TestMoreSpells);
    RUN(TestGddTargets);
    RUN_ARG(TestEveryQuestCanBeFinished, mapPath);
    RUN(TestMoreSmithedTypes);
    RUN(TestKitchenAndWoodwork);
    RUN(TestRoundTenSkillingData);
    RUN(TestUseItemQuests);
    RUN(TestLootRatesMatchTables);
    RUN_ARG(TestEveryItemHasAModel, mapPath);
    RUN(TestEverySkillReachesLevelTen);
    RUN_ARG(TestNewTreesInTheMap, mapPath);
    RUN(TestWakeUpInTheNearestTown);
    RUN(TestQuestsOpenShelves);
    RUN(TestWeaponSpeeds);
    RUN(TestBowSpeeds);
    RUN(TestDarts);
    RUN(TestWeaponBalance);
    RUN(TestSkullRules);
    RUN(TestSkulledDeathLosesEverything);
    RUN_ARG(TestAbyssHunters, mapPath);
    RUN_ARG(TestHuntersTakeWhatTheDeadLeave, mapPath);
    RUN_ARG(TestAbyssRiches, mapPath);
    RUN_ARG(TestAbyssLevels, mapPath);
    RUN(TestAbyssalAmulet);
    RUN_ARG(TestPvpRecord, mapPath);
    RUN_ARG(TestAbyssPairs, mapPath);
    RUN_ARG(TestSkullsAreSaved, mapPath);
    RUN_ARG(TestAbyssPeopleSurviveTheCreatures, mapPath);
    RUN_ARG(TestAbyssCreaturesAttackPlayers, mapPath);
    RUN_ARG(TestRealAdventurersFightAbyssCreatures, mapPath);
    RUN_ARG(TestRealAdventurersFightEachOther, mapPath);
    RUN_ARG(TestRealAdventurersVentureIntoTheAbyss, mapPath);
    RUN(TestAbyssQuests);
    RUN(TestBootsGlovesAndCapes);
    RUN(TestTheGuildOfMasters);
    if (g_failed == 0) std::printf("All core tests passed.\n");
    else std::printf("%d check(s) failed.\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
