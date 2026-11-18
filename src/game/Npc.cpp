#include "game/Npc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "game/Gear.h"
#include "world/Pathfinder.h"

namespace journey {

namespace {

std::vector<SpeciesInfo> BuildSpecies() {
    std::vector<SpeciesInfo> v;
    // key       name       lvl hp  att str def  speed respawn examine
    SpeciesInfo chicken{"chicken", "Chicken", 1, 3, 1, 1, 1, 1.9f, 25.0f, "Yep, definitely a chicken.",
                        3, {{Item::Bones, 1.0f, 1, 1}, {Item::RawChicken, 1.0f, 1, 1}, {Item::Egg, 0.30f, 1, 1}}};
    chicken.height = 0.55f;
    SpeciesInfo cow{"cow", "Cow", 3, 6, 3, 3, 3, 1.4f, 45.0f, "Converts grass to beef.",
                    3, {{Item::Bones, 1.0f, 1, 1}, {Item::Cowhide, 1.0f, 1, 1}, {Item::RawBeef, 0.50f, 1, 1}}};
    cow.height = 1.45f;
    SpeciesInfo calf{"calf", "Calf", 2, 4, 2, 1, 2, 2.1f, 35.0f, "A young cow.",
                     3, {{Item::Bones, 1.0f, 1, 1}, {Item::RawBeef, 0.40f, 1, 1}, {Item::Cowhide, 0.25f, 1, 1}}};
    calf.height = 0.95f;
    // Goblins: aggressive, small, and they carry a few coins. Coins and bones every time, the odd weapon or helmet now and then.
    SpeciesInfo goblin{"goblin", "Goblin", 2, 5, 2, 2, 2, 1.7f, 40.0f, "An ugly green creature with a bad attitude.",
                       4, {{Item::Bones, 1.0f, 1, 1}, {Item::Coins, 1.0f, 3, 14}, {GearItem(Tier::Bronze, GearType::Dagger), 0.06f, 1, 1},
                           {GearItem(Tier::Bronze, GearType::Helm), 0.04f, 1, 1}}};
    goblin.aggressive = true;
    goblin.aggroRange = 3;
    goblin.model = "goblin";
    goblin.colorway = 0;
    goblin.height = 1.25f;
    SpeciesInfo warrior{"goblin_warrior", "Goblin warrior", 5, 10, 5, 5, 4, 1.6f, 70.0f, "A big goblin in scraps of armour.",
                        4, {{Item::Bones, 1.0f, 1, 1}, {Item::Coins, 1.0f, 10, 35}, {GearItem(Tier::Iron, GearType::Dagger), 0.06f, 1, 1},
                            {Item::Bread, 0.25f, 1, 1}}};
    warrior.aggressive = true;
    warrior.aggroRange = 3;
    warrior.model = "goblin";
    warrior.colorway = 1;
    warrior.scale = 1.15f;
    warrior.height = 1.45f;
    v.push_back(chicken);
    v.push_back(cow);
    v.push_back(calf);
    v.push_back(goblin);
    v.push_back(warrior);
    return v;
}

const std::vector<SpeciesInfo>& Species_Table() {
    static const std::vector<SpeciesInfo> table = BuildSpecies();
    return table;
}

int Chebyshev(TileCoord a, TileCoord b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

int RandInt(const std::function<double()>& roll, int lo, int hi) {   // inclusive
    return lo + std::min(hi - lo, static_cast<int>(roll() * (hi - lo + 1)));
}

// Keeps only the part of a path that stays inside the pen.
void TrimToPen(std::vector<TileCoord>* path, const TileRect& pen) {
    size_t keep = 0;
    while (keep < path->size() && pen.Contains((*path)[keep].x, (*path)[keep].z)) ++keep;
    path->resize(keep);
}

}  // namespace

const SpeciesInfo& GetSpecies(Species s) {
    const int i = static_cast<int>(s);
    return Species_Table()[(i >= 0 && i < kSpeciesCount) ? i : 0];
}

bool SpeciesFromKey(const std::string& key, Species* out) {
    for (int i = 0; i < kSpeciesCount; ++i)
        if (key == Species_Table()[i].key) {
            *out = static_cast<Species>(i);
            return true;
        }
    return false;
}

std::vector<ItemStack> RollLoot(Species s, const std::function<double()>& roll) {
    std::vector<ItemStack> out;
    const SpeciesInfo& info = GetSpecies(s);
    for (int i = 0; i < info.lootCount; ++i) {
        const LootEntry& e = info.loot[i];
        if (roll() < e.chance) out.push_back({e.item, RandInt(roll, e.minCount, e.maxCount)});
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------

void Npcs::Init(const TileMap& map) {
    npcs_.clear();
    int id = 1;
    for (const NpcDef& d : map.Npcs()) {
        Species sp;
        const PenDef* pen = map.FindPen(d.pen);
        if (!SpeciesFromKey(d.species, &sp) || !pen) continue;
        Npc n;
        n.id = id++;
        n.species = sp;
        n.colorway = GetSpecies(sp).colorway >= 0 ? GetSpecies(sp).colorway : n.id % 2;
        n.pen = pen->rect;
        n.homeX = n.x = d.x;
        n.homeZ = n.z = d.z;
        n.facing = (n.id * 2.399f);   // a different heading each
        n.maxHp = n.hp = GetSpecies(sp).hitpoints;
        n.idleLeft = 0.5f + (n.id % 5) * 0.7f;
        npcs_.push_back(n);
    }
}

Npc* Npcs::Find(int id) {
    for (Npc& n : npcs_)
        if (n.id == id) return &n;
    return nullptr;
}

const Npc* Npcs::Find(int id) const {
    for (const Npc& n : npcs_)
        if (n.id == id) return &n;
    return nullptr;
}

int Npcs::AliveAt(TileCoord t) const {
    for (const Npc& n : npcs_)
        if (n.Alive() && n.Tile() == t) return n.id;
    return -1;
}

bool Npcs::Hurt(int id, int damage) {
    Npc* n = Find(id);
    if (!n || n->dead) return false;
    n->hurtT = 0.0f;
    n->hp = std::max(0, n->hp - std::max(0, damage));
    if (n->hp > 0) return false;
    n->dead = true;
    n->lootPending = true;
    n->deathT = 0.0f;
    n->hunting = false;
    n->path.clear();
    n->respawnLeft = n->Info().respawnSeconds;
    return true;
}

void Npcs::StartHunt(int id) {
    if (Npc* n = Find(id)) {
        n->hunting = true;
        n->huntLeft = kHuntSeconds;
        n->idleLeft = 0.0f;
    }
}

void Npcs::RefreshHunt(int id) {
    if (Npc* n = Find(id))
        if (n->hunting) n->huntLeft = kHuntSeconds;
}

void Npcs::StopAllHunts() {
    for (Npc& n : npcs_) n.hunting = false;
}

bool Npcs::Occupied(TileCoord t, int exceptId, TileCoord player) const {
    if (t == player) return true;
    for (const Npc& n : npcs_) {
        if (n.id == exceptId || n.dead) continue;
        if (n.Tile() == t) return true;
        if (!n.path.empty() && n.path.back() == t) return true;
    }
    return false;
}

void Npcs::Move(Npc& n, float dt) {
    float budget = n.Info().speed * dt;
    while (budget > 0.0f && !n.path.empty()) {
        const float tx = n.path.front().x + 0.5f, tz = n.path.front().z + 0.5f;
        const float dx = tx - n.x, dz = tz - n.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 1e-4f) n.facing = std::atan2(dx, dz);
        if (dist <= budget) {
            n.x = tx;
            n.z = tz;
            budget -= dist;
            n.path.pop_front();
        } else {
            n.x += dx / dist * budget;
            n.z += dz / dist * budget;
            budget = 0.0f;
        }
    }
    if (!n.path.empty()) n.walkPhase += dt * n.Info().speed * 3.4f;
}

void Npcs::Wander(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    // Somewhere within 3 tiles that is inside the pen, open, and not already taken.
    for (int tries = 0; tries < 12; ++tries) {
        const TileCoord here = n.Tile();
        const TileCoord t{here.x + RandInt(roll, -3, 3), here.z + RandInt(roll, -3, 3)};
        if (t == here || !n.pen.Contains(t.x, t.z) || !map.Walkable(t) || Occupied(t, n.id, player)) continue;
        PathResult p = FindPath(map, here, t);
        if (!p.found) continue;
        TrimToPen(&p.tiles, n.pen);
        if (p.tiles.empty()) continue;
        n.path.assign(p.tiles.begin(), p.tiles.end());
        break;
    }
    // Chickens dart about; cows stand and chew for longer.
    const bool quick = n.species == Species::Chicken || n.species == Species::Goblin || n.species == Species::GoblinWarrior;
    const float base = quick ? 1.2f : 2.5f;
    n.idleLeft = base + static_cast<float>(roll()) * (quick ? 3.5f : 5.5f);
    if (n.path.empty()) n.facing += static_cast<float>(roll() - 0.5) * 2.0f;   // just turn on the spot
}

void Npcs::Chase(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    (void)roll;
    const TileCoord here = n.Tile();
    if (Chebyshev(here, player) <= 1) {   // next to the hero: stand and fight
        n.path.clear();
        n.facing = std::atan2(player.x + 0.5f - n.x, player.z + 0.5f - n.z);
        return;
    }
    if (n.repathLeft > 0.0f && !n.path.empty()) return;
    n.repathLeft = 0.4f;
    // Walk to the free tile next to the hero that is nearest to us.
    bool found = false;
    PathResult best;
    int bestLen = 1 << 20;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord t{player.x + dx, player.z + dz};
            if (t == player || !n.pen.Contains(t.x, t.z) || !map.Walkable(t) || Occupied(t, n.id, player)) continue;
            PathResult p = FindPath(map, here, t);
            if (!p.found) continue;
            TrimToPen(&p.tiles, n.pen);
            if (p.tiles.empty() || static_cast<int>(p.tiles.size()) >= bestLen) continue;
            bestLen = static_cast<int>(p.tiles.size());
            best = p;
            found = true;
        }
    if (found) n.path.assign(best.tiles.begin(), best.tiles.end());
}

void Npcs::Respawn(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    TileCoord t{static_cast<int>(n.homeX), static_cast<int>(n.homeZ)};
    for (int tries = 0; tries < 40 && (Occupied(t, n.id, player) || !map.Walkable(t)); ++tries)
        t = {RandInt(roll, n.pen.x0, n.pen.x1 - 1), RandInt(roll, n.pen.z0, n.pen.z1 - 1)};
    n.x = t.x + 0.5f;
    n.z = t.z + 0.5f;
    n.hp = n.maxHp;
    n.dead = false;
    n.lootPending = false;
    n.hunting = false;
    n.path.clear();
    n.spawnT = 0.0f;
    n.idleLeft = 1.0f + static_cast<float>(roll()) * 3.0f;
}

void Npcs::Update(float dt, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    for (Npc& n : npcs_) {
        if (n.dead) {
            n.deathT += dt;
            n.respawnLeft -= dt;
            if (n.respawnLeft <= 0.0f && n.deathT > kCorpseSeconds) Respawn(n, map, player, roll);
            continue;
        }
        n.spawnT += dt;
        n.attackT += dt;
        n.hurtT += dt;
        n.repathLeft -= dt;
        if (n.hunting) {
            n.huntLeft -= dt;
            if (n.huntLeft <= 0.0f) n.hunting = false;
        }
        // Aggressive creatures pick a fight with a hero who comes close - unless he is far too strong to bother with.
        const SpeciesInfo& info = n.Info();
        if (info.aggressive && !n.hunting && heroAlive_ && n.spawnT > 1.0f && heroCombat_ < info.level * 2 + 5 &&
            Chebyshev(n.Tile(), player) <= info.aggroRange) {
            n.hunting = true;
            n.huntLeft = kHuntSeconds;
            n.idleLeft = 0.0f;
        }
        if (n.hunting) {
            Chase(n, map, player, roll);
        } else {
            n.idleLeft -= dt;
            if (n.path.empty() && n.idleLeft <= 0.0f) Wander(n, map, player, roll);
        }
        Move(n, dt);
    }
}

}  // namespace journey
