// What a real adventurer decides to do next: training, quests, shopping, upgrades. See Adventurers.h.
#include <algorithm>
#include <cmath>
#include <set>

#include "game/AdventurerUtil.h"
#include "game/Cooking.h"
#include "game/Skilling.h"

namespace journey {

using namespace advutil;

namespace {

// Which creature gives an item that is not made or grown: the quests ask for eggs, hides and meat.
bool DropSpecies(Item item, Species* sp) {
    switch (item) {
        case Item::Egg:
        case Item::RawChicken:
        case Item::Feathers: *sp = Species::Chicken; return true;
        case Item::RawBeef:
        case Item::Cowhide: *sp = Species::Cow; return true;
        case Item::RawRatMeat: *sp = Species::GiantRat; return true;
        case Item::RawWolfMeat: *sp = Species::Wolf; return true;
        case Item::Bones: *sp = Species::Goblin; return true;
        default: return false;
    }
}

const char* const kSpotKinds[] = {"net", "rod", "harpoon", "cage"};

}  // namespace

// ------------------------------------------------------------------------------------------------ finding things

int Adventurers::NearestTree(Adventurer& a, AdvWorld& w, const std::string& kind) const {
    const std::vector<TreeState>& trees = w.forest->Trees();
    std::vector<std::pair<int, int>> best;   // (distance, index)
    for (size_t i = 0; i < trees.size(); ++i) {
        const TreeState& t = trees[i];
        if (t.stump || (!kind.empty() && t.def.kind != kind)) continue;
        if (a.mind.hasGoal && a.mind.goal.abyss && AbyssLevelAt(*w.map, t.def.Tile().x, t.def.Tile().z) <= 0) continue;
        auto av = a.mind.avoidTree.find(static_cast<int>(i));
        if (av != a.mind.avoidTree.end() && av->second > clock_) continue;
        bool claimed = false;
        for (const Adventurer& o : advs_)
            if (&o != &a && o.kind == AdvKind::Real && o.mind.hasGoal && o.mind.goal.task == AdvTask::Chop && o.mind.target == static_cast<int>(i)) claimed = true;
        if (claimed) continue;
        best.push_back({Cheb(a.Tile(), t.def.Tile()), static_cast<int>(i)});
    }
    if (best.empty()) return -1;
    std::sort(best.begin(), best.end());
    const size_t pick = static_cast<size_t>(const_cast<Adventurers*>(this)->Rand() * std::min<size_t>(3, best.size()));
    return best[std::min(pick, best.size() - 1)].second;
}

int Adventurers::NearestRock(Adventurer& a, AdvWorld& w, const std::string& key) const {
    const std::vector<RockState>& rocks = w.rocks->All();
    std::vector<std::pair<int, int>> best;
    for (size_t i = 0; i < rocks.size(); ++i) {
        const RockState& r = rocks[i];
        if (r.depleted || key != GetRockInfo(r.kind).key) continue;
        if (a.mind.hasGoal && a.mind.goal.abyss && AbyssLevelAt(*w.map, r.def.Tile().x, r.def.Tile().z) <= 0) continue;
        auto av = a.mind.avoidRock.find(static_cast<int>(i));
        if (av != a.mind.avoidRock.end() && av->second > clock_) continue;
        bool claimed = false;
        for (const Adventurer& o : advs_)
            if (&o != &a && o.kind == AdvKind::Real && o.mind.hasGoal && o.mind.goal.task == AdvTask::Mine && o.mind.target == static_cast<int>(i)) claimed = true;
        if (claimed) continue;
        best.push_back({Cheb(a.Tile(), r.def.Tile()), static_cast<int>(i)});
    }
    if (best.empty()) return -1;
    std::sort(best.begin(), best.end());
    const size_t pick = static_cast<size_t>(const_cast<Adventurers*>(this)->Rand() * std::min<size_t>(2, best.size()));
    return best[std::min(pick, best.size() - 1)].second;
}

int Adventurers::NearestSpot(const Adventurer& a, const AdvWorld& w, const std::string& kind) const {
    std::vector<std::pair<int, int>> best;
    const std::vector<SpotDef>& spots = w.map->Spots();
    for (size_t i = 0; i < spots.size(); ++i)
        if (spots[i].kind == kind) best.push_back({Cheb(a.Tile(), spots[i].Tile()), static_cast<int>(i)});
    if (best.empty()) return -1;
    std::sort(best.begin(), best.end());
    const size_t pick = static_cast<size_t>(const_cast<Adventurers*>(this)->Rand() * std::min<size_t>(3, best.size()));
    return best[std::min(pick, best.size() - 1)].second;
}

int Adventurers::NearestStation(const Adventurer& a, const AdvWorld& w, const char* kind) const {
    int best = -1, bd = 1 << 30;
    const std::vector<StationDef>& st = w.map->Stations();
    for (size_t i = 0; i < st.size(); ++i) {
        if (st[i].kind != kind) continue;
        const int d = Cheb(a.Tile(), st[i].Tile());
        if (d < bd) {
            bd = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Adventurers::NearestCreature(Adventurer& a, AdvWorld& w, Species s) const {
    int best = -1, bd = 1 << 30;
    for (const Npc& n : w.npcs->All()) {
        if (n.species != s || !n.Alive() || n.hunting || (n.engagedBy != 0 && n.engagedBy != a.id) || n.spawnT < 1.0f) continue;
        auto av = a.mind.avoidNpc.find(n.id);
        if (av != a.mind.avoidNpc.end() && av->second > clock_) continue;
        const int d = Cheb(a.Tile(), n.Tile());
        if (d < bd) {
            bd = d;
            best = n.id;
        }
    }
    return best;
}

// ------------------------------------------------------------------------------------------------ fighting

bool Adventurers::FightSafe(const Adventurer& a, Species s) const {
    const SpeciesInfo& si = GetSpecies(s);
    if (s == Species::AbyssWarrior || s == Species::AbyssArcher || s == Species::AbyssMage || s == Species::AbyssChampion) return false;
    if (si.attackKind != AttackKind::Melee && a.Combat() < 40) return false;   // shooters and casters hit from afar
    Fighter c;
    c.attack = si.attack;
    c.strength = si.strength;
    c.defence = si.defence;
    c.creature = true;
    const int mh = std::max(1, MaxHit(c));
    const int cb = a.Combat();
    const int maxHp = a.skills.MaxHp();
    if (si.level > cb + 4 && si.level * 10 > cb * 14) return false;
    if (mh * 3 > maxHp) return false;                                  // three good hits would kill him
    if (mh * 4 > maxHp && FoodCount(a.bag) < 3) return false;
    return true;
}

Species Adventurers::BestFightTarget(const Adventurer& a, const AdvWorld& w) const {
    std::set<int> present;
    for (const Npc& n : w.npcs->All())
        if (n.Alive()) present.insert(static_cast<int>(n.species));
    Species best = Species::Count;
    int bl = -1;
    const int cb = a.Combat();
    for (int sp : present) {
        const Species s = static_cast<Species>(sp);
        const SpeciesInfo& si = GetSpecies(s);
        if (!FightSafe(a, s)) continue;
        if (si.level > std::max(2, (cb * 11) / 10 + 2)) continue;
        if (si.level < cb / 2 - 2 && bl >= 0) continue;
        if (si.level > bl) {
            bl = si.level;
            best = s;
        }
    }
    return best;
}

// ------------------------------------------------------------------------------------------------ shops

bool Adventurers::ShopLineOpen(const Adventurer& a, const ShopStockDef& d) const {
    if (d.quest && d.quest[0]) {
        const int qi = FindQuest(d.quest);
        if (qi < 0 || !a.quests.State(qi).done) return false;
    }
    if (d.reqLevel > 0 && a.skills.Level(d.reqSkill) < d.reqLevel) return false;
    if (d.reqQuestPoints < 0) return a.quests.Points() >= static_cast<int>(AllQuests().size());
    if (d.reqQuestPoints > 0 && a.quests.Points() < d.reqQuestPoints) return false;
    return true;
}

// The shop that sells `item` (or, selling, buys everything) nearest to him: returns its index, fills where (the keeper's tile) and the keeper's index in People.
int Adventurers::NearestShopFor(const Adventurer& a, const AdvWorld& w, Item item, bool selling, TileCoord* where, int* personIndex) const {
    int best = -1, bd = 1 << 30;
    for (const auto& sp : shopPeople_) {
        const int si = ShopIndex(sp.first);
        if (si < 0) continue;
        const ShopDef& d = shops_.Def(si);
        if (d.key == "bank" || d.key == "masters") continue;
        bool ok = false;
        if (selling) {
            ok = item == Item::None ? d.buys == kCatEverything : shops_.Buys(si, item);
        } else {
            for (const Shops::Line& l : shops_.Lines(si))
                if (l.item == item && l.count > 0) {
                    for (const ShopStockDef& sd : d.stock)
                        if (sd.item == item && ShopLineOpen(a, sd)) ok = true;
                }
        }
        if (!ok) continue;
        const Person& p = w.people->All()[static_cast<size_t>(sp.second)];
        const int dist = Cheb(a.Tile(), p.Tile());
        if (dist < bd) {
            bd = dist;
            best = si;
            if (where) *where = p.Tile();
            if (personIndex) *personIndex = sp.second;
        }
    }
    return best;
}

bool Adventurers::PlanShopTrip(Adventurer& a, AdvWorld& w, bool sell, std::vector<ItemStack> buy, int quest) {
    bool any = false;
    for (const ItemStack& want : buy) {
        TileCoord at;
        int person = -1;
        const int si = NearestShopFor(a, w, want.item, false, &at, &person);
        if (si < 0) continue;
        AdvGoal g;
        g.task = AdvTask::Shop;
        g.key = w.people->All()[static_cast<size_t>(person)].info->key;
        g.buy = {want};
        g.quest = quest;
        g.left = 900.0f;
        a.mind.queue.push_back(g);
        any = true;
    }
    if (sell) {
        TileCoord at;
        int person = -1;
        if (NearestShopFor(a, w, Item::None, true, &at, &person) >= 0) {
            AdvGoal g;
            g.task = AdvTask::Shop;
            g.key = w.people->All()[static_cast<size_t>(person)].info->key;
            g.sellAll = true;
            g.quest = quest;
            g.left = 900.0f;
            a.mind.queue.push_back(g);
            any = true;
        }
    }
    return any;
}

// Is there something in a shop he can afford that would make him better at what he does?
bool Adventurers::WantsUpgrade(Adventurer& a, AdvWorld& w) {
    const int coins = a.bag.Count(Item::Coins);
    if (coins < 40) return false;
    const int reserve = 25;
    Item bestItem = Item::None;
    double bestGain = 0;
    int bestPrice = 0;
    const Item axe = BestHatchet(a.bag, a.skills.Level(Skill::Woodcutting));
    const double axeSpeed = axe == Item::None ? 0.0 : HatchetSpeed(axe);
    const Item pick = BestPickaxe(a.bag, a.skills.Level(Skill::Mining));
    Tier pt = Tier::Bronze;
    double pickSpeed = 0;
    if (pick != Item::None && GearOf(pick, &pt, nullptr)) pickSpeed = PickaxeSpeed(pt);
    for (const auto& sp : shopPeople_) {
        const int si = ShopIndex(sp.first);
        if (si < 0) continue;
        const ShopDef& d = shops_.Def(si);
        if (d.key == "masters") continue;
        for (const ShopStockDef& sd : d.stock) {
            if (!ShopLineOpen(a, sd)) continue;
            const Item item = sd.item;
            if (a.bag.Has(item)) continue;
            const int price = shops_.BuyPrice(si, item);
            if (price <= 0 || price > coins - reserve) continue;
            double gain = 0;
            EquipSlot slot;
            Skill rs;
            int rl;
            Tier t;
            GearType g;
            if (IsHatchet(item)) {
                if (HatchetLevel(item) > a.skills.Level(Skill::Woodcutting)) continue;
                gain = (HatchetSpeed(item) - axeSpeed) * 40.0;
            } else if (GearOf(item, &t, &g) && g == GearType::Pickaxe) {
                if (ItemRequirement(item, &rs, &rl) && a.skills.Level(rs) < rl) continue;
                gain = (PickaxeSpeed(t) - pickSpeed) * 40.0;
            } else if (item == Item::FlyFishingRod || item == Item::Harpoon || item == Item::LobsterPot) {
                const char* kind = item == Item::FlyFishingRod ? "rod" : item == Item::Harpoon ? "harpoon" : "cage";
                const SpotInfo& spot = GetSpotInfo(kind);
                int minLevel = 999;
                for (const FishDef& f : spot.fish) minLevel = std::min(minLevel, f.level);
                if (a.skills.Level(Skill::Fishing) >= minLevel && a.bag.Count(item) == 0) gain = 12.0;
            } else if (IsWearable(item) && Equippable(item, &slot)) {
                if (ItemRequirement(item, &rs, &rl) && a.skills.Level(rs) < rl) continue;
                const Item cur = a.equip.Get(slot);
                double have = cur == Item::None ? 0.0 : GearScore(cur);
                for (int i = 0; i < Inventory::kSlots; ++i) {   // something better already in the bag will be worn soon
                    EquipSlot s2;
                    const Item bi = a.bag.Slot(i).item;
                    if (bi != Item::None && Equippable(bi, &s2) && s2 == slot) have = std::max(have, static_cast<double>(GearScore(bi)));
                }
                gain = GearScore(item) - have;
                if (gain < 3.0) gain = 0;
            }
            if (gain <= 0) continue;
            if (gain > bestGain || (gain == bestGain && price < bestPrice)) {
                bestGain = gain;
                bestItem = item;
                bestPrice = price;
            }
        }
    }
    if (bestItem == Item::None) return false;
    return PlanShopTrip(a, w, false, {{bestItem, 1}}, -1);
}

// ------------------------------------------------------------------------------------------------ getting things

bool Adventurers::CanAcquire(const Adventurer& a, const AdvWorld& w, Item item, int count, int depth) const {
    std::vector<AdvGoal> scratch;
    return const_cast<Adventurers*>(this)->PlanAcquire(const_cast<Adventurer&>(a), const_cast<AdvWorld&>(w), item, count, -1, &scratch, depth);
}

bool Adventurers::PlanAcquire(Adventurer& a, AdvWorld& w, Item item, int count, int quest, std::vector<AdvGoal>* out, int depth) {
    const int need = count - a.bag.Count(item);
    if (need <= 0) return true;
    if (depth > 4) return false;
    const int coins = a.bag.Count(Item::Coins);
    const int wc = a.skills.Level(Skill::Woodcutting), fl = a.skills.Level(Skill::Fishing), ml = a.skills.Level(Skill::Mining);
    auto goal = [&](AdvTask t) {
        AdvGoal g;
        g.task = t;
        g.quest = quest;
        g.want = item;
        g.wantCount = count;
        return g;
    };
    // 1. buy it, if some shop sells it and he can pay
    {
        TileCoord at;
        int person = -1;
        const int si = NearestShopFor(a, w, item, false, &at, &person);
        if (si >= 0 && shops_.BuyPrice(si, item) > 0 && coins >= shops_.BuyPrice(si, item) * need && a.bag.FreeSlots() > 1) {
            AdvGoal g = goal(AdvTask::Shop);
            g.key = w.people->All()[static_cast<size_t>(person)].info->key;
            g.buy = {{item, need}};
            out->push_back(g);
            return true;
        }
    }
    // 2. logs
    if (IsLog(item)) {
        for (const TreeState& t : w.forest->Trees()) {
            const TreeInfo& ti = GetTreeInfo(t.def.kind);
            if (ti.logs != item || ti.level > wc || BestHatchet(a.bag, wc) == Item::None) continue;
            AdvGoal g = goal(AdvTask::Chop);
            g.key = t.def.kind;
            g.quota = need;
            out->push_back(g);
            return true;
        }
        return false;
    }
    // 3. raw fish
    if (const FishDef* f = FindFish(item)) {
        if (f->level > fl) return false;
        for (const char* kind : kSpotKinds) {
            const SpotInfo& spot = GetSpotInfo(kind);
            bool has = false;
            for (const FishDef& fd : spot.fish) has = has || fd.raw == item;
            if (!has || !a.bag.Has(spot.tool) || (spot.bait != Item::None && a.bag.Count(spot.bait) < 10)) continue;
            bool onMap = false;
            for (const SpotDef& s : w.map->Spots()) onMap = onMap || s.kind == kind;
            if (!onMap) continue;
            AdvGoal g = goal(AdvTask::Fish);
            g.key = kind;
            g.quota = need;
            out->push_back(g);
            return true;
        }
        return false;
    }
    // 4. cooked food
    if (const Recipe* r = RecipeForCooked(item)) {
        if (a.skills.Level(Skill::Cooking) < r->level) return false;
        if (!PlanAcquire(a, w, r->raw, count + need / 2 + 1, quest, out, depth + 1)) return false;
        AdvGoal g = goal(AdvTask::Cook);
        g.quota = need;
        out->push_back(g);
        return true;
    }
    // 5. ore
    for (int k = 0; k < kRockKindCount; ++k) {
        const RockInfo& ri = GetRockInfo(static_cast<RockKind>(k));
        if (ri.ore != item) continue;
        if (ri.level > ml || BestPickaxe(a.bag, ml) == Item::None) return false;
        bool onMap = false;
        for (const RockState& r : w.rocks->All()) onMap = onMap || GetRockInfo(r.kind).key == std::string(ri.key);
        if (!onMap) return false;
        AdvGoal g = goal(AdvTask::Mine);
        g.key = ri.key;
        g.quota = need;
        out->push_back(g);
        return true;
    }
    // 6. a bar: mine the ore, smelt it
    Tier tier;
    if (TierOfBar(item, &tier)) {
        const SmeltRecipe& r = GetSmeltRecipe(tier);
        if (a.skills.Level(Skill::Smithing) < r.level) return false;
        for (int i = 0; i < 2; ++i) {
            if (r.ore[i] == Item::None) continue;
            const int ores = static_cast<int>(std::ceil(need * r.count[i] / std::max(0.4f, r.success))) + a.bag.Count(r.ore[i]);
            if (!PlanAcquire(a, w, r.ore[i], std::min(ores, 26), quest, out, depth + 1)) return false;
        }
        AdvGoal g = goal(AdvTask::Smelt);
        g.a = static_cast<int>(tier);
        g.quota = need;
        out->push_back(g);
        return true;
    }
    // 7. a piece of smithed gear
    Tier gt;
    GearType gg;
    if (GearOf(item, &gt, &gg)) {
        const SmithRecipe r = GetSmithRecipe(gt, gg);
        if (r.product != item || a.skills.Level(Skill::Smithing) < r.level || !a.bag.Has(Item::Hammer)) return false;
        const int bars = r.bars * need;
        if (bars > 20) return false;
        if (!PlanAcquire(a, w, BarFor(gt), bars + a.bag.Count(BarFor(gt)), quest, out, depth + 1)) return false;
        AdvGoal g = goal(AdvTask::Smith);
        g.a = static_cast<int>(gt);
        g.b = static_cast<int>(gg);
        g.quota = need;
        out->push_back(g);
        return true;
    }
    // 8. something a creature drops
    Species sp;
    if (DropSpecies(item, &sp) && FightSafe(a, sp) && NearestCreature(a, w, sp) >= 0) {
        AdvGoal g = goal(AdvTask::Fight);
        g.a = static_cast<int>(sp);
        g.quota = 40;
        g.left = 1500.0f;
        out->push_back(g);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------ quests

bool Adventurers::QuestFeasible(const Adventurer& a, const AdvWorld& w, int qi) const {
    const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
    if (!a.quests.CanStart(qi, a.skills)) return false;
    auto fails = a.mind.questFails.find(qi);
    if (fails != a.mind.questFails.end() && fails->second >= 3) return false;
    auto personOk = [&](const std::string& key) {
        for (const Person& p : w.people->All())
            if (key == p.info->key) return true;
        return false;
    };
    auto areaOk = [&](const std::string& name) {
        for (const AreaDef& d : w.map->Areas())
            if (d.name == name) return true;
        return false;
    };
    if (!personOk(q.giver)) return false;
    for (const QuestStep& st : q.steps) {
        switch (st.kind) {
            case StepKind::Kill:
                if (st.killKind != kAnyWeapon && st.killKind != static_cast<int>(AttackKind::Melee)) return false;   // (they fight with their hands and swords: no bows or spells)
                if (!FightSafe(a, st.species) || NearestCreature(const_cast<Adventurer&>(a), const_cast<AdvWorld&>(w), st.species) < 0) return false;
                break;
            case StepKind::Have:
                if (!personOk(st.person) || !CanAcquire(a, w, st.item, st.count)) return false;
                break;
            case StepKind::Talk:
                if (!personOk(st.person)) return false;
                break;
            case StepKind::Reach:
                if (!areaOk(st.area)) return false;
                break;
            case StepKind::Use:
                if (!areaOk(st.area) || !CanAcquire(a, w, st.item, 1) || (st.tool != Item::None && !CanAcquire(a, w, st.tool, 1))) return false;
                break;
        }
    }
    return true;
}

// Queues what the quest he is on needs next. False if he is on none (or cannot go on with it).
bool Adventurers::PlanQuestStep(Adventurer& a, AdvWorld& w) {
    AdvMind& m = a.mind;
    for (int qi = 0; qi < a.quests.Count(); ++qi) {
        const QuestState& st = a.quests.State(qi);
        if (!st.started || st.done) continue;
        auto fails = m.questFails.find(qi);
        if (fails != m.questFails.end() && fails->second >= 3) continue;   // he has given up on this one
        const QuestStep* step = a.quests.Current(qi);
        if (!step) continue;
        const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
        auto fail = [&]() {
            m.questFails[qi] += 1;
            return false;
        };
        switch (step->kind) {
            case StepKind::Kill: {
                if (!FightSafe(a, step->species) || (step->killKind != kAnyWeapon && step->killKind != static_cast<int>(AttackKind::Melee))) return fail();
                AdvGoal g;
                g.task = AdvTask::Fight;
                g.a = static_cast<int>(step->species);
                g.quota = std::max(1, step->count - st.count);
                g.quest = qi;
                g.left = 1500.0f;
                m.queue.push_back(g);
                m.doing = "on a quest: " + q.name;
                return true;
            }
            case StepKind::Have: {
                std::vector<AdvGoal> plan;
                if (a.bag.Count(step->item) < step->count && !PlanAcquire(a, w, step->item, step->count, qi, &plan)) return fail();
                for (AdvGoal& g : plan) m.queue.push_back(g);
                AdvGoal t;
                t.task = AdvTask::Talk;
                t.key = step->person;
                t.quest = qi;
                m.queue.push_back(t);
                m.doing = "on a quest: " + q.name;
                return true;
            }
            case StepKind::Talk: {
                AdvGoal t;
                t.task = AdvTask::Talk;
                t.key = step->person;
                t.quest = qi;
                m.queue.push_back(t);
                m.doing = "on a quest: " + q.name;
                return true;
            }
            case StepKind::Reach: {
                AdvGoal g;
                g.task = AdvTask::Reach;
                g.key = step->area;
                g.quest = qi;
                m.queue.push_back(g);
                m.doing = "on a quest: " + q.name;
                return true;
            }
            case StepKind::Use: {
                std::vector<AdvGoal> plan;
                if (a.bag.Count(step->item) < 1 && !PlanAcquire(a, w, step->item, 1, qi, &plan)) return fail();
                if (step->tool != Item::None && a.bag.Count(step->tool) < 1 && !PlanAcquire(a, w, step->tool, 1, qi, &plan)) return fail();
                for (AdvGoal& g : plan) m.queue.push_back(g);
                AdvGoal g;
                g.task = AdvTask::UseHere;
                g.key = step->area;
                g.quest = qi;
                m.queue.push_back(g);
                m.doing = "on a quest: " + q.name;
                return true;
            }
        }
    }
    return false;
}

void Adventurers::OnQuestStepDone(Adventurer& a, AdvWorld& w, int qi) {
    (void)w;
    AdvMind& m = a.mind;
    m.queue.clear();
    if (m.hasGoal && m.goal.quest == qi) m.goal.left = 0.0f;   // the errand is over: the next plan picks up the next step
}

void Adventurers::FinishQuest(Adventurer& a, AdvWorld& w, int qi) {
    (void)w;
    const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
    a.quests.Finish(qi);
    for (const std::pair<Skill, int>& x : q.reward.xp) GainXp(a, x.first, x.second);
    if (q.reward.coins > 0) Gain(a, Item::Coins, q.reward.coins, kSrcQuest);
    for (const ItemStack& s : q.reward.items) Gain(a, s.item, s.count, kSrcQuest);
    Say(a, "Quest complete: " + q.name + "!", 6.0f, true);
    ManageGear(a);
    a.mind.queue.clear();
}

// ------------------------------------------------------------------------------------------------ the plan

namespace {

struct KindChoice {
    std::string key;
    double score = 0;
};

}  // namespace

// A tile at the given Abyss level (the deeper, the further from the gate): somewhere to walk to.
TileCoord Adventurers::AbyssWaypoint(const Adventurer& a, const AdvWorld& w, int level) {
    (void)a;
    const AreaDef* area = nullptr;
    for (const AreaDef& d : w.map->Areas())
        if (d.name == "The Abyss") area = &d;
    if (!area) return {-1, -1};
    const int span = std::max(1, area->rect.z1 - area->rect.z0 - 1);
    const int zBase = area->rect.z0 + (std::clamp(level, 1, kAbyssMaxLevel) - 1) * span / (kAbyssMaxLevel - 1);
    for (int tries = 0; tries < 60; ++tries) {
        const int x = area->rect.x0 + 3 + RandInt(std::max(1, area->rect.x1 - area->rect.x0 - 6));
        const int z = std::clamp(zBase + RandInt(7) - 3, area->rect.z0 + 1, area->rect.z1 - 2);
        if (w.map->Walkable(x, z) && AbyssLevelAt(*w.map, x, z) > 0) return {x, z};
    }
    return {-1, -1};
}

// Setting out for the Abyss: first what he can gather there (the best rock and tree his levels allow), then a stint of roaming at a depth that suits his
// courage. Now and then; hurt, hungry or weak (under combat level 18) he stays at home.
// What he does on an Abyss expedition: gather the best he can there, then roam at the given depth.
void Adventurers::QueueAbyssTrip(Adventurer& a, int level, bool pk, float seconds) {
    AdvMind& m = a.mind;
    Skills& sk = a.skills;
    const int ml = sk.Level(Skill::Mining), wcl = sk.Level(Skill::Woodcutting);
    if (BestPickaxe(a.bag, ml) != Item::None) {   // the best rock he can work
        const char* key = ml >= 70 ? "adamant" : (ml >= 55 ? "mithril" : (ml >= 40 ? "gems" : nullptr));
        if (key) {
            AdvGoal g;
            g.task = AdvTask::Mine;
            g.key = key;
            g.quota = 5;
            g.left = 600.0f;
            g.abyss = true;
            m.queue.push_back(g);
        }
    }
    if (BestHatchet(a.bag, wcl) != Item::None) {   // and the best tree
        const char* key = wcl >= 75 ? "magic" : (wcl >= 60 ? "yew" : nullptr);
        if (key) {
            AdvGoal g;
            g.task = AdvTask::Chop;
            g.key = key;
            g.quota = 6;
            g.left = 600.0f;
            g.abyss = true;
            m.queue.push_back(g);
        }
    }
    AdvGoal g;   // then a stint at a depth
    g.task = AdvTask::Abyss;
    g.a = level;
    g.b = pk ? 1 : 0;   // 1 = he picks fights with other players
    g.left = seconds;
    m.queue.push_back(g);
    m.doing = "setting out for the Abyss";
    ++m.abyssTrips;
}

Adventurer* Adventurers::PartnerOf(const Adventurer& a) {
    if (a.mind.partner == 0 || clock_ >= a.mind.partnerUntil) return nullptr;
    Adventurer* p = FindMutable(a.mind.partner);
    if (!p || p->pvp.dead || p->mind.partner != a.id) return nullptr;
    return p;
}

// Setting out for the Abyss: what he can gather there, then a stint of roaming at a depth that suits his courage. Now and then; hurt, hungry or weak
// (under combat level 18) he stays at home. Often he asks another adventurer who is free and fit to come along: the two go together, follow each
// other about, never fight each other and help each other in a fight.
bool Adventurers::PlanAbyssTrip(Adventurer& a, AdvWorld& w) {
    AdvMind& m = a.mind;
    Skills& sk = a.skills;
    if (clock_ < m.nextAbyss || a.Combat() < 18 || FoodCount(a.bag) < 3 || sk.Hp() * 10 < sk.MaxHp() * 8) return false;
    if (!m.abyssEager && Rand() > 0.12) return false;
    bool exists = false;
    for (const AreaDef& d : w.map->Areas()) exists = exists || d.name == "The Abyss";
    if (!exists) return false;
    m.nextAbyss = clock_ + 900.0f + static_cast<float>(Rand()) * 900.0f;
    const int level = std::clamp(static_cast<int>(a.Combat() * (0.35 + Rand() * 1.1)), 2, kAbyssMaxLevel);
    const bool pk = Rand() < 0.4;
    const float seconds = 240.0f + static_cast<float>(Rand()) * 300.0f;
    m.partner = 0;
    m.follower = false;
    QueueAbyssTrip(a, level, pk, seconds);
    if (m.abyssEager || Rand() < 0.6) {   // a companion: the nearest adventurer who is free and fit
        Adventurer* mate = nullptr;
        int best = 1 << 20;
        for (Adventurer& o : advs_) {
            AdvMind& om = o.mind;
            if (&o == &a || o.kind != AdvKind::Real || om.hasGoal || !om.queue.empty() || om.phase != AdvPhase::Plan || om.partner != 0) continue;
            if (clock_ < om.nextAbyss || o.Combat() < 18 || FoodCount(o.bag) < 3 || o.skills.Hp() * 10 < o.skills.MaxHp() * 8) continue;
            const int d = std::max(std::abs(o.Tile().x - a.Tile().x), std::abs(o.Tile().z - a.Tile().z));
            if (d < best) {
                best = d;
                mate = &o;
            }
        }
        if (mate) {
            mate->mind.nextAbyss = clock_ + 900.0f + static_cast<float>(Rand()) * 900.0f;
            QueueAbyssTrip(*mate, level, pk, seconds);
            m.partner = mate->id;
            m.partnerUntil = mate->mind.partnerUntil = clock_ + 1800.0f;
            mate->mind.partner = a.id;
            mate->mind.follower = true;
            mate->mind.thinkLeft = 0.0f;
            m.doing = "setting out for the Abyss with " + mate->name;
            mate->mind.doing = "setting out for the Abyss with " + a.name;
        }
    }
    return true;
}

void Adventurers::PlanSession(Adventurer& a, AdvWorld& w) {
    AdvMind& m = a.mind;
    Skills& sk = a.skills;
    const int coins = a.bag.Count(Item::Coins);
    // 1. hurt, with nothing to eat: sit down in town until he is better
    if (sk.Hp() * 10 < sk.MaxHp() * 6 && FoodCount(a.bag) == 0) {
        AdvGoal g;
        g.task = AdvTask::Rest;
        g.a = std::min(270, (sk.MaxHp() - sk.Hp()) * 30);
        m.queue.push_back(g);
        m.doing = "resting";
        return;
    }
    // 2. nearly full bag: sell what he does not need
    if (a.bag.FreeSlots() <= 3 && PlanShopTrip(a, w, true, {}, -1)) {
        m.doing = "heading to the shop";
        return;
    }
    // 2b. now and then, an expedition into the Abyss (when he is strong enough, fed and well)
    if (PlanAbyssTrip(a, w)) return;
    // 3. a quest he is on
    if (PlanQuestStep(a, w)) return;
    // 4. something he can afford that makes him better
    if (WantsUpgrade(a, w)) {
        m.doing = "going shopping for gear";
        return;
    }
    // 5. a new quest
    if (Rand() < 0.45) {
        std::vector<int> candidates;
        for (int qi = 0; qi < a.quests.Count(); ++qi)
            if (!a.quests.State(qi).started && !a.quests.State(qi).done && QuestFeasible(a, w, qi)) candidates.push_back(qi);
        if (!candidates.empty()) {
            const int qi = candidates[static_cast<size_t>(RandInt(static_cast<int>(candidates.size())))];
            const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
            AdvGoal g;
            g.task = AdvTask::Talk;
            g.key = q.giver;
            g.quest = qi;
            g.startQuest = true;
            g.left = 1200.0f;
            m.queue.push_back(g);
            m.doing = "off to find someone about a quest";
            return;
        }
    }
    // 6. training: what he likes, and what he is worst at
    const int cb = a.Combat();
    const Species target = BestFightTarget(a, w);
    double weight[5];
    weight[0] = m.bias[static_cast<int>(Skill::Woodcutting)] / (1.0 + sk.Level(Skill::Woodcutting) * 0.06);
    weight[1] = m.bias[static_cast<int>(Skill::Fishing)] / (1.0 + sk.Level(Skill::Fishing) * 0.06);
    weight[2] = m.bias[static_cast<int>(Skill::Mining)] / (1.0 + sk.Level(Skill::Mining) * 0.06);
    weight[3] = m.bias[static_cast<int>(Skill::Smithing)] / (1.0 + sk.Level(Skill::Smithing) * 0.06);
    weight[4] = target == Species::Count ? 0.0
                                         : (m.bias[static_cast<int>(Skill::Attack)] + m.bias[static_cast<int>(Skill::Strength)] + m.bias[static_cast<int>(Skill::Defence)]) / 3.0 /
                                               (1.0 + cb * 0.05);
    if (BestPickaxe(a.bag, sk.Level(Skill::Mining)) == Item::None) weight[2] = weight[3] = 0.0;
    if (!a.bag.Has(Item::Hammer)) weight[3] = 0.0;
    double total = 0;
    for (double x : weight) total += x;
    if (total <= 0) return;
    double r = Rand() * total;
    int pick = 0;
    for (; pick < 4; ++pick) {
        if (r < weight[pick]) break;
        r -= weight[pick];
    }
    const int free = std::max(4, a.bag.FreeSlots() - 4);
    if (pick == 0) {   // woodcutting (and perhaps firemaking)
        const int wcl = sk.Level(Skill::Woodcutting);
        const Item axe = BestHatchet(a.bag, wcl);
        std::map<std::string, double> best;
        std::string bestKind;
        double bestScore = 0;
        for (const TreeState& t : w.forest->Trees()) {
            if (t.stump) continue;
            const TreeInfo& ti = GetTreeInfo(t.def.kind);
            if (ti.level > wcl || axe == Item::None) continue;
            auto f = best.find(t.def.kind);
            const double dist = Cheb(a.Tile(), t.def.Tile());
            if (f != best.end() && f->second <= dist) continue;
            best[t.def.kind] = dist;
        }
        for (const auto& kv : best) {
            const TreeInfo& ti = GetTreeInfo(kv.first);
            const double score = ti.xp * TreeChopChance(ti, wcl, HatchetSpeed(axe)) / (1.0 + kv.second / 45.0);
            if (score > bestScore) {
                bestScore = score;
                bestKind = kv.first;
            }
        }
        if (!bestKind.empty()) {
            AdvGoal g;
            g.task = AdvTask::Chop;
            g.key = bestKind;
            g.quota = std::min(22, free);
            g.left = 900.0f;
            m.queue.push_back(g);
            if (a.bag.Has(Item::Tinderbox) && Rand() < std::min(0.85, m.bias[static_cast<int>(Skill::Firemaking)] * 0.55)) {
                AdvGoal b;
                b.task = AdvTask::Burn;
                b.left = 900.0f;
                m.queue.push_back(b);
            }
            m.doing = "training woodcutting";
            return;
        }
    } else if (pick == 1) {   // fishing (and cooking what he catches)
        const int fl = sk.Level(Skill::Fishing);
        std::string bestKind;
        double bestScore = 0;
        for (const char* kind : kSpotKinds) {
            const SpotInfo& spot = GetSpotInfo(kind);
            if (!a.bag.Has(spot.tool) || (spot.bait != Item::None && a.bag.Count(spot.bait) < 10)) continue;
            double wsum = 0, xsum = 0;
            for (const FishDef& f : spot.fish)
                if (f.level <= fl) {
                    wsum += f.weight;
                    xsum += f.weight * FishXp(f.raw);
                }
            if (wsum <= 0) continue;
            const int si = NearestSpot(a, w, kind);
            if (si < 0) continue;
            const double score = xsum / wsum / (1.0 + Cheb(a.Tile(), w.map->Spots()[static_cast<size_t>(si)].Tile()) / 45.0);
            if (score > bestScore) {
                bestScore = score;
                bestKind = kind;
            }
        }
        if (!bestKind.empty()) {
            AdvGoal g;
            g.task = AdvTask::Fish;
            g.key = bestKind;
            g.quota = std::min(20, free);
            g.left = 900.0f;
            m.queue.push_back(g);
            if (Rand() < 0.75) {
                AdvGoal c;
                c.task = AdvTask::Cook;
                c.left = 900.0f;
                m.queue.push_back(c);
            }
            m.doing = "training fishing";
            return;
        }
    } else if (pick == 2 || pick == 3) {   // mining, and perhaps smithing what he digs
        const int ml = sk.Level(Skill::Mining);
        // smithing: pick a piece of gear he does not have and can make, and plan the whole job (ore, bars, hammering)
        if (pick == 3 || Rand() < 0.5) {
            static const GearType kOrder[] = {GearType::Body, GearType::Legs, GearType::Helm, GearType::Shield, GearType::Sword, GearType::Axe, GearType::Mace,
                                              GearType::Chainbody, GearType::Fullhelm, GearType::SquareShield, GearType::Scimitar, GearType::Longsword,
                                              GearType::Boots, GearType::Gauntlets, GearType::Dagger};
            const int sl = sk.Level(Skill::Smithing);
            for (int t = kTierCount - 1; t >= 0; --t) {
                const Tier tier = static_cast<Tier>(t);
                if (sl < GetSmeltRecipe(tier).level) continue;
                for (GearType type : kOrder) {
                    const Item product = GearItem(tier, type);
                    if (product == Item::None || !IsWearable(product)) continue;
                    const SmithRecipe sr = GetSmithRecipe(tier, type);
                    if (sr.product != product || sr.level > sl) continue;
                    Skill rs;
                    int rl;
                    if (ItemRequirement(product, &rs, &rl) && sk.Level(rs) < rl) continue;
                    EquipSlot slot;
                    if (!Equippable(product, &slot)) continue;
                    const Item cur = a.equip.Get(slot);
                    if (cur != Item::None && GearScore(cur) + 2.0f >= GearScore(product)) continue;
                    if (a.bag.Has(product)) continue;
                    std::vector<AdvGoal> plan;
                    if (!PlanAcquire(a, w, product, 1, -1, &plan)) continue;
                    for (AdvGoal& g : plan) m.queue.push_back(g);
                    m.doing = "smithing a " + ItemNameLower(product);
                    return;
                }
            }
        }
        // plain mining: the rock that pays best for his level
        std::string bestKey;
        double bestScore = 0;
        for (int k = 0; k < kRockKindCount; ++k) {
            const RockKind kind = static_cast<RockKind>(k);
            const RockInfo& ri = GetRockInfo(kind);
            if (ri.level > ml || kind == RockKind::Gems || kind == RockKind::Clay) continue;
            int dist = 1 << 20;
            for (const RockState& r : w.rocks->All())
                if (r.kind == kind && !r.depleted) dist = std::min(dist, Cheb(a.Tile(), r.def.Tile()));
            if (dist == (1 << 20)) continue;
            const double score = ri.xp * MineChance(kind, ml) / (1.0 + dist / 45.0);
            if (score > bestScore) {
                bestScore = score;
                bestKey = ri.key;
            }
        }
        if (!bestKey.empty()) {
            AdvGoal g;
            g.task = AdvTask::Mine;
            g.key = bestKey;
            g.quota = std::min(18, free);
            g.left = 900.0f;
            m.queue.push_back(g);
            m.doing = "training mining";
            return;
        }
    } else {   // fighting
        if (target != Species::Count) {
            if (FoodCount(a.bag) < 3 && coins >= 25) {
                const int si = ShopIndex("general_town");
                const int price = si >= 0 ? std::max(1, shops_.BuyPrice(si, Item::Bread)) : 12;
                PlanShopTrip(a, w, false, {{Item::Bread, std::min(10, std::max(1, (coins - 10) / price))}}, -1);
            }
            AdvGoal g;
            g.task = AdvTask::Fight;
            g.a = static_cast<int>(target);
            g.quota = 10 + RandInt(14);
            g.left = 1200.0f;
            m.queue.push_back(g);
            m.doing = std::string("fighting ") + GetSpecies(target).name;
            return;
        }
    }
    // nothing worked out: stand about in town for a while
    AdvGoal g;
    g.task = AdvTask::Rest;
    g.a = 20;
    m.queue.push_back(g);
    m.doing = "taking a break";
}

}  // namespace journey
