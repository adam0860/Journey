# Android plan

Status: **not built or tested yet.** The code is written to be portable, but nothing has run on a phone.

Already portable:
- Input: `PointerInput` uses raylib's mouse API, which raylib maps from touch (tap = click, one-finger drag =
  rotate camera) and handles two-finger pinch/twist through `GetTouchPosition`.
- Shader: `LitShader.cpp` switches to GLSL ES 1.00 under `__ANDROID__` (raylib uses GLES2 on Android).
- Assets: loaded with `LoadFileText` / `LoadModel`, which read from the APK's `assets/` folder on Android.
- No desktop-only calls outside `#if !defined(__ANDROID__)` (the `ChangeDirectory` at start-up).

Still to do:
1. Install Android Studio (SDK + NDK r26+ + CMake) and JDK 17.
2. Create a NativeActivity Gradle project whose CMake builds raylib with `-DPLATFORM=Android` and our sources,
   and copies `assets/` into the APK. (raylib's `projects/Android` template is the reference.)
3. Run on a real phone; check the GLES2 shader, frame rate, and that vertex colours (baked ambient occlusion) load.
4. Handle app pause/resume and the Android back button.
5. UI scale for high-DPI phones (the HUD already scales with screen height).
