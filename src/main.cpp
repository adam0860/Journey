// Journey - prototype. Click/tap to walk around the castle town.
//
// Dev flags (handy for automated screenshots):
//   --shot out.png --frames 60    render N frames then save a screenshot and quit
//   --at X Z                      start the hero on tile X,Z
//   --cam YAW PITCH DIST          camera angles in degrees / distance
//   --click X Z                   pretend the player clicked tile X,Z on frame 5
//   --debug                       show the collision grid and path
//   --size W H                    window size
//   --style a|b                   art style: A = low-poly classic, B = textured + shadows (F4 toggles in game)
//   --creator                     open the character creator (also: --newchar ignores the saved character)
//   --look H S P F SKIN HC SC PC FC   pick appearance indices (hair shirt pants shoes skin hairCol shirtCol pantsCol shoeCol)
//   --osk                         use the on-screen keyboard for the name (default on Android);  --uiclick X Y  fake a tap (tests)
//   --female / --name NAME        creator presets;  --cyaw DEG  preview angle;  --cwalk  preview walk cycle
//   --shadows                     experimental real-time sun shadow map (style B). Off by default: B uses shadows
//                                 baked into the ground at build time. F5 toggles it in game.
//   --tab N / --sel N             open a HUD tab panel (0 Bag, 1 Skills) / select bag slot N     --seed S  fixed random seed (chop luck)
//   --xp N / --logs N             give woodcutting XP / logs      --fell N   cut down the first N trees
//   --skill NAME XP               give XP in any skill (e.g. --skill fishing 5000)    --raw N   give N raw shrimps   --item ID N   give N of an item by id
//   --tree I / --fish I           pretend a tap on tree I / fishing spot I on frame 5
//   --light                       light a fire (needs --logs) on frame 5            --cook   tap the nearest fire on frame 5
//   --fire X Z                    start with a burning fire on tile X,Z (several allowed)
//   --attack N                    tap animal number N (1-based) on frame 5     --hp N   set hit points
//   --cstyle N                    combat style 0-3     --pile X Z ID   drop item ID on tile X,Z    --zoo 1-5  line up the animals
//   --menu X Y                    open the context menu at pixel X,Y on frame 8 (like a long press);  --menunpc N  right on animal N
//   --tapxy X Y / --tapnpc N      a plain tap at pixel X,Y / on animal N on frame 5 (the real tap path)
//   --hover X Y                   pretend the mouse is at pixel X,Y (shows the 'what a tap does' line)
//   --anim chop|fish|light|cook|punch|bury|mine|smelt|smith   force the hero's animation (punch = a swing of whatever he wields);
//                                 --atime T  freeze its clock at T seconds;  --face DEG  hero heading
//   --wear ID [ID...]             put items on (no level check);  --block T  pose a blocked strike T seconds after it began (--blockhit: it lands)
//   --make 0|1 [TIER]             open the furnace (0) or anvil (1, with metal TIER 0-3) make panel on frame 6
//   --rock I / --station I        pretend a tap on rock I / station I on frame 5     --tab 2  the Gear tab
//   --coins N                     give N coins;  --shop I  open shop I (0 general_town, 1 smith_town, 2.. village) on frame 6;  --talk I  walk up to person I (1-based) on frame 5
//   --dosmelt TIER / --dosmith TIER TYPE   start smelting / smithing on frame 6 (stand next to the furnace / anvil first; TYPE 0-8 = dagger..pickaxe)
//   (with --shot nothing is saved, so test flags never touch your real progress)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "game/Appearance.h"
#include "game/Equipment.h"
#include "game/Gear.h"
#include "game/Mining.h"
#include "game/People.h"
#include "game/Shops.h"
#include "game/Smithing.h"
#include "game/Fires.h"
#include "game/Forest.h"
#include "game/Gameplay.h"
#include "game/GroundItems.h"
#include "game/Npc.h"
#include "game/Inventory.h"
#include "game/Player.h"
#include "game/Progress.h"
#include "game/Skills.h"
#include "game/Woodcutting.h"
#include "platform/PointerInput.h"
#include "raylib.h"
#include "render/CameraRig.h"
#include "render/AnimalRenderer.h"
#include "render/CharacterRenderer.h"
#include "render/LitShader.h"
#include "render/PropRenderer.h"
#include "render/TownRenderer.h"
#include "rlgl.h"
#include "ui/CharacterCreator.h"
#include "ui/Hud.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

using namespace journey;

namespace {

struct Args {
    const char* shot = nullptr;
    int frames = 30;
    bool hasAt = false;
    int atX = 0, atZ = 0;
    bool hasCam = false;
    float yaw = 0, pitch = 46, dist = 15;
    bool hasClick = false;
    int clickX = 0, clickZ = 0;
    bool debug = false;
    int width = 1280, height = 720;
    int style = 0;           // 0 = A, 1 = B
    bool shadows = false;
    bool creator = false, newchar = false, female = false, cwalk = false, hasLook = false, hasName = false;
    int look[9] = {0, 0, 0, 0, 2, 2, 7, 3, 0};
    std::string name;
    float cyaw = -1000.0f;
    bool osk = false;
    std::vector<Vector2> uiClicks;
    int tab = -1;
    bool hasSeed = false;
    unsigned seed = 0;
    int xp = 0, logs = 0, fell = 0;
    int raw = 0;
    std::vector<std::pair<std::string, int>> skillXp;
    int tapTree = -1, tapSpot = -1;
    bool light = false, cook = false;
    std::vector<TileCoord> fires;
    int anim = 0;            // dev: force an animation (1 chop, 2 fish, 3 light, 4 cook)
    float atime = -1.0f;     // dev: freeze the animation clock at this time
    int sel = -1;            // dev: select this bag slot
    int attack = 0, hp = -1, cstyle = -1, menuNpc = 0, tapNpc = 0;
    bool hasTapXY = false;
    float tapX = 0, tapY = 0;
    bool hasMenu = false, hasHover = false;
    float menuX = 0, menuY = 0, hoverX = 0, hoverY = 0;
    struct PileArg { int x, z, item; };
    std::vector<PileArg> piles;
    int zoo = 0;             // dev: line up one of every animal in front of the hero (1 idle, 2 walking, 3 attack, 4 hurt, 5 dead)
    std::vector<std::pair<int, int>> items;   // dev: --item ID N
    float face = -1000.0f;   // dev: hero facing in degrees (0 = south, 90 = east)
    std::vector<int> wear;   // dev: --wear ID
    float block = -1.0f;     // dev: --block T
    bool blockHit = false;
    int make = -1, makeTier = 0;   // dev: --make 0|1 [TIER]
    int tapRock = -1, tapStation = -1;
    int coins = 0, shop = -1, talk = 0;
    int doSmelt = -1;                 // dev: --dosmelt TIER   start smelting on frame 6 (stand next to the furnace)
    int doSmithTier = -1, doSmithType = 0;   // dev: --dosmith TIER TYPE   start smithing on frame 6 (stand next to the anvil)
};

Args ParseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        auto is = [&](const char* s) { return std::strcmp(argv[i], s) == 0; };
        auto more = [&](int n) { return i + n < argc; };
        if (is("--shot") && more(1)) a.shot = argv[++i];
        else if (is("--frames") && more(1)) a.frames = std::atoi(argv[++i]);
        else if (is("--at") && more(2)) { a.hasAt = true; a.atX = std::atoi(argv[i + 1]); a.atZ = std::atoi(argv[i + 2]); i += 2; }
        else if (is("--cam") && more(3)) { a.hasCam = true; a.yaw = (float)std::atof(argv[i + 1]); a.pitch = (float)std::atof(argv[i + 2]); a.dist = (float)std::atof(argv[i + 3]); i += 3; }
        else if (is("--click") && more(2)) { a.hasClick = true; a.clickX = std::atoi(argv[i + 1]); a.clickZ = std::atoi(argv[i + 2]); i += 2; }
        else if (is("--size") && more(2)) { a.width = std::atoi(argv[i + 1]); a.height = std::atoi(argv[i + 2]); i += 2; }
        else if (is("--style") && more(1)) { a.style = (argv[++i][0] == 'b' || argv[i][0] == 'B') ? 1 : 0; }
        else if (is("--shadows")) a.shadows = true;
        else if (is("--creator")) a.creator = true;
        else if (is("--newchar")) a.newchar = true;
        else if (is("--female")) a.female = true;
        else if (is("--cwalk")) a.cwalk = true;
        else if (is("--osk")) a.osk = true;
        else if (is("--uiclick") && more(2)) { a.uiClicks.push_back({(float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2])}); i += 2; }
        else if (is("--name") && more(1)) { a.hasName = true; a.name = argv[++i]; }
        else if (is("--cyaw") && more(1)) a.cyaw = (float)std::atof(argv[++i]);
        else if (is("--look") && more(9)) { a.hasLook = true; for (int k = 0; k < 9; ++k) a.look[k] = std::atoi(argv[i + 1 + k]); i += 9; }
        else if (is("--tab") && more(1)) a.tab = std::atoi(argv[++i]);
        else if (is("--seed") && more(1)) { a.hasSeed = true; a.seed = (unsigned)std::atoi(argv[++i]); }
        else if (is("--xp") && more(1)) a.xp = std::atoi(argv[++i]);
        else if (is("--logs") && more(1)) a.logs = std::atoi(argv[++i]);
        else if (is("--fell") && more(1)) a.fell = std::atoi(argv[++i]);
        else if (is("--raw") && more(1)) a.raw = std::atoi(argv[++i]);
        else if (is("--skill") && more(2)) { a.skillXp.push_back({argv[i + 1], std::atoi(argv[i + 2])}); i += 2; }
        else if (is("--tree") && more(1)) a.tapTree = std::atoi(argv[++i]);
        else if (is("--fish") && more(1)) a.tapSpot = std::atoi(argv[++i]);
        else if (is("--anim") && more(1)) { const char* n = argv[++i]; a.anim = !std::strcmp(n, "chop") ? 1 : !std::strcmp(n, "fish") ? 2 : !std::strcmp(n, "light") ? 3 : !std::strcmp(n, "cook") ? 4 : !std::strcmp(n, "punch") ? 5 : !std::strcmp(n, "bury") ? 6 : !std::strcmp(n, "mine") ? 7 : !std::strcmp(n, "smelt") ? 8 : !std::strcmp(n, "smith") ? 9 : 0; }
        else if (is("--item") && more(2)) { a.items.push_back({std::atoi(argv[i + 1]), std::atoi(argv[i + 2])}); i += 2; }
        else if (is("--attack") && more(1)) a.attack = std::atoi(argv[++i]);
        else if (is("--tapxy") && more(2)) { a.hasTapXY = true; a.tapX = (float)std::atof(argv[i + 1]); a.tapY = (float)std::atof(argv[i + 2]); i += 2; }
        else if (is("--tapnpc") && more(1)) a.tapNpc = std::atoi(argv[++i]);
        else if (is("--menunpc") && more(1)) a.menuNpc = std::atoi(argv[++i]);
        else if (is("--hp") && more(1)) a.hp = std::atoi(argv[++i]);
        else if (is("--cstyle") && more(1)) a.cstyle = std::atoi(argv[++i]);
        else if (is("--pile") && more(3)) { a.piles.push_back({std::atoi(argv[i + 1]), std::atoi(argv[i + 2]), std::atoi(argv[i + 3])}); i += 3; }
        else if (is("--menu") && more(2)) { a.hasMenu = true; a.menuX = (float)std::atof(argv[i + 1]); a.menuY = (float)std::atof(argv[i + 2]); i += 2; }
        else if (is("--hover") && more(2)) { a.hasHover = true; a.hoverX = (float)std::atof(argv[i + 1]); a.hoverY = (float)std::atof(argv[i + 2]); i += 2; }
        else if (is("--zoo") && more(1)) a.zoo = std::atoi(argv[++i]);
        else if (is("--sel") && more(1)) a.sel = std::atoi(argv[++i]);
        else if (is("--face") && more(1)) a.face = (float)std::atof(argv[++i]);
        else if (is("--atime") && more(1)) a.atime = (float)std::atof(argv[++i]);
        else if (is("--light")) a.light = true;
        else if (is("--cook")) a.cook = true;
        else if (is("--fire") && more(2)) { a.fires.push_back({std::atoi(argv[i + 1]), std::atoi(argv[i + 2])}); i += 2; }
        else if (is("--debug")) a.debug = true;
        else if (is("--wear") && more(1)) { while (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') a.wear.push_back(std::atoi(argv[++i])); }
        else if (is("--block") && more(1)) a.block = (float)std::atof(argv[++i]);
        else if (is("--blockhit")) a.blockHit = true;
        else if (is("--make") && more(1)) {
            a.make = std::atoi(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') a.makeTier = std::atoi(argv[++i]);
        }
        else if (is("--rock") && more(1)) a.tapRock = std::atoi(argv[++i]);
        else if (is("--coins") && more(1)) a.coins = std::atoi(argv[++i]);
        else if (is("--shop") && more(1)) a.shop = std::atoi(argv[++i]);
        else if (is("--talk") && more(1)) a.talk = std::atoi(argv[++i]);
        else if (is("--dosmelt") && more(1)) a.doSmelt = std::atoi(argv[++i]);
        else if (is("--dosmith") && more(2)) { a.doSmithTier = std::atoi(argv[i + 1]); a.doSmithType = std::atoi(argv[i + 2]); i += 2; }
        else if (is("--station") && more(1)) a.tapStation = std::atoi(argv[++i]);
    }
    return a;
}

// Where the hero last clicked, drawn as the classic X: yellow = walk, red = blocked/interact.
struct ClickMarker {
    float x = 0, z = 0, age = 99.0f;
    bool blocked = false;
};

void DrawMarker(const ClickMarker& m) {
    if (m.age > 1.1f) return;
    const float pulse = 1.0f - m.age / 1.1f;
    const Color c = m.blocked ? Color{235, 60, 50, 255} : Color{255, 214, 40, 255};
    const float len = 0.34f * (0.7f + 0.3f * pulse);
    rlPushMatrix();
    rlTranslatef(m.x, 0.05f, m.z);
    rlRotatef(45.0f, 0, 1, 0);
    DrawCube({0, 0, 0}, len * 2, 0.02f, 0.09f, Fade(c, 0.4f + 0.6f * pulse));
    DrawCube({0, 0, 0}, 0.09f, 0.02f, len * 2, Fade(c, 0.4f + 0.6f * pulse));
    rlPopMatrix();
}

void DrawDebugGrid(const TileMap& map, const Player& player) {
    for (int z = 0; z < map.Height(); ++z)
        for (int x = 0; x < map.Width(); ++x)
            if (!map.Walkable(x, z)) DrawCube({x + 0.5f, 0.04f, z + 0.5f}, 0.92f, 0.02f, 0.92f, Fade(RED, 0.35f));
    float px = player.X(), pz = player.Z();
    for (const TileCoord& t : player.Path()) {
        DrawLine3D({px, 0.12f, pz}, {t.x + 0.5f, 0.12f, t.z + 0.5f}, SKYBLUE);
        px = t.x + 0.5f;
        pz = t.z + 0.5f;
    }
}

// What is under a finger? Every thing the screen ray passes through, with its distance, so the nearest wins a tap and a
// long press lists them all. Each thing is a simple box around it (so tapping the leaves picks the tree, not the ground behind).
struct RayHit {
    float dist;
    Target target;
};

void CollectTrees(const Forest& forest, const Ray& ray, std::vector<RayHit>* out) {
    for (size_t i = 0; i < forest.Trees().size(); ++i) {
        const TreeState& t = forest.Trees()[i];
        const float sc = t.def.scale;
        const float half = (t.stump ? 0.5f : (t.def.kind == "pine" ? 1.0f : 1.15f)) * sc;
        const float height = (t.stump ? 0.55f : 3.9f * (0.2f + 0.8f * t.growAnim)) * sc;
        const BoundingBox box = {{t.def.x - half, 0.0f, t.def.z - half}, {t.def.x + half, height, t.def.z + half}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Tree, static_cast<int>(i)}});
    }
}

void CollectFires(const Fires& fires, const Ray& ray, std::vector<RayHit>* out) {
    for (const Fire& f : fires.All()) {
        if (!f.Burning()) continue;
        const float cx = f.tile.x + 0.5f, cz = f.tile.z + 0.5f;
        const BoundingBox box = {{cx - 0.55f, 0.0f, cz - 0.55f}, {cx + 0.55f, 1.0f, cz + 0.55f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Fire, f.id}});
    }
}

void CollectSpots(const std::vector<SpotDef>& spots, const Ray& ray, std::vector<RayHit>* out) {
    for (size_t i = 0; i < spots.size(); ++i) {
        const BoundingBox box = {{spots[i].x - 0.7f, -0.6f, spots[i].z - 0.7f}, {spots[i].x + 0.7f, 0.5f, spots[i].z + 0.7f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Spot, static_cast<int>(i)}});
    }
}

void CollectNpcs(const Npcs& npcs, const Ray& ray, std::vector<RayHit>* out) {
    for (const Npc& n : npcs.All()) {
        if (!n.Alive()) continue;
        const float half = n.species == Species::Chicken ? 0.38f : (n.species == Species::Calf ? 0.55f : (n.species == Species::Cow ? 0.8f : 0.45f));
        const float height = AnimalRenderer::Height(n.species);
        const BoundingBox box = {{n.x - half, 0.0f, n.z - half}, {n.x + half, height, n.z + half}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Npc, n.id}});
    }
}

void CollectRocks(const Rocks& rocks, const Ray& ray, std::vector<RayHit>* out) {
    for (size_t i = 0; i < rocks.All().size(); ++i) {
        const RockState& r = rocks.All()[i];
        const float h = r.depleted ? 0.5f : 0.95f;
        const BoundingBox box = {{r.def.x - 0.55f, 0.0f, r.def.z - 0.55f}, {r.def.x + 0.55f, h, r.def.z + 0.55f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Rock, static_cast<int>(i)}});
    }
}

void CollectStations(const std::vector<StationDef>& stations, const Ray& ray, std::vector<RayHit>* out) {
    for (size_t i = 0; i < stations.size(); ++i) {
        const StationDef& st = stations[i];
        const bool furnace = st.kind == "furnace";
        const float half = furnace ? 0.5f : 0.45f, h = furnace ? 2.3f : 0.9f;
        const BoundingBox box = {{st.x - half, 0.0f, st.z - half}, {st.x + half, h, st.z + half}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Station, static_cast<int>(i)}});
    }
}

void CollectPeople(const People& people, const Ray& ray, std::vector<RayHit>* out) {
    for (const Person& p : people.All()) {
        const BoundingBox box = {{p.x - 0.42f, 0.0f, p.z - 0.42f}, {p.x + 0.42f, 1.9f, p.z + 0.42f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Person, p.id}});
    }
}

void CollectPiles(const GroundItems& ground, const Ray& ray, std::vector<RayHit>* out) {
    for (const Pile& p : ground.All()) {
        const float cx = p.tile.x + 0.5f, cz = p.tile.z + 0.5f;
        const BoundingBox box = {{cx - 0.5f, 0.0f, cz - 0.5f}, {cx + 0.5f, 0.45f, cz + 0.5f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit) out->push_back({hit.distance, {TargetKind::Pile, p.id}});
    }
}

// Connects the game rules (Gameplay) to the screen: chat lines, XP drops, the level-up banner and saving.
class HudEvents : public GameEvents {
public:
    // where(onPlayer, npcId) = a point above that fighter's chest (for the hit splat); died = what to do after a death.
    HudEvents(Hud& hud, std::function<void()> save, std::function<Vector3(bool, int)> where, std::function<void()> died,
              float* heroBarTimer)
        : hud_(hud), save_(std::move(save)), where_(std::move(where)), died_(std::move(died)), heroBar_(heroBarTimer) {}
    void Say(const std::string& text, Tone tone) override {
        Color c = {255, 255, 255, 255};
        if (tone == Tone::Info) c = {255, 224, 120, 255};
        else if (tone == Tone::Warn) c = {255, 120, 110, 255};
        else if (tone == Tone::Good) c = {255, 235, 120, 255};
        hud_.Say(text, c);
    }
    void XpDrop(Skill skill, int amount) override { hud_.XpDrop(skill, amount); }
    void LevelUp(Skill skill, int level) override {
        hud_.Say(TextFormat("Congratulations, you just advanced a %s level. You are now level %d.", SkillName(skill), level),
                 {255, 235, 120, 255});
        hud_.ShowArea(TextFormat("%s level %d", SkillName(skill), level));
    }
    void Hit(bool onPlayer, int npcId, int damage) override {
        hud_.AddHitsplat(where_(onPlayer, npcId), damage);
        if (onPlayer && heroBar_) *heroBar_ = 6.0f;   // show the hero's health bar for a few seconds after he is struck at
    }
    void PlayerDied() override {
        hud_.ShowArea("Oh dear, you are dead!");
        died_();
    }
    void Changed() override { save_(); }
    void OpenMake(const MakeMenu& menu) override { hud_.OpenMake(menu); }
    void OpenShop(const ShopMenu& menu) override { hud_.OpenShop(menu); }

private:
    Hud& hud_;
    std::function<void()> save_;
    std::function<Vector3(bool, int)> where_;
    std::function<void()> died_;
    float* heroBar_;
};

Anim AnimFor(Activity a) {
    switch (a) {
        case Activity::Chopping: return Anim::Chop;
        case Activity::Fishing: return Anim::Fish;
        case Activity::Lighting: return Anim::Light;
        case Activity::Cooking: return Anim::Cook;
        case Activity::Fighting: return Anim::Attack;
        case Activity::Burying: return Anim::Bury;
        case Activity::Mining: return Anim::Mine;
        case Activity::Smelting: return Anim::Smelt;
        case Activity::Smithing: return Anim::Smith;
        default: return Anim::None;
    }
}

// The model files for a set of worn items.
GearLook GearLookOf(Item weapon, Item shield, Item helm, Item body, Item legs, Item pick) {
    GearLook look;
    auto name = [](Item item, const char* prefix) {
        Tier t;
        GearType g;
        if (!GearOf(item, &t, &g)) return std::string();
        return std::string(prefix) + TierKey(t) + "_" + GearTypeKey(g);
    };
    look.weapon = name(weapon, "held_");
    look.shield = name(shield, "held_");
    look.helm = name(helm, "gear_");
    look.body = name(body, "gear_");
    look.legs = name(legs, "gear_");
    look.pick = name(pick, "held_");
    return look;
}

// What he wears and wields right now.
GearLook LookOf(const Equipment& eq, const Inventory& bag, const Skills& skills) {
    return GearLookOf(eq.Get(EquipSlot::Weapon), eq.Get(EquipSlot::Shield), eq.Get(EquipSlot::Head), eq.Get(EquipSlot::Body),
                      eq.Get(EquipSlot::Legs), BestPickaxe(bag, skills.Level(Skill::Mining)));
}

bool SkillFromKey(const std::string& key, Skill* out) {
    for (int i = 0; i < kSkillCount; ++i)
        if (key == SkillKey(static_cast<Skill>(i))) {
            *out = static_cast<Skill>(i);
            return true;
        }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = ParseArgs(argc, argv);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | (args.shot ? 0 : FLAG_VSYNC_HINT));
    InitWindow(args.width, args.height, "Journey");
#if !defined(__ANDROID__)
    ChangeDirectory(GetApplicationDirectory());
#endif
    SetExitKey(KEY_NULL);
    if (!args.shot) SetTargetFPS(120);

    // ---- load content ----
    TileMap map;
    {
        char* text = LoadFileText("assets/maps/town.map");
        std::string err;
        if (!text || !map.ParseText(text, &err)) {
            TraceLog(LOG_FATAL, "Cannot load town map: %s", text ? err.c_str() : "file missing");
            return 1;
        }
        UnloadFileText(text);
    }
    LitShader lit;
    if (!lit.Load()) TraceLog(LOG_FATAL, "Lit shader failed to compile");
    static const char* kStyleDir[2] = {"assets/models/town", "assets/models/town_b"};
    static const char* kStyleName[2] = {"A classic", "B enhanced"};
    TownRenderer towns[2];
    bool loaded[2] = {false, false};
#if defined(__ANDROID__)
    int style = 1;    // phones: the enhanced look by default (F4 toggle is not reachable on a phone yet)
#else
    int style = args.style;
#endif
    auto ensureLoaded = [&](int s) {
        if (loaded[s]) return true;
        loaded[s] = towns[s].Load(map, lit, kStyleDir[s]);
        return loaded[s];
    };
    if (!ensureLoaded(style)) TraceLog(LOG_FATAL, "Town models failed to load");
    bool shadows = args.shadows;
    CharacterRenderer chars;
    if (!chars.Load(lit)) TraceLog(LOG_FATAL, "Character models failed to load");
    const CharacterCatalog& cat = chars.Catalog();
    const int nHair = (int)cat.hair.size(), nShirt = (int)cat.shirt.size(), nPants = (int)cat.pants.size(), nShoes = (int)cat.shoes.size();

    // ---- who are we playing? saved character, or the creator ----
    const char* kSaveFile = "journey_save.cfg";
    Appearance look;
    bool haveSave = false;
    if (!args.newchar && !args.creator) {   // (on Android this reads from the app's private storage)
        SetTraceLogLevel(LOG_ERROR);   // a missing save file is normal on first run
        char* t = LoadFileText(kSaveFile);
        SetTraceLogLevel(LOG_INFO);
        if (t) {
            haveSave = Parse(t, &look);
            UnloadFileText(t);
        }
    }
    if (args.hasLook || args.female || args.hasName) {   // dev presets (screenshots)
        if (args.hasLook) {
            look.hair = args.look[0]; look.shirt = args.look[1]; look.pants = args.look[2]; look.shoes = args.look[3];
            look.skin = args.look[4]; look.hairColor = args.look[5]; look.shirtColor = args.look[6];
            look.pantsColor = args.look[7]; look.shoesColor = args.look[8];
        }
        if (args.female) look.female = true;
        if (args.hasName) look.name = args.name;
    }
    Sanitize(look, nHair, nShirt, nPants, nShoes);
    {   // a saved/preset hairstyle must be one this body type may use
        const std::vector<int> ok = cat.HairFor(look.female);
        if (!ok.empty() && std::find(ok.begin(), ok.end(), look.hair) == ok.end()) look.hair = ok[0];
    }
    CharacterCreator creator;
    const bool devPreset = args.hasLook || args.hasName || args.female;
    bool inCreator = args.creator || (!haveSave && !devPreset);
    auto openCreator = [&]() {
        creator.Begin(look);
        if (args.cyaw > -999.0f) creator.SetPreviewYaw(args.cyaw);
        creator.SetWalking(args.cwalk);
#if defined(__ANDROID__)
        creator.SetOnScreenKeyboard(true);
#else
        creator.SetOnScreenKeyboard(args.osk);
#endif
        inCreator = true;
    };
    if (inCreator) openCreator();
    auto saveLook = [&]() {
        const std::string text = Serialize(look);
        SaveFileText(kSaveFile, (char*)text.c_str());
    };

    Player player;
    player.Teleport(args.hasAt ? TileCoord{args.atX, args.atZ} : map.Spawn());
    if (args.face > -999.0f) player.FaceToward(player.X() + std::sin(args.face * DEG2RAD), player.Z() + std::cos(args.face * DEG2RAD));
    CameraRig camera;
    camera.SnapTo(player.X(), player.Z());
    if (args.hasCam) camera.SetView(args.yaw, args.pitch, args.dist);
    PointerInput pointer;
    Hud hud;
    ClickMarker marker;
    bool debug = args.debug;
    std::string lastArea;

    // ---- progress: skills, bag, trees ----
    const char* kProgressFile = "journey_progress.cfg";
    Skills skills;
    Inventory bag;
    Equipment equip;
    CombatStyle combatStyle = CombatStyle::Accurate;
    {
        SetTraceLogLevel(LOG_ERROR);   // a missing file is normal on first run
        char* t = LoadFileText(kProgressFile);
        SetTraceLogLevel(LOG_INFO);
        if (t) {
            ParseProgress(t, &skills, &bag, &combatStyle, &equip);
            UnloadFileText(t);
        }
    }
    // The starting tools (bronze axe, small fishing net, tinderbox). Tools can't be dropped, so this only adds
    // them on a first run, or to a save from before tools existed.
    const bool gotKit = GiveStarterKit(&bag) > 0;
    if (args.xp > 0) skills.AddXp(Skill::Woodcutting, args.xp);   // dev flags
    for (const auto& sx : args.skillXp) {
        Skill sk;
        if (SkillFromKey(sx.first, &sk)) skills.AddXp(sk, sx.second);
    }
    if (args.hp >= 0) skills.SetHp(args.hp);
    if (args.cstyle >= 0) combatStyle = StyleFromIndex(args.cstyle);
    if (args.logs > 0) bag.Add(Item::Logs, args.logs);
    if (args.raw > 0) bag.Add(Item::RawShrimps, args.raw);
    for (const auto& it : args.items)
        if (it.first > 0 && it.first < static_cast<int>(Item::Count)) bag.Add(static_cast<Item>(it.first), it.second);
    for (int id : args.wear) {   // dev: put it on, whatever the levels
        EquipSlot es;
        if (id > 0 && id < static_cast<int>(Item::Count) && Equippable(static_cast<Item>(id), &es)) equip.Set(es, static_cast<Item>(id));
    }
    auto saveProgress = [&]() {
        if (args.shot) return;   // screenshot/test runs never touch the real save
        const std::string text = SerializeProgress(skills, bag, combatStyle, &equip);
        SaveFileText(kProgressFile, (char*)text.c_str());
    };
    if (gotKit) saveProgress();
    hud.Bind(&skills, &bag, &equip);
    if (args.tab >= 0) hud.ToggleTab(args.tab);
    if (args.sel >= 0) hud.SelectSlot(args.sel);

    Forest forest;
    forest.Init(map.Trees());
    for (int i = 0; i < args.fell; ++i) forest.Fell(i);
    Rocks rocks;
    rocks.Init(map.Rocks());
    Fires fires;
    for (const TileCoord& t : args.fires) fires.Light(&map, t);
    std::mt19937 rng(args.hasSeed ? args.seed : std::random_device{}());
    auto roll = [&rng]() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); };
    Npcs npcs;
    npcs.Init(map);
    GroundItems ground;
    Shops shops;
    shops.Init();
    People people;
    people.Init(map);
    if (args.coins > 0) bag.Add(Item::Coins, args.coins);
    for (const auto& pa : args.piles)
        if (pa.item > 0 && pa.item < static_cast<int>(Item::Count)) ground.Drop({pa.x, pa.z}, {static_cast<Item>(pa.item), 1});
    float heroBar = 0.0f;   // seconds left to show the hero's health bar
    HudEvents events(
        hud, saveProgress,
        [&](bool onPlayer, int npcId) {
            if (onPlayer) return Vector3{player.X(), 1.25f, player.Z()};
            const Npc* n = npcs.Find(npcId);
            return n ? Vector3{n->x, AnimalRenderer::Height(n->species) * 0.62f, n->z} : Vector3{0, 0, 0};
        },
        [&]() { camera.SnapTo(player.X(), player.Z()); }, &heroBar);
    Gameplay game(map, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, events, roll);
    game.SetStyle(combatStyle);
    hud.SetCombatStyle(combatStyle);
    PropRenderer props;
    props.Load(lit);
    AnimalRenderer animals;
    animals.Load(lit);

    hud.Say("Welcome to Journey.", {255, 255, 255, 255});
    hud.Say("Tap to walk, chop, fish or fight.", {190, 210, 235, 255});
    hud.Say("Hold (or right-click) for more options.", {190, 210, 235, 255});
    hud.Say("Chickens and cows: bones, meat and hides.", {190, 210, 235, 255});
    hud.Say("Mine ore in the quarry, smelt and smith at the smithy.", {190, 210, 235, 255});
    hud.Say("Goblins pay in coins. Millbrook village is a long walk south.", {190, 210, 235, 255});

    // What is under this screen point? Everything the ray touches (nearest first), plus the things standing on the tile
    // it lands on (so tapping the foot of something works too). hasGround/gx/gz = where it hits the ground.
    auto gatherHits = [&](Vector2 pos, bool* hasGround, float* gx, float* gz) {
        const Camera3D c = camera.Get();
        const Ray ray = GetScreenToWorldRay(pos, c);
        std::vector<RayHit> hits;
        CollectNpcs(npcs, ray, &hits);
        CollectPeople(people, ray, &hits);
        CollectFires(fires, ray, &hits);
        CollectRocks(rocks, ray, &hits);
        CollectStations(map.Stations(), ray, &hits);
        CollectPiles(ground, ray, &hits);
        CollectSpots(map.Spots(), ray, &hits);
        CollectTrees(forest, ray, &hits);
        *hasGround = camera.ScreenToGround(pos, gx, gz);
        if (*hasGround) {
            const TileCoord t{(int)std::floor(*gx), (int)std::floor(*gz)};
            const float d = std::sqrt((*gx - c.position.x) * (*gx - c.position.x) + (*gz - c.position.z) * (*gz - c.position.z) + c.position.y * c.position.y);
            if (map.InBounds(t.x, t.z)) {
                const int tree = forest.IndexAtTile(t);
                if (tree >= 0) hits.push_back({d, {TargetKind::Tree, tree}});
                for (size_t i = 0; i < map.Spots().size(); ++i)
                    if (map.Spots()[i].Tile() == t) hits.push_back({d, {TargetKind::Spot, static_cast<int>(i)}});
                if (const Fire* f = fires.BurningAt(t)) hits.push_back({d, {TargetKind::Fire, f->id}});
                const int who = people.AtTile(t);
                if (who >= 0) hits.push_back({d, {TargetKind::Person, who}});
                const int rock = rocks.IndexAtTile(t);
                if (rock >= 0) hits.push_back({d, {TargetKind::Rock, rock}});
                for (size_t i = 0; i < map.Stations().size(); ++i)
                    if (map.Stations()[i].Tile() == t) hits.push_back({d, {TargetKind::Station, static_cast<int>(i)}});
                if (const Pile* p = ground.AtTile(t)) hits.push_back({d, {TargetKind::Pile, p->id}});
                const int npc = npcs.AliveAt(t);
                if (npc >= 0) hits.push_back({d, {TargetKind::Npc, npc}});
            } else {
                *hasGround = false;
            }
        }
        std::stable_sort(hits.begin(), hits.end(), [](const RayHit& a, const RayHit& b) { return a.dist < b.dist; });
        std::vector<Target> out;
        for (const RayHit& h : hits) {   // each thing once (its nearest hit), at most 6
            bool dup = false;
            for (const Target& t : out) dup = dup || (t.kind == h.target.kind && t.id == h.target.id);
            if (!dup && out.size() < 6) out.push_back(h.target);
        }
        return out;
    };
    // A plain tap on the world: do the default (first) option of what is under it - Attack, Chop down, Take, Walk here...
    auto worldTap = [&](Vector2 pos) {
        bool hasGround = false;
        float gx = 0, gz = 0;
        const std::vector<Target> under = gatherHits(pos, &hasGround, &gx, &gz);   // (separate statement: argument order is unspecified)
        const std::vector<MenuEntry> e = game.BuildMenu(under, hasGround, gx, gz);
        if (!e.empty() && e[0].act != MenuAct::Cancel) game.Perform(e[0]);
    };
    // Open the context menu for a screen point (long press / right click / --menu).
    auto openContextMenu = [&](Vector2 pos) {
        if (hud.MakeOpen() || hud.ShopOpen()) return;
        const int worn = hud.EquipSlotAt(pos);
        if (worn >= 0) {
            std::vector<MenuEntry> e = game.BuildEquipMenu(static_cast<EquipSlot>(worn));
            if (!e.empty()) hud.OpenMenu(pos, std::move(e));
            return;
        }
        const int slot = hud.BagSlotAt(pos);
        if (slot >= 0) {
            std::vector<MenuEntry> e = game.BuildItemMenu(slot);
            if (!e.empty()) hud.OpenMenu(pos, std::move(e));
            return;
        }
        if (hud.Hits(pos)) return;   // a panel with nothing to offer
        bool hasGround = false;
        float gx = 0, gz = 0;
        const std::vector<Target> hits = gatherHits(pos, &hasGround, &gx, &gz);
        hud.OpenMenu(pos, game.BuildMenu(hits, hasGround, gx, gz));
    };

    int frame = 0;
    while (!WindowShouldClose()) {
        const float dt = args.shot ? 1.0f / 60.0f : std::fmin(GetFrameTime(), 0.1f);
        ++frame;

        if (inCreator) {
            if (frame >= 3 && frame - 3 < (int)args.uiClicks.size() * 3 && (frame - 3) % 3 == 0) creator.InjectClick(args.uiClicks[(frame - 3) / 3]);
            BeginDrawing();
            ClearBackground(BLACK);
            if (creator.Frame(dt, lit, chars)) {
                look = creator.Look();
                saveLook();
                inCreator = false;
                hud.Say(TextFormat("Welcome, %s.", look.name.c_str()), {255, 255, 255, 255});
                camera.SnapTo(player.X(), player.Z());
            }
            EndDrawing();
            if (args.shot && frame >= args.frames) {
                TakeScreenshot(args.shot);
                break;
            }
            continue;
        }

        // ---- input ----
        const PointerFrame in = pointer.Poll();
        float rot = in.rotateDegrees;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_Q)) rot -= 110.0f * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_E)) rot += 110.0f * dt;
        camera.RotateDegrees(rot);
        float zoom = in.zoomDelta;
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_UP)) zoom -= 14.0f * dt;
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_DOWN)) zoom += 14.0f * dt;
        camera.ZoomBy(zoom);
        if (IsKeyPressed(KEY_F3)) debug = !debug;
        if (IsKeyPressed(KEY_F4) && ensureLoaded(1 - style)) style = 1 - style;
        if (IsKeyPressed(KEY_F5)) shadows = !shadows;
        if (IsKeyPressed(KEY_F6)) { game.Cancel(); player.Stop(); openCreator(); continue; }

        // Long press / right click: the full menu for what is under the finger.
        if (in.context) {
            hud.CloseMenu();
            openContextMenu(in.contextPos);
        }
        if (args.hasMenu && frame == 8) openContextMenu({args.menuX, args.menuY});
        if (args.menuNpc > 0 && frame == 8) {   // dev: long-press right on animal N
            if (const Npc* n = npcs.Find(args.menuNpc)) openContextMenu(GetWorldToScreen({n->x, AnimalRenderer::Height(n->species) * 0.5f, n->z}, camera.Get()));
        }

        // desktop: the line at the top-left says what a plain click would do (and how many other options there are)
        hud.SetPointer(args.hasHover ? Vector2{args.hoverX, args.hoverY} : in.hoverPos);
        hud.SetHover({}, 0);
#if !defined(__ANDROID__)
        {
            const Vector2 hp = args.hasHover ? Vector2{args.hoverX, args.hoverY} : in.hoverPos;
            if ((!args.shot || args.hasHover) && !hud.MenuOpen() && !hud.Hits(hp) && hp.x >= 0 && hp.y >= 0 && hp.x < GetScreenWidth() &&
                hp.y < GetScreenHeight()) {
                bool hasGround = false;
                float gx = 0, gz = 0;
                const std::vector<Target> under = gatherHits(hp, &hasGround, &gx, &gz);   // (separate statement: argument order is unspecified)
                const std::vector<MenuEntry> e = game.BuildMenu(under, hasGround, gx, gz);
                if (!e.empty() && e[0].act != MenuAct::Cancel) hud.SetHover(e[0].label, std::max(0, (int)e.size() - 2));
            }
        }
#endif

        if (in.tap) {
            if (hud.ShopOpen()) {
                // The shop window is a dialog: it takes every tap until it is closed.
                const Hud::ShopHit h = hud.ShopAt(in.tapPos);
                const ShopMenu menu = hud.Shop();
                const std::vector<ShopLine>& list = hud.ShopSellTab() ? menu.sell : menu.buy;
                if (h.kind == Hud::ShopHitKind::Buy && h.index < static_cast<int>(list.size())) {
                    game.Buy(menu.shop, list[h.index].index, hud.ShopQuantity());
                    hud.RefreshShop(game.BuildShopMenu(menu.shop));
                } else if (h.kind == Hud::ShopHitKind::Sell && h.index < static_cast<int>(list.size())) {
                    game.Sell(menu.shop, list[h.index].item, hud.ShopQuantity());
                    hud.RefreshShop(game.BuildShopMenu(menu.shop));
                } else if (h.kind == Hud::ShopHitKind::TabBuy) {
                    hud.SetShopTab(false);
                } else if (h.kind == Hud::ShopHitKind::TabSell) {
                    hud.SetShopTab(true);
                } else if (h.kind == Hud::ShopHitKind::Qty) {
                    hud.SetShopQuantity(h.index);
                } else if (h.kind == Hud::ShopHitKind::PagePrev) {
                    hud.ShopPage(-1);
                } else if (h.kind == Hud::ShopHitKind::PageNext) {
                    hud.ShopPage(1);
                } else if (h.kind == Hud::ShopHitKind::Close || !hud.ShopHits(in.tapPos)) {
                    hud.CloseShop();
                }
            } else if (hud.MakeOpen()) {
                // The make panel is a dialog: it takes every tap until it is closed.
                const Hud::MakeHit h = hud.MakeAt(in.tapPos);
                const MakeMenu menu = hud.Make();
                if (h.kind == Hud::MakeHitKind::Option && h.index < static_cast<int>(menu.options.size())) {
                    const MakeOption& o = menu.options[h.index];
                    if (menu.kind == MakeMenu::Kind::Smelt) game.StartSmelt(static_cast<Tier>(o.index), hud.MakeQuantity());
                    else game.StartSmith(static_cast<Tier>(menu.tier), static_cast<GearType>(o.index), hud.MakeQuantity());
                    if (game.Current() == Activity::Smelting || game.Current() == Activity::Smithing) hud.CloseMake();   // (otherwise it says why not)
                } else if (h.kind == Hud::MakeHitKind::Tier) {
                    hud.OpenMake(game.BuildSmithMenu(static_cast<Tier>(h.index)));
                } else if (h.kind == Hud::MakeHitKind::Qty) {
                    hud.SetMakeQuantity(h.index);
                } else if (h.kind == Hud::MakeHitKind::Close || !hud.MakeHits(in.tapPos)) {
                    hud.CloseMake();
                }
            } else if (hud.MenuOpen()) {
                // A menu is up: a tap on a row does that, a tap anywhere else just closes it.
                const int row = hud.MenuEntryAt(in.tapPos);
                if (row >= 0) {
                    const MenuEntry chosen = hud.MenuEntryRef(row);
                    hud.CloseMenu();
                    game.Perform(chosen);
                } else {
                    hud.CloseMenu();
                }
            } else {
                const bool onUi = hud.Hits(in.tapPos);
                const int tab = hud.TabAt(in.tapPos);
                if (tab == Hud::kTabMenu) { game.Cancel(); player.Stop(); openCreator(); continue; }   // "Menu" tab = change appearance (for now)
                if (tab >= 0) {
                    hud.ToggleTab(tab);
                } else if (onUi) {
                    // Bag: tap an item to select it (tap again to deselect), then tap one of its buttons.
                    // While an item is being "used", tapping another bag item is the thing to use it with.
                    const int slot = hud.BagSlotAt(in.tapPos);
                    ItemAction action = ItemAction::Drop;
                    const int styleBtn = hud.StyleAt(in.tapPos);
                    const int wornSlot = hud.EquipSlotAt(in.tapPos);
                    if (wornSlot >= 0) {
                        game.Unequip(static_cast<EquipSlot>(wornSlot));   // tap a worn piece to take it off
                    } else if (slot >= 0) {
                        if (game.UseSlot() >= 0) {
                            game.UseItemOnSlot(slot);
                        } else if (bag.Slot(slot).item != Item::None) {
                            hud.SelectSlot(slot == hud.SelectedSlot() ? -1 : slot);
                        }
                    } else if (hud.ActionAt(in.tapPos, &action)) {
                        game.DoItemAction(hud.SelectedSlot(), action);
                    } else if (styleBtn >= 0) {
                        combatStyle = StyleFromIndex(styleBtn);
                        game.SetStyle(combatStyle);
                        hud.SetCombatStyle(combatStyle);
                        saveProgress();
                    }
                } else {
                    worldTap(in.tapPos);
                }
            }
        }
        if (args.hasTapXY && frame == 5) worldTap({args.tapX, args.tapY});   // dev: a real tap through the whole menu pipeline
        if (args.tapNpc > 0 && frame == 5) {
            if (const Npc* n = npcs.Find(args.tapNpc)) worldTap(GetWorldToScreen({n->x, AnimalRenderer::Height(n->species) * 0.5f, n->z}, camera.Get()));
        }
        if (args.hasClick && frame == 5) game.WalkTo(args.clickX + 0.5f, args.clickZ + 0.5f);
        if (args.doSmelt >= 0 && frame == 6) game.StartSmelt(static_cast<Tier>(std::clamp(args.doSmelt, 0, kTierCount - 1)), 99);
        if (args.doSmithTier >= 0 && frame == 6)
            game.StartSmith(static_cast<Tier>(std::clamp(args.doSmithTier, 0, kTierCount - 1)), static_cast<GearType>(std::clamp(args.doSmithType, 0, kGearTypeCount - 1)), 99);
        if (args.talk > 0 && frame == 5) game.TapPerson(args.talk);
        if (args.shop >= 0 && frame == 6) hud.OpenShop(game.BuildShopMenu(std::clamp(args.shop, 0, shops.Count() - 1)));
        if (args.make >= 0 && frame == 6) hud.OpenMake(args.make == 0 ? game.BuildSmeltMenu() : game.BuildSmithMenu(static_cast<Tier>(std::clamp(args.makeTier, 0, kTierCount - 1))));
        if (frame == 5) {   // dev: pretend taps
            if (args.tapTree >= 0 && args.tapTree < (int)forest.Trees().size()) game.TapTree(args.tapTree);
            if (args.tapSpot >= 0 && args.tapSpot < (int)map.Spots().size()) game.TapSpot(args.tapSpot);
            if (args.attack > 0) game.TapNpc(args.attack);
            if (args.tapRock >= 0) game.TapRock(args.tapRock);
            if (args.tapStation >= 0) game.TapStation(args.tapStation);
            if (args.light) game.DoItemAction(bag.FindSlot(Item::Logs), ItemAction::Light);
            if (args.cook) {
                if (const Fire* f = fires.NearestBurning(player.Tile())) game.TapFire(f->id);
            }
        }
        {   // a tap that started a job flashes a marker on its target (red = interact, yellow = walk)
            float mx, mz;
            bool red = true;
            if (game.TakeMarker(&mx, &mz, &red)) marker = {mx, mz, 0.0f, red};
        }

        // ---- update ----
        player.Update(dt);
        forest.Update(dt);
        rocks.Update(dt);
        shops.Update(dt);
        npcs.SetHero(skills.CombatLevel(), skills.Hp() > 0);
        fires.Update(dt, &map);
        ground.Update(dt);
        npcs.Update(dt, map, player.Tile(), roll);
        game.Update(dt);
        heroBar = std::max(0.0f, heroBar - dt);
        if (game.FightTarget() >= 0) heroBar = 6.0f;
        camera.Follow(player.X(), player.Z(), dt);
        marker.age += dt;
        const TileCoord pt = player.Tile();
        const std::string& area = map.AreaAt(pt.x, pt.z);
        if (area != lastArea) {
            lastArea = area;
            if (!area.empty()) hud.ShowArea(area);
        }
        if (hud.MakeOpen() && player.Moving()) hud.CloseMake();   // he walked off: the dialog goes away
        if (hud.ShopOpen() && player.Moving()) hud.CloseShop();
        hud.Update(dt);
        hud.SetUseSlot(game.UseSlot());

        // ---- draw ----
        const Camera3D cam = camera.Get();
        TownRenderer& town = towns[style];
        const bool hi = (style == 1);
        const bool sunShadows = hi && shadows;
        CharacterPose pose;
        pose.x = player.X();
        pose.z = player.Z();
        pose.facing = player.FacingRadians();
        pose.phase = player.WalkPhase();
        pose.moving = player.Moving();
        pose.anim = AnimFor(game.Current());
        pose.strikeAge = game.StrikeAge();
        pose.strikeLeft = game.StrikeLeft();
        pose.time = args.shot ? frame * dt : (float)GetTime();
        pose.gear = LookOf(equip, bag, skills);
        pose.weaponAnim = equip.Anim();
        pose.workPhase = game.WorkPhase();
        pose.blockAge = game.BlockAge();
        pose.blockLanded = game.BlockLanded();
        if (args.anim > 0) pose.anim = static_cast<Anim>(args.anim);
        if (args.atime >= 0.0f) pose.time = args.atime;
        if (args.anim == 5) pose.strikeAge = std::fmod(pose.time, 1.2f) * 0.5f;   // dev: a swing every 1.2 s (or frozen with --atime)
        if (args.anim >= 7) pose.workPhase = std::fmod(pose.time / (args.anim == 7 ? kMineSwingSeconds : 0.75f), 1.0f);   // dev: pose a work swing
        if (args.block >= 0.0f) {                                                  // dev: a blocked strike, frozen T seconds in
            pose.blockAge = args.block;
            pose.blockLanded = args.blockHit;
        }
        lit.SetFrame(cam.position, args.shot ? frame * dt : (float)GetTime(), hi);
        BeginDrawing();
        if (sunShadows) {   // pass 1: depth from the sun's point of view
            town.SetShadowSampling(lit, false);
            chars.SetShadowSampling(lit, false);
            lit.BeginShadowPass(player.X(), player.Z());
            town.Draw(player.X(), player.Z(), pt, true);
            town.DrawTrees(forest, cam.position, cam.target, player.X(), player.Z(), true, false);
            chars.Draw(look, pose);
            lit.EndShadowPass();
        }
        ClearBackground(LitShader::SkyColor());
        town.SetShadowSampling(lit, sunShadows);
        chars.SetShadowSampling(lit, sunShadows);
        BeginMode3D(cam);
        lit.BindShadow(sunShadows);
        town.Draw(player.X(), player.Z(), pt);
        town.DrawTrees(forest, cam.position, cam.target, player.X(), player.Z(), false, !sunShadows);
        props.SetShadowSampling(lit, sunShadows);
        props.DrawFires(fires, pose.time, player.X(), player.Z());
        props.DrawFishSpots(map.Spots(), pose.time, player.X(), player.Z());
        props.DrawRocks(rocks, pose.time, player.X(), player.Z());
        int busyStation = -1;
        float workX = 0, workZ = 0;
        const bool working = game.WorkSpot(&workX, &workZ);
        if (working && game.Current() == Activity::Smelting)
            for (size_t i = 0; i < map.Stations().size(); ++i)
                if (map.Stations()[i].Tile() == TileCoord{(int)workX, (int)workZ}) busyStation = (int)i;
        props.DrawStations(map.Stations(), busyStation, pose.time, player.X(), player.Z());
        props.DrawGroundItems(ground, pose.time, player.X(), player.Z());
        animals.SetShadowSampling(lit, sunShadows);
        for (const Npc& n : npcs.All())
            if (animals.Visible(n, player.X(), player.Z())) animals.Draw(n, pose.time);
        if (args.zoo > 0) {   // dev: a row of every animal (zoo 11-15 = goblins only)
            const int zmode = args.zoo >= 10 ? args.zoo - 10 : args.zoo, sp0 = args.zoo >= 10 ? 3 : 0;
            int k = 0;
            for (int sp = sp0; sp < kSpeciesCount; ++sp)
                for (int cw = 0; cw < 2; ++cw, ++k) {
                    Npc z;
                    z.id = k + 1;
                    z.species = static_cast<Species>(sp);
                    z.colorway = cw;
                    z.x = player.X() + (cw * 2 - 1) * 1.3f;
                    z.z = player.Z() + 2.0f + (sp - sp0) * 1.9f;
                    z.facing = 3.14159f + 0.5f * (k % 3 - 1);
                    const float zt = args.atime >= 0.0f ? args.atime : pose.time;
                    if (zmode == 2) { z.path.push_back({0, 0}); z.walkPhase = zt * 6.0f; }
                    if (zmode == 3) z.attackT = std::fmod(zt, 1.0f) * 0.6f;
                    if (zmode == 4) z.hurtT = std::fmod(zt, 1.0f) * 0.4f;
                    if (zmode == 5) { z.dead = true; z.deathT = std::fmod(zt, 3.0f); }
                    animals.Draw(z, pose.time);
                }
        }
        chars.Draw(look, pose);
        if (working && game.Current() == Activity::Mining) {            // sparks fly from the rock each time the pick lands
            const int ri = rocks.IndexAtTile({(int)workX, (int)workZ});
            if (ri >= 0) {
                const float dx = player.X() - workX, dz = player.Z() - workZ, len = std::max(0.01f, std::sqrt(dx * dx + dz * dz));
                props.DrawSparks({workX + dx / len * 0.42f, 0.6f, workZ + dz / len * 0.42f}, rocks.All()[ri].hitAge / 0.35f, pose.time, {255, 226, 150, 255});
            }
        } else if (working && game.Current() == Activity::Smithing && pose.workPhase > 0.001f) {   // and from the anvil at every blow
            props.DrawSparks({workX, 0.82f, workZ}, pose.workPhase / 0.30f, pose.time, {255, 190, 80, 255});
        }
        if (!sunShadows) DrawCylinder({player.X(), 0.02f, player.Z()}, 0.46f, 0.46f, 0.01f, 16, Fade(BLACK, 0.28f));
        DrawMarker(marker);
        if (debug) DrawDebugGrid(map, player);
        EndMode3D();
        // health bars over whoever is fighting, then hit splats (drawn in screen space, under the HUD panels)
        for (const Npc& n : npcs.All())
            if (n.Alive() && n.hp < n.maxHp && (n.hurtT < 6.0f || n.attackT < 6.0f))
                hud.QueueBar({n.x, AnimalRenderer::Height(n.species) + 0.18f, n.z}, (float)n.hp / (float)n.maxHp, false);
        if (heroBar > 0.0f) hud.QueueBar({player.X(), 2.0f, player.Z()}, (float)skills.Hp() / (float)std::max(1, skills.MaxHp()), true);
        hud.DrawWorld(cam);
        hud.Draw(debug, TextFormat("Journey prototype  |  %d FPS  |  tile %d,%d  |  style %s  |  F3 debug  F4 style  F5 shadows  F6 character", GetFPS(),
                                    pt.x, pt.z, kStyleName[style]));
        EndDrawing();

        if (args.shot && frame >= args.frames) {
            TraceLog(LOG_INFO, "SHOT hero tile %d,%d moving=%d", pt.x, pt.z, player.Moving());
            TakeScreenshot(args.shot);
            break;
        }
    }

    animals.Unload();
    props.Unload();
    chars.Unload();
    for (int i = 0; i < 2; ++i)
        if (loaded[i]) towns[i].Unload();
    lit.Unload();
    CloseWindow();
    return 0;
}
