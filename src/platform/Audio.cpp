#include "platform/Audio.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace journey {

Sound AudioSystem::MakeSound(const std::vector<int16_t>& pcm) const {
    Wave w{};
    w.frameCount = static_cast<unsigned int>(pcm.size());
    w.sampleRate = kSampleRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = const_cast<int16_t*>(pcm.data());   // (LoadSoundFromWave copies it)
    return LoadSoundFromWave(w);
}

void AudioSystem::Init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        TraceLog(LOG_WARNING, "AUDIO: no audio device, the game will be silent");
        return;
    }
    for (int i = 0; i < kSfxCount; ++i) {
        SfxSlot& s = sfx_[i];
        s.base = MakeSound(RenderSfx(static_cast<Sfx>(i)));
        for (int k = 0; k < kAliases; ++k) s.alias[k] = LoadSoundAlias(s.base);
    }
}

AudioSystem::~AudioSystem() {
    // (the device is closed with the window; nothing to free one by one)
}

void AudioSystem::SetMusicVolume(float v) {
    musicVolume_ = std::clamp(v, 0.0f, 1.0f);
}

void AudioSystem::Play(Sfx sfx, float volume, float pitch) {
    if (!ready_ || sfxVolume_ <= 0.0f) return;
    SfxSlot& s = sfx_[static_cast<int>(sfx)];
    Sound& snd = s.alias[s.next];
    s.next = (s.next + 1) % kAliases;
    SetSoundVolume(snd, std::clamp(volume * sfxVolume_, 0.0f, 1.0f));
    SetSoundPitch(snd, pitch);
    PlaySound(snd);
}

void AudioSystem::Update(float dt, const std::string& area) {
    if (!ready_) return;
    const int want = static_cast<int>(MoodForArea(area));
    if (want != current_) {   // a new kind of place: the old tune fades out, the new one starts
        if (current_ >= 0) {
            fadingOut_ = current_;
            fadeOut_ = fadeIn_;
        }
        current_ = want;
        fadeIn_ = 0.0f;
        if (!musicMade_[want]) {
            music_[want] = MakeSound(RenderMusic(static_cast<Mood>(want)));
            musicMade_[want] = true;
        }
        PlaySound(music_[want]);
    }
    fadeIn_ = std::min(1.0f, fadeIn_ + dt / 2.5f);
    // keep the current tune looping
    if (!IsSoundPlaying(music_[current_])) PlaySound(music_[current_]);
    SetSoundVolume(music_[current_], musicVolume_ * 0.6f * fadeIn_);
    if (fadingOut_ >= 0) {
        fadeOut_ = std::max(0.0f, fadeOut_ - dt / 2.0f);
        SetSoundVolume(music_[fadingOut_], musicVolume_ * 0.6f * fadeOut_);
        if (fadeOut_ <= 0.0f) {
            StopSound(music_[fadingOut_]);
            fadingOut_ = -1;
        }
    }
}

}  // namespace journey
