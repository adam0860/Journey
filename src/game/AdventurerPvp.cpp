// The Abyss fights: players as opponents. See Foes.h (the rules) and Adventurers.h.
//
// Everybody in the Abyss (the hunters, the real adventurers on an expedition, the miners and woodcutters who live there) fights by the hero's rules:
// a blow is rolled like any other, a skull goes to whoever strikes first at somebody not skulled, and, as for the hero, the DEEPER the Abyss, the
// wider the gap in combat level two players may fight across (Foes.h: AbyssLevelAt, CanFightPlayers). Whoever dies drops what they carry (everything,
// if skulled) where they fall, and whoever is near can take it.
#include <algorithm>
#include <cmath>

#include "game/Adventurers.h"
#include "game/Gear.h"
#include "world/Pathfinder.h"

namespace journey {

Fighter Adventurers::FighterOf(const Adventurer& a) const {
    const EquipBonuses b = a.equip.Total();
    Fighter f = PlayerFighter(a.skills, a.style, b.attack, b.strength, b.defence);
    f.swingScale = static_cast<float>(WeaponTicks(a.equip.Get(EquipSlot::Weapon))) / static_cast<float>(kDefaultSwingTicks);
    return f;
}

Adventurer* Adventurers::FindMutable(int id) {
    for (Adventurer& a : advs_)
        if (a.id == id) return &a;
    return nullptr;
}

bool Adventurers::Get(int foeId, FoeView* out) const {
    const Adventurer* a = Find(foeId - kFoeIdBase);
    if (!a || !out) return false;
    out->id = foeId;
    out->name = a->name;
    out->x = a->x;
    out->z = a->z;
    out->hp = a->skills.Hp();
    out->maxHp = a->skills.MaxHp();
    out->combat = a->Combat();
    out->skulled = a->pvp.skull > 0.0f;
    out->alive = !a->pvp.dead && a->skills.Hp() > 0;
    out->fighter = FighterOf(*a);
    return true;
}

bool Adventurers::HeroHits(int foeId, int damage) {
    Adventurer* a = FindMutable(foeId - kFoeIdBase);
    if (!a || a->pvp.dead || !world_) return false;
    a->pvp.provoked = 30.0f;   // he fights back (and when he hits back he earns no skull)
    a->pvp.provokedBy = -1;
    if (damage > 0) {
        a->skills.Hurt(damage);
        a->hurtAge = 0.0f;
    }
    if (a->skills.Hp() > 0) return false;
    PvpKill(*a, *world_, -1);
    return true;
}

void Adventurers::HeroKilledBy(int foeId) {
    if (Adventurer* k = FindMutable(foeId - kFoeIdBase)) ++k->pkKills;
}

std::vector<PkRank> Adventurers::PkRanking(const std::string& heroName, int heroKills, int heroDeaths) const {
    std::vector<PkRank> r;
    for (const Adventurer& a : advs_)
        if (a.pkKills > 0 || a.pkDeaths > 0) r.push_back({a.name, a.pkKills, a.pkDeaths, false});
    if (heroKills > 0 || heroDeaths > 0) r.push_back({heroName, heroKills, heroDeaths, true});
    std::stable_sort(r.begin(), r.end(), [](const PkRank& x, const PkRank& y) { return x.kills != y.kills ? x.kills > y.kills : x.deaths < y.deaths; });
    return r;
}

// What he carries falls where he stands: everything if skulled, else all but the three most valuable things.
void Adventurers::PvpDrop(Adventurer& a, AdvWorld& w) {
    struct Entry {
        Item item;
        int count;
        int worn;   // -1 = in the bag, otherwise the EquipSlot
    };
    std::vector<Entry> all;
    for (int s = 0; s < kEquipSlotCount; ++s) {
        const Item it = a.equip.Get(static_cast<EquipSlot>(s));
        if (it == Item::None) continue;
        all.push_back({it, static_cast<EquipSlot>(s) == EquipSlot::Ammo ? std::max(1, a.equip.AmmoCount()) : 1, s});
    }
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemStack& st = a.bag.Slot(i);
        if (st.item != Item::None) all.push_back({st.item, st.count, -1});
    }
    std::stable_sort(all.begin(), all.end(), [](const Entry& x, const Entry& y) { return ItemValue(x.item) > ItemValue(y.item); });
    const size_t keep = a.pvp.skull > 0.0f ? 0 : static_cast<size_t>(kItemsKeptOnDeath);
    for (size_t i = keep; i < all.size(); ++i) {
        const Entry& e = all[i];
        if (e.worn >= 0) {
            if (static_cast<EquipSlot>(e.worn) == EquipSlot::Ammo) a.equip.SetAmmo(e.item, 0);
            else a.equip.Set(static_cast<EquipSlot>(e.worn), Item::None);
        } else {
            a.bag.Remove(e.item, e.count);
        }
        if (w.ground) w.ground->Drop(a.Tile(), {e.item, e.count}, GroundItems::kDeathLifeSeconds);
    }
}

// A death in a fight: a real adventurer wakes up in town (with what he kept), a filler is gone for a while.
void Adventurers::PvpKill(Adventurer& a, AdvWorld& w, int killer) {
    if (killer != 0) {   // killed by a player (not by a creature): his death and their kill go on the record
        ++a.pkDeaths;
        if (killer > 0)
            if (Adventurer* k = FindMutable(killer)) ++k->pkKills;
    }
    PvpDrop(a, w);
    a.pvp.skull = 0.0f;
    a.pvp.provoked = 0.0f;
    a.pvp.fightingHero = false;
    a.pvp.foe = 0;
    if (a.kind == AdvKind::Real) {
        Die(a, w);
        return;
    }
    a.pvp.dead = true;
    a.pvp.respawn = 240.0f;
    a.path.clear();
    a.act = AdvAct::Idle;
    a.chatPartner = -1;
    a.speech.text.clear();
}

// Back at home with the same gear as ever (fillers never change).
void Adventurers::PvpRespawn(Adventurer& a, const AdvWorld& w) {
    a.equip = a.kitEquip;
    a.bag = a.kitBag;
    a.skills.SetHp(a.skills.MaxHp());
    a.pvp = AdvPvp{};
    TileCoord t = a.filler.hubTile;
    for (int tries = 0; tries < 60; ++tries) {
        const int x = a.filler.hubTile.x + RandInt(11) - 5, z = a.filler.hubTile.z + RandInt(11) - 5;
        if (w.map->Walkable(x, z) && (!a.filler.bound || w.map->AreaAt(x, z) == a.filler.area)) {
            t = {x, z};
            break;
        }
    }
    a.x = t.x + 0.5f;
    a.z = t.z + 0.5f;
    a.path.clear();
    a.filler.working = a.filler.heading = false;
    a.filler.stateLeft = 3.0f;
    a.act = AdvAct::Idle;
}

namespace {
struct FoeT {   // a resolved opponent: the hero, or another adventurer
    bool ok = false;
    bool hero = false;
    Adventurer* adv = nullptr;
    float x = 0, z = 0;
    int combat = 1, level = 0;
    bool skulled = false;
};
}  // namespace

// What the dead left lying about (a dying player's pile stays five minutes: ordinary drops, which last a minute, belong to whoever killed the creature) is
// theirs to take. True while walking to it or taking it.
bool Adventurers::Scavenge(Adventurer& a, float dt, AdvWorld& w) {
    AdvPvp& p = a.pvp;
    if (!w.ground) return false;
    p.lootT -= dt;
    if (p.lootT > 0.0f) return !a.path.empty() && a.act == AdvAct::Walk;
    p.lootT = 0.8f;
    const Pile* best = nullptr;
    float bestD = 15.0f * 15.0f;
    for (const Pile& pl : w.ground->All()) {
        bool old = false;
        for (float life : pl.lives) old = old || life > 100.0f;
        if (!old || AbyssLevelAt(*w.map, pl.tile.x, pl.tile.z) <= 0) continue;
        const float ex = pl.tile.x + 0.5f - a.x, ez = pl.tile.z + 0.5f - a.z;
        if (ex * ex + ez * ez < bestD) {
            bestD = ex * ex + ez * ez;
            best = &pl;
        }
    }
    if (!best) return false;
    const int id = best->id;
    const TileCoord at = best->tile;
    const TileCoord me = a.Tile();
    if (std::max(std::abs(at.x - me.x), std::abs(at.z - me.z)) <= 1) {
        const std::vector<ItemStack> taken = w.ground->TakeAll(id, &a.bag);
        if (a.kind == AdvKind::Real) {
            for (const ItemStack& s : taken) a.source[s.item] |= kSrcDrop;
            ManageGear(a);
        }
        if (!taken.empty() && a.speech.text.empty() && Rand() < 0.5) Say(a, "Mine now.", 3.0f);
        a.path.clear();
        a.filler.stateLeft = 0.0f;
        return false;
    }
    if (a.path.empty()) {
        GoTo(a, w, at, 1);
        a.filler.working = a.filler.heading = false;
        a.filler.stateLeft = 30.0f;   // (so a filler's usual wandering waits)
        a.act = AdvAct::Walk;
    }
    return !a.path.empty();
}

// One blow at the opponent, and what follows from it.
void Adventurers::PvpBlow(Adventurer& a, void* target, AdvWorld& w) {
    const FoeT& t = *static_cast<const FoeT*>(target);
    AdvPvp& p = a.pvp;
    p.swingT = static_cast<float>(WeaponTicks(a.equip.Get(EquipSlot::Weapon))) * kTickSeconds;
    const bool fresh = t.hero ? !w.heroSkulled : (t.adv->pvp.skull <= 0.0f);
    const int key = t.hero ? -1 : t.adv->id;
    const bool theyStartedIt = p.provoked > 0.0f && p.provokedBy == key;
    if (fresh && !theyStartedIt) p.skull = kSkullSeconds;   // the first blow at somebody not skulled who did not start it
    const Fighter defender = t.hero ? w.heroFighter : FighterOf(*t.adv);
    const AttackResult r = RollAttack(FighterOf(a), defender, w.roll);
    a.strikeAge = 0.0f;
    a.strikeLeft = !a.strikeLeft;
    if (t.hero) {
        if (w.hitHero) w.hitHero(kFoeIdBase + a.id, r.damage);
        return;
    }
    Adventurer& b = *t.adv;
    b.pvp.provoked = 30.0f;
    b.pvp.provokedBy = a.id;
    if (r.damage > 0) {
        b.skills.Hurt(r.damage);
        b.hurtAge = 0.0f;
        AwardFightXp(a, r.damage);   // a fight trains him like any other
    }
    if (b.skills.Hp() <= 0) {
        PvpKill(b, w, a.id);
        p.foe = 0;
        a.act = AdvAct::Idle;
    }
}

// A real adventurer's fights train him like any other.
void Adventurers::AwardFightXp(Adventurer& a, int damage) {
    if (a.kind != AdvKind::Real || damage <= 0) return;
    const CombatXp xp = XpForDamage(a.style, damage);
    GainXp(a, Skill::Attack, xp.attack);
    GainXp(a, Skill::Strength, xp.strength);
    GainXp(a, Skill::Defence, xp.defence);
    a.mind.hpXpCarry += damage * 4.0 / 3.0;
    const int hpXp = static_cast<int>(a.mind.hpXpCarry + 1e-9);
    a.mind.hpXpCarry -= hpXp;
    GainXp(a, Skill::Hitpoints, hpXp);
}

namespace {
Fighter CreatureFighter(const Npc& n) {
    const SpeciesInfo& si = n.Info();
    Fighter c;
    c.attack = si.attack;
    c.strength = si.strength;
    c.defence = si.defence;
    c.creature = true;
    return c;
}
int ChebTiles(TileCoord a, TileCoord b) { return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z)); }
}  // namespace

// Would a real adventurer take this creature on? (not far above his level, not a thing that kills him in three blows, and he is in good health)
bool Adventurers::BeastOk(const Adventurer& a, const Npc& n) const {
    const SpeciesInfo& si = n.Info();
    if (n.dead || n.hunting || n.engagedBy != 0 || n.spawnT < 1.0f) return false;
    const int cb = a.Combat();
    if (si.level > cb + 12) return false;
    if (si.attackKind != AttackKind::Melee && cb < 30) return false;   // shooters and casters hit from afar
    if (std::max(1, MaxHit(CreatureFighter(n))) * 3 > a.skills.MaxHp()) return false;
    return a.skills.Hp() * 100 >= a.skills.MaxHp() * 60;
}

// A player's blow at a creature: damage, experience, and (a kill) its loot, which is his.
void Adventurers::BeastBlow(Adventurer& a, Npc& n, AdvWorld& w) {
    AdvPvp& p = a.pvp;
    p.swingT = static_cast<float>(WeaponTicks(a.equip.Get(EquipSlot::Weapon))) * kTickSeconds;
    const AttackResult r = RollAttack(FighterOf(a), CreatureFighter(n), w.roll);
    a.strikeAge = 0.0f;
    a.strikeLeft = !a.strikeLeft;
    if (r.damage <= 0) return;
    AwardFightXp(a, r.damage);
    const Species sp = n.species;
    if (!w.npcs->Hurt(n.id, r.damage)) return;
    if (Npc* dead = w.npcs->Find(n.id)) dead->lootPending = false;   // (the loot is his, not a pile on the ground)
    for (const ItemStack& s : RollLoot(sp, w.roll)) {
        if (a.kind == AdvKind::Real) Gain(a, s.item, s.count, kSrcDrop);
        else a.bag.Add(s.item, s.count);
    }
    if (a.kind == AdvKind::Real) {
        const int qi = a.quests.OnKill(sp, AttackKind::Melee);
        if (qi >= 0) OnQuestStepDone(a, w, qi);
        ManageGear(a);
    }
    p.beast = 0;
}

// The Abyss creatures go for the players who come within their reach, as they do for the hero (a creature that is already after the hero leaves them
// alone): they walk up and strike, and the player fights back. A real adventurer who is out hunting asks for a fight (PvpUpdate), and gets it.
void Adventurers::UpdateBeasts(float dt, AdvWorld& w) {
    if (!w.npcs || !w.map) return;
    const TileMap& map = *w.map;
    TileRect abyss{0, 0, 0, 0};
    for (const AreaDef& d : map.Areas())
        if (d.name == "The Abyss") abyss = d.rect;
    if (abyss.x1 <= abyss.x0) return;
    // the fights going on
    for (auto it = beasts_.begin(); it != beasts_.end();) {
        Npc* n = w.npcs->Find(it->first);
        Adventurer* ad = FindMutable(it->second.adv);
        BeastFight& bf = it->second;
        bf.left -= dt;
        bool ok = n && ad && !n->dead && !n->hunting && !ad->pvp.dead && ad->skills.Hp() > 0 && AbyssLevelAt(map, ad->Tile().x, ad->Tile().z) > 0 && bf.left > 0.0f;
        int d = 0;
        if (ok) {
            d = ChebTiles(n->Tile(), ad->Tile());
            ok = d <= std::max(9, n->Info().aggroRange * 3);
        }
        if (!ok) {
            if (ad && ad->pvp.beast == it->first) ad->pvp.beast = 0;
            it = beasts_.erase(it);
            continue;
        }
        const SpeciesInfo& si = n->Info();
        w.npcs->ChaseTile(n->id, ad->Tile(), 0.6f);
        const bool reach = d <= 1 || (si.attackKind != AttackKind::Melee && si.attackRange > 1 && d <= si.attackRange && map.ProjectileClear(n->Tile(), ad->Tile()));
        if (reach) {
            bf.swingT -= dt;
            if (bf.swingT <= 0.0f) {
                bf.swingT = kCreatureSwingSeconds;
                const AttackResult r = RollAttack(CreatureFighter(*n), FighterOf(*ad), w.roll);
                n->attackT = 0.0f;
                n->facing = std::atan2(ad->x - n->x, ad->z - n->z);
                if (r.damage > 0) {
                    ad->skills.Hurt(r.damage);
                    ad->hurtAge = 0.0f;
                }
                if (ad->skills.Hp() <= 0) {
                    PvpKill(*ad, w);
                    it = beasts_.erase(it);
                    continue;
                }
            }
        }
        ++it;
    }
    // new ones: a creature in the Abyss and a player within its reach (or one who is after it)
    for (Npc& n : w.npcs->All()) {
        const SpeciesInfo& si = n.Info();
        if (n.dead || !si.aggressive || n.hunting || n.spawnT < 1.0f || beasts_.count(n.id)) continue;
        if (n.x < abyss.x0 || n.x >= abyss.x1 || n.z < abyss.z0 || n.z >= abyss.z1) continue;
        Adventurer* pick = nullptr;
        int bestD = 1 << 20;
        for (Adventurer& o : advs_) {
            if (o.pvp.dead || o.skills.Hp() <= 0 || o.pvp.cooldown > 0.0f) continue;
            if (AbyssLevelAt(map, o.Tile().x, o.Tile().z) <= 0) continue;
            const int d = ChebTiles(o.Tile(), n.Tile());
            const bool asked = o.pvp.beast == n.id;
            if (!asked && (d > si.aggroRange || o.Combat() >= si.level * 2 + 5)) continue;   // (as for the hero: too strong to bother with)
            if (asked || d < bestD) {
                pick = &o;
                bestD = d;
                if (asked) break;
            }
        }
        if (!pick) continue;
        BeastFight bf;
        bf.adv = pick->id;
        beasts_[n.id] = bf;
        if (pick->pvp.beast == 0 && !pick->pvp.fightingHero && pick->pvp.foe == 0) pick->pvp.beast = n.id;
    }
}

// Fighting, picking fights and scavenging in the Abyss. True when this took the frame (a fight, or a walk to loot).
bool Adventurers::PvpUpdate(Adventurer& a, float dt, AdvWorld& w) {
    AdvPvp& p = a.pvp;
    p.provoked = std::max(0.0f, p.provoked - dt);
    p.cooldown = std::max(0.0f, p.cooldown - dt);
    const TileMap& map = *w.map;
    const int here = AbyssLevelAt(map, a.Tile().x, a.Tile().z);
    if (here <= 0) {   // outside the Abyss nobody fights
        p.fightingHero = false;
        p.foe = 0;
        p.beast = 0;
        return false;
    }
    FillerMind& m = a.filler;
    const bool hunter = a.kind == AdvKind::Filler && m.job == FillerJob::Hunt;
    const bool initiator = hunter || (a.kind == AdvKind::Real && p.pk);
    const int myCombat = a.Combat();

    auto resolve = [&](int key, FoeT* t) {   // key -1 = the hero, otherwise an adventurer id
        *t = FoeT{};
        if (key == -1) {
            if (!w.heroAlive) return false;
            const int lv = AbyssLevelAt(map, static_cast<int>(w.heroX), static_cast<int>(w.heroZ));
            if (lv <= 0) return false;
            t->ok = t->hero = true;
            t->x = w.heroX;
            t->z = w.heroZ;
            t->combat = w.heroCombat;
            t->level = lv;
            t->skulled = w.heroSkulled;
            return true;
        }
        Adventurer* b = key > 0 ? FindMutable(key) : nullptr;
        if (!b || b == &a || b->pvp.dead || b->skills.Hp() <= 0) return false;
        const int lv = AbyssLevelAt(map, b->Tile().x, b->Tile().z);
        if (lv <= 0) return false;
        t->ok = true;
        t->adv = b;
        t->x = b->x;
        t->z = b->z;
        t->combat = b->Combat();
        t->level = lv;
        t->skulled = b->pvp.skull > 0.0f;
        return true;
    };
    auto distTo = [&](const FoeT& t) { return std::sqrt((t.x - a.x) * (t.x - a.x) + (t.z - a.z) * (t.z - a.z)); };
    auto allowed = [&](const FoeT& t) { return CanFightPlayers(myCombat, t.combat, here, t.level); };

    // choosing a player to fight: whoever started it, or (for those who pick fights) somebody within the gap of levels. Players come before creatures.
    auto choosePlayer = [&]() -> bool {
        if (p.cooldown > 0.0f) return false;
        FoeT pick;
        float bestD = 1e9f;
        const Adventurer* mate = PartnerOf(a);
        auto consider = [&](int key, float maxDist, bool needAllowedTarget) {
            FoeT t;
            if (mate && key == mate->id) return;   // (never his companion)
            if (!resolve(key, &t) || !allowed(t)) return;
            const float d = distTo(t);
            if (d > maxDist || d >= bestD) return;
            if (needAllowedTarget && !t.hero && a.kind == AdvKind::Real && t.combat > myCombat + 10) return;   // (a player killer is not suicidal)
            bestD = d;
            pick = t;
        };
        if (p.provoked > 0.0f && p.provokedBy != 0) consider(p.provokedBy, 16.0f, false);
        if (!pick.ok && mate) {   // a hand with his companion's fight
            if (mate->pvp.fightingHero) consider(-1, 16.0f, false);
            else if (mate->pvp.foe != 0) consider(mate->pvp.foe, 16.0f, false);
        }
        if (!pick.ok && initiator) {
            const float reach = 9.0f;
            if (w.heroAlive && w.heroInDanger) consider(-1, reach, true);
            for (const Adventurer& o : advs_) {
                if (&o == &a || o.pvp.dead) continue;
                if (hunter && o.kind != AdvKind::Real) continue;   // (the hunters prey on real adventurers and the hero, not on their own kind)
                if (AbyssLevelAt(map, o.Tile().x, o.Tile().z) <= 0) continue;
                consider(o.id, reach, true);
            }
        }
        if (pick.ok) {
            p.fightingHero = pick.hero;
            p.foe = pick.hero ? 0 : pick.adv->id;
            p.swingT = 0.9f;
            p.chaseT = 0.0f;
            a.path.clear();
            m.working = m.heading = false;
            if (hunter && a.speech.text.empty() && Rand() < 0.5) Say(a, Rand() < 0.5 ? "Fresh meat." : "Nowhere to run.", 3.0f);
            return true;
        }
        return false;
    };

    // ---- a fight with an Abyss creature (it came for him, or he went for it)
    if (p.beast != 0 && !p.fightingHero && p.foe == 0) {
        if ((initiator || p.provoked > 0.0f) && choosePlayer()) {   // a player comes before a creature
            p.beast = 0;
            return true;
        }
        Npc* n = w.npcs->Find(p.beast);
        const bool ok = n && !n->dead && !n->hunting && AbyssLevelAt(map, n->Tile().x, n->Tile().z) > 0 && ChebTiles(a.Tile(), n->Tile()) <= 14;
        if (!ok) {
            p.beast = 0;
            a.act = AdvAct::Idle;
            m.stateLeft = 0.0f;
            return false;
        }
        if (ChebTiles(a.Tile(), n->Tile()) > 1) {   // it keeps its distance (a shooter or a caster): go to it
            p.chaseT -= dt;
            if (p.chaseT <= 0.0f || a.path.empty()) {
                p.chaseT = 0.5f;
                GoTo(a, w, n->Tile(), 1);
            }
            a.act = AdvAct::Walk;
            return true;
        }
        a.path.clear();
        a.facing = std::atan2(n->x - a.x, n->z - a.z);
        a.act = AdvAct::Fight;
        p.swingT -= dt;
        if (p.swingT <= 0.0f) BeastBlow(a, *n, w);
        return true;
    }

    // ---- a fight in progress
    if (p.fightingHero || p.foe != 0) {
        FoeT t;
        const bool have = resolve(p.fightingHero ? -1 : p.foe, &t);
        const float d = have ? distTo(t) : 1e9f;
        if (!have || !allowed(t) || d > (p.provoked > 0.0f ? 24.0f : 16.0f) || (!initiator && p.provoked <= 0.0f)) {   // gone, out of range of levels, or over
            p.fightingHero = false;
            p.foe = 0;
            a.act = AdvAct::Idle;
            m.stateLeft = 0.0f;
            return false;
        }
        const TileCoord me = a.Tile();
        const int cheb = std::max(std::abs(static_cast<int>(t.x) - me.x), std::abs(static_cast<int>(t.z) - me.z));
        if (cheb > 1) {   // chase
            p.chaseT -= dt;
            if (p.chaseT <= 0.0f || a.path.empty()) {
                p.chaseT = 0.5f;
                GoTo(a, w, {static_cast<int>(t.x), static_cast<int>(t.z)}, 1);
            }
            a.act = AdvAct::Walk;
            return true;
        }
        a.path.clear();
        a.facing = std::atan2(t.x - a.x, t.z - a.z);
        a.act = AdvAct::Fight;
        p.swingT -= dt;
        if (p.swingT <= 0.0f) PvpBlow(a, &t, w);
        return true;
    }

    if (choosePlayer()) return true;
    // a real adventurer out hunting in the Abyss takes on a creature he can beat (or the one his companion is fighting)
    if (a.kind == AdvKind::Real && p.cooldown <= 0.0f && a.mind.hasGoal && a.mind.goal.task == AdvTask::Abyss) {
        if (const Adventurer* mate = PartnerOf(a)) {
            Npc* theirs = mate->pvp.beast != 0 ? w.npcs->Find(mate->pvp.beast) : nullptr;
            if (theirs && !theirs->dead && !theirs->hunting && ChebTiles(a.Tile(), theirs->Tile()) <= 12 && a.skills.Hp() * 100 >= a.skills.MaxHp() * 50) {
                p.beast = theirs->id;
                p.swingT = 0.9f;
                p.chaseT = 0.0f;
                a.path.clear();
                return true;
            }
        }
        Npc* best = nullptr;
        int bestD = 9;
        for (Npc& n : w.npcs->All()) {
            if (!n.Info().aggressive || !BeastOk(a, n)) continue;
            const int d = ChebTiles(a.Tile(), n.Tile());
            if (d < bestD && AbyssLevelAt(map, n.Tile().x, n.Tile().z) > 0) {
                bestD = d;
                best = &n;
            }
        }
        if (best) {
            p.beast = best->id;
            p.swingT = 0.9f;
            p.chaseT = 0.0f;
            a.path.clear();
            return true;
        }
    }
    if (hunter) return Scavenge(a, dt, w);
    return false;
}

}  // namespace journey
