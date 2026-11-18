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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "game/Appearance.h"
#include "game/Player.h"
#include "platform/PointerInput.h"
#include "raylib.h"
#include "render/CameraRig.h"
#include "render/CharacterRenderer.h"
#include "render/LitShader.h"
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
        else if (is("--debug")) a.debug = true;
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
    CameraRig camera;
    camera.SnapTo(player.X(), player.Z());
    if (args.hasCam) camera.SetView(args.yaw, args.pitch, args.dist);
    PointerInput pointer;
    Hud hud;
    ClickMarker marker;
    bool debug = args.debug;
    std::string lastArea;

    hud.Say("Welcome to Journey.", {255, 255, 255, 255});
    hud.Say("Click or tap the ground to walk. Drag to turn the camera.", {190, 210, 235, 255});

    auto handleTap = [&](float gx, float gz) {
        const TileCoord target{(int)std::floor(gx), (int)std::floor(gz)};
        if (!map.InBounds(target.x, target.z)) return;
        const PathResult path = FindPathToward(map, player.Tile(), target);
        if (!path.found) {
            hud.Say("I can't reach that!", {255, 120, 110, 255});
            return;
        }
        player.SetPath(path.tiles);
        marker = {gx, gz, 0.0f, !map.Walkable(target)};
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
        if (IsKeyPressed(KEY_F6)) { player.Stop(); openCreator(); continue; }

        if (in.tap) {
            float gx = 0, gz = 0;
            const bool onUi = hud.Hits(in.tapPos);
            if (hud.TabAt(in.tapPos) == 5) { player.Stop(); openCreator(); continue; }   // "Menu" tab = change appearance (for now)
            const bool hit = !onUi && camera.ScreenToGround(in.tapPos, &gx, &gz);
            TraceLog(LOG_DEBUG, "TAP at %.0f,%.0f ui=%d ground=%d -> %.2f,%.2f", in.tapPos.x, in.tapPos.y, onUi, hit, gx, gz);
            if (hit) handleTap(gx, gz);
        }
        if (args.hasClick && frame == 5) handleTap(args.clickX + 0.5f, args.clickZ + 0.5f);

        // ---- update ----
        player.Update(dt);
        camera.Follow(player.X(), player.Z(), dt);
        marker.age += dt;
        const TileCoord pt = player.Tile();
        const std::string& area = map.AreaAt(pt.x, pt.z);
        if (area != lastArea) {
            lastArea = area;
            if (!area.empty()) hud.ShowArea(area);
        }
        hud.Update(dt);

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
        pose.time = args.shot ? frame * dt : (float)GetTime();
        lit.SetFrame(cam.position, args.shot ? frame * dt : (float)GetTime(), hi);
        BeginDrawing();
        if (sunShadows) {   // pass 1: depth from the sun's point of view
            town.SetShadowSampling(lit, false);
            chars.SetShadowSampling(lit, false);
            lit.BeginShadowPass(player.X(), player.Z());
            town.Draw(player.X(), player.Z(), pt, true);
            chars.Draw(look, pose);
            lit.EndShadowPass();
        }
        ClearBackground(LitShader::SkyColor());
        town.SetShadowSampling(lit, sunShadows);
        chars.SetShadowSampling(lit, sunShadows);
        BeginMode3D(cam);
        lit.BindShadow(sunShadows);
        town.Draw(player.X(), player.Z(), pt);
        chars.Draw(look, pose);
        if (!sunShadows) DrawCylinder({player.X(), 0.02f, player.Z()}, 0.46f, 0.46f, 0.01f, 16, Fade(BLACK, 0.28f));
        DrawMarker(marker);
        if (debug) DrawDebugGrid(map, player);
        EndMode3D();
        hud.Draw(debug, TextFormat("Journey prototype  |  %d FPS  |  tile %d,%d  |  style %s  |  F3 debug  F4 style  F5 shadows  F6 character", GetFPS(),
                                    pt.x, pt.z, kStyleName[style]));
        EndDrawing();

        if (args.shot && frame >= args.frames) {
            TraceLog(LOG_INFO, "SHOT hero tile %d,%d moving=%d", pt.x, pt.z, player.Moving());
            TakeScreenshot(args.shot);
            break;
        }
    }

    chars.Unload();
    for (int i = 0; i < 2; ++i)
        if (loaded[i]) towns[i].Unload();
    lit.Unload();
    CloseWindow();
    return 0;
}
