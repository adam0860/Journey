# Journey

An offline, single-player, RuneScape-Classic-style RPG for Windows and Android.
C++17 + [raylib](https://www.raylib.com), art built procedurally in Blender.
Design doc: *Journey - Game Design Document* (kept in Claude Docs).

**Status: playable prototype.** A 192 x 400 world: Castle Town (castle, throne hall, bank, inn, market, smithy, quarry, crypt), Millbrook village
(shops, bank, crafters' yard, chapel, mine, farm) and a long road south through Lakeside (lake, ferry), the Wolf Wood, the Troll Mountains,
the Mining Caves, the Desert Ruins (and a dragon) down to the dangerous Abyss, with wild country on every side: pine forests, the woodcutters' village of
Elmswick, swamp and barrow downs in the west; farmland, the harbour of Saltmere on the Eastern Sea, the standing stones, the Iron Range and the dune
desert in the east; and the Northern Reach under the Frostpeak Range, with roads, bridges and a ruined watchtower. Click or tap to walk, chop, fish, cook (fires and ranges),
mine, smelt, smith (five metals, up to adamant, 18 kinds of weapon and armour), craft (leather, gems, jewellery, pottery), carve and string your own
bows, bake pies, stews, cakes and pizzas, fight 34 kinds of creature in melee, with a bow or with 16 spells, talk to people, do 58 quests (some
ask you to dig, light candles or turn a key), bank your items, take the ferry or read a teleport scroll (to towns you have visited), and follow gold quest markers on the map. The dangerous Abyss holds the best rocks and trees in the game and the players who hunt there: the deeper you go the higher the Abyss level, and the wider the gap of combat levels at which players (the other adventurers among them) can attack each other; attack another player and a skull marks you for ten minutes; die skulled and you lose everything you carry. Over 320 items,
including boots, gloves and the skill capes that heroes of level 99 can buy from the king; every weapon swings at its own speed.
12 skills, bag, skills, gear (with rings and amulets), quests, a world map, combat and spellbook tabs, a how-to-play page, and procedural
music and sound effects (volume in the Menu tab). Everything is offline and single-player, but the world is full of other "players": eight adventurers who start the game with you and train, quest and fight in real time, and dozens of others of every level at their trades.

## Run it

Windows: unzip `journey-windows.zip` and run `journey.exe` (keep the `assets` folder next to it).

| | |
|---|---|
| Click / tap ground | walk there (A* pathfinding; click a wall and you walk next to it) |
| Drag | rotate camera (a drag that starts on the quest log or the how-to-play page scrolls it) |
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
