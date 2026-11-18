# Journey

An offline, single-player, RuneScape-Classic-style RPG for Windows and Android.
C++17 + [raylib](https://www.raylib.com), art built procedurally in Blender.
Design doc: *Journey - Game Design Document* (kept in Claude Docs).

**Status: prototype.** You can walk around Castle Town (castle, throne hall, bank, inn, market square,
houses, river) by clicking, chop trees for logs, net fish at the river, light fires and cook, mine ore in the quarry, smelt it
in the furnace and smith weapons and armour at the anvils, wear them, and fight the chickens, cows and calves in their pens
(turn-based combat with hit splats, loot piles, bury bones, weapon swings and shield blocks), fight goblins for coins, spend them in
the shops, and walk the long road south to the village of Millbrook (five more shops, a second smithy and mine, a farm, a windmill) -
14 skills, bag, skills, gear and combat tabs.
No quests or human NPCs yet - see the milestone list in the GDD.

## Run it

Windows: unzip `journey-windows.zip` and run `journey.exe` (keep the `assets` folder next to it).

| | |
|---|---|
| Click / tap ground | walk there (A* pathfinding; click a wall and you walk next to it) |
| Drag | rotate camera |
| Mouse wheel / pinch | zoom |
| Arrow keys or Q / E | rotate; Up/Down or +/- zoom |
| Tap a tree | walk up and chop it with your axe (logs go in the Bag tab, XP in the Skills tab) |
| Tap a swirl in the river | stand on the bank / pier and net shrimps (anchovies from Fishing 15) until the bag is full |
| Bag tab | tap an item to select it, then its buttons: **Eat**, **Bury** (bones), **Cook**, **Light** (logs + tinderbox), **Wield** / **Wear**, **Use** (with another item, a fire, the furnace or an anvil), **Drop**, **Examine** |
| Tap a fire | cook all the raw food in your bag on it (low levels burn some) |
| Tap a rock in the quarry | mine it with your pickaxe: copper and tin (Mining 1), iron (15), coal (30), mithril (55). Each rock gives one ore, then grows back |
| Tap the furnace | choose a bar: bronze (copper + tin), iron (50% fail), steel (iron + 2 coal), mithril (+ 4 coal) |
| Tap an anvil | needs the hammer and bars: pick a metal tab, then a dagger, sword, battleaxe, mace, helm, kiteshield, platelegs, platebody or pickaxe |
| Tap a shopkeeper (or hold for **Trade**) | walk up and open the shop window: **Buy** and **Sell** tabs, quantity 1 / 5 / all. Shops pay a share of an item's value and put what you sell on their shelves |
| Goblins (the camp beside the road south) | they attack anyone who comes within 3 tiles (until you are much stronger than them) and drop coins and bones, now and then a bronze or iron weapon |
| Coins | shown top-left next to your hit points; they stack in one bag slot |
| Gear tab | what you wear: tap a piece to take it off. Bag items have **Wield** / **Wear** (needs Attack / Defence 1, 5, 20, 30 for bronze, iron, steel, mithril) |
| Tap an animal | attack it: you and it take turns; red numbers are damage, blue 0 is a miss; it drops a pile - tap the pile to take it |
| Hold a finger (or right-click) | the full menu for what is there: Attack Chicken (level-1), Examine, Take, Walk here... |
| Combat tab | pick Accurate / Aggressive / Defensive / Controlled (which skill you train) |
| Skills tab | tap to open or close |
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
src/render/     LitShader, TownRenderer, CameraRig, CharacterRenderer, AnimalRenderer, PropRenderer
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
