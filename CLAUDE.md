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
* **Gameplay v1 (woodcutting):** Bag tab (28 slots, tap an item to drop it) and Skills tab (13 skills, all start at level 1, cap 99,
  RuneScape XP curve). Tap a tree: hero walks next to it and chops (0.6 s ticks, chance rises with level, one log per tree, 25 xp).
  The tree becomes a stump and regrows after 3 minutes (`Forest::kRegrowSeconds`). Progress saves to `journey_progress.cfg`.
  Logic is pure C++ in `src/game/` (Skills, Inventory, Forest, Woodcutting, Progress) with unit tests in `tests/test_core.cpp`.
* Choppable trees are NOT baked into the town models: `build_town.py` exports `tree_round/pine/tall.glb` + `stump.glb` and writes
  `@tree x z kind scale yaw` lines into town.map; `TownRenderer::DrawTrees` draws them from `Forest` state. The far forest ring is still baked.
  (Tree ground shadows are a runtime blob disc, since the baked shadows no longer include them.)
* On-screen keyboard in the creator for phones (tap the name box). Android defaults to style B.

## Layout
* `src/main.cpp` game loop + dev flags (see header comment: `--shot out.png --frames N`, `--creator`, `--look ...`, `--uiclick X Y` ...)
* `src/render/` LitShader (GLSL 330 / GLSL ES 100 auto), TextureCache, TownRenderer, CharacterRenderer, CameraRig
* `src/ui/` Hud, CharacterCreator.  `src/game/` Player, Appearance (palettes + save format).  `src/world/` TileMap, Pathfinder.
* `tools/` Blender (bpy) generators: `build_town.py` (`--style a|b`), `build_character.py` (60+ part GLBs +
  `assets/models/character/catalog.txt`), `textures_proc.py`, preview helpers. Blender coords = (x, -z, height).
* `assets/` generated models/maps (committed so the game runs without Blender). `third_party/raylib` = bundled raylib 5.5.
* `android/` Android Studio project (Gradle + the same CMakeLists via NativeActivity). Untested on a real phone.
* `tests/test_core.cpp` unit tests for map + pathfinder (no raylib).

## Build
* Windows (MSVC or MinGW) / Linux: `cmake -B build -G Ninja && cmake --build build`, run `build/journey` (assets are copied next to it).
* Android: open `android/` in Android Studio, Run. Needs SDK Platform 34, NDK (side by side), CMake. See `android/README.md`.
* Regenerate models (needs Blender 4+/`bpy`): `python tools/build_character.py`, `python tools/build_town.py --style b`.
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
2. Gameplay: more skills on the same pattern (mining, fishing...), NPCs, monsters, "The Abyss" danger zone, top-3-items-kept-on-death rule.
3. Procedural music, more areas, body/hair polish.
