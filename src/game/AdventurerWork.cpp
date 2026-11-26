// What a real adventurer does with his hands: walking to a job, working at it, fighting, trading, handing in quests. See Adventurers.h.
#include <algorithm>
#include <cmath>

#include "game/AdventurerUtil.h"
#include "game/Cooking.h"
#include "game/Skilling.h"

namespace journey {

using namespace advutil;

namespace {

constexpr float kBurySeconds = 1.8f;

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// ------------------------------------------------------------------------------------------------ owning things

void Adventurers::Gain(Adventurer& a, Item item, int count, unsigned src) {
    if (count <= 0 || item == Item::None) return;
    const int put = a.bag.Add(item, count);
    if (put > 0) a.source[item] |= src;
}

void Adventurers::GainXp(Adventurer& a, Skill s, int amount) {
    if (amount <= 0) return;
    int level = 0;
    if (a.skills.AddXp(s, amount, &level)) {
        if (level >= 3 && (level % 5 == 0 || level <= 10 || Rand() < 0.25) && Rand() < 0.7)
            Say(a, std::string("Level ") + std::to_string(level) + " " + SkillName(s) + "!", 5.0f, true);
    }
}

bool Adventurers::EatNow(Adventurer& a) {
    const int slot = BestFoodSlot(a.bag);
    if (slot < 0) return false;
    const Item it = a.bag.Slot(slot).item;
    a.bag.Remove(it, 1);
    a.skills.Heal(GetItemInfo(it).heals);
    return true;
}

// Wears the best of what he carries (a better piece goes on and the old one back in the bag).
void Adventurers::ManageGear(Adventurer& a) {
    for (int pass = 0; pass < 12; ++pass) {
        bool changed = false;
        for (int i = 0; i < Inventory::kSlots && !changed; ++i) {
            const Item it = a.bag.Slot(i).item;
            if (it == Item::None || !IsWearable(it)) continue;
            EquipSlot slot;
            if (!Equippable(it, &slot)) continue;
            Skill rs;
            int rl;
            if (ItemRequirement(it, &rs, &rl) && a.skills.Level(rs) < rl) continue;
            const Item cur = a.equip.Get(slot);
            const float now = cur == Item::None ? 0.0f : GearScore(cur);
            if (GearScore(it) <= now + 0.5f) continue;
            if (EquipFromBag(&a.equip, &a.bag, i, a.skills) == EquipResult::Done) {
                changed = true;
                if (GearScore(it) - now >= 6.0f && Rand() < 0.5) Say(a, "Got myself a new " + Lower(GetItemInfo(it).name) + "!", 5.0f);
            }
        }
        if (!changed) break;
    }
}

void Adventurers::Die(Adventurer& a, AdvWorld& w) {
    (void)w;
    AdvMind& m = a.mind;
    ++m.deaths;
    Say(a, "Oh dear, I am dead!", 5.0f, true);
    const TileCoord t = TownNear(a.Tile());
    a.x = t.x + 0.5f;
    a.z = t.z + 0.5f;
    a.path.clear();
    a.skills.SetHp(a.skills.MaxHp());
    m.queue.clear();
    m.hasGoal = false;
    m.phase = AdvPhase::Plan;
    m.thinkLeft = 4.0f;
    m.chop.Stop();
    m.mine.Stop();
    m.fish.Stop();
    m.light.Stop();
    m.buryLeft = 0;
    m.fleeing = false;
    m.wasFighting = false;
    m.partner = 0;
    m.follower = false;
    a.pvp = AdvPvp{};
    a.act = AdvAct::Idle;
}

// ------------------------------------------------------------------------------------------------ fighting

void Adventurers::BotStrike(Adventurer& a, Npc& n, AdvWorld& w) {
    AdvMind& m = a.mind;
    const EquipBonuses b = a.equip.Total();
    Fighter f = PlayerFighter(a.skills, a.style, b.attack, b.strength, b.defence);
    f.swingScale = static_cast<float>(WeaponTicks(a.equip.Get(EquipSlot::Weapon))) / static_cast<float>(kDefaultSwingTicks);
    const SpeciesInfo& si = n.Info();
    Fighter c;
    c.attack = si.attack;
    c.strength = si.strength;
    c.defence = si.defence;
    c.creature = true;
    const AttackResult r = RollAttack(f, c, w.roll);
    a.strikeAge = 0.0f;
    a.strikeLeft = !a.strikeLeft;
    if (r.damage <= 0) return;
    const CombatXp xp = XpForDamage(a.style, r.damage);
    GainXp(a, Skill::Attack, xp.attack);
    GainXp(a, Skill::Strength, xp.strength);
    GainXp(a, Skill::Defence, xp.defence);
    m.hpXpCarry += r.damage * 4.0 / 3.0;
    const int hpXp = static_cast<int>(m.hpXpCarry + 1e-9);
    m.hpXpCarry -= hpXp;
    GainXp(a, Skill::Hitpoints, hpXp);
    const Species sp = n.species;
    if (w.npcs->Hurt(n.id, r.damage)) {
        Npc* dead = w.npcs->Find(n.id);
        if (dead) dead->lootPending = false;   // (the loot is his, not a pile on the ground)
        for (const ItemStack& s : RollLoot(sp, w.roll)) Gain(a, s.item, s.count, kSrcDrop);
        ++m.goal.got;
        const int qi = a.quests.OnKill(sp, AttackKind::Melee);
        if (qi >= 0) OnQuestStepDone(a, w, qi);
        for (int i = 0; i < Inventory::kSlots; ++i) {   // bones to bury
            const Item it = a.bag.Slot(i).item;
            if (it != Item::None && GetItemInfo(it).buryXp > 0 && m.buryLeft <= 0.0f) {
                m.burying = it;
                m.buryLeft = kBurySeconds;
                break;
            }
        }
        m.sub = 0;
        ManageGear(a);
    }
}

void Adventurers::CreatureStrike(Adventurer& a, Npc& n, AdvWorld& w) {
    const SpeciesInfo& si = n.Info();
    Fighter c;
    c.attack = si.attack;
    c.strength = si.strength;
    c.defence = si.defence;
    c.creature = true;
    const EquipBonuses b = a.equip.Total();
    const Fighter me = PlayerFighter(a.skills, a.style, b.attack, b.strength, b.defence);
    const AttackResult r = RollAttack(c, me, w.roll);
    n.attackT = 0.0f;
    n.facing = std::atan2(a.x - n.x, a.z - n.z);
    if (r.damage > 0) {
        a.skills.Hurt(r.damage);
        a.hurtAge = 0.0f;
    }
}

// ------------------------------------------------------------------------------------------------ goals: start, work, end

bool Adventurers::StartGoal(Adventurer& a, AdvWorld& w) {
    AdvMind& m = a.mind;
    AdvGoal& g = m.goal;
    m.phase = AdvPhase::Travel;
    m.target = -1;
    m.sub = 0;
    m.timer = 0;
    m.travelLeft = 480.0f;
    m.swingT = m.creatureT = 0;
    m.chop.Stop();
    m.mine.Stop();
    m.fish.Stop();
    m.light.Stop();
    TileMap& map = *w.map;
    auto setDoing = [&](const std::string& s) {
        if (g.quest < 0) m.doing = s;
    };
    switch (g.task) {
        case AdvTask::Rest: {
            TileCoord t = TownNear(a.Tile());
            t.x += RandInt(5) - 2;
            t.z += RandInt(5) - 2;
            if (!GoTo(a, w, t, 3)) return false;
            m.doing = g.quest >= 0 ? m.doing : "resting";
            return true;
        }
        case AdvTask::Chop: {
            const int idx = NearestTree(a, w, g.key);
            if (idx < 0) return false;
            m.target = idx;
            if (!GoTo(a, w, w.forest->Trees()[static_cast<size_t>(idx)].def.Tile(), 1)) {
                m.avoidTree[idx] = clock_ + 120.0f;
                return false;
            }
            setDoing("chopping " + Lower(GetTreeInfo(g.key).name) + "s");
            return true;
        }
        case AdvTask::Fish: {
            const int idx = NearestSpot(a, w, g.key);
            if (idx < 0) return false;
            m.target = idx;
            if (!GoTo(a, w, map.Spots()[static_cast<size_t>(idx)].Tile(), 2)) return false;
            setDoing(std::string("fishing (") + GetSpotInfo(g.key).verb + ")");
            return true;
        }
        case AdvTask::Mine: {
            const int idx = NearestRock(a, w, g.key);
            if (idx < 0) return false;
            m.target = idx;
            if (!GoTo(a, w, w.rocks->All()[static_cast<size_t>(idx)].def.Tile(), 1)) {
                m.avoidRock[idx] = clock_ + 120.0f;
                return false;
            }
            setDoing("mining " + g.key);
            return true;
        }
        case AdvTask::Burn: {
            if (BurnableLog(a.bag, a.skills.Level(Skill::Firemaking)) == Item::None || !a.bag.Has(Item::Tinderbox)) return false;
            setDoing("burning logs");
            m.phase = AdvPhase::Work;
            return true;
        }
        case AdvTask::Cook: {
            int slot = -1;
            for (int i = 0; i < Inventory::kSlots && slot < 0; ++i) {
                const Recipe* r = RecipeFor(a.bag.Slot(i).item);
                if (r && a.skills.Level(Skill::Cooking) >= r->level) slot = i;
            }
            if (slot < 0) return false;
            const Fire* f = w.fires->NearestBurning(a.Tile());
            const int range = NearestStation(a, w, "range");
            const int fd = f ? Cheb(a.Tile(), f->tile) : 1 << 20;
            const int rd = range >= 0 ? Cheb(a.Tile(), map.Stations()[static_cast<size_t>(range)].Tile()) : 1 << 20;
            if (fd >= (1 << 20) && rd >= (1 << 20)) return false;
            if (fd <= rd + 6) {   // a fire a little further off is as good as a range (he does not like the walk)
                m.sub = 1;
                m.target = f->id;
                if (!GoTo(a, w, f->tile, 1)) return false;
            } else {
                m.sub = 2;
                m.target = range;
                if (!GoTo(a, w, map.Stations()[static_cast<size_t>(range)].Tile(), 1)) return false;
            }
            setDoing("cooking");
            return true;
        }
        case AdvTask::Smelt:
        case AdvTask::Smith: {
            const int idx = NearestStation(a, w, g.task == AdvTask::Smelt ? "furnace" : "anvil");
            if (idx < 0) return false;
            m.target = idx;
            if (!GoTo(a, w, map.Stations()[static_cast<size_t>(idx)].Tile(), 1)) return false;
            setDoing(g.task == AdvTask::Smelt ? "smelting ore" : "smithing");
            return true;
        }
        case AdvTask::Fight: {
            const int id = NearestCreature(a, w, static_cast<Species>(g.a));
            if (id < 0) return false;
            m.target = id;
            const Npc* n = w.npcs->Find(id);
            if (!GoTo(a, w, n->Tile(), 1)) {
                m.avoidNpc[id] = clock_ + 60.0f;
                return false;
            }
            // train what is lowest of his three fighting skills
            const int at = a.skills.Level(Skill::Attack), st = a.skills.Level(Skill::Strength), df = a.skills.Level(Skill::Defence);
            a.style = (at <= st && at <= df) ? CombatStyle::Accurate : (st <= df ? CombatStyle::Aggressive : CombatStyle::Defensive);
            setDoing(std::string("fighting ") + GetSpecies(static_cast<Species>(g.a)).name);
            return true;
        }
        case AdvTask::Shop:
        case AdvTask::Talk: {
            int pi = -1;
            for (size_t i = 0; i < w.people->All().size(); ++i)
                if (g.key == w.people->All()[i].info->key) pi = static_cast<int>(i);
            if (pi < 0) return false;
            m.target = pi;
            const Person& p = w.people->All()[static_cast<size_t>(pi)];
            if (!GoTo(a, w, p.Tile(), std::max(1, p.info->reach))) return false;
            setDoing(g.task == AdvTask::Shop ? "shopping" : std::string("looking for ") + p.info->name);
            return true;
        }
        case AdvTask::Abyss: {
            Adventurer* mate = m.follower ? PartnerOf(a) : nullptr;
            TileCoord t = AbyssWaypoint(a, w, g.a);
            if (mate && AbyssLevelAt(map, mate->Tile().x, mate->Tile().z) > 0) t = mate->Tile();   // (the follower starts at his leader)
            if (t.x < 0 || !GoTo(a, w, t, 2)) return false;
            a.pvp.pk = g.b != 0;
            m.roamWait = 0.0f;
            setDoing(std::string("hunting in the Abyss (level ") + std::to_string(g.a) + ")");
            return true;
        }
        case AdvTask::Reach:
        case AdvTask::UseHere: {
            TileCoord centre{-1, -1};
            for (const AreaDef& d : map.Areas())
                if (d.name == g.key) centre = {(d.rect.x0 + d.rect.x1) / 2, (d.rect.z0 + d.rect.z1) / 2};
            if (centre.x < 0) return false;
            if (!GoTo(a, w, centre, 6)) return false;
            return true;
        }
        default: return false;
    }
}

void Adventurers::EndGoal(Adventurer& a, AdvWorld& w, bool success) {
    (void)w;
    AdvMind& m = a.mind;
    m.chop.Stop();
    m.mine.Stop();
    m.fish.Stop();
    m.light.Stop();
    if (!success) {
        ++m.failStreak;
        if (m.goal.quest >= 0) {
            m.questFails[m.goal.quest] += 1;
            m.queue.clear();
        }
        if (m.goal.task == AdvTask::Chop && m.target >= 0) m.avoidTree[m.target] = clock_ + 120.0f;
        if (m.goal.task == AdvTask::Mine && m.target >= 0) m.avoidRock[m.target] = clock_ + 120.0f;
        if (m.goal.task == AdvTask::Fight && m.target >= 0) m.avoidNpc[m.target] = clock_ + 60.0f;
        if (m.failStreak >= 3) {   // nothing is working: sit down in town for a bit and think again
            m.queue.clear();
            AdvGoal r;
            r.task = AdvTask::Rest;
            r.a = 25;
            m.queue.push_back(r);
            m.failStreak = 0;
        }
    } else {
        m.failStreak = 0;
    }
    m.hasGoal = false;
    m.phase = AdvPhase::Plan;
    m.thinkLeft = 0.4f + static_cast<float>(Rand()) * 1.2f;
    m.target = -1;
    m.sub = 0;
    a.act = AdvAct::Idle;
    ManageGear(a);
}

void Adventurers::WorkGoal(Adventurer& a, float dt, AdvWorld& w) {
    AdvMind& m = a.mind;
    AdvGoal& g = m.goal;
    TileMap& map = *w.map;
    auto face = [&](float tx, float tz) {
        if ((tx - a.x) * (tx - a.x) + (tz - a.z) * (tz - a.z) > 1e-4f) a.facing = std::atan2(tx - a.x, tz - a.z);
        a.workX = tx;
        a.workZ = tz;
    };
    auto wantMet = [&]() { return g.want != Item::None && a.bag.Count(g.want) >= g.wantCount; };
    auto done = [&]() { return (g.quota > 0 && g.got >= g.quota) || wantMet(); };
    auto retarget = [&](AdvTask t) {   // the tree / rock is gone: the next nearest one
        int idx = -1;
        if (t == AdvTask::Chop) idx = NearestTree(a, w, g.key);
        else idx = NearestRock(a, w, g.key);
        if (idx < 0) {
            EndGoal(a, w, g.got > 0);
            return;
        }
        m.target = idx;
        m.chop.Stop();
        m.mine.Stop();
        const TileCoord at = t == AdvTask::Chop ? w.forest->Trees()[static_cast<size_t>(idx)].def.Tile() : w.rocks->All()[static_cast<size_t>(idx)].def.Tile();
        if (!GoTo(a, w, at, 1)) {
            (t == AdvTask::Chop ? m.avoidTree : m.avoidRock)[idx] = clock_ + 120.0f;
            EndGoal(a, w, g.got > 0);
            return;
        }
        m.phase = AdvPhase::Travel;
        m.travelLeft = 240.0f;
    };
    switch (g.task) {
        case AdvTask::Abyss: {   // roam at the chosen depth: stand a while, walk to another spot, take what the dead left (fights are PvpUpdate's)
            a.act = AdvAct::Idle;
            if (Scavenge(a, dt, w)) break;
            m.roamWait -= dt;
            if (m.roamWait > 0.0f) break;
            m.roamWait = 3.0f + static_cast<float>(Rand()) * 6.0f;
            Adventurer* mate = PartnerOf(a);
            if (mate && m.follower && AbyssLevelAt(map, mate->Tile().x, mate->Tile().z) > 0) {   // he stays with his leader
                const int d = std::max(std::abs(mate->Tile().x - a.Tile().x), std::abs(mate->Tile().z - a.Tile().z));
                if (d > 4) GoTo(a, w, mate->Tile(), 2);
                m.roamWait = 1.5f;
                break;
            }
            const TileCoord t = AbyssWaypoint(a, w, g.a);
            if (t.x >= 0) GoTo(a, w, t, 2);
            break;
        }
        case AdvTask::Rest: {
            a.act = AdvAct::Rest;
            m.timer += dt;
            if (m.timer >= static_cast<float>(g.a) || (a.skills.Hp() >= a.skills.MaxHp() && m.timer > 12.0f && g.a > 40)) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Chop: {
            const std::vector<TreeState>& trees = w.forest->Trees();
            if (m.target < 0 || m.target >= static_cast<int>(trees.size()) || trees[static_cast<size_t>(m.target)].stump) {
                retarget(AdvTask::Chop);
                break;
            }
            const TreeState& t = trees[static_cast<size_t>(m.target)];
            if (Cheb(a.Tile(), t.def.Tile()) > 1) {
                m.avoidTree[m.target] = clock_ + 120.0f;
                retarget(AdvTask::Chop);
                break;
            }
            const int wcl = a.skills.Level(Skill::Woodcutting);
            const Item axe = BestHatchet(a.bag, wcl);
            if (axe == Item::None) {
                EndGoal(a, w, false);
                break;
            }
            a.act = AdvAct::Chop;
            face(t.def.x, t.def.z);
            if (!m.chop.Active()) m.chop.Start(m.target);
            const TreeInfo& ti = GetTreeInfo(t.def.kind);
            if (m.chop.UpdateWithChance(dt, TreeChopChance(ti, wcl, HatchetSpeed(axe)), w.roll) != ChopEvent::GotLog) break;
            m.chop.Stop();
            if (a.bag.FreeSlots() == 0) {
                EndGoal(a, w, true);
                break;
            }
            w.forest->Fell(m.target);
            Gain(a, ti.logs, 1, kSrcSkill);
            GainXp(a, Skill::Woodcutting, ti.xp);
            ++g.got;
            if (done() || a.bag.FreeSlots() <= 1) EndGoal(a, w, true);
            else retarget(AdvTask::Chop);
            break;
        }
        case AdvTask::Fish: {
            const std::vector<SpotDef>& spots = map.Spots();
            if (m.target < 0 || m.target >= static_cast<int>(spots.size())) {
                EndGoal(a, w, false);
                break;
            }
            const SpotInfo& spot = GetSpotInfo(g.key);
            if (Cheb(a.Tile(), spots[static_cast<size_t>(m.target)].Tile()) > 2 || !a.bag.Has(spot.tool)) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            a.act = AdvAct::Fish;
            face(spots[static_cast<size_t>(m.target)].x, spots[static_cast<size_t>(m.target)].z);
            if (!m.fish.Active()) m.fish.Start(m.target);
            const int fl = a.skills.Level(Skill::Fishing);
            const FishEvent ev = m.fish.Update(dt, fl, w.roll);
            if (ev == FishEvent::None) break;
            if (spot.bait != Item::None && !a.bag.Remove(spot.bait)) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            if (ev != FishEvent::Caught) break;
            const Item fish = RollSpotCatch(spot, fl, w.roll());
            if (fish == Item::None) break;
            if (a.bag.FreeSlots() == 0) {
                EndGoal(a, w, true);
                break;
            }
            Gain(a, fish, 1, kSrcSkill);
            GainXp(a, Skill::Fishing, FishXp(fish));
            ++g.got;
            if (done() || a.bag.FreeSlots() <= 1) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Mine: {
            const std::vector<RockState>& rocks = w.rocks->All();
            if (m.target < 0 || m.target >= static_cast<int>(rocks.size()) || rocks[static_cast<size_t>(m.target)].depleted) {
                retarget(AdvTask::Mine);
                break;
            }
            const RockState& r = rocks[static_cast<size_t>(m.target)];
            if (Cheb(a.Tile(), r.def.Tile()) > 1) {
                m.avoidRock[m.target] = clock_ + 120.0f;
                retarget(AdvTask::Mine);
                break;
            }
            const int ml = a.skills.Level(Skill::Mining);
            const Item pick = BestPickaxe(a.bag, ml);
            Tier pt = Tier::Bronze;
            if (pick == Item::None || !GearOf(pick, &pt, nullptr)) {
                EndGoal(a, w, false);
                break;
            }
            a.act = AdvAct::Mine;
            face(r.def.x, r.def.z);
            if (!m.mine.Active()) m.mine.Start(m.target);
            const RockKind kind = r.kind;
            const MineEvent ev = m.mine.Update(dt, kind, ml, PickaxeSpeed(pt), w.roll);
            a.workPhase = m.mine.Phase();
            if (ev == MineEvent::None) break;
            w.rocks->Hit(m.target);
            if (ev == MineEvent::Swing) break;
            const Item yield = RockYield(kind, w.roll());
            m.mine.Stop();
            if (a.bag.FreeSlots() == 0) {
                EndGoal(a, w, true);
                break;
            }
            Gain(a, yield, 1, kSrcSkill);
            w.rocks->Deplete(m.target);
            GainXp(a, Skill::Mining, GetRockInfo(kind).xp);
            ++g.got;
            if (done() || a.bag.FreeSlots() <= 1) EndGoal(a, w, true);
            else retarget(AdvTask::Mine);
            break;
        }
        case AdvTask::Burn: {
            const int fm = a.skills.Level(Skill::Firemaking);
            const Item log = BurnableLog(a.bag, fm);
            if (log == Item::None || !a.bag.Has(Item::Tinderbox)) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            if (!m.light.Active()) {
                if (!w.fires->CanLight(map, a.Tile())) {   // a clear tile nearby
                    bool moved = false;
                    for (int r = 1; r <= 3 && !moved; ++r)
                        for (int dz = -r; dz <= r && !moved; ++dz)
                            for (int dx = -r; dx <= r && !moved; ++dx) {
                                const TileCoord t{a.Tile().x + dx, a.Tile().z + dz};
                                if (std::max(std::abs(dx), std::abs(dz)) == r && w.fires->CanLight(map, t)) {
                                    moved = GoTo(a, w, t, 0);
                                }
                            }
                    if (!moved) EndGoal(a, w, g.got > 0);
                    break;
                }
                m.light.Start();
            }
            a.act = AdvAct::Light;
            if (m.light.Update(dt, fm, w.roll) != LightEvent::Lit) break;
            m.light.Stop();
            const TileCoord here = a.Tile();
            const LogInfo* li = GetLogInfo(log);
            a.bag.Remove(log);
            if (w.fires->Light(&map, here) >= 0) {
                GainXp(a, Skill::Firemaking, li ? li->fmXp : kLightXp);
                ++g.got;
            }
            // step to the next free tile (a line of fires)
            static const int kDx[8] = {1, -1, 0, 0, 1, -1, 1, -1}, kDz[8] = {0, 0, 1, -1, 1, -1, -1, 1};
            for (int k = 0; k < 8; ++k) {
                const TileCoord t{here.x + kDx[k], here.z + kDz[k]};
                if (map.Walkable(t)) {
                    a.path.clear();
                    a.path.push_back(t);
                    break;
                }
            }
            if (g.quota > 0 && g.got >= g.quota) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Cook: {
            const bool onRange = m.sub == 2;
            if (!onRange) {
                const Fire* f = w.fires->Find(m.target);
                if (!f || !f->Burning()) {
                    EndGoal(a, w, g.got > 0);
                    break;
                }
                face(f->tile.x + 0.5f, f->tile.z + 0.5f);
            } else if (m.target >= 0 && m.target < static_cast<int>(map.Stations().size())) {
                face(map.Stations()[static_cast<size_t>(m.target)].x, map.Stations()[static_cast<size_t>(m.target)].z);
            }
            a.act = AdvAct::Cook;
            m.cookLeft += dt;
            if (m.cookLeft < kCookSeconds) break;
            m.cookLeft -= kCookSeconds;
            const int level = a.skills.Level(Skill::Cooking);
            int slot = -1;
            for (int i = 0; i < Inventory::kSlots && slot < 0; ++i) {
                const Recipe* cand = RecipeFor(a.bag.Slot(i).item);
                if (cand && level >= cand->level) slot = i;
            }
            const Recipe* r = slot >= 0 ? RecipeFor(a.bag.Slot(slot).item) : nullptr;
            if (!r) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            const bool burnt = w.roll() < BurnChance(*r, level + (onRange ? kRangeLevelBonus : 0));
            a.bag.SetSlot(slot, {burnt ? r->burnt : r->cooked, 1});
            if (!burnt) {
                a.source[r->cooked] |= kSrcCook;
                GainXp(a, Skill::Cooking, r->xp);
                ++g.got;
            } else {
                a.source[r->burnt] |= kSrcCook;
            }
            if (a.bag.FindRawFood() < 0 || done()) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Smelt: {
            if (m.target < 0 || m.target >= static_cast<int>(map.Stations().size()) || Cheb(a.Tile(), map.Stations()[static_cast<size_t>(m.target)].Tile()) > 1) {
                EndGoal(a, w, false);
                break;
            }
            const SmeltRecipe& r = GetSmeltRecipe(static_cast<Tier>(g.a));
            if (a.skills.Level(Skill::Smithing) < r.level || CanSmelt(r, a.bag) < 1) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            a.act = AdvAct::Smelt;
            face(map.Stations()[static_cast<size_t>(m.target)].x, map.Stations()[static_cast<size_t>(m.target)].z);
            m.timer += dt;
            if (m.timer < kSmeltSeconds) break;
            m.timer = 0;
            for (int i = 0; i < 2; ++i)
                if (r.ore[i] != Item::None) a.bag.Remove(r.ore[i], r.count[i]);
            if (w.roll() < r.success) {
                Gain(a, r.bar, 1, kSrcSmith);
                GainXp(a, Skill::Smithing, r.xp);
                ++g.got;
            }
            if ((g.quota > 0 && g.got >= g.quota) || CanSmelt(r, a.bag) < 1) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Smith: {
            if (m.target < 0 || m.target >= static_cast<int>(map.Stations().size()) || Cheb(a.Tile(), map.Stations()[static_cast<size_t>(m.target)].Tile()) > 1) {
                EndGoal(a, w, false);
                break;
            }
            const SmithRecipe r = GetSmithRecipe(static_cast<Tier>(g.a), static_cast<GearType>(g.b));
            if (r.product == Item::None || a.skills.Level(Skill::Smithing) < r.level || !a.bag.Has(Item::Hammer) || CanSmith(r, a.bag) < 1) {
                EndGoal(a, w, g.got > 0);
                break;
            }
            a.act = AdvAct::Smith;
            face(map.Stations()[static_cast<size_t>(m.target)].x, map.Stations()[static_cast<size_t>(m.target)].z);
            m.timer += dt;
            a.workPhase = std::fmod(m.timer / 0.75f, 1.0f);
            if (m.timer < kSmithSeconds) break;
            m.timer = 0;
            a.bag.Remove(BarFor(static_cast<Tier>(g.a)), r.bars);
            Gain(a, r.product, r.yield, kSrcSmith);
            GainXp(a, Skill::Smithing, r.xp);
            ++g.got;
            ManageGear(a);
            if (done() || CanSmith(r, a.bag) < 1) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Fight: {
            const Species sp = static_cast<Species>(g.a);
            Npc* n = w.npcs->Find(m.target);
            if (!n || n->dead || n->hunting || (n->engagedBy != 0 && n->engagedBy != a.id)) {
                const int id = NearestCreature(a, w, sp);
                if (id < 0) {
                    EndGoal(a, w, g.got > 0);
                    break;
                }
                m.target = id;
                m.sub = 0;
                if (!GoTo(a, w, w.npcs->Find(id)->Tile(), 1)) {
                    m.avoidNpc[id] = clock_ + 60.0f;
                    break;
                }
                m.phase = AdvPhase::Travel;
                m.travelLeft = 120.0f;
                break;
            }
            if (done()) {
                EndGoal(a, w, true);
                break;
            }
            if (Cheb(a.Tile(), n->Tile()) > 1) {   // it wandered off (or he stopped short): go after it
                m.timer += dt;
                if (m.timer > 0.5f) {
                    m.timer = 0;
                    GoTo(a, w, n->Tile(), 1);
                }
                break;
            }
            w.npcs->Engage(n->id, a.id, a.x, a.z);
            a.act = AdvAct::Fight;
            face(n->x, n->z);
            const int ticks = WeaponTicks(a.equip.Get(EquipSlot::Weapon));
            const float swing = static_cast<float>(ticks) * kTickSeconds;
            if (m.sub == 0) {   // the first blows come half a second after he arrives, as with the hero
                m.sub = 1;
                m.swingT = swing - 0.5f;
                m.creatureT = kCreatureSwingSeconds - (0.5f + kTurnSeconds);
            }
            // eat when hurt; run away when hurt with nothing to eat
            const int hp = a.skills.Hp(), maxHp = a.skills.MaxHp();
            if (hp * 100 <= maxHp * 45 && m.eatLeft <= 0.0f && FoodCount(a.bag) > 0) {
                EatNow(a);
                m.eatLeft = 1.8f;
            } else if (hp * 100 <= maxHp * 30 && FoodCount(a.bag) == 0) {
                m.queue.clear();
                EndGoal(a, w, true);
                AdvGoal r;
                r.task = AdvTask::Rest;
                r.a = std::min(270, (maxHp - hp) * 30);
                m.queue.push_back(r);
                Say(a, "That's enough for now...", 4.0f);
                break;
            }
            m.swingT += dt;
            m.creatureT += dt;
            if (m.swingT >= swing) {
                m.swingT -= swing;
                BotStrike(a, *n, w);
            }
            n = w.npcs->Find(m.target);
            if (n && !n->dead && m.creatureT >= kCreatureSwingSeconds) {
                m.creatureT -= kCreatureSwingSeconds;
                CreatureStrike(a, *n, w);
                if (a.skills.Hp() <= 0) Die(a, w);
            }
            break;
        }
        case AdvTask::Shop: {
            if (m.target < 0 || m.target >= static_cast<int>(w.people->All().size())) {
                EndGoal(a, w, false);
                break;
            }
            const Person& p = w.people->All()[static_cast<size_t>(m.target)];
            if (Cheb(a.Tile(), p.Tile()) > std::max(1, p.info->reach) + 1) {
                EndGoal(a, w, false);
                break;
            }
            a.act = AdvAct::Idle;
            face(p.x, p.z);
            if (m.timer == 0.0f) {
                const int si = ShopIndex(p.info->shop);
                if (si < 0) {
                    EndGoal(a, w, false);
                    break;
                }
                if (g.sellAll) {
                    std::vector<Item> keep;   // what a current quest step still needs
                    for (int qi = 0; qi < a.quests.Count(); ++qi)
                        if (const QuestStep* st = a.quests.Current(qi)) {
                            keep.push_back(st->item);
                            keep.push_back(st->tool);
                        }
                    for (int i = 0; i < Inventory::kSlots; ++i) {
                        const ItemStack s = a.bag.Slot(i);
                        if (s.item == Item::None || s.item == Item::Coins || IsTool(s.item) || !ItemSellable(s.item) || !shops_.Buys(si, s.item)) continue;
                        if (GetItemInfo(s.item).food && GetItemInfo(s.item).heals > 0) continue;   // he eats it
                        if (GetItemInfo(s.item).buryXp > 0) continue;
                        if (std::find(keep.begin(), keep.end(), s.item) != keep.end()) continue;
                        EquipSlot slot;
                        if (IsWearable(s.item) && Equippable(s.item, &slot)) {   // something he may still wear
                            Skill rs;
                            int rl;
                            const bool canWear = !(ItemRequirement(s.item, &rs, &rl) && a.skills.Level(rs) < rl);
                            const Item cur = a.equip.Get(slot);
                            if (canWear && (cur == Item::None || GearScore(s.item) > GearScore(cur))) continue;
                        }
                        const int price = shops_.SellPrice(si, s.item);
                        if (price <= 0) continue;
                        const ItemStack taken = a.bag.TakeSlot(i);
                        Gain(a, Item::Coins, taken.count * price, kSrcShop);
                        shops_.Add(si, taken.item, taken.count);
                    }
                }
                for (const ItemStack& want : g.buy) {
                    const std::vector<Shops::Line>& lines = shops_.Lines(si);
                    int li = -1;
                    for (size_t k = 0; k < lines.size(); ++k)
                        if (lines[k].item == want.item && lines[k].count > 0) li = static_cast<int>(k);
                    if (li < 0) continue;
                    const int price = shops_.BuyPrice(si, want.item);
                    if (price <= 0) continue;
                    int qty = std::min({want.count, lines[static_cast<size_t>(li)].count, a.bag.Count(Item::Coins) / price});
                    if (qty <= 0) continue;
                    a.bag.Remove(Item::Coins, qty * price);
                    const int before = a.bag.Count(want.item);
                    Gain(a, want.item, qty, kSrcShop);
                    const int got = a.bag.Count(want.item) - before;
                    if (got < qty) Gain(a, Item::Coins, (qty - got) * price, kSrcShop);   // no room: refund
                    if (got > 0) shops_.Take(si, li, got);
                    if (got > 0 && IsWearable(want.item) && Rand() < 0.5) Say(a, "Just bought a " + Lower(GetItemInfo(want.item).name) + "!", 5.0f);
                }
                ManageGear(a);
            }
            m.timer += dt;
            if (m.timer >= 1.6f) EndGoal(a, w, true);
            break;
        }
        case AdvTask::Talk: {
            if (m.target < 0 || m.target >= static_cast<int>(w.people->All().size())) {
                EndGoal(a, w, false);
                break;
            }
            const Person& p = w.people->All()[static_cast<size_t>(m.target)];
            if (Cheb(a.Tile(), p.Tile()) > std::max(1, p.info->reach) + 1) {
                EndGoal(a, w, false);
                break;
            }
            a.act = AdvAct::Idle;
            face(p.x, p.z);
            m.timer += dt;
            if (m.timer < 1.4f) break;
            const std::string key = p.info->key;
            bool handled = false;
            for (int qi = 0; qi < a.quests.Count(); ++qi) {   // hand in what they asked for
                if (!a.quests.StepReady(qi, key, a.bag)) continue;
                handled = true;
                if (a.quests.CompleteStep(qi, &a.bag)) FinishQuest(a, w, qi);
            }
            if (g.startQuest && g.quest >= 0 && a.quests.CanStart(g.quest, a.skills)) {
                a.quests.Start(g.quest);
                handled = true;
                Say(a, "I'll take the job: " + AllQuests()[static_cast<size_t>(g.quest)].name + ".", 5.0f);
            }
            EndGoal(a, w, handled);
            break;
        }
        case AdvTask::Reach: {
            m.timer += dt;
            if (m.timer < 0.5f) break;
            const int qi = a.quests.OnReach(g.key);
            if (qi >= 0) OnQuestStepDone(a, w, qi);
            EndGoal(a, w, true);
            break;
        }
        case AdvTask::UseHere: {
            m.timer += dt;
            if (m.timer < 1.5f) break;
            const QuestStep* st = g.quest >= 0 ? a.quests.Current(g.quest) : nullptr;
            const std::string area = map.AreaAt(a.Tile().x, a.Tile().z);
            if (!st || st->kind != StepKind::Use || a.quests.FindUse(st->item, area) != g.quest || (st->tool != Item::None && !a.bag.Has(st->tool))) {
                EndGoal(a, w, false);
                break;
            }
            const QuestStep step = *st;
            a.quests.CompleteStep(g.quest, &a.bag);
            if (step.give != Item::None) Gain(a, step.give, step.giveCount, kSrcQuest);
            OnQuestStepDone(a, w, g.quest);
            EndGoal(a, w, true);
            break;
        }
        default: EndGoal(a, w, false); break;
    }
}

// ------------------------------------------------------------------------------------------------ the loop

void Adventurers::UpdateReal(Adventurer& a, float dt, AdvWorld& w) {
    AdvMind& m = a.mind;
    // one hit point back every 30 seconds
    m.regen += dt;
    if (m.regen >= kRegenSeconds) {
        m.regen -= kRegenSeconds;
        a.skills.Heal(1);
    }
    m.eatLeft -= dt;
    // a bite to eat when hurt (outside fights: in them he decides himself)
    if (!(m.hasGoal && m.goal.task == AdvTask::Fight && m.phase == AdvPhase::Work) && a.skills.Hp() * 100 <= a.skills.MaxHp() * 50 && m.eatLeft <= 0.0f &&
        FoodCount(a.bag) > 0) {
        EatNow(a);
        m.eatLeft = 2.0f;
    }
    // walking into a named place (quests ask for that)
    const std::string& area = w.map->AreaAt(a.Tile().x, a.Tile().z);
    if (area != m.area) {
        m.area = area;
        if (!area.empty()) {
            const int qi = a.quests.OnReach(area);
            if (qi >= 0) OnQuestStepDone(a, w, qi);
        }
    }
    // the Abyss: fights with other players, and running away when badly hurt
    if (AbyssLevelAt(*w.map, a.Tile().x, a.Tile().z) > 0) {
        const bool low = a.skills.Hp() * 100 < a.skills.MaxHp() * 30 && (FoodCount(a.bag) == 0 || a.skills.Hp() * 100 < a.skills.MaxHp() * 15);
        if (low && !m.fleeing) {
            m.fleeing = true;
            a.pvp.foe = 0;
            a.pvp.beast = 0;
            a.pvp.fightingHero = false;
            a.pvp.cooldown = 150.0f;
            m.queue.clear();
            if (m.hasGoal) EndGoal(a, w, true);
            AdvGoal r;
            r.task = AdvTask::Rest;
            r.a = 150;
            m.queue.push_back(r);
            m.doing = "running from the Abyss";
            m.wasFighting = false;
        }
        if (PvpUpdate(a, dt, w)) {
            m.wasFighting = true;
            return;
        }
        if (m.wasFighting) {   // the fight is over: back to what he was doing
            m.wasFighting = false;
            if (m.hasGoal && m.phase != AdvPhase::Plan && !StartGoal(a, w)) EndGoal(a, w, false);
        }
    } else {
        m.fleeing = false;
        m.wasFighting = false;
        a.pvp.foe = 0;
        a.pvp.fightingHero = false;
    }
    // burying bones takes a moment
    if (m.buryLeft > 0.0f) {
        a.act = AdvAct::Bury;
        m.buryLeft -= dt;
        if (m.buryLeft <= 0.0f && a.bag.Remove(m.burying, 1)) GainXp(a, Skill::Prayer, GetItemInfo(m.burying).buryXp);
        return;
    }
    switch (m.phase) {
        case AdvPhase::Plan: {
            a.act = a.path.empty() ? AdvAct::Idle : AdvAct::Walk;
            m.thinkLeft -= dt;
            if (m.thinkLeft > 0.0f) break;
            if (m.queue.empty()) PlanSession(a, w);
            if (m.queue.empty()) {
                m.thinkLeft = 4.0f;
                break;
            }
            m.goal = m.queue.front();
            m.queue.pop_front();
            m.hasGoal = true;
            if (!StartGoal(a, w)) EndGoal(a, w, false);
            break;
        }
        case AdvPhase::Travel: {
            m.travelLeft -= dt;
            if (a.path.empty()) {
                m.phase = AdvPhase::Work;
                m.timer = 0;
                break;
            }
            if (m.travelLeft <= 0.0f) EndGoal(a, w, false);
            break;
        }
        case AdvPhase::Work: {
            if (m.goal.task != AdvTask::Rest) m.goal.left -= dt;
            if (m.goal.left <= 0.0f) {
                EndGoal(a, w, m.goal.got > 0 || m.goal.quest >= 0);
                break;
            }
            if (!a.path.empty()) break;   // (stepping to the next fire, say)
            WorkGoal(a, dt, w);
            break;
        }
        default: break;
    }
}

}  // namespace journey
