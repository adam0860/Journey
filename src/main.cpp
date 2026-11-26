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
//   --bank                        open the bank window on frame 6 (with a few things in the bank);  --arrows N  put N bronze arrows in the quiver;  --spell N  choose spell N (0..7)
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
#include <set>
#include <string>
#include <vector>

#include "game/Appearance.h"
#include "game/Archery.h"
#include "game/Bank.h"
#include "game/Magic.h"
#include "game/Equipment.h"
#include "game/Gear.h"
#include "game/Mining.h"
#include "game/People.h"
#include "game/Shops.h"
#include "game/Smithing.h"
#include "game/Fires.h"
#include "game/Forest.h"
#include "game/Adventurers.h"
#include "game/Gameplay.h"
#include "game/GroundItems.h"
#include "game/Npc.h"
#include "game/Inventory.h"
#include "game/Player.h"
#include "game/Progress.h"
#include "game/Skills.h"
#include "game/Woodcutting.h"
#include "game/Settings.h"
#include "platform/Audio.h"
#include "platform/PointerInput.h"
#include "raylib.h"
#include "render/CameraRig.h"
#include "render/AnimalRenderer.h"
#include "render/CharacterRenderer.h"
#include "render/Effects.h"
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
    int spellPage = 0;       // dev: --spellpage N (turn the spellbook page)
    bool helpPage = false;   // dev: --helppage (with --tab 7)
    float skull = 0.0f;      // dev: --skull SECONDS (start skulled)
    int scroll = 0;          // dev: --scroll N (scroll the open page down N pixels, for the quest log and the how-to-play page)
    bool hasSeed = false;
    unsigned seed = 0;
    int xp = 0, logs = 0, fell = 0;
    int raw = 0;
    std::vector<std::pair<std::string, int>> skillXp;
    int tapTree = -1, tapSpot = -1;
    bool light = false, cook = false, bonfire = false;
    std::vector<TileCoord> fires;
    int anim = 0;            // dev: force an animation (1 chop, 2 fish, 3 light, 4 cook)
    float atime = -1.0f;     // dev: freeze the animation clock at this time
    int sel = -1;            // dev: select this bag slot
    int attack = 0, hp = -1, cstyle = -1, menuNpc = 0, tapNpc = 0;
    float advFast = 0.0f;    // dev: let the other adventurers play this many seconds before the first frame
    int advNear = -1;        // dev: stand next to adventurer number N (0 = the first real one)
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
    bool bank = false;
    int arrows = 0, spell = -1;
    int quest = -1;                 // --quest N: start quest N
    int zooFrom = -1, zooTo = -1;   // --zsp A B: only species A..B in the zoo
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
        else if (is("--spellpage") && more(1)) a.spellPage = std::atoi(argv[++i]);
        else if (is("--scroll") && more(1)) a.scroll = std::atoi(argv[++i]);
        else if (is("--skull") && more(1)) a.skull = static_cast<float>(std::atof(argv[++i]));
        else if (is("--helppage")) a.helpPage = true;
        else if (is("--seed") && more(1)) { a.hasSeed = true; a.seed = (unsigned)std::atoi(argv[++i]); }
        else if (is("--xp") && more(1)) a.xp = std::atoi(argv[++i]);
        else if (is("--logs") && more(1)) a.logs = std::atoi(argv[++i]);
        else if (is("--advfast") && more(1)) a.advFast = static_cast<float>(std::atof(argv[++i]));
        else if (is("--advnear") && more(1)) a.advNear = std::atoi(argv[++i]);
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
        else if (is("--bonfire")) a.bonfire = true;
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
        else if (is("--bank")) a.bank = true;
        else if (is("--quest") && more(1)) a.quest = std::atoi(argv[++i]);
        else if (is("--zsp") && more(2)) { a.zooFrom = std::atoi(argv[i + 1]); a.zooTo = std::atoi(argv[i + 2]); i += 2; }
        else if (is("--arrows") && more(1)) a.arrows = std::atoi(argv[++i]);
        else if (is("--spell") && more(1)) a.spell = std::atoi(argv[++i]);
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
        const float height = AnimalRenderer::Height(n.species);
        const float half = std::clamp(0.38f * height, 0.38f, 1.7f);
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

// The nearest adventurer the ray touches (their id), or -1.
int AdventurerHit(const Adventurers& advs, const Ray& ray) {
    int best = -1;
    float bd = 1e9f;
    for (const Adventurer& a : advs.All()) {
        if (a.pvp.dead) continue;
        const BoundingBox box = {{a.x - 0.45f, 0.0f, a.z - 0.45f}, {a.x + 0.45f, 1.9f, a.z + 0.45f}};
        const RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit && hit.distance < bd) {
            bd = hit.distance;
            best = a.id;
        }
    }
    return best;
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
        if (audio_ && text.rfind("You eat the", 0) == 0) audio_->Play(Sfx::Eat);
    }
    void XpDrop(Skill skill, int amount) override {
        hud_.XpDrop(skill, amount);
        if (audio_) audio_->Play(Sfx::Ding, 0.45f);
    }
    void LevelUp(Skill skill, int level) override {
        hud_.Say(TextFormat("Congratulations, you just advanced a %s level. You are now level %d.", SkillName(skill), level),
                 {255, 235, 120, 255});
        hud_.ShowArea(TextFormat("%s level %d", SkillName(skill), level));
        if (audio_) audio_->Play(Sfx::LevelUp);
    }
    void Hit(bool onPlayer, int npcId, int damage) override {
        hud_.AddHitsplat(where_(onPlayer, npcId), damage);
        if (audio_) audio_->Play(damage > 0 ? Sfx::Hit : Sfx::Miss, onPlayer ? 0.6f : 0.8f, onPlayer ? 0.85f : 1.0f);
        if (onPlayer && heroBar_) *heroBar_ = 6.0f;   // show the hero's health bar for a few seconds after he is struck at
    }
    void PlayerDied() override {
        hud_.ShowArea("Oh dear, you are dead!");
        if (audio_) audio_->Play(Sfx::Death);
        died_();
    }
    void Changed() override { save_(); }
    void OpenMake(const MakeMenu& menu) override { hud_.OpenMake(menu); }
    void OpenShop(const ShopMenu& menu) override { hud_.OpenShop(menu); }
    void OpenBank() override { hud_.OpenBank(); }
    void Shoot(const ShotFx& fx) override {
        if (audio_) audio_->Play(fx.magic ? Sfx::Cast : Sfx::Shoot, fx.toHero ? 0.5f : 0.8f);
        if (!effects_) return;
        if (fx.toHero) effects_->AddShot(fx, where_(false, fx.npcId).y, 1.2f);   // a monster shooting at the hero
        else effects_->AddShot(fx, 1.2f, where_(false, fx.npcId).y);
    }
    void SetEffects(Effects* e) { effects_ = e; }
    void SetAudio(AudioSystem* a) { audio_ = a; }

private:
    Effects* effects_ = nullptr;
    AudioSystem* audio_ = nullptr;
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
        case Activity::Crafting: return Anim::Craft;
        default: return Anim::None;
    }
}

// The model files for a set of worn items.
GearLook GearLookOf(Item weapon, Item shield, Item helm, Item body, Item legs, Item pick, Item feet = Item::None, Item hands = Item::None,
                    Item cape = Item::None) {
    GearLook look;
    auto name = [](Item item, const char* prefix) {
        Tier t;
        GearType g;
        if (const RobeInfo* robe = FindRobe(item)) return std::string(robe->model);
        if (const LeatherInfo* leather = FindLeather(item)) return std::string(leather->model);
        if (!GearOf(item, &t, &g)) return std::string();
        return std::string(prefix) + TierKey(t) + "_" + GearTypeKey(g);
    };
    look.weapon = name(weapon, "held_");
    if (const BowInfo* bow = FindBow(weapon)) {           // a bow goes in the left hand, a staff in the right
        look.weapon.clear();
        look.bow = std::string("held_") + bow->model;
    } else if (const StaffInfo* staff = FindStaff(weapon)) {
        look.weapon = std::string("held_") + staff->model;
    }
    look.shield = name(shield, "held_");
    look.helm = name(helm, "gear_");
    look.body = name(body, "gear_");
    look.legs = name(legs, "gear_");
    look.pick = name(pick, "held_");
    look.boots = name(feet, "gear_");
    look.gloves = name(hands, "gear_");
    if (const CapeInfo* c = FindCape(cape)) look.cape = std::string("gear_cape_") + c->key;
    return look;
}

// What he wears and wields right now.
GearLook LookOf(const Equipment& eq, const Inventory& bag, const Skills& skills) {
    GearLook look = GearLookOf(eq.Get(EquipSlot::Weapon), eq.Get(EquipSlot::Shield), eq.Get(EquipSlot::Head), eq.Get(EquipSlot::Body),
                               eq.Get(EquipSlot::Legs), BestPickaxe(bag, skills.Level(Skill::Mining)), eq.Get(EquipSlot::Feet), eq.Get(EquipSlot::Hands),
                               eq.Get(EquipSlot::Cape));
    switch (eq.Get(EquipSlot::Amulet)) {   // the pendant is drawn on his chest (rings are too small to see)
        case Item::GoldAmulet: look.amulet = "gear_amulet_gold"; break;
        case Item::SapphireAmulet: look.amulet = "gear_amulet_sapphire"; break;
        case Item::EmeraldAmulet: look.amulet = "gear_amulet_emerald"; break;
        case Item::RubyAmulet: look.amulet = "gear_amulet_ruby"; break;
        case Item::DiamondAmulet: look.amulet = "gear_amulet_diamond"; break;
        case Item::DragonstoneAmulet: look.amulet = "gear_amulet_dragonstone"; break;
        case Item::MummysAmulet: look.amulet = "gear_amulet_mummy"; break;
        case Item::TrollToothAmulet: look.amulet = "gear_amulet_troll"; break;
        case Item::AbyssalAmulet: look.amulet = "gear_amulet_diamond"; break;
        case Item::OpalAmulet: look.amulet = "gear_amulet_opal"; break;
        case Item::JadeAmulet: look.amulet = "gear_amulet_jade"; break;
        case Item::RedTopazAmulet: look.amulet = "gear_amulet_topaz"; break;
        default: break;
    }
    return look;
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

    // ---- sound: music and effects are made in code (game/Synth); the volumes live in their own small file ----
    AudioSystem audio;
    Settings settings;
    const char* kSettingsFile = "journey_settings.cfg";
    {
        char* t = LoadFileText(kSettingsFile);
        if (t) {
            settings = ParseSettings(t);
            UnloadFileText(t);
        }
    }
    if (!args.shot) audio.Init();   // (screenshot runs stay silent)
    audio.SetMusicVolume(settings.music);
    audio.SetSfxVolume(settings.sfx);

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
    Bank bank;
    QuestLog savedQuests;   // read from the save now, handed to the game once it exists
    QuestLog* questsLive = nullptr;
    Adventurers* advsLive = nullptr;
    const char* kAdventurersFile = "journey_adventurers.cfg";
    int savedPkKills = 0, savedPkDeaths = 0;   // his record against other players
    std::function<int()> pkKillsNow, pkDeathsNow;
    float savedSkull = 0.0f;                   // the skull he wore when he left (seconds)
    std::function<float()> skullNow;           // ...and, once the game exists, the skull he wears now
    std::set<std::string> savedVisited;   // places he has been to (read now, handed to the game; saved from the game afterwards)
    const std::set<std::string>* visitedLive = nullptr;
    int spellSel = 0;
    CombatStyle combatStyle = CombatStyle::Accurate;
    bool firstRun = true;
    {
        SetTraceLogLevel(LOG_ERROR);   // a missing file is normal on first run
        char* t = LoadFileText(kProgressFile);
        SetTraceLogLevel(LOG_INFO);
        if (t) {
            firstRun = false;
            ParseProgress(t, &skills, &bag, &combatStyle, &equip, &bank, &spellSel, &savedQuests, &savedVisited, &savedSkull, &savedPkKills, &savedPkDeaths);
            UnloadFileText(t);
        }
    }
    // The starting tools (axe, net, tinderbox, pickaxe, hammer) are given on the very first run only. They can be dropped
    // or sold like anything else, so they must not come back every time the game starts.
    const bool gotKit = firstRun && GiveStarterKit(&bag) > 0;
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
        if (id > 0 && id < static_cast<int>(Item::Count) && Equippable(static_cast<Item>(id), &es)) {
            if (es == EquipSlot::Ammo) equip.SetAmmo(static_cast<Item>(id), 60);
            else equip.Set(es, static_cast<Item>(id));
        }
    }
    auto saveProgress = [&]() {
        if (args.shot) return;   // screenshot/test runs never touch the real save
        const std::string text = SerializeProgress(skills, bag, combatStyle, &equip, &bank, spellSel, questsLive ? questsLive : &savedQuests, visitedLive ? visitedLive : &savedVisited,
                                                  skullNow ? skullNow() : savedSkull, pkKillsNow ? pkKillsNow() : savedPkKills, pkDeathsNow ? pkDeathsNow() : savedPkDeaths);
        SaveFileText(kProgressFile, (char*)text.c_str());
        if (advsLive) {   // the other adventurers are saved with him
            const std::string adv = advsLive->Save();
            SaveFileText(kAdventurersFile, (char*)adv.c_str());
        }
    };
    if (gotKit) saveProgress();
    if (args.arrows > 0) equip.SetAmmo(Item::BronzeArrows, args.arrows);
    if (args.spell >= 0) spellSel = std::clamp(args.spell, 0, SpellCount() - 1);
    if (args.bank) {   // dev: something in the bank to look at
        bank.Deposit(Item::Coins, 12500);
        bank.Deposit(Item::Logs, 80);
        bank.Deposit(Item::IronOre, 30);
        bank.Deposit(Item::BronzeBar, 12);
        bank.Deposit(Item::BronzeArrows, 400);
        bank.Deposit(Item::FireRune, 150);
        bank.Deposit(Item::Shortbow, 1);
        bank.Deposit(Item::FireStaff, 1);
        bank.Deposit(Item::Bread, 7);
    }
    hud.Bind(&skills, &bag, &equip, &bank);
    hud.SetVolumes(settings.music, settings.sfx);
    if (args.tab >= 0) hud.ToggleTab(args.tab);
    if (args.spellPage > 0) hud.StepSpellPage(args.spellPage);
    if (args.helpPage) hud.SetHelpPage(true);
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
    hud.SetMap(&map, &people);
    if (args.coins > 0) bag.Add(Item::Coins, args.coins);
    for (const auto& pa : args.piles)
        if (pa.item > 0 && pa.item < static_cast<int>(Item::Count)) ground.Drop({pa.x, pa.z}, {static_cast<Item>(pa.item), 1});
    float heroBar = 0.0f;   // seconds left to show the hero's health bar
    HudEvents events(
        hud, saveProgress,
        [&](bool onPlayer, int npcId) {
            if (onPlayer) return Vector3{player.X(), 1.25f, player.Z()};
            if (IsFoeId(npcId)) {   // another player
                const Adventurer* ad = advsLive ? advsLive->Find(npcId - kFoeIdBase) : nullptr;
                return ad ? Vector3{ad->x, 1.25f, ad->z} : Vector3{0, 0, 0};
            }
            const Npc* n = npcs.Find(npcId);
            return n ? Vector3{n->x, AnimalRenderer::Height(n->species) * 0.62f, n->z} : Vector3{0, 0, 0};
        },
        [&]() { camera.SnapTo(player.X(), player.Z()); }, &heroBar);
    Effects effects;
    events.SetEffects(&effects);
    events.SetAudio(&audio);
    float actSound = 0.0f;                  // the rhythm of the chopping / mining / fishing sounds
    Activity lastActivity = Activity::None;
    Gameplay game(map, player, skills, bag, equip, forest, rocks, fires, npcs, ground, shops, people, bank, events, roll);
    game.SetStyle(combatStyle);
    game.SetSpell(spellSel);
    hud.SetSpell(spellSel);
    game.Quests() = savedQuests;
    questsLive = &game.Quests();
    game.SetVisited(savedVisited);
    game.SetSkull(savedSkull);
    skullNow = [&]() { return game.SkullLeft(); };
    game.SetPvpRecord(savedPkKills, savedPkDeaths);
    pkKillsNow = [&]() { return game.PvpKills(); };
    pkDeathsNow = [&]() { return game.PvpDeaths(); };
    visitedLive = &game.Visited();
    hud.Bind(&skills, &bag, &equip, &bank, questsLive);
    if (args.quest >= 0 && args.quest < game.Quests().Count()) game.Quests().Start(args.quest);   // dev
    hud.SetCombatStyle(combatStyle);
    // ---- the other adventurers: they start the game when he does, and play it by the same rules (game/Adventurers.h)
    Adventurers advs;
    AdvWorld advWorld;
    advWorld.map = &map;
    advWorld.forest = &forest;
    advWorld.rocks = &rocks;
    advWorld.fires = &fires;
    advWorld.npcs = &npcs;
    advWorld.people = &people;
    advWorld.roll = roll;
    advWorld.hero = player.Tile();
    {
        char* t = args.shot ? nullptr : LoadFileText(kAdventurersFile);
        if (t && advs.Load(advWorld, t)) {
            // (saved ones carry on)
        } else {
            advs.Init(advWorld);
        }
        if (t) UnloadFileText(t);
    }
    advsLive = &advs;
    game.SetFoes(&advs);
    if (args.skull > 0.0f) game.SetSkull(args.skull);   // dev
    advWorld.ground = &ground;
    advWorld.hitHero = [&](int foeId, int damage) { game.StrikeHero(foeId, damage); };
    if (args.advFast > 0.0f) advs.FastForward(args.advFast, advWorld);
    if (args.advNear >= 0) {   // dev: look at somebody
        int k = 0;
        for (const Adventurer& ad : advs.All())
            if (k++ == args.advNear) {
                player.Teleport(ad.Tile());
                camera.SnapTo(player.X(), player.Z());
            }
    }
    float advSaveLeft = 45.0f, rankLeft = 0.0f;
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
        {   // another adventurer: look at them, and they say something back
            const int who = AdventurerHit(advs, GetScreenToWorldRay(pos, camera.Get()));
            if (who >= 0 && IsDangerousArea(map.AreaAt(player.Tile().x, player.Tile().z))) {   // in the Abyss: a tap on a player is an attack
                game.TapFoe(kFoeIdBase + who);
                return;
            }
            if (who >= 0) {
                hud.Say(advs.ExamineText(who), {255, 224, 120, 255});
                const Adventurer* ad = advs.Find(who);
                const std::string reply = advs.TalkLine(who, roll);
                if (ad && !reply.empty()) hud.Say(ad->name + ": " + reply, {150, 235, 255, 255});
                return;
            }
        }
        bool hasGround = false;
        float gx = 0, gz = 0;
        const std::vector<Target> under = gatherHits(pos, &hasGround, &gx, &gz);   // (separate statement: argument order is unspecified)
        const std::vector<MenuEntry> e = game.BuildMenu(under, hasGround, gx, gz);
        if (!e.empty() && e[0].act != MenuAct::Cancel) game.Perform(e[0]);
    };
    // Open the context menu for a screen point (long press / right click / --menu).
    auto openContextMenu = [&](Vector2 pos) {
        if (hud.MakeOpen() || hud.ShopOpen() || hud.BankOpen() || hud.DialogueOpen()) return;
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
        {   // another adventurer: their name and combat level, coloured against the hero's like a creature's
            const int who = AdventurerHit(advs, GetScreenToWorldRay(pos, camera.Get()));
            const Adventurer* ad = who >= 0 ? advs.Find(who) : nullptr;
            if (ad) {
                const Rgb c = LevelColor(skills.CombatLevel(), ad->Combat());
                const std::string level = " (level-" + std::to_string(ad->Combat()) + ")";
                std::vector<MenuEntry> e;
                std::vector<const char*> verbs;
                if (IsDangerousArea(map.AreaAt(player.Tile().x, player.Tile().z))) verbs.push_back("Attack ");   // (the Abyss only)
                verbs.push_back("Talk-to ");
                verbs.push_back("Examine ");
                for (const char* verb : verbs) {
                    MenuEntry m;
                    m.label = {{verb, kMenuWhite}, {ad->name, kMenuNpc}, {level, c}};
                    m.act = std::string(verb) == "Examine " ? MenuAct::ExamineTarget : (std::string(verb) == "Attack " ? MenuAct::Attack : MenuAct::TalkTo);
                    m.target = {TargetKind::Adventurer, who};
                    e.push_back(m);
                }
                MenuEntry cancel;
                cancel.label = {{"Cancel", kMenuWhite}};
                cancel.act = MenuAct::Cancel;
                e.push_back(cancel);
                hud.OpenMenu(pos, std::move(e));
                return;
            }
        }
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
        // a drag that starts on a panel scrolls it (the quest log, the how-to-play page) and never turns the camera
        const bool dragOnHud = in.dragging && hud.Hits(in.dragStart);
        if (dragOnHud && hud.ScrollableAt(in.dragStart)) hud.ScrollBy(-in.dragDelta.y);
        float rot = dragOnHud ? 0.0f : in.rotateDegrees;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_Q)) rot -= 110.0f * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_E)) rot += 110.0f * dt;
        camera.RotateDegrees(rot);
        float zoom = in.zoomDelta;
        if (in.wheel != 0.0f && hud.ScrollableAt(in.hoverPos)) {   // the wheel over a scrollable page scrolls it instead of zooming
            zoom += in.wheel * kWheelZoomUnits;
            hud.ScrollBy(-in.wheel * 48.0f);
        }
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_UP)) zoom -= 14.0f * dt;
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_DOWN)) zoom += 14.0f * dt;
        camera.ZoomBy(zoom);
        if (IsKeyPressed(KEY_F3)) debug = !debug;
        if (IsKeyPressed(KEY_F4) && ensureLoaded(1 - style)) style = 1 - style;
        if (IsKeyPressed(KEY_F5)) shadows = !shadows;
        if (IsKeyPressed(KEY_F6)) { game.Cancel(); player.Stop(); openCreator(); continue; }
        if (IsKeyPressed(KEY_ESCAPE)) {   // Esc closes the top-most panel (the design document's keyboard control), one at a time
            if (hud.MenuOpen()) hud.CloseMenu();
            else if (hud.MakeOpen()) hud.CloseMake();
            else if (hud.ShopOpen()) hud.CloseShop();
            else if (hud.BankOpen()) hud.CloseBank();
            else if (hud.DialogueOpen()) game.DialogueClose();
            else if (hud.OpenTab() >= 0) hud.CloseTabs();
        }

        // Long press / right click: the full menu for what is under the finger.
        if (in.context) {
            hud.CloseMenu();
            openContextMenu(in.contextPos);
        }
        if (args.scroll != 0 && frame == 6) hud.ScrollBy(static_cast<float>(args.scroll));   // dev (the page's limit is known after one draw)
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
            if (hud.DialogueOpen()) {
                // A conversation: a tap on an option chooses it, a tap on the box of a plain page goes on to the next.
                const int choice = hud.DialogueAt(in.tapPos);
                if (choice >= 0) game.DialogueChoose(choice);
                else if (choice == Hud::kDialogueContinue) game.DialogueChoose(-1);
            } else if (hud.BankOpen()) {
                // The bank window is a dialog: it takes every tap until it is closed.
                const Hud::BankHit h = hud.BankAt(in.tapPos);
                if (h.kind == Hud::BankHitKind::BankCell && h.index < static_cast<int>(bank.Slots().size())) {
                    game.BankWithdraw(bank.Slots()[static_cast<size_t>(h.index)].item, hud.BankQuantity());
                    hud.BankPage(0);   // (a page that has emptied is clamped back)
                } else if (h.kind == Hud::BankHitKind::BagCell) {
                    game.BankDeposit(bag.Slot(h.index).item, hud.BankQuantity());
                } else if (h.kind == Hud::BankHitKind::DepositAll) {
                    game.BankDepositAll();
                } else if (h.kind == Hud::BankHitKind::Qty) {
                    hud.SetBankQuantity(h.index);
                } else if (h.kind == Hud::BankHitKind::PagePrev) {
                    hud.BankPage(-1);
                } else if (h.kind == Hud::BankHitKind::PageNext) {
                    hud.BankPage(1);
                } else if (h.kind == Hud::BankHitKind::Close || !hud.BankHits(in.tapPos)) {
                    hud.CloseBank();
                }
            } else if (hud.ShopOpen()) {
                // The shop window is a dialog: it takes every tap until it is closed.
                const Hud::ShopHit h = hud.ShopAt(in.tapPos);
                const ShopMenu menu = hud.Shop();
                const std::vector<ShopLine>& list = hud.ShopSellTab() ? menu.sell : menu.buy;
                if (h.kind == Hud::ShopHitKind::Buy && h.index < static_cast<int>(list.size())) {
                    game.Buy(menu.shop, list[h.index].index, hud.ShopQuantity());
                    audio.Play(Sfx::Coin);
                    hud.RefreshShop(game.BuildShopMenu(menu.shop));
                } else if (h.kind == Hud::ShopHitKind::Sell && h.index < static_cast<int>(list.size())) {
                    game.Sell(menu.shop, list[h.index].item, hud.ShopQuantity());
                    audio.Play(Sfx::Coin);
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
                    else if (menu.kind == MakeMenu::Kind::Craft) game.StartCraft(o.index, hud.MakeQuantity());
                    else game.StartSmith(static_cast<Tier>(menu.tier), static_cast<GearType>(o.index), hud.MakeQuantity());
                    if (game.Current() == Activity::Smelting || game.Current() == Activity::Smithing || game.Current() == Activity::Crafting) hud.CloseMake();   // (otherwise it says why not)
                } else if (h.kind == Hud::MakeHitKind::Tier) {
                    hud.OpenMake(game.BuildSmithMenu(static_cast<Tier>(h.index)));
                } else if (h.kind == Hud::MakeHitKind::Qty) {
                    hud.SetMakeQuantity(h.index);
                } else if (h.kind == Hud::MakeHitKind::Page) {
                    audio.Play(Sfx::Click);
                    hud.StepMakePage(h.index);
                } else if (h.kind == Hud::MakeHitKind::Close || !hud.MakeHits(in.tapPos)) {
                    hud.CloseMake();
                }
            } else if (hud.MenuOpen()) {
                // A menu is up: a tap on a row does that, a tap anywhere else just closes it.
                const int row = hud.MenuEntryAt(in.tapPos);
                if (row >= 0) {
                    const MenuEntry chosen = hud.MenuEntryRef(row);
                    hud.CloseMenu();
                    if (chosen.target.kind == TargetKind::Adventurer) {   // another "player": examine them, or have a word
                        const Adventurer* ad = advs.Find(chosen.target.id);
                        if (ad && chosen.act == MenuAct::Attack) game.TapFoe(kFoeIdBase + ad->id);
                        if (ad && chosen.act == MenuAct::ExamineTarget) hud.Say(advs.ExamineText(ad->id), {255, 224, 120, 255});
                        if (ad && chosen.act == MenuAct::TalkTo) {
                            const std::string reply = advs.TalkLine(ad->id, roll);
                            hud.Say(ad->name + ": " + (reply.empty() ? std::string("Hello there.") : reply), {150, 235, 255, 255});
                        }
                    } else {
                        game.Perform(chosen);
                    }
                } else {
                    hud.CloseMenu();
                }
            } else {
                const bool onUi = hud.Hits(in.tapPos);
                const int tab = hud.TabAt(in.tapPos);
                if (tab >= 0) {
                    audio.Play(Sfx::Click, 0.6f);
                    hud.ToggleTab(tab);
                } else if (onUi) {
                    // Bag: tap an item to select it (tap again to deselect), then tap one of its buttons.
                    // While an item is being "used", tapping another bag item is the thing to use it with.
                    const int slot = hud.BagSlotAt(in.tapPos);
                    ItemAction action = ItemAction::Drop;
                    const int styleBtn = hud.StyleAt(in.tapPos);
                    const int wornSlot = hud.EquipSlotAt(in.tapPos);
                    const int spellBtn = hud.SpellAt(in.tapPos);
                    const Hud::SettingsHit setHit = hud.SettingsAt(in.tapPos);
                    if (setHit != Hud::SettingsHit::None) {   // the Menu tab: appearance, music and sound volume
                        audio.Play(Sfx::Click);
                        if (setHit == Hud::SettingsHit::Help || setHit == Hud::SettingsHit::Back) {
                            hud.SetHelpPage(setHit == Hud::SettingsHit::Help);
                            continue;
                        }
                        if (setHit == Hud::SettingsHit::Appearance) {
                            hud.CloseTabs();
                            game.Cancel();
                            player.Stop();
                            openCreator();
                            continue;
                        }
                        const float step = setHit == Hud::SettingsHit::MusicUp || setHit == Hud::SettingsHit::SfxUp ? 0.1f : -0.1f;
                        if (setHit == Hud::SettingsHit::MusicUp || setHit == Hud::SettingsHit::MusicDown) settings.music = std::clamp(std::round((settings.music + step) * 10.0f) / 10.0f, 0.0f, 1.0f);
                        else settings.sfx = std::clamp(std::round((settings.sfx + step) * 10.0f) / 10.0f, 0.0f, 1.0f);
                        audio.SetMusicVolume(settings.music);
                        audio.SetSfxVolume(settings.sfx);
                        hud.SetVolumes(settings.music, settings.sfx);
                        if (!args.shot) {
                            const std::string text = SerializeSettings(settings);
                            SaveFileText(kSettingsFile, (char*)text.c_str());
                        }
                    } else if (hud.SpellPageStep(in.tapPos) != 0) {
                        audio.Play(Sfx::Click);
                        hud.StepSpellPage(hud.SpellPageStep(in.tapPos));
                    } else if (spellBtn >= 0) {
                        game.SetSpell(spellBtn);
                        spellSel = game.SpellIndex();
                        hud.SetSpell(spellSel);
                        saveProgress();
                    } else if (wornSlot >= 0) {
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
        if (args.bank && frame == 6) hud.OpenBank();
        if (args.shop >= 0 && frame == 6) hud.OpenShop(game.BuildShopMenu(std::clamp(args.shop, 0, shops.Count() - 1)));
        if (args.make >= 0 && frame == 6) hud.OpenMake(args.make == 0 ? game.BuildSmeltMenu() : game.BuildSmithMenu(static_cast<Tier>(std::clamp(args.makeTier, 0, kTierCount - 1))));
        if (frame == 5) {   // dev: pretend taps
            if (args.tapTree >= 0 && args.tapTree < (int)forest.Trees().size()) game.TapTree(args.tapTree);
            if (args.tapSpot >= 0 && args.tapSpot < (int)map.Spots().size()) game.TapSpot(args.tapSpot);
            if (args.attack > 0) game.TapNpc(args.attack);
            if (args.tapRock >= 0) game.TapRock(args.tapRock);
            if (args.tapStation >= 0) game.TapStation(args.tapStation);
            if (args.light) game.DoItemAction(bag.FindSlot(Item::Logs), ItemAction::Light);
            if (args.bonfire) game.DoItemAction(bag.FindSlot(Item::Logs), ItemAction::Bonfire);   // dev: four logs, lit as a bonfire
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
        advWorld.hero = player.Tile();
        advWorld.heroX = player.X();
        advWorld.heroZ = player.Z();
        advWorld.heroAlive = skills.Hp() > 0;
        advWorld.heroInDanger = IsDangerousArea(map.AreaAt(player.Tile().x, player.Tile().z));
        advWorld.heroSkulled = game.Skulled();
        advWorld.heroCombat = skills.CombatLevel();
        advWorld.heroFighter = game.HeroFighter();
        advs.Update(dt, advWorld);
        for (const AdvChat& c : advs.TakeChat()) {   // what people nearby say goes in the chat box too
            const float dx = c.x - player.X(), dz = c.z - player.Z();
            const float range = c.major ? 26.0f : 14.0f;
            if (dx * dx + dz * dz < range * range) hud.Say(c.name + ": " + c.text, {150, 235, 255, 255});
        }
        if ((rankLeft -= dt) <= 0.0f) {   // the hiscores for the help page
            rankLeft = 3.0f;
            std::vector<std::string> lines = {TextFormat("Players online: %d", advs.Online()), "Hiscores (total level):"};
            const std::vector<AdvRank> rank = advs.Ranking(look.name, skills.TotalLevel(), skills.CombatLevel());
            for (size_t i = 0; i < rank.size() && i < 8; ++i)
                lines.push_back(TextFormat("%2d. %s%s  total %d, combat %d", (int)i + 1, rank[i].name.c_str(), rank[i].hero ? " (you)" : "", rank[i].total, rank[i].combat));
            for (size_t i = 0; i < rank.size(); ++i)
                if (rank[i].hero) lines.push_back(TextFormat("Your rank: %d of %d (total level %d)", (int)i + 1, (int)rank.size(), rank[i].total));
            lines.push_back("");   // the Abyss: who has killed whom
            lines.push_back("Abyss fighters (players killed / times killed):");
            const std::vector<PkRank> pk = advs.PkRanking(look.name, game.PvpKills(), game.PvpDeaths());
            if (pk.empty()) lines.push_back("Nobody has fought in the Abyss yet.");
            for (size_t i = 0; i < pk.size() && i < 5; ++i)
                lines.push_back(TextFormat("%d. %s%s  %d / %d", (int)i + 1, pk[i].name.c_str(), pk[i].hero ? " (you)" : "", pk[i].kills, pk[i].deaths));
            lines.push_back(TextFormat("Your record: %d killed, %d times killed", game.PvpKills(), game.PvpDeaths()));
            hud.SetPlayerLines(lines);
        }
        if (!args.shot && (advSaveLeft -= dt) <= 0.0f) {
            advSaveLeft = 45.0f;
            saveProgress();
        }
        game.Update(dt);
        heroBar = std::max(0.0f, heroBar - dt);
        if (game.FightTarget() >= 0) heroBar = 6.0f;
        camera.Follow(player.X(), player.Z(), dt);
        camera.Avoid([&](Vector3 from, Vector3 to) { return loaded[style] && towns[style].SegmentBlocked(from, to, player.Tile()); }, dt);   // never behind a building
        marker.age += dt;
        const TileCoord pt = player.Tile();
        const std::string& area = map.AreaAt(pt.x, pt.z);
        if (area != lastArea) {
            lastArea = area;
            if (!area.empty()) hud.ShowArea(area);
        }
        audio.Update(dt, area);
        {   // the sound of the work: a chop, a clink or a splash in time with the swings
            const Activity act = game.Current();
            if (act != lastActivity) {
                lastActivity = act;
                actSound = 0.5f;
            }
            const bool working = act == Activity::Chopping || act == Activity::Mining || act == Activity::Smithing || act == Activity::Fishing;
            if (working) {
                actSound -= dt;
                if (actSound <= 0.0f) {
                    audio.Play(act == Activity::Chopping ? Sfx::Chop : (act == Activity::Fishing ? Sfx::Splash : Sfx::Mine), 0.7f);
                    actSound = act == Activity::Chopping ? 1.2f : (act == Activity::Fishing ? 2.4f : 1.4f);
                }
            }
        }
        if (hud.MakeOpen() && player.Moving()) hud.CloseMake();   // he walked off: the dialog goes away
        if (hud.ShopOpen() && player.Moving()) hud.CloseShop();
        if (hud.BankOpen() && player.Moving()) hud.CloseBank();
        if (game.Dialogue().open && player.Moving()) game.DialogueClose();
        hud.SetDialogue(game.Dialogue());
        hud.SetPlayerTile(player.Tile());
        if (hud.OpenTab() == Hud::kTabMap) hud.SetQuestMarkers(game.QuestMarkers());
        hud.SetAttackKind(game.Kind());
        hud.SetPoisoned(game.Poisoned());
        hud.SetCombatStyle(game.Style());
        effects.Update(dt);
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
        {   // the axe he chops with
            Tier at;
            const Item axe = game.AxeInUse();
            if (axe != Item::None && GearOf(axe, &at, nullptr)) pose.gear.axe = std::string("held_") + TierKey(at) + "_hatchet";
        }
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
        town.Stream(player.X(), player.Z(), args.shot ? 1000 : 2);   // bring in the ground around him (the nearest of it on the first frame), drop what is far away
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
        effects.Draw();
        animals.SetShadowSampling(lit, sunShadows);
        for (const Npc& n : npcs.All())
            if (n.Info().rig != Rig::Human && animals.Visible(n, player.X(), player.Z())) animals.Draw(n, pose.time);
        if (args.zoo > 0) {   // dev: a row of every animal (zoo 11-15 = goblins only)
            const int zmode = args.zoo >= 10 ? args.zoo - 10 : args.zoo, sp0 = args.zoo >= 10 ? 3 : 0;
            int k = 0;
            const int spFrom = args.zooFrom >= 0 ? args.zooFrom : sp0, spTo = args.zooTo >= 0 ? std::min(args.zooTo + 1, kSpeciesCount) : kSpeciesCount;
            for (int sp = spFrom; sp < spTo; ++sp)
                for (int cw = 0; cw < 2; ++cw, ++k) {
                    if (GetSpecies(static_cast<Species>(sp)).rig == Rig::Human) continue;
                    Npc z;
                    z.id = k + 1;
                    z.species = static_cast<Species>(sp);
                    z.colorway = cw;
                    z.x = player.X() + (cw * 2 - 1) * 1.3f;
                    z.z = player.Z() + 2.0f + (sp - spFrom) * 1.9f;
                    z.facing = 3.14159f + 0.5f * (k % 3 - 1);
                    const float zt = args.atime >= 0.0f ? args.atime : pose.time;
                    if (zmode == 2) { z.path.push_back({0, 0}); z.walkPhase = zt * 6.0f; }
                    if (zmode == 3) z.attackT = std::fmod(zt, 1.0f) * 0.6f;
                    if (zmode == 4) z.hurtT = std::fmod(zt, 1.0f) * 0.4f;
                    if (zmode == 5) { z.dead = true; z.deathT = std::fmod(zt, 3.0f); }
                    animals.Draw(z, pose.time);
                }
        }
        // People (shopkeepers, bankers, smiths, villagers) and the people-shaped monsters (zombies, skeletons, trolls, mummies and the
        // fighters of the Abyss): the same modular body as the hero, so they cost no extra models.
        auto bodyOf = [&](const PersonLook& pl) {
            Appearance pa;
            pa.female = pl.female;
            const std::vector<int> hairs = cat.HairFor(pa.female);
            pa.hair = hairs.empty() ? 0 : hairs[pl.hairPick % hairs.size()];
            pa.skin = pl.skin;
            pa.hairColor = pl.hairColor;
            pa.shirt = pl.shirt;
            pa.shirtColor = pl.shirtColor;
            pa.pants = pl.pants;
            pa.pantsColor = pl.pantsColor;
            pa.shoes = pl.shoes;
            pa.shoesColor = pl.shoesColor;
            Sanitize(pa, nHair, nShirt, nPants, nShoes);
            return pa;
        };
        for (const Person& pr : people.All()) {
            const float dx = pr.x - player.X(), dz = pr.z - player.Z();
            if (dx * dx + dz * dz > 45.0f * 45.0f) continue;
            const PersonInfo& info = *pr.info;
            CharacterPose cp;
            cp.x = pr.x;
            cp.z = pr.z;
            cp.facing = pr.facing;
            cp.time = pose.time + pr.id * 1.37f;
            cp.gear = GearLookOf(info.weapon, info.shield, info.helm, info.body, info.legs, Item::None);
            if (info.work == PersonWork::Smith) {
                cp.anim = Anim::Smith;
                cp.workPhase = std::fmod(cp.time / 0.75f, 1.0f);
                for (const StationDef& st : map.Stations()) {   // sparks fly from the anvil he is working at
                    const float ax = st.x - pr.x, az = st.z - pr.z;
                    if (st.kind == "anvil" && ax * ax + az * az < 2.4f * 2.4f && cp.workPhase > 0.001f)
                        props.DrawSparks({st.x, 0.82f, st.z}, cp.workPhase / 0.30f, cp.time, {255, 190, 80, 255});
                }
            }
            chars.Draw(bodyOf(info.look), cp);
            if (!sunShadows) DrawCylinder({pr.x, 0.02f, pr.z}, 0.46f, 0.46f, 0.01f, 16, Fade(BLACK, 0.28f));
        }
        // The other adventurers: the same body, wearing what they wear, doing what they do.
        for (const Adventurer& ad : advs.All()) {
            if (ad.pvp.dead) continue;
            const float dx = ad.x - player.X(), dz = ad.z - player.Z();
            if (dx * dx + dz * dz > 45.0f * 45.0f) continue;
            CharacterPose cp;
            cp.x = ad.x;
            cp.z = ad.z;
            cp.facing = ad.facing;
            cp.phase = ad.walkPhase;
            cp.moving = ad.Moving();
            cp.time = pose.time + ad.id * 1.37f;
            cp.gear = LookOf(ad.equip, ad.bag, ad.skills);
            {
                Tier at;
                const Item axe = BestHatchet(ad.bag, ad.skills.Level(Skill::Woodcutting));
                if (axe != Item::None && GearOf(axe, &at, nullptr)) cp.gear.axe = std::string("held_") + TierKey(at) + "_hatchet";
            }
            cp.weaponAnim = ad.equip.Anim();
            cp.workPhase = ad.workPhase;
            switch (ad.act) {
                case AdvAct::Chop: cp.anim = Anim::Chop; break;
                case AdvAct::Fish: cp.anim = Anim::Fish; break;
                case AdvAct::Mine: cp.anim = Anim::Mine; break;
                case AdvAct::Smelt: cp.anim = Anim::Smelt; break;
                case AdvAct::Smith: cp.anim = Anim::Smith; break;
                case AdvAct::Cook: cp.anim = Anim::Cook; break;
                case AdvAct::Light: cp.anim = Anim::Light; break;
                case AdvAct::Bury: cp.anim = Anim::Bury; break;
                case AdvAct::Fight: cp.anim = Anim::Attack; break;
                default: break;
            }
            cp.strikeAge = ad.strikeAge;
            cp.strikeLeft = ad.strikeLeft;
            if (ad.hurtAge < 0.75f) {
                cp.blockAge = ad.hurtAge;
                cp.blockLanded = true;
            }
            chars.Draw(bodyOf(ad.look), cp);
            if (!sunShadows) DrawCylinder({ad.x, 0.02f, ad.z}, 0.46f, 0.46f, 0.01f, 16, Fade(BLACK, 0.28f));
        }
        for (const Npc& n : npcs.All()) {
            const SpeciesInfo& si = n.Info();
            if (si.rig != Rig::Human || (n.dead && n.deathT > Npcs::kCorpseSeconds)) continue;
            const float dx = n.x - player.X(), dz = n.z - player.Z();
            if (dx * dx + dz * dz > 45.0f * 45.0f) continue;
            CharacterPose cp;
            cp.x = n.x;
            cp.z = n.z;
            cp.facing = n.facing;
            cp.time = pose.time + n.id * 1.37f;
            cp.moving = n.Moving();
            cp.phase = n.walkPhase;
            cp.gear = GearLookOf(si.weapon, si.shield, si.helm, si.body, si.legs, Item::None);
            if (IsBow(si.weapon)) cp.weaponAnim = WeaponAnim::Shoot;
            else if (IsStaff(si.weapon)) cp.weaponAnim = WeaponAnim::Cast;
            else {
                GearType g;
                if (GearOf(si.weapon, nullptr, &g)) cp.weaponAnim = AnimOf(g);
            }
            if (!n.dead && (n.attackT < 0.7f || (n.hunting && !n.Moving()))) cp.anim = Anim::Attack;
            cp.strikeAge = n.attackT;
            if (n.hurtT < 0.75f) {   // struck: he flinches (and raises his guard)
                cp.blockAge = n.hurtT;
                cp.blockLanded = true;
            }
            const float grow = n.dead ? 1.0f : 0.35f + 0.65f * std::min(1.0f, n.spawnT / 0.9f);
            cp.scale = si.scale * grow;
            cp.tintSkin = si.tintSkin;
            cp.skinTint = {si.skinTint.r, si.skinTint.g, si.skinTint.b, 255};
            cp.tintShirt = si.tintShirt;
            cp.shirtTint = {si.shirtTint.r, si.shirtTint.g, si.shirtTint.b, 255};
            cp.tintPants = si.tintPants;
            cp.pantsTint = {si.pantsTint.r, si.pantsTint.g, si.pantsTint.b, 255};
            cp.hideHair = si.bald;
            if (n.dead) {
                const float u = std::clamp(n.deathT / 0.9f, 0.0f, 1.0f);
                cp.fall = u * u * (3.0f - 2.0f * u);
            }
            chars.Draw(bodyOf(si.look), cp);
            if (!sunShadows && (!n.dead || n.deathT < 1.5f)) DrawCylinder({n.x, 0.02f, n.z}, 0.46f * cp.scale, 0.46f * cp.scale, 0.01f, 16, Fade(BLACK, 0.28f));
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
        const bool heroInDanger = IsDangerousArea(map.AreaAt(pt.x, pt.z));
        for (const Adventurer& ad : advs.All()) {
            if (ad.pvp.dead) continue;
            const float dx = ad.x - player.X(), dz = ad.z - player.Z();
            if (dx * dx + dz * dz > 22.0f * 22.0f) continue;
            const bool real = ad.kind == AdvKind::Real;
            Color nameColor = real ? Color{255, 255, 255, 255} : Color{190, 215, 255, 255};
            std::string nameText = ad.name;
            if (heroInDanger) {   // in the Abyss every player is somebody to fight: their level, coloured against his like a creature's
                const Rgb lc = LevelColor(skills.CombatLevel(), ad.Combat());
                nameColor = {lc.r, lc.g, lc.b, 255};
                nameText += " (level-" + std::to_string(ad.Combat()) + ")";
            }
            hud.QueueLabel({ad.x, 2.12f, ad.z}, nameText, nameColor, 12.0f);
            if (ad.pvp.skull > 0.0f) hud.QueueSkull({ad.x, 2.5f, ad.z});
            if (!ad.speech.text.empty()) hud.QueueLabel({ad.x, 2.5f, ad.z}, ad.speech.text, {255, 236, 90, 255}, 13.0f);
            if (((ad.kind == AdvKind::Real && ad.act == AdvAct::Fight) || ad.hurtAge < 6.0f || ad.pvp.fightingHero) && ad.skills.Hp() < ad.skills.MaxHp())
                hud.QueueBar({ad.x, 2.0f, ad.z}, (float)ad.skills.Hp() / (float)std::max(1, ad.skills.MaxHp()), false);
        }
        if (game.Skulled()) hud.QueueSkull({player.X(), 2.35f, player.Z()});
        hud.SetSkullTimer(game.SkullLeft());
        hud.SetAbyssLevel(game.AbyssLevel());
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

    saveProgress();   // (and when he quits)
    animals.Unload();
    props.Unload();
    chars.Unload();
    for (int i = 0; i < 2; ++i)
        if (loaded[i]) towns[i].Unload();
    lit.Unload();
    CloseWindow();
    return 0;
}
