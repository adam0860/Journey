#include "game/Synth.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {

constexpr float kPi = 3.14159265f;

// A tiny deterministic random generator (xorshift), so the music is the same on every machine.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    float Next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s >> 8) / 16777216.0f;   // [0,1)
    }
    int Pick(int n) { return std::min(n - 1, static_cast<int>(Next() * static_cast<float>(n))); }
};

float MidiHz(float note) { return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f); }

enum class Timbre { Pad, Pluck, Bass, Bell };

// Mixes one note into a float buffer. `start` and `len` are in seconds.
void AddNote(std::vector<float>* buf, float start, float len, float hz, float vol, Timbre t) {
    const int i0 = static_cast<int>(start * kSampleRate);
    float attack = 0.01f, release = 0.05f;
    float decay = 0.0f;   // > 0: an exponential fade instead of a held note
    switch (t) {
        case Timbre::Pad: attack = std::min(0.6f, len * 0.4f); release = std::min(1.0f, len * 0.5f); break;
        case Timbre::Pluck: decay = 3.2f; release = 0.04f; break;
        case Timbre::Bass: attack = 0.02f; release = 0.25f; decay = 0.9f; break;
        case Timbre::Bell: attack = 0.005f; decay = 1.4f; release = 0.1f; break;
    }
    const float total = len + release;
    const int n = static_cast<int>(total * kSampleRate);
    for (int k = 0; k < n; ++k) {
        const int idx = i0 + k;
        if (idx < 0 || idx >= static_cast<int>(buf->size())) continue;
        const float tt = static_cast<float>(k) / kSampleRate;
        float env = tt < attack ? tt / attack : 1.0f;
        if (decay > 0.0f) env *= std::exp(-decay * tt);
        if (tt > len) env *= std::max(0.0f, 1.0f - (tt - len) / release);
        const float ph = 2.0f * kPi * hz * tt;
        float s;
        switch (t) {
            case Timbre::Pad: s = std::sin(ph) + 0.5f * std::sin(2.0f * ph * 1.003f) + 0.25f * std::sin(3.0f * ph); break;
            case Timbre::Pluck: s = std::sin(ph) + 0.45f * std::sin(2.0f * ph) + 0.2f * std::sin(3.0f * ph); break;
            case Timbre::Bass: s = std::sin(ph) + 0.35f * std::sin(2.0f * ph); break;
            default: s = std::sin(ph) + 0.4f * std::sin(2.76f * ph) + 0.2f * std::sin(5.4f * ph); break;
        }
        (*buf)[static_cast<size_t>(idx)] += s * env * vol;
    }
}

struct Chord {
    int root;      // semitones above the mood's root note
    bool minor;
};

struct MoodDef {
    int root;                 // MIDI note of the key
    float bpm;
    std::vector<Chord> prog;  // one chord per bar, repeated
    std::vector<int> scale;   // the melody's notes (semitones above the root)
    float pad, arp, bass, melody, bells, wind;   // how loud each layer is
    int bars;
    uint32_t seed;
};

const MoodDef& Def(Mood m) {
    static const MoodDef defs[kMoodCount] = {
        // town: bright and warm, a gentle waltz of arpeggios and a tune
        {55, 100.0f, {{0, false}, {7, false}, {9, true}, {5, false}}, {0, 2, 4, 7, 9, 12, 14}, 0.20f, 0.17f, 0.30f, 0.20f, 0.0f, 0.0f, 8, 1234u},
        // field: open and calm, slower, with a little wind
        {52, 80.0f, {{0, false}, {9, true}, {5, false}, {7, false}}, {0, 2, 4, 7, 9, 12}, 0.24f, 0.12f, 0.26f, 0.15f, 0.0f, 0.03f, 8, 777u},
        // cave: low drones, sparse echoing drips
        {45, 56.0f, {{0, true}, {8, false}, {3, false}, {10, false}}, {0, 3, 7, 10, 12, 15}, 0.30f, 0.0f, 0.22f, 0.0f, 0.22f, 0.05f, 6, 4242u},
        // desert: a snaking minor-with-a-flat-second melody over a slow drone
        {50, 76.0f, {{0, true}, {1, false}, {0, true}, {10, false}}, {0, 1, 4, 5, 7, 8, 10, 12}, 0.20f, 0.0f, 0.24f, 0.26f, 0.06f, 0.04f, 8, 909u},
        // abyss: a dissonant drone, the tritone and rumbling wind
        {41, 62.0f, {{0, true}, {6, true}, {1, true}, {6, true}}, {0, 1, 6, 7, 10}, 0.34f, 0.0f, 0.30f, 0.0f, 0.15f, 0.09f, 6, 666u},
    };
    return defs[static_cast<int>(m)];
}

int16_t ToPcm(float v) { return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 30000.0f); }

// --- sound-effect helpers
float Noise(Rng* r) { return r->Next() * 2.0f - 1.0f; }

}  // namespace

const char* MoodName(Mood m) {
    switch (m) {
        case Mood::Town: return "town";
        case Mood::Field: return "field";
        case Mood::Cave: return "cave";
        case Mood::Desert: return "desert";
        case Mood::Abyss: return "abyss";
        default: return "?";
    }
}

Mood MoodForArea(const std::string& a) {
    if (a == "The Abyss") return Mood::Abyss;
    if (a == "Desert Ruins" || a == "The Dragon's Temple" || a == "The Dune Sea") return Mood::Desert;
    if (a == "Mining Caves" || a == "Castle Crypt" || a == "Troll Mountains" || a == "Mirewood Swamp" || a == "Barrow Downs" || a == "Darkwood" ||
        a == "The Iron Range" || a == "Frostpeak Range")
        return Mood::Cave;
    static const char* towns[] = {"Castle Town", "Market Square", "Castle Courtyard", "Throne Hall", "Bank", "Smithy", "Quarry",
                                  "The Journey's Rest Inn", "Millbrook Village", "Millbrook Square", "Millbrook Smithy", "Millbrook Mine",
                                  "Millbrook Bank", "Millbrook Chapel", "Millbrook Crafters' Yard", "Fishing Pier", "Elmswick", "Saltmere"};
    for (const char* t : towns)
        if (a == t) return Mood::Town;
    return Mood::Field;
}

std::vector<int16_t> RenderMusic(Mood mood) {
    const MoodDef& d = Def(mood);
    const float beat = 60.0f / d.bpm, bar = beat * 4.0f;
    const int loopN = static_cast<int>(bar * static_cast<float>(d.bars) * kSampleRate);
    const int tailN = 3 * kSampleRate;   // notes ring past the end of the loop and wrap round to its start
    std::vector<float> buf(static_cast<size_t>(loopN + tailN), 0.0f);
    Rng rng(d.seed);
    for (int b = 0; b < d.bars; ++b) {
        const Chord& c = d.prog[static_cast<size_t>(b) % d.prog.size()];
        const float t0 = static_cast<float>(b) * bar;
        const int root = d.root + c.root;
        const int tones[3] = {root, root + (c.minor ? 3 : 4), root + 7};
        // the pad: the whole chord, held, an octave up from the bass
        for (int k = 0; k < 3; ++k) AddNote(&buf, t0, bar, MidiHz(static_cast<float>(tones[k] + 12)), d.pad * 0.5f, Timbre::Pad);
        // the bass: the root on the first beat (and the fifth on the third beat for the livelier moods)
        AddNote(&buf, t0, beat * 2.0f, MidiHz(static_cast<float>(root - 12)), d.bass, Timbre::Bass);
        if (d.arp > 0.0f) AddNote(&buf, t0 + beat * 2.0f, beat * 1.5f, MidiHz(static_cast<float>(root - 5)), d.bass * 0.7f, Timbre::Bass);
        // the arpeggio: eighth notes up and down the chord
        if (d.arp > 0.0f) {
            static const int pat[8] = {0, 1, 2, 1, 2, 1, 0, 1};
            for (int k = 0; k < 8; ++k)
                AddNote(&buf, t0 + beat * 0.5f * static_cast<float>(k), beat * 0.5f, MidiHz(static_cast<float>(tones[pat[k]] + 24)), d.arp, Timbre::Pluck);
        }
        // the melody: a random walk on the scale, quarter notes with rests
        if (d.melody > 0.0f) {
            int pos = rng.Pick(static_cast<int>(d.scale.size()));
            for (int k = 0; k < 4; ++k) {
                if (rng.Next() < 0.30f) continue;
                pos = std::clamp(pos + rng.Pick(5) - 2, 0, static_cast<int>(d.scale.size()) - 1);
                const float len = rng.Next() < 0.35f ? beat * 2.0f : beat;
                AddNote(&buf, t0 + beat * static_cast<float>(k), len, MidiHz(static_cast<float>(d.root + 24 + d.scale[static_cast<size_t>(pos)])), d.melody,
                        mood == Mood::Desert ? Timbre::Pluck : Timbre::Bell);
            }
        }
        // bells / drips: a couple of high, quiet notes per bar
        if (d.bells > 0.0f) {
            for (int k = 0; k < 2; ++k) {
                if (rng.Next() < 0.35f) continue;
                const int n = d.scale[static_cast<size_t>(rng.Pick(static_cast<int>(d.scale.size())))];
                AddNote(&buf, t0 + beat * (static_cast<float>(rng.Pick(8)) * 0.5f), beat, MidiHz(static_cast<float>(d.root + 36 + n)), d.bells, Timbre::Bell);
            }
        }
    }
    // wind: low-pass noise that swells and fades slowly
    if (d.wind > 0.0f) {
        float lp = 0.0f;
        for (int i = 0; i < loopN + tailN; ++i) {
            const float t = static_cast<float>(i) / kSampleRate;
            lp += 0.015f * (Noise(&rng) - lp);
            const float swell = 0.55f + 0.45f * std::sin(2.0f * kPi * t / (bar * 2.0f * static_cast<float>(d.bars) / static_cast<float>(d.bars)));
            buf[static_cast<size_t>(i)] += lp * 6.0f * d.wind * swell;
        }
    }
    // wrap the tail round to the start, so the loop has no seam
    for (int i = 0; i < tailN; ++i) buf[static_cast<size_t>(i)] += buf[static_cast<size_t>(loopN + i)];
    buf.resize(static_cast<size_t>(loopN));
    float peak = 0.001f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    std::vector<int16_t> out(buf.size());
    const float gain = 0.8f / peak;
    for (size_t i = 0; i < buf.size(); ++i) out[i] = ToPcm(buf[i] * gain);
    return out;
}

const char* SfxName(Sfx s) {
    static const char* names[] = {"hit", "miss", "chop", "mine", "splash", "coin", "levelup", "pickup", "shoot", "cast", "eat", "death", "click", "ding"};
    return names[static_cast<int>(s)];
}

std::vector<int16_t> RenderSfx(Sfx s) {
    Rng rng(1000u + static_cast<uint32_t>(s));
    std::vector<float> b;
    auto len = [&](float sec) { b.assign(static_cast<size_t>(sec * kSampleRate), 0.0f); };
    auto t = [&](size_t i) { return static_cast<float>(i) / kSampleRate; };
    switch (s) {
        case Sfx::Hit: {   // a dull thump with a burst of noise
            len(0.18f);
            float lp = 0.0f;
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i), env = std::exp(-24.0f * x);
                lp += 0.25f * (Noise(&rng) - lp);
                b[i] = (0.7f * std::sin(2.0f * kPi * (150.0f - 400.0f * x) * x) + 0.8f * lp) * env;
            }
            break;
        }
        case Sfx::Miss: {  // a swish: noise whose brightness sweeps up and fades
            len(0.22f);
            float lp = 0.0f, prev = 0.0f;
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i) / 0.22f;
                lp += (0.1f + 0.5f * x) * (Noise(&rng) - lp);
                const float hp = lp - prev;
                prev = lp;
                b[i] = hp * 2.5f * std::sin(kPi * x);
            }
            break;
        }
        case Sfx::Chop: {  // a wooden knock
            len(0.16f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                b[i] = (std::sin(2.0f * kPi * (210.0f - 600.0f * x) * x) * std::exp(-30.0f * x)) + (i < 300 ? Noise(&rng) * 0.6f * (1.0f - static_cast<float>(i) / 300.0f) : 0.0f);
            }
            break;
        }
        case Sfx::Mine: {  // a metal clink
            len(0.28f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                b[i] = (0.6f * std::sin(2.0f * kPi * 1850.0f * x) + 0.4f * std::sin(2.0f * kPi * 2710.0f * x)) * std::exp(-18.0f * x) + (i < 200 ? Noise(&rng) * 0.5f : 0.0f);
            }
            break;
        }
        case Sfx::Splash: {  // water: noise that brightens and fades
            len(0.4f);
            float lp = 0.0f;
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i) / 0.4f;
                lp += (0.05f + 0.3f * (1.0f - x)) * (Noise(&rng) - lp);
                b[i] = lp * 2.2f * std::sin(kPi * std::min(1.0f, x * 1.3f)) * (1.0f - x);
            }
            break;
        }
        case Sfx::Coin: {  // two bright pings
            len(0.4f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                const float f = x < 0.09f ? 1318.5f : 1760.0f;
                const float xx = x < 0.09f ? x : x - 0.09f;
                b[i] = (std::sin(2.0f * kPi * f * x) + 0.3f * std::sin(4.0f * kPi * f * x)) * std::exp(-9.0f * xx) * 0.55f;
            }
            break;
        }
        case Sfx::LevelUp: {  // a rising arpeggio
            len(1.1f);
            const float notes[5] = {523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f};
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                for (int k = 0; k < 5; ++k) {
                    const float st = 0.11f * static_cast<float>(k);
                    if (x < st) continue;
                    const float xx = x - st;
                    b[i] += (std::sin(2.0f * kPi * notes[k] * xx) + 0.3f * std::sin(4.0f * kPi * notes[k] * xx)) * std::exp(-4.5f * xx) * 0.35f;
                }
            }
            break;
        }
        case Sfx::Pickup: {  // a short rising blip
            len(0.1f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                b[i] = std::sin(2.0f * kPi * (700.0f + 4000.0f * x) * x) * (1.0f - x / 0.1f) * 0.6f;
            }
            break;
        }
        case Sfx::Shoot: {  // a bowstring twang and a whoosh
            len(0.3f);
            float lp = 0.0f;
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                lp += 0.3f * (Noise(&rng) - lp);
                b[i] = std::sin(2.0f * kPi * (320.0f - 500.0f * x) * x) * std::exp(-14.0f * x) * 0.8f + lp * std::exp(-9.0f * x) * 0.5f;
            }
            break;
        }
        case Sfx::Cast: {  // a rising whoosh with a shimmer
            len(0.55f);
            float lp = 0.0f;
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i) / 0.55f;
                lp += (0.05f + 0.4f * x) * (Noise(&rng) - lp);
                b[i] = (lp * 2.0f + 0.35f * std::sin(2.0f * kPi * (300.0f + 900.0f * x) * t(i))) * std::sin(kPi * x);
            }
            break;
        }
        case Sfx::Eat: {   // three crunches
            len(0.5f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                const float ph = std::fmod(x, 0.16f);
                if (x < 0.48f) b[i] = Noise(&rng) * std::exp(-45.0f * ph) * 0.8f;
            }
            break;
        }
        case Sfx::Death: {  // a slow falling tone
            len(1.0f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                b[i] = (std::sin(2.0f * kPi * (330.0f - 220.0f * x) * x) + 0.4f * std::sin(4.0f * kPi * (330.0f - 220.0f * x) * x)) * std::exp(-2.2f * x) * 0.6f;
            }
            break;
        }
        case Sfx::Click: {
            len(0.04f);
            for (size_t i = 0; i < b.size(); ++i) b[i] = std::sin(2.0f * kPi * 900.0f * t(i)) * (1.0f - t(i) / 0.04f) * 0.5f;
            break;
        }
        case Sfx::Ding:
        default: {         // a soft single note
            len(0.3f);
            for (size_t i = 0; i < b.size(); ++i) {
                const float x = t(i);
                b[i] = std::sin(2.0f * kPi * 1046.5f * x) * std::exp(-12.0f * x) * 0.4f;
            }
            break;
        }
    }
    std::vector<int16_t> out(b.size());
    for (size_t i = 0; i < b.size(); ++i) out[i] = ToPcm(b[i]);
    return out;
}

}  // namespace journey
