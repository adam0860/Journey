// The creatures: chickens, cows, calves and goblins. Pure C++ (unit tested).
//
// They live in an area (the map's @pen / @npc lines - fenced for the farm animals, an open camp for the goblins) and wander
// about it. Animals are not aggressive, but once you attack one it fights back and chases you (never out of its area).
// Goblins ARE aggressive: they attack a hero who comes close, unless he is far too strong for them to bother with.
// A dead creature leaves a pile of loot where it fell and comes back to life after a while.
#pragma once
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "game/Inventory.h"
#include "world/TileMap.h"

namespace journey {

enum class Species { Chicken, Cow, Calf, Goblin, GoblinWarrior, Count };
constexpr int kSpeciesCount = static_cast<int>(Species::Count);

struct LootEntry {
    Item item;
    float chance;      // 0..1
    int minCount, maxCount;
};

struct SpeciesInfo {
    const char* key;       // "chicken" (map file / model names)
    const char* name;      // "Chicken" (menus)
    int level;             // the number shown next to its name
    int hitpoints;
    int attack, strength, defence;
    float speed;           // tiles per second
    float respawnSeconds;  // time after death until it is back
    const char* examine;
    int lootCount;
    LootEntry loot[4];
    bool aggressive = false;   // attacks the hero when he comes close
    int aggroRange = 0;        // ...within this many tiles
    const char* model = nullptr;   // model files are <model>_<colorway>_<part>.glb (null = key)
    int colorway = -1;         // which look (-1 = alternate by id)
    float scale = 1.0f;
    float height = 1.0f;       // how tall it stands, in metres (health bars and hit splats)
};

const SpeciesInfo& GetSpecies(Species s);
bool SpeciesFromKey(const std::string& key, Species* out);

// What a kill drops: each loot entry is rolled separately. `roll` returns numbers in [0,1).
std::vector<ItemStack> RollLoot(Species s, const std::function<double()>& roll);

struct Npc {
    int id = 0;
    Species species = Species::Chicken;
    int colorway = 0;              // which coat/feathers (a model variant)
    TileRect pen;                  // it never leaves this rectangle
    float homeX = 0, homeZ = 0;
    float x = 0, z = 0;            // world position (tile units)
    float facing = 0;              // radians, 0 = facing +z
    int hp = 1, maxHp = 1;
    bool dead = false;
    bool lootPending = false;      // died, loot not yet dropped (the game drops it when the fall animation ends)
    float deathT = 0;              // seconds since it died
    float respawnLeft = 0;
    float spawnT = 99;             // seconds since it (re)appeared: it grows in
    float attackT = 99;            // seconds since it last struck (animation)
    float hurtT = 99;              // seconds since it last got hurt (animation)
    float walkPhase = 0;
    float idleLeft = 1;            // seconds until it wanders somewhere else
    bool hunting = false;          // fighting the hero: chases him and may hit him
    float huntLeft = 0;
    float repathLeft = 0;
    std::deque<TileCoord> path;

    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
    bool Moving() const { return !path.empty(); }
    bool Alive() const { return !dead; }
    const SpeciesInfo& Info() const { return GetSpecies(species); }
};

class Npcs {
public:
    static constexpr float kDeathAnimSeconds = 1.2f;   // the fall; the loot appears at the end of it
    static constexpr float kCorpseSeconds = 2.6f;      // the body stays this long, then vanishes
    static constexpr float kHuntSeconds = 10.0f;       // a fight with nothing happening for this long is forgotten

    void Init(const TileMap& map);
    // `player` is where the hero stands (animals keep out of his tile and chase him when hunting).
    void Update(float dt, const TileMap& map, TileCoord player, const std::function<double()>& roll);
    // The hero's combat level and whether he is alive: aggressive creatures leave a hero who is far too strong alone.
    void SetHero(int combatLevel, bool alive) {
        heroCombat_ = combatLevel;
        heroAlive_ = alive;
    }

    std::vector<Npc>& All() { return npcs_; }
    const std::vector<Npc>& All() const { return npcs_; }
    Npc* Find(int id);
    const Npc* Find(int id) const;
    // A living animal standing on this tile (its id), or -1.
    int AliveAt(TileCoord t) const;

    // Damages an animal. Returns true if that killed it.
    bool Hurt(int id, int damage);
    void StartHunt(int id);
    void RefreshHunt(int id);
    void StopAllHunts();

private:
    bool Occupied(TileCoord t, int exceptId, TileCoord player) const;
    void Wander(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll);
    void Chase(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll);
    void Respawn(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll);
    void Move(Npc& n, float dt);

    std::vector<Npc> npcs_;
    int heroCombat_ = 3;
    bool heroAlive_ = true;
};

}  // namespace journey
