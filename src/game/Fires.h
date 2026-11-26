// Firemaking: lighting logs with a tinderbox, and the campfires that result. Pure C++ (unit tested).
//
// A fire lives on one tile, blocks it while it burns (you walk round it), burns for 2.5 minutes, then leaves
// ashes for a few seconds. You cook raw food on it.
#pragma once
#include <functional>
#include <vector>

#include "world/TileMap.h"

namespace journey {

constexpr float kLightAttemptSeconds = 1.2f;   // each try to light the logs
constexpr int kLightXp = 40;                   // Firemaking XP for one lit log
constexpr float kFireBurnSeconds = 150.0f;
constexpr float kAshSeconds = 15.0f;
// A bonfire: four logs of one kind heaped together (round 13: the design document wants 15 firemaking actions, and every kind of log now has two:
// a campfire and a bonfire). It burns 4 times as long, gives 25% more XP than four campfires, and is as good as a cooking range.
constexpr int kBonfireLogs = 4;
constexpr float kBonfireBurnSeconds = 4.0f * kFireBurnSeconds;
constexpr float kBonfireXpFactor = 1.25f;

// Chance (0..1) that one attempt lights the logs, for a Firemaking level 1..99.
float LightChance(int firemakingLevel);

enum class LightEvent { None, Failed, Lit };

class LightSession {
public:
    void Start() { active_ = true; timer_ = 0.0f; }
    void Stop() { active_ = false; }
    bool Active() const { return active_; }
    // At most one event per call. Lit ends the session.
    LightEvent Update(float dt, int firemakingLevel, const std::function<double()>& roll);

private:
    bool active_ = false;
    float timer_ = 0.0f;
};

struct Fire {
    int id = 0;                 // stable handle (indices shift as old fires disappear)
    TileCoord tile;
    bool big = false;           // a bonfire
    float burnLeft = kFireBurnSeconds;
    float ashLeft = kAshSeconds;
    float age = 0.0f;           // seconds since it was lit (the renderer fades it in)
    bool Burning() const { return burnLeft > 0.0f; }
};

class Fires {
public:
    // Can a fire be lit on this tile right now? (An ordinary open tile with no fire on it.)
    bool CanLight(const TileMap& map, TileCoord t) const;
    // Lights a fire and blocks the tile. Returns its id, or -1 if CanLight() is false.
    int Light(TileMap* map, TileCoord t, bool big = false);
    // Burns the fires down; a fire that goes out frees its tile.
    void Update(float dt, TileMap* map);

    const std::vector<Fire>& All() const { return fires_; }
    const Fire* Find(int id) const;
    // Burning fire on this tile, or nullptr.
    const Fire* BurningAt(TileCoord t) const;
    // Nearest burning fire (by tile distance) to `from`, or nullptr when none burns.
    const Fire* NearestBurning(TileCoord from) const;

private:
    std::vector<Fire> fires_;
    int nextId_ = 1;
};

}  // namespace journey
