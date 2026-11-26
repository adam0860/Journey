// The filler adventurers: fixed level, fixed gear, a trade and a place. See Adventurers.h.
#include <algorithm>
#include <cmath>

#include "game/Adventurers.h"
#include "game/Gear.h"
#include "world/Pathfinder.h"

namespace journey {

namespace {

struct Hub {
    const char* area;
    int count;
    int lo, hi;                 // the levels of the people who live there
    FillerJob jobs[3];          // what they do (picked at random)
    int radius;
};

// Where the other players are: the towns have the whole range of levels, the training grounds have the people they suit.
const Hub kHubs[] = {
    {"Market Square", 5, 3, 70, {FillerJob::Chat, FillerJob::Chat, FillerJob::Wander}, 12},
    {"Bank", 2, 10, 80, {FillerJob::Chat, FillerJob::Wander, FillerJob::Chat}, 8},
    {"Smithy", 2, 15, 60, {FillerJob::Smith, FillerJob::Smith, FillerJob::Chat}, 8},
    {"Quarry", 3, 10, 50, {FillerJob::Mine, FillerJob::Mine, FillerJob::Chat}, 10},
    {"Fishing Pier", 3, 3, 40, {FillerJob::Fish, FillerJob::Fish, FillerJob::Chat}, 10},
    {"Millbrook Square", 4, 5, 75, {FillerJob::Chat, FillerJob::Wander, FillerJob::Chat}, 12},
    {"Millbrook Mine", 3, 15, 60, {FillerJob::Mine, FillerJob::Mine, FillerJob::Chat}, 10},
    {"Millbrook Smithy", 2, 20, 55, {FillerJob::Smith, FillerJob::Smith, FillerJob::Chat}, 8},
    {"Goblin Camp", 3, 5, 25, {FillerJob::Fight, FillerJob::Fight, FillerJob::Fight}, 14},
    {"Elmswick", 4, 8, 55, {FillerJob::Chop, FillerJob::Chat, FillerJob::Chop}, 16},
    {"Woodcutters' Camp", 2, 20, 60, {FillerJob::Chop, FillerJob::Chop, FillerJob::Chop}, 14},
    {"Saltmere", 4, 10, 65, {FillerJob::Fish, FillerJob::Chat, FillerJob::Fish}, 16},
    {"Lakeside", 3, 20, 60, {FillerJob::Fish, FillerJob::Fish, FillerJob::Chat}, 14},
    {"Wolf Wood", 3, 20, 45, {FillerJob::Fight, FillerJob::Fight, FillerJob::Fight}, 16},
    {"Troll Mountains", 3, 35, 65, {FillerJob::Fight, FillerJob::Fight, FillerJob::Mine}, 16},
    {"Mining Caves", 3, 30, 65, {FillerJob::Mine, FillerJob::Mine, FillerJob::Fight}, 16},
    {"Desert Ruins", 2, 55, 80, {FillerJob::Fight, FillerJob::Fight, FillerJob::Fight}, 18},
    {"Giant's Rise", 2, 45, 75, {FillerJob::Fight, FillerJob::Fight, FillerJob::Chat}, 18},
    {"The Iron Range", 2, 45, 75, {FillerJob::Mine, FillerJob::Mine, FillerJob::Mine}, 18},
    {"Darkwood", 2, 40, 65, {FillerJob::Fight, FillerJob::Fight, FillerJob::Fight}, 18},
    {"Barrow Downs", 2, 35, 60, {FillerJob::Fight, FillerJob::Fight, FillerJob::Fight}, 18},
    {"Sunfield Farm", 2, 1, 30, {FillerJob::Wander, FillerJob::Chat, FillerJob::Wander}, 14},
    {"The Abyss", 6, 45, 99, {FillerJob::Hunt, FillerJob::Hunt, FillerJob::Hunt}, 26},   // the hunters: they attack the hero
    {"The Abyss", 4, 40, 85, {FillerJob::Mine, FillerJob::Mine, FillerJob::Chop}, 22},   // the ones after the Abyss's riches
    {"Northwatch Tower", 1, 50, 80, {FillerJob::Chat, FillerJob::Wander, FillerJob::Chat}, 8},
};

Tier TierForCombat(int cb, bool* any) {
    *any = true;
    if (cb < 8) {
        *any = false;
        return Tier::Bronze;
    }
    if (cb < 16) return Tier::Bronze;
    if (cb < 30) return Tier::Iron;
    if (cb < 45) return Tier::Steel;
    if (cb < 65) return Tier::Mithril;
    return Tier::Adamant;
}

}  // namespace

void Adventurers::MakeFillers(const AdvWorld& w, unsigned seed) {
    const TileMap& map = *w.map;
    unsigned look = seed * 40503u + 99991u;
    int nameIx = 0;
    hubs_.clear();
    for (const Hub& hub : kHubs) {
        // the area (the last one with that name wins, as AreaAt does)
        const AreaDef* area = nullptr;
        for (const AreaDef& d : map.Areas())
            if (d.name == hub.area) area = &d;
        if (!area) continue;
        const TileRect r = area->rect;
        TileCoord centre{(r.x0 + r.x1) / 2, (r.z0 + r.z1) / 2};
        hubs_.push_back(centre);
        for (int i = 0; i < hub.count && nameIx < kAdvFillerCount; ++i, ++nameIx) {
            Adventurer a;
            a.id = nextId_++;
            a.kind = AdvKind::Filler;
            a.name = AdvFillerName(nameIx);
            a.look = RandomLook(&look, Rand() < 0.45);
            // a spot in the hub's area
            TileCoord t = centre;
            for (int tries = 0; tries < 80; ++tries) {
                const int x = r.x0 + RandInt(std::max(1, r.x1 - r.x0)), z = r.z0 + RandInt(std::max(1, r.z1 - r.z0));
                if (map.Walkable(x, z)) {
                    t = {x, z};
                    break;
                }
            }
            a.x = t.x + 0.5f;
            a.z = t.z + 0.5f;
            a.facing = static_cast<float>(Rand() * 6.28);
            // his level: spread across the hub's range, with the odd outlier so a group is never all the same level
            const int lo = hub.lo, hi = hub.hi;
            int level = lo + static_cast<int>(Rand() * (hi - lo + 1));
            if (Rand() < 0.12) level = std::clamp(level + static_cast<int>((Rand() - 0.5) * 30), 1, 99);
            a.level = level;
            FillerJob job = hub.jobs[RandInt(3)];
            a.filler.job = job;
            a.filler.hubTile = centre;
            a.filler.hubRadius = hub.radius;
            if (std::string(hub.area) == "The Abyss") {   // they live in the Abyss and stay there
                a.filler.bound = true;
                a.filler.area = hub.area;
            }
            a.filler.stateLeft = static_cast<float>(Rand() * 6.0);
            // skills: the trade he works at is his best, combat follows his level
            auto setLevel = [&](Skill s, int lv) { a.skills.SetXp(s, XpForLevel(std::clamp(lv, 1, 99))); };
            auto frac = [&](double lo2, double hi2) { return lo2 + (hi2 - lo2) * Rand(); };
            for (int s = 0; s < kSkillCount; ++s) setLevel(static_cast<Skill>(s), std::max(1, static_cast<int>(level * frac(0.25, 0.85))));
            setLevel(Skill::Attack, static_cast<int>(level * frac(0.75, 1.0)));
            setLevel(Skill::Strength, static_cast<int>(level * frac(0.75, 1.0)));
            setLevel(Skill::Defence, static_cast<int>(level * frac(0.7, 1.0)));
            setLevel(Skill::Hitpoints, std::max(10, static_cast<int>(level * frac(0.8, 1.0)) + 5));
            setLevel(Skill::Prayer, static_cast<int>(level * frac(0.2, 0.7)));
            switch (job) {
                case FillerJob::Chop: setLevel(Skill::Woodcutting, level); setLevel(Skill::Firemaking, static_cast<int>(level * 0.8)); break;
                case FillerJob::Fish: setLevel(Skill::Fishing, level); setLevel(Skill::Cooking, static_cast<int>(level * 0.8)); break;
                case FillerJob::Mine: setLevel(Skill::Mining, level); setLevel(Skill::Smithing, static_cast<int>(level * 0.7)); break;
                case FillerJob::Smith: setLevel(Skill::Smithing, level); setLevel(Skill::Mining, static_cast<int>(level * 0.8)); break;
                case FillerJob::Fight: break;
                default: setLevel(static_cast<Skill>(RandInt(kSkillCount)), level); break;
            }
            a.skills.SetHp(a.skills.MaxHp());
            // what he wears and carries follows his combat level
            const int cb = a.skills.CombatLevel();
            bool any = false;
            const Tier tier = TierForCombat(cb, &any);
            auto wear = [&](EquipSlot slot, GearType type, double chance) {
                if (!any || Rand() > chance) return;
                const Item it = GearItem(tier, type);
                if (it != Item::None) a.equip.Set(slot, it);
            };
            static const GearType kWeapons[] = {GearType::Sword, GearType::Scimitar, GearType::Longsword, GearType::Axe, GearType::Mace, GearType::Spear, GearType::Dagger,
                                                GearType::Warhammer};
            wear(EquipSlot::Weapon, kWeapons[RandInt(8)], 0.9);
            wear(EquipSlot::Shield, Rand() < 0.5 ? GearType::Shield : GearType::SquareShield, 0.55);
            wear(EquipSlot::Head, Rand() < 0.5 ? GearType::Helm : GearType::Fullhelm, 0.7);
            wear(EquipSlot::Body, Rand() < 0.5 ? GearType::Body : GearType::Chainbody, 0.75);
            wear(EquipSlot::Legs, GearType::Legs, 0.65);
            if (cb >= 20) wear(EquipSlot::Feet, GearType::Boots, 0.6);
            if (cb >= 30) wear(EquipSlot::Hands, GearType::Gauntlets, 0.45);
            if (cb >= 40 && Rand() < 0.4) {
                static const Item amulets[] = {Item::GoldAmulet, Item::SapphireAmulet, Item::EmeraldAmulet, Item::RubyAmulet, Item::DiamondAmulet};
                a.equip.Set(EquipSlot::Amulet, amulets[RandInt(5)]);
            }
            if (cb >= 75 && Rand() < 0.5 && CapeCount() > 0) a.equip.Set(EquipSlot::Cape, GetCape(RandInt(CapeCount())).item);
            // his tools (a woodcutter carries an axe, a miner a pickaxe: the renderer shows what he is holding)
            const Tier tt = any ? tier : Tier::Bronze;
            a.bag.Add(GearItem(tt, GearType::Pickaxe));
            a.bag.Add(GearItem(tt, GearType::Hatchet));
            a.bag.Add(Item::SmallNet);
            if (a.filler.bound) {   // the Abyss people carry real wealth: whoever beats them takes it
                a.bag.Add(Item::Coins, 80 + level * 45);
                a.bag.Add(Item::Shark, 2 + RandInt(4));
                if (level >= 55) a.bag.Add(Item::AdamantOre, 1 + RandInt(3));
                if (level >= 70 && Rand() < 0.6) a.bag.Add(Item::Diamond, 1 + RandInt(2));
                if (level >= 85 && Rand() < 0.4) a.bag.Add(Item::Dragonstone, 1);
                if (level >= 90 && Rand() < 0.35) a.bag.Add(Item::AbyssalAmulet, 1);
                if (level >= 70 && Rand() < 0.3) a.bag.Add(Item::AbyssalRing, 1);
                if (Rand() < 0.5) a.bag.Add(Item::ChaosRune, 10 + RandInt(30));
            }
            a.kitEquip = a.equip;
            a.kitBag = a.bag;
            advs_.push_back(std::move(a));
        }
    }
}

// ------------------------------------------------------------------------------------------------ what they do

void Adventurers::PickFillerWork(Adventurer& a, AdvWorld& w) {
    FillerMind& m = a.filler;
    TileMap& map = *w.map;
    m.working = false;
    m.heading = false;
    m.npcId = -1;
    a.act = AdvAct::Idle;
    const double r = Rand();
    const float hubR2 = static_cast<float>(m.hubRadius * m.hubRadius * 4);
    auto nearHub = [&](float x, float z) {
        const float dx = x - (m.hubTile.x + 0.5f), dz = z - (m.hubTile.z + 0.5f);
        return dx * dx + dz * dz < hubR2;
    };
    // now and then somebody moves house: a long walk to another hub (so the roads are never empty)
    if (r < 0.05 && !hubs_.empty() && !m.bound) {
        const TileCoord dest = hubs_[RandInt(static_cast<int>(hubs_.size()))];
        if (GoTo(a, w, dest, 6)) {
            m.hubTile = dest;
            m.stateLeft = 4.0f;
            return;
        }
    }
    const bool trade = m.job != FillerJob::Chat && m.job != FillerJob::Wander;
    if (trade && r < 0.80) {
        std::vector<std::pair<float, float>> spots;   // candidate things to work at
        std::vector<int> ids;
        switch (m.job) {
            case FillerJob::Chop:
                for (const TreeState& t : w.forest->Trees())
                    if (!t.stump && nearHub(t.def.x, t.def.z)) spots.push_back({t.def.x, t.def.z});
                break;
            case FillerJob::Fish:
                for (const SpotDef& s : map.Spots())
                    if (nearHub(s.x, s.z)) spots.push_back({s.x, s.z});
                break;
            case FillerJob::Mine:
                for (const RockState& rk : w.rocks->All())
                    if (!rk.depleted && nearHub(rk.def.x, rk.def.z)) spots.push_back({rk.def.x, rk.def.z});
                break;
            case FillerJob::Smith:
                for (const StationDef& s : map.Stations())
                    if (s.kind == "anvil" && nearHub(s.x, s.z)) spots.push_back({s.x, s.z});
                break;
            case FillerJob::Cook:
                for (const StationDef& s : map.Stations())
                    if (s.kind == "range" && nearHub(s.x, s.z)) spots.push_back({s.x, s.z});
                break;
            case FillerJob::Fight:
                for (const Npc& n : w.npcs->All())
                    if (n.Alive() && !n.hunting && n.engagedBy == 0 && n.Info().rig != Rig::Bird && nearHub(n.x, n.z)) {
                        spots.push_back({n.x, n.z});
                        ids.push_back(n.id);
                    }
                break;
            default: break;
        }
        if (!spots.empty()) {
            const int k = RandInt(static_cast<int>(spots.size()));
            if (m.job == FillerJob::Fight && static_cast<size_t>(k) < ids.size()) m.npcId = ids[static_cast<size_t>(k)];
            m.workX = spots[static_cast<size_t>(k)].first;
            m.workZ = spots[static_cast<size_t>(k)].second;
            if (GoTo(a, w, {static_cast<int>(m.workX), static_cast<int>(m.workZ)}, 2)) {
                m.heading = true;
                m.stateLeft = 400.0f;   // (long enough to get there; reset on arrival)
                return;
            }
        }
    }
    // otherwise: wander a little about the hub and stand around (and talk, if anybody is near)
    for (int tries = 0; tries < 30; ++tries) {
        const int x = m.hubTile.x + RandInt(m.hubRadius * 2 + 1) - m.hubRadius, z = m.hubTile.z + RandInt(m.hubRadius * 2 + 1) - m.hubRadius;
        if (!map.Walkable(x, z)) continue;
        if (m.bound && map.AreaAt(x, z) != m.area) continue;
        if (GoTo(a, w, {x, z}, 1)) break;
    }
    m.stateLeft = 7.0f + static_cast<float>(Rand() * 22.0);
}

void Adventurers::UpdateFiller(Adventurer& a, float dt, AdvWorld& w) {
    FillerMind& m = a.filler;
    m.stateLeft -= dt;
    if (m.job == FillerJob::Hunt || a.pvp.provoked > 0.0f || a.pvp.fightingHero || a.pvp.foe != 0 || a.pvp.beast != 0) {
        if (PvpUpdate(a, dt, w)) return;
    }
    if (!a.path.empty()) return;   // walking (Walk() has made the legs move)
    if (m.heading) {               // arrived at the spot he was heading for: work there for a while
        m.heading = false;
        m.working = true;
        m.stateLeft = 25.0f + static_cast<float>(Rand() * 80.0);
        m.flinchLeft = 1.0f;
    }
    if (m.working) {
        Npc* n = m.job == FillerJob::Fight && m.npcId >= 0 ? w.npcs->Find(m.npcId) : nullptr;
        if (m.job == FillerJob::Fight && (!n || n->dead || n->hunting)) {   // his creature is gone: find another
            m.stateLeft = 0.0f;
        }
        if (n && !n->dead) {
            m.workX = n->x;
            m.workZ = n->z;
            // he stands right next to it: if it wandered off, walk after it
            const float dx = m.workX - a.x, dz = m.workZ - a.z;
            if (dx * dx + dz * dz > 3.2f * 3.2f) {
                GoTo(a, w, {static_cast<int>(m.workX), static_cast<int>(m.workZ)}, 1);
                return;
            }
            w.npcs->Engage(n->id, a.id, a.x, a.z);
            m.flinchLeft -= dt;
            if (m.flinchLeft <= 0.0f) {   // a blow lands now and then, so the fight looks like one (the creature never dies)
                m.flinchLeft = 1.2f + static_cast<float>(Rand() * 1.8);
                a.strikeAge = 0.0f;
                a.strikeLeft = !a.strikeLeft;
                if (Rand() < 0.5) n->hurtT = 0.0f;
                else n->attackT = 0.0f;
            }
        }
        const float dx = m.workX - a.x, dz = m.workZ - a.z;
        if (dx * dx + dz * dz > 1e-4f) a.facing = std::atan2(dx, dz);
        a.workX = m.workX;
        a.workZ = m.workZ;
        switch (m.job) {
            case FillerJob::Chop: a.act = AdvAct::Chop; break;
            case FillerJob::Fish: a.act = AdvAct::Fish; break;
            case FillerJob::Mine: a.act = AdvAct::Mine; a.workPhase = std::fmod(a.actTime / kMineSwingSeconds, 1.0f); break;
            case FillerJob::Smith: a.act = AdvAct::Smith; a.workPhase = std::fmod(a.actTime / 0.75f, 1.0f); break;
            case FillerJob::Cook: a.act = AdvAct::Cook; break;
            case FillerJob::Fight: a.act = AdvAct::Fight; break;
            default: a.act = AdvAct::Idle; break;
        }
        if (m.stateLeft <= 0.0f) PickFillerWork(a, w);
        return;
    }
    if (m.stateLeft <= 0.0f) {
        PickFillerWork(a, w);
        return;
    }
    a.act = AdvAct::Chat;   // just standing about
}

}  // namespace journey
