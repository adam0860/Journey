# Journey

An offline, single-player, RuneScape-Classic-style RPG for Windows and Android.
C++17 + [raylib](https://www.raylib.com), art built procedurally in Blender.
Design doc: *Journey - Game Design Document* (kept in Claude Docs).

**Status: prototype.** You can walk around Castle Town (castle, throne hall, bank, inn, market square,
houses, river) by clicking, and chop trees for logs (Woodcutting XP, bag, skills tab; stumps regrow after 3 minutes).
No combat or NPCs yet - see the milestone list in the GDD.

## Run it

Windows: unzip `journey-windows.zip` and run `journey.exe` (keep the `assets` folder next to it).

| | |
|---|---|
| Click / tap ground | walk there (A* pathfinding; click a wall and you walk next to it) |
| Drag | rotate camera |
| Mouse wheel / pinch | zoom |
| Arrow keys or Q / E | rotate; Up/Down or +/- zoom |
| Tap a tree | walk up and chop it (logs go in the Bag tab, XP in the Skills tab) |
| Bag / Skills tabs | tap to open or close; tap a bag item to drop it |
| F3 | debug: collision grid + path |

Walk into the bank, inn or throne hall and their roofs disappear, like the original.

## Build from source

Needs CMake 3.20+, a C++17 compiler, and internet the first time (CMake downloads raylib 5.5).

```
cmake -S . -B build -G Ninja          # or omit -G for Visual Studio / Make
cmake --build build
./build/journey                        # Windows: build\Release\journey.exe
ctest --test-dir build                 # unit tests: pathfinding + the real town map
```
Linux also needs the X11/GL dev packages for GLFW (`libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).
Windows `.exe` from Linux: `cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake`.

## Regenerating the town (Blender)

The town models AND the collision map come from one script, so what you see is what blocks you:

```
pip install bpy
python tools/build_town.py            # writes assets/models/town/*.glb and assets/maps/town.map
python tools/build_town.py --blend    # also saves source/town.blend to open in Blender
python tools/render_preview.py out/ overview plaza throne bank inn castle    # Cycles stills
```
Edit the layout in `build()` in `tools/build_town.py` (buildings are one-liners such as
`T.house(17, 31, 24, 38, 'S', roof='roof_red')`). Tile units: x east, z south, 1 tile ~ 1 m.

## Code layout

```
src/world/      TileMap, Pathfinder           pure C++, no raylib, unit tested
src/game/       Player                        tile pathing + smooth movement
src/render/     LitShader, TownRenderer, CameraRig, HeroRenderer
src/platform/   PointerInput                  mouse + touch behind one interface
src/ui/         Hud                           chat box, tab bar, area banner
tools/          Blender scripts
assets/         models/town/*.glb, maps/town.map
```

## Dev flags (for screenshots / tests)

`journey --shot out.png --frames 60 --at 32 30 --cam 20 46 22 --click 32 10 --debug --size 1280 720`
(`--shot` saves next to the executable, then exits.)

## Android

Not built yet - see `docs/ANDROID.md`.
