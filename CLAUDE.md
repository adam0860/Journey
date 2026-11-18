# Journey - project notes for Claude Code

Adam's offline, single-player, RuneScape-Classic-style 3D RPG. Personal use only. C++17 + raylib 5.5, targets
Windows and Android. Adam is an aspiring game programmer practising C++; he wants Claude to do most of the building
but is learning, so explain non-obvious choices briefly.

## Where we are
* Castle Town: click/tap-to-walk (A* on a tile map), orbit camera, HUD (chat box, tab bar, area banner).
* Two art styles: A = classic low-poly, B = "enhanced" (textures, bevels, baked ground shadows, animated water).
  Adam chose **B**. Real-time shadow map exists but is OFF by default (it caused an invisible-hero/flicker bug on his GPU);
  F5 toggles it. Ground shadows are baked into the models in Blender.
* **Character system (new):** modular rigid-part character (body, hair, shirt, pants, shoes as separate GLBs in pivot
  space, tinted at runtime, walk-cycle by swinging parts). Character creator screen: name, male/female, 8 skin tones,
  15 hair styles (men-only / women-only / unisex via `for=` in the catalog), 5 shirts, 5 pants, 5 shoes, each with colours,
  Randomize, Preview walk, drag to spin. Saved to `journey_save.cfg`. F6 or the HUD "Menu" tab reopens it.
  The name is NOT shown above the character (Adam asked for that to be removed).
* **Gameplay v3 (skills + combat + loot):** Bag tab (28 slots), Skills tab (14 skills incl. Prayer; all start at level 1 except
  Hitpoints = 10; cap 99; RuneScape XP curve) and a Combat tab (combat level, HP, attack style). Item ids are saved: only append to `enum Item`.
  * **Bag:** tap an item to select it, then its buttons (`ActionsFor`): Eat / Bury / Cook / Light first, then Use, Drop, Examine.
    **Use** = pick what to use it with (tinderbox + logs, raw food + fire). Drop puts the item on the ground under you.
    Tools (bronze axe, small net, tinderbox) are given on the first run (`GiveStarterKit`), can't be dropped and are required.
  * **Context menus:** long-press (touch) or right-click opens the full menu for everything under the finger (`Gameplay::BuildMenu`):
    "Attack Chicken (level-1)" (name yellow, level coloured by `LevelColor`: green well below you, yellow level, red well above),
    "Chop down Tree", "Net Fishing spot", "Cook Fire", "Take Bones"/"Take all", "Examine ...", "Walk here", "Cancel".
    A plain tap does the FIRST entry of the same menu. Bag items have their own long-press menu (`BuildItemMenu`). Desktop also shows the
    first entry top-left on hover. All of it is data (`game/Menu.h`); the HUD only draws it.
  * **Combat** (`game/Combat.*`, `Gameplay`): TURN BASED. Every 1.2 s the turn passes: hero strikes, then every creature next to him strikes,
    so hits alternate. A strike misses (blue 0) or lands for 1..max (red number). Hit chance = OSRS formula (attack roll vs defence roll),
    max hit from Strength (`0.5 + (str+8)*(64)/640`), creatures are clumsy (+4 to aim). Styles: Accurate/Aggressive/Defensive/Controlled
    (+3 / +1 effective level). XP: 4 per damage to the trained skill + 1.33 to Hitpoints. Hits land 0.3 s after the strike starts
    (`kImpactSeconds`) so the animation and the hit splat line up. Auto-retaliate when something hits you while idle. HP regen 1 per 30 s.
    Dying ("Oh dear, you are dead!") puts you at the spawn with full HP (no item loss in the safe zone). Eating heals (`ItemInfo::heals`).
  * **Animals** (`game/Npc.*`): chickens (lvl 1, 3 HP), calves (lvl 2, 4 HP), cows (lvl 3, 6 HP). They live in a pen (`@pen`/`@npc` lines in
    town.map), wander, never leave it, are not aggressive but chase and fight back once attacked, and respawn (25-45 s) after the loot drops.
    Loot (data in `Npc.cpp`): chicken = bones, raw chicken, 30% egg; cow = bones, cowhide, 50% raw beef; calf = bones, 40% beef, 25% hide.
  * **Ground items** (`game/GroundItems.*`): loot/dropped items form a pile per tile (4 min life). Tap = walk onto it and take everything
    that fits; the menu also offers "Take <item>" one by one. Piles are drawn with little models `assets/models/props/drop_<ItemInfo::model>.glb`.
  * **Cooking/Prayer:** raw chicken/beef cook like fish (`Cooking.cpp` table; burnt = Burnt meat). **Bury** bones: 1.8 s, 5 Prayer XP.
  * Woodcutting, fishing, firemaking: as before (tap tree/spot; Light on logs; tap a fire). The river has 7 fishing spots (`@spot`).
  * All rules are in `src/game/Gameplay.cpp` (walk to target -> timer -> result; pure C++ with a `GameEvents` callback interface),
    unit tested in `tests/test_core.cpp` with a mini world (`MiniWorld`). main.cpp only gathers what is under the finger and draws.
  * Progress saves to `journey_progress.cfg` (skills, HP, combat style, worn gear, bag). Trees, fires, piles and animals are not saved.
  * Rendering: `AnimalRenderer` animates rigid parts (walk, peck/graze, strike, flinch, fall over, grow-in); hero animations in
    `CharacterRenderer` (`Anim`: Chop, Fish, Light, Cook, Attack = fists up + jabs, Bury); `PropRenderer` draws fires, fishing spots and
    ground piles; `Hud` draws hit splats (red = damage, blue = 0), health bars, HP pill, the menu. NOTE: raylib culling/blend state needs
    `rlDrawRenderBatchActive()` before AND after changing it, or queued triangles are drawn with the wrong state.
* **Mining, smithing and gear (Gameplay v4):**
  * **Items** (`Inventory.h`, append-only ids): ores (copper, tin, iron, coal, mithril), bars (bronze, iron, steel, mithril), hammer, 4 pickaxes,
    and 32 smithed pieces = 4 metals x 8 types in the order `Gear.h` documents (dagger, sword, battleaxe, mace, helm, kiteshield, platelegs,
    platebody). `GearItem(tier,type)` / `GearOf(item,&tier,&type)` convert; `GetItemInfo` builds the gear names/models at first use.
    Starter kit is now axe, net, tinderbox, bronze pickaxe, hammer (undroppable). Smithed pickaxes can be dropped.
  * **Gear stats** (`game/Gear.*`): each metal adds attack/strength/defence bonuses (bronze 6/5/4 ... mithril 36/32/28), each type takes a share:
    dagger = accurate but weak, battleaxe = clumsy but heavy, body armour protects most. Level to use a piece: Attack (weapons) / Defence (armour)
    / Mining (pickaxes) = 1 / 5 / 20 / 30 by metal. Weapon types pick the swing: Stab, Slash, Chop, Crush (`WeaponAnim`).
  * **Equipment** (`game/Equipment.*`): 5 slots (head, body, legs, weapon, shield). `EquipFromBag` swaps with the bag, `Total()` feeds
    `PlayerFighter(skills, style, atkBonus, strBonus, defBonus)` (so gear changes hit chance, max hit and how often you get hit). Saved as `worn.N=id`.
  * **Mining** (`game/Mining.*`): `@rock x z kind yaw` lines in town.map. A swing takes 2.4 s / pickaxe speed (bronze 1.0 ... mithril 2.1); each rock
    gives ONE ore then becomes `rock_empty` and respawns (copper/tin 6 s, iron 10, coal 30, mithril 90). Chance per swing = base + 0.012 per level above the requirement.
    XP: copper/tin 18, iron 35, coal 50, mithril 80. The best pickaxe the level allows is used.
  * **Smithing** (`game/Smithing.*`): `@station furnace|anvil x z yaw` lines. Smelting 2.4 s per bar (iron ore fails 50% and is lost), smithing 3 s per piece.
    Bars per piece: dagger/mace/helm 1, sword 2, pickaxe 2, battleaxe/shield/legs 3, body 5; level = metal's first level (1/15/30/50) + an offset per type.
    Tapping a furnace/anvil walks there, then `GameEvents::OpenMake(MakeMenu)` shows the dialog; its buttons call `StartSmelt` / `StartSmith`.
    "Use" ore on the furnace or a bar/hammer on an anvil does the same. All rules and numbers are unit tested.
  * **Animations** (`CharacterRenderer`): weapon swings (stab = lunge, slash = diagonal cut with chest twist, chop = overhead two-hander, crush = heavy
    smash and hold) share one timeline whose contact is at 0.30 s (`kImpactSeconds`); `Anim::Mine/Smelt/Smith`; the shield is counter-rotated against the
    arm swing so it stays upright; being struck at (`blockAge`, `blockLanded`) raises the shield (or guard) and knocks him back if the hit lands.
    Worn armour = extra rigid parts on the same joints (`gear_<metal>_body_torso_m` ...), tinted nothing (the metal's colours are in the models).
    A helm hides the hair, body armour hides the shirt, platelegs hide the trousers.
  * **World:** smithy (stone yard, furnace, 2 anvils, trough, weapon rack) and quarry (9 rocks, mine cart) in the south-east corner (`build_town.py`).
    `PropRenderer` draws rocks (shake when struck), the furnace (flames, chimney smoke, brighter while smelting), anvils and sparks.
  * **HUD:** tab bar is now Bag, Skills, Gear, Quests, Map, Combat, Menu. Gear tab = paper doll + bonus totals; the make panel is a modal dialog.
* **Coins, shops, goblins, Millbrook (Gameplay v5):**
  * **Coins** (`Item::Coins`, the first stackable item) and `Bread`; every item has a value (`ItemValue`, gear = its bars x 1.15). Starter tools are not sellable.
  * **People** (`game/People.*`): `@person key x z yaw` lines in town.map; the table in `People.cpp` gives each one a name, look (indices into the character
    creator's choices), what they wear, their shop key, their `reach` (how far across a counter you can talk to them) and 3 sayings. They are drawn with the
    same modular `CharacterRenderer` as the hero (so no new models), the smiths hammer (`PersonWork::Smith`) with sparks. They stand on blocked tiles.
    Tap = `TapPerson` (walk up to `reach`, they turn to face you, chat line, open the shop); long-press menu has Trade / Talk-to / Examine.
  * **Shops** (`game/Shops.*`): 7 shops (2 in the town, 5 in Millbrook). Sell price = value x markup, they pay value x sellRate, only for the categories they deal in
    (`kCat*` mask). Stock restocks 1 per 12 s, things you sell go on the shelf and fade away (40 s each) unless they are normal stock. Rules are in
    `Gameplay::Buy/Sell/BuildShopMenu`; the HUD (`Hud::OpenShop`) is a modal dialog (tabs, 7 rows a page, quantity 1/5/all) and closes when you walk away.
  * **Goblins** (`Npc.cpp`): `goblin` (lvl 2, 5 HP) and `goblin_warrior` (lvl 5, 10 HP, horned cap, shield, sword). AGGRESSIVE (`SpeciesInfo::aggressive`): they start
    a fight with a hero within 3 tiles unless his combat level is >= 2 x level + 5 (`Npcs::SetHero`). They live in an open camp (`@pen goblincamp`), never leave it.
    Loot: bones + coins every time (3-14 / 10-35) + a small chance of a bronze/iron dagger or helm. Model files `animals/goblin_<look>_<part>.glb`
    (`tools/build_animals.py`), animated by `AnimalRenderer::AnimateGoblin` (walk, hunting stance, club swing timed to the hit, flinch, fall over backwards).
  * **The map is now 64x176:** the castle town is rows 0-63 (its random layout is unchanged: loops use `HT`), a dirt road wanders south to the goblin camp
    (x 40-57, z 73-89) and on to Millbrook (z 96-175): gate arch, cobbled main street, square with fountain + 5 shop stalls, cottages, an inn, a windmill,
    a chapel, a second smithy (furnace + 2 anvils + Wilf), Millbrook mine (16 rocks incl. 4 coal and 2 mithril), a farm, more trees and river fishing spots.
    `build_town.py` sections: "V1" ground floors near the top of `build()`, "THE PEOPLE OF JOURNEY..." structures before the tree code, village trees/fishing after it.
    The renderer loads `chunk_X_Z` for every 16x16 chunk of the map (`TownRenderer::Load`), so a taller map just works. Style B town models are ~70 MB.
* **Animals / pens / items are built in Blender:** `tools/build_animals.py` (chicken/cow/calf, 2 colourways each, `assets/models/animals/`
  + catalog.txt of joints), `tools/build_items.py` (held tools, campfire, ground items), shared helpers in `tools/lowpoly.py`.
  `tools/build_gear.py` (weapons, shield, pickaxe, worn armour and ground models for the four metals), `tools/build_mining.py` (rocks, furnace, anvil,
  ores, bars, hammer). The chicken coop (NW) and cow pen (E) are in `build_town.py`; pens, fences, gates and the animals' start positions go into town.map.
* Choppable trees are NOT baked into the town models: `build_town.py` exports `tree_round/pine/tall.glb` + `stump.glb` and writes
  `@tree x z kind scale yaw` lines into town.map; `TownRenderer::DrawTrees` draws them from `Forest` state. The far forest ring is still baked.
  (Tree ground shadows are a runtime blob disc, since the baked shadows no longer include them.)
* On-screen keyboard in the creator for phones (tap the name box). Android defaults to style B.

## Layout
* `src/main.cpp` game loop + dev flags (see header comment: `--shot out.png --frames N`, `--creator`, `--look ...`, `--uiclick X Y`,
  and for gameplay screenshots `--fish I`, `--tree I`, `--light`, `--cook`, `--fire X Z`, `--anim chop|fish|light|cook|punch|bury --atime T --face DEG`,
  `--tab 0 --sel N`, `--attack N`, `--menunpc N`, `--menu X Y`, `--hover X Y`, `--pile X Z ITEMID`, `--zoo 1-5`, `--hp N`,
  `--coins N`, `--shop I`, `--talk I`, `--zoo 11-15` (goblins only), `--wear ID...`, `--anim mine|smelt|smith|punch --atime T`, `--block T [--blockhit]`, `--make 0|1 [TIER]`, `--rock I`, `--station I`, `--dosmelt T`, `--dosmith T TYPE`)
* `src/render/` LitShader (GLSL 330 / GLSL ES 100 auto), TextureCache, TownRenderer, CharacterRenderer, CameraRig
* `src/ui/` Hud, CharacterCreator.  `src/game/` Player, Appearance (palettes + save format).  `src/world/` TileMap, Pathfinder.
* `tools/` Blender (bpy) generators: `build_town.py` (`--style a|b`), `build_character.py` (60+ part GLBs +
  `assets/models/character/catalog.txt`), `textures_proc.py`, preview helpers. Blender coords = (x, -z, height).
* `assets/` generated models/maps (committed so the game runs without Blender). `third_party/raylib` = bundled raylib 5.5.
* `android/` Android Studio project (Gradle + the same CMakeLists via NativeActivity). Untested on a real phone.
* `tests/test_core.cpp` unit tests for map, pathfinder, skills, bag, fishing, fires, cooking, combat maths, animals, loot, menus, gear, equipment, mining, smelting, smithing and whole gameplay flows (no raylib).

## Build
* Windows (MSVC or MinGW) / Linux: `cmake -B build -G Ninja && cmake --build build`, run `build/journey` (assets are copied next to it).
* Android: open `android/` in Android Studio, Run. Needs SDK Platform 34, NDK (side by side), CMake. See `android/README.md`.
* Regenerate models (needs Blender 4+/`bpy`): `python tools/build_character.py`, `python tools/build_town.py --style b`; held tools + campfire: `blender -b --python tools/build_items.py`; animals: `blender -b --python tools/build_animals.py`; gear: `blender -b --python tools/build_gear.py`; mining models: `blender -b --python tools/build_mining.py`.
  Without bpy installed, use Blender itself: `blender -b --python tools/build_town.py -- --style b` (run for both `--style a` and `b`;
  both rewrite town.map). Blender's Python has no Pillow - build_town.py falls back to bpy for texture PNGs.
* Windows build on Adam's PC: `cmake -S . -B build` (Visual Studio generator) then `cmake --build build --config Release`; tests:
  `build\Release	est_core.exe assets\maps	own.map`. Android on his PC: Gradle 9.3.1 + AGP 9.1.0; JDK doesn't trust his network's
  certificates, so command-line Gradle needs `JAVA_TOOL_OPTIONS=-Djavax.net.ssl.trustStoreType=Windows-ROOT` (Android Studio is fine).
* Android: raylib is patched to request a 24-bit depth buffer and near/far clip is 1.0/400 (16-bit depth + 0.01 near caused z-fighting on his phone).

## Gotchas learned the hard way
* raylib: each primitive must have < 65k vertices (16-bit indices). `FileExists()` can't see Android APK assets - don't use it for assets.
  On Android `LoadFileText/SaveFileText` map to the app's private storage (that's where the save lives).
* `TakeScreenshot` writes into the app's directory, using only the file name.
* Headless testing on Linux: `Xvfb :99` + `LIBGL_ALWAYS_SOFTWARE=1`; a desktop OpenGL ES 2 build (`-DGRAPHICS=GRAPHICS_API_OPENGL_ES2`)
  checks the phone shader path.
* Character parts are separate meshes, so keep clothing slightly larger than the body (shirt over pants hem, tucked thighs);
  the pelvis/thigh/shirt-hem proportions were tuned carefully - re-render close-ups after changing `TORSO`/`LEG` tables.

## Honest status
* Windows and Android both build on Adam's PC and the game runs on his phone (z-fighting fixed). Tree-chopping is verified on desktop
  with screenshots; the phone build of it has not been tried yet.
* Known polish: puffy shoulders / mannequin proportions, weak male-vs-female body difference, Side Part and Afro hair are the roughest.

## Likely next steps (Adam's order)
1. (done) Android build runs on his phone.
2. Gameplay: (done) woodcutting, fishing, firemaking, cooking, melee combat, loot, bury bones, mining, smelting, smithing, worn gear, coins, shops, goblins, Millbrook. Next: tanning hides (Crafting) and a range for cooking, a bank, quests and talking NPCs with real dialogue, bigger monsters (a red-level one!) and a dangerous zone, more goblin gear, a quest to clear the camp, "The Abyss" danger zone, top-3-items-kept-on-death rule.
3. Procedural music, more areas, body/hair polish.
