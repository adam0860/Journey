#include "game/Mining.h"

#include <algorithm>

namespace journey {

namespace {
const RockInfo kRocks[kRockKindCount] = {
    {"Copper rocks",  "copper",  Item::CopperOre,  1,  18, 6.0f,  0.40f, "Rocks containing copper ore."},
    {"Tin rocks",     "tin",     Item::TinOre,     1,  18, 6.0f,  0.40f, "Rocks containing tin ore."},
    {"Iron rocks",    "iron",    Item::IronOre,    15, 35, 10.0f, 0.35f, "Rocks containing iron ore."},
    {"Coal rocks",    "coal",    Item::Coal,       30, 50, 30.0f, 0.30f, "Rocks containing coal."},
    {"Mithril rocks", "mithril", Item::MithrilOre, 55, 80, 90.0f, 0.25f, "Rocks containing mithril ore."},
};
}  // namespace

const RockInfo& GetRockInfo(RockKind k) {
    return kRocks[std::clamp(static_cast<int>(k), 0, kRockKindCount - 1)];
}

bool RockKindFromKey(const std::string& key, RockKind* out) {
    for (int i = 0; i < kRockKindCount; ++i)
        if (key == kRocks[i].key) {
            if (out) *out = static_cast<RockKind>(i);
            return true;
        }
    return false;
}

float MineChance(RockKind k, int level) {
    const RockInfo& r = GetRockInfo(k);
    // the required level gives the base chance, and every level above it adds a little (never below the base, never above 92%)
    const float c = r.baseChance + 0.012f * static_cast<float>(std::max(0, level - r.level));
    return std::min(0.92f, c);
}

Item BestPickaxe(const Inventory& bag, int miningLevel) {
    for (int t = kTierCount - 1; t >= 0; --t) {
        const Tier tier = static_cast<Tier>(t);
        if (miningLevel < TierRequirement(tier)) continue;
        const Item pick = GearItem(tier, GearType::Pickaxe);
        if (bag.Has(pick)) return pick;
    }
    return Item::None;
}

MineEvent MineSession::Update(float dt, RockKind kind, int level, float speed, const std::function<double()>& roll) {
    if (!Active()) return MineEvent::None;
    timer_ += dt * std::max(0.1f, speed);
    if (timer_ < kMineSwingSeconds) return MineEvent::None;
    timer_ -= kMineSwingSeconds;
    return roll() < MineChance(kind, level) ? MineEvent::GotOre : MineEvent::Swing;
}

void Rocks::Init(const std::vector<RockDef>& defs) {
    rocks_.clear();
    for (const RockDef& d : defs) {
        RockState r;
        r.def = d;
        if (!RockKindFromKey(d.kind, &r.kind)) continue;
        rocks_.push_back(r);
    }
}

void Rocks::Update(float dt) {
    for (RockState& r : rocks_) {
        r.hitAge += dt;
        if (r.depleted) {
            r.respawnLeft -= dt;
            if (r.respawnLeft <= 0.0f) {
                r.depleted = false;
                r.respawnLeft = 0.0f;
                r.growAnim = 0.0f;
            }
        } else if (r.growAnim < 1.0f) {
            r.growAnim = std::min(1.0f, r.growAnim + dt / kRockGrowSeconds);
        }
    }
}

int Rocks::IndexAtTile(TileCoord t) const {
    for (size_t i = 0; i < rocks_.size(); ++i)
        if (rocks_[i].def.Tile() == t) return static_cast<int>(i);
    return -1;
}

bool Rocks::Deplete(int index) {
    if (index < 0 || index >= static_cast<int>(rocks_.size()) || rocks_[index].depleted) return false;
    RockState& r = rocks_[index];
    r.depleted = true;
    r.respawnLeft = GetRockInfo(r.kind).respawn;
    return true;
}

float Rocks::SecondsUntilRespawn(int index) const {
    if (index < 0 || index >= static_cast<int>(rocks_.size()) || !rocks_[index].depleted) return 0.0f;
    return rocks_[index].respawnLeft;
}

}  // namespace journey
