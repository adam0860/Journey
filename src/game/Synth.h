// Procedural audio: the game's music and sound effects are made from maths, not from audio files. Pure C++ (unit tested).
//
// Music: one seamless loop per Mood (town, field, cave, desert, abyss), built from a chord progression, a bass line, a soft pad and
// a melody picked by a seeded random generator (so it is the same every time). Sound effects: short synthesised blips and thumps.
// Everything is mono 16-bit at kSampleRate; src/platform/Audio.* plays it with raylib.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace journey {

constexpr int kSampleRate = 22050;

enum class Mood { Town, Field, Cave, Desert, Abyss, Count };
constexpr int kMoodCount = static_cast<int>(Mood::Count);
const char* MoodName(Mood m);
// Which music plays in a named place (an empty or unknown name = the open countryside).
Mood MoodForArea(const std::string& areaName);
// One seamless loop of music (about 20-40 seconds).
std::vector<int16_t> RenderMusic(Mood mood);

enum class Sfx { Hit, Miss, Chop, Mine, Splash, Coin, LevelUp, Pickup, Shoot, Cast, Eat, Death, Click, Ding, Count };
constexpr int kSfxCount = static_cast<int>(Sfx::Count);
const char* SfxName(Sfx s);
std::vector<int16_t> RenderSfx(Sfx s);

}  // namespace journey
