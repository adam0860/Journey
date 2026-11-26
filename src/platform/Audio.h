// Plays the game's procedural music and sound effects with raylib's audio (src/game/Synth.* makes the samples).
//
// Music: one looping track per Mood; when the hero walks into another kind of place the old track fades out and the new one fades in.
// Sound effects: short sounds that may overlap (each effect keeps a few aliases so a second hit does not cut the first one off).
#pragma once
#include <string>
#include <vector>

#include "game/Synth.h"
#include "raylib.h"

namespace journey {

class AudioSystem {
public:
    ~AudioSystem();
    // Opens the audio device and makes the sounds. Safe to call on a machine with no sound: everything then does nothing.
    void Init();
    bool Ready() const { return ready_; }
    // Call every frame: the place he is in picks the music (empty = countryside).
    void Update(float dt, const std::string& area);
    void Play(Sfx sfx, float volume = 1.0f, float pitch = 1.0f);
    void SetMusicVolume(float v);
    void SetSfxVolume(float v) { sfxVolume_ = v; }
    float MusicVolume() const { return musicVolume_; }
    float SfxVolume() const { return sfxVolume_; }

private:
    static constexpr int kAliases = 4;
    struct SfxSlot {
        Sound base{};
        Sound alias[kAliases]{};
        int next = 0;
    };
    Sound MakeSound(const std::vector<int16_t>& pcm) const;

    bool ready_ = false;
    float musicVolume_ = 0.5f, sfxVolume_ = 0.8f;
    SfxSlot sfx_[kSfxCount];
    Sound music_[kMoodCount]{};
    bool musicMade_[kMoodCount] = {};
    int current_ = -1;      // the mood now playing (-1 = none yet)
    int fadingOut_ = -1;
    float fadeIn_ = 0.0f, fadeOut_ = 0.0f;   // 0..1 volume factors
};

}  // namespace journey
