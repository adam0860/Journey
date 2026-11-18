# Journey on Android (your phone)

This folder is an Android Studio project. It builds the same C++ game as the Windows version
(CMake project one folder up) into `libjourney.so` and wraps it in an APK.

## One-time setup (about 20 minutes, mostly downloads)
1. Install **Android Studio** (free): https://developer.android.com/studio
2. Open it, choose **Open**, and pick this `android` folder.
   Android Studio will offer to download the Gradle version, **Android SDK Platform 34**, the **NDK (Side by side)**
   and **CMake** - click Install/Accept for each. (If it doesn't prompt: Tools > SDK Manager > SDK Tools tab,
   tick "NDK (Side by side)" and "CMake", Apply.)
3. Wait for "Gradle sync" to finish (bottom bar).

## Put it on your phone
1. On the phone: Settings > About phone > tap **Build number** 7 times, then Settings > System > Developer options >
   turn on **USB debugging**.
2. Plug the phone into the PC with a USB cable, accept the "Allow USB debugging" prompt on the phone.
3. In Android Studio pick your phone in the device drop-down at the top and press the green **Run** button.
   The first build compiles raylib + the game (a few minutes). The game then starts on the phone.

Prefer a plain APK file? Build > Build Bundle(s)/APK(s) > Build APK(s), then copy
`app/build/outputs/apk/debug/app-debug.apk` to the phone and open it (allow "install unknown apps" when asked).

## What to expect / how it plays
* The character creator opens first. Tap the name box to get the on-screen keyboard.
* Tap the ground to walk, drag to turn the camera, two-finger pinch/twist to zoom/rotate.
* Tap **Menu** (top-right) to change your appearance again. The look is saved in the app's private storage.
* The phone always runs the enhanced "style B" look. The shadow map option is off (the ground shadows are baked in).

## If something goes wrong
* Black screen / instant close: in Android Studio open **Logcat**, filter by `raylib` or `journey`, and send me the
  last ~30 lines. Most issues (a shader that a particular GPU dislikes, a texture limit) show up there.
* Choppy frame rate: tell me the phone model - the easiest wins are fewer MSAA samples and lighter textures.
* Build error about NDK/CMake versions: Tools > SDK Manager > SDK Tools and install the versions it names.
