#include "game/Gameplay.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "world/Pathfinder.h"

namespace journey {

namespace {

constexpr float kBurySeconds = 1.8f;

int Chebyshev(TileCoord a, TileCoord b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

// "Raw shrimps" -> "raw shrimps" (for sentences).
std::string Lower(const char* s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

TextRun Run(const std::string& text, Rgb color = kMenuWhite) {
    return {text, color};
}

std::string Plural(int n, const char* word) {
    return std::to_string(n) + " " + word + (n == 1 ? "" : "s");
}

// "an iron dagger", "a bronze mace"
std::string WithArticle(const std::string& name) {
    const char c = name.empty() ? 'x' : static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
    return std::string(c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ? "an " : "a ") + name;
}

const char* StationLabel(const StationDef& s) {
    if (s.kind == "anvil") return "Anvil";
    if (s.kind == "tanner") return "Tanning rack";
    if (s.kind == "jeweller") return "Jeweller's bench";
    if (s.kind == "wheel") return "Potter's wheel";
    if (s.kind == "range") return "Cooking range";
    return "Furnace";
}

// Which kind of crafting a station does (false for the furnace and the anvil).
bool StationCraft(const StationDef& s, CraftKind* kind) {
    if (s.kind == "tanner") *kind = CraftKind::Tan;
    else if (s.kind == "jeweller") *kind = CraftKind::Jewellery;
    else if (s.kind == "wheel") *kind = CraftKind::Pottery;
    else return false;
    return true;
}

// "1 copper ore + 1 tin ore"
std::string OreList(const SmeltRecipe& r) {
    std::string out;
    for (int i = 0; i < 2; ++i) {
        if (r.ore[i] == Item::None || r.count[i] <= 0) continue;
        if (!out.empty()) out += " + ";
        out += std::to_string(r.count[i]) + " " + Lower(GetItemInfo(r.ore[i]).name);
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- taps

void Gameplay::WalkTo(float gx, float gz) {
    Cancel();
    const TileCoord target{static_cast<int>(std::floor(gx)), static_cast<int>(std::floor(gz))};
    if (!map_.InBounds(target.x, target.z)) return;
    const PathResult path = FindPathToward(map_, player_.Tile(), target);
    if (!path.found) {
        Say("I can't reach that!", Tone::Warn);
        return;
    }
    player_.SetPath(path.tiles);
    markerPending_ = true;
    markerRed_ = !map_.Walkable(target);
    markerX_ = gx;
    markerZ_ = gz;
}

void Gameplay::TapTree(int ti) {
    Cancel();
    if (ti < 0 || ti >= static_cast<int>(forest_.Trees().size())) return;
    const TreeState& t = forest_.Trees()[ti];
    if (t.stump) {
        Examine({TargetKind::Tree, ti});
        return;
    }
    if (AxeInUse() == Item::None) {
        Say("You need an axe to chop down this tree.", Tone::Warn);
        return;
    }
    const TreeInfo& kind = GetTreeInfo(t.def.kind);
    if (skills_.Level(Skill::Woodcutting) < kind.level) {
        Say("You need a Woodcutting level of " + std::to_string(kind.level) + " to chop down " + WithArticle(Lower(kind.name)) + ".", Tone::Warn);
        return;
    }
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = t.def.x;
    markerZ_ = t.def.z;
    goal_ = Goal::Chop;
    target_ = ti;
    Approach(t.def.Tile());
}

void Gameplay::TapSpot(int si) {
    Cancel();
    if (si < 0 || si >= static_cast<int>(map_.Spots().size())) return;
    const SpotDef& s = map_.Spots()[si];
    const SpotInfo& spot = GetSpotInfo(s.kind);
    if (!bag_.Has(spot.tool)) {
        Say(std::string("You need ") + WithArticle(Lower(GetItemInfo(spot.tool).name)) + " to fish here.", Tone::Warn);
        return;
    }
    if (spot.bait != Item::None && !bag_.Has(spot.bait)) {
        Say(std::string("You need ") + Lower(GetItemInfo(spot.bait).name) + " to fish here.", Tone::Warn);
        return;
    }
    {
        int minLevel = 99;
        for (const FishDef& f : spot.fish) minLevel = std::min(minLevel, f.level);
        if (skills_.Level(Skill::Fishing) < minLevel) {
            Say("You need a Fishing level of " + std::to_string(minLevel) + " to fish here.", Tone::Warn);
            return;
        }
    }
    if (bag_.Full()) {
        Say("Your inventory is too full to hold any more fish.", Tone::Warn);
        return;
    }
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = s.x;
    markerZ_ = s.z;
    goal_ = Goal::Fish;
    target_ = si;
    Approach(s.Tile());
}

void Gameplay::TapRock(int ri) {
    Cancel();
    if (ri < 0 || ri >= static_cast<int>(rocks_.All().size())) return;
    const RockState& r = rocks_.All()[ri];
    const RockInfo& info = GetRockInfo(r.kind);
    if (r.depleted) {
        Examine({TargetKind::Rock, ri});
        return;
    }
    if (BestPickaxe(bag_, skills_.Level(Skill::Mining)) == Item::None) {
        Say("You need a pickaxe to mine this rock.", Tone::Warn);
        return;
    }
    if (skills_.Level(Skill::Mining) < info.level) {
        Say("You need a Mining level of " + std::to_string(info.level) + " to mine " + Lower(info.name) + ".", Tone::Warn);
        return;
    }
    if (bag_.Full()) {
        Say("Your inventory is too full to hold any more ore.", Tone::Warn);
        return;
    }
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = r.def.x;
    markerZ_ = r.def.z;
    goal_ = Goal::Mine;
    target_ = ri;
    Approach(r.def.Tile());
}

void Gameplay::TapStation(int si) {
    Cancel();
    if (si < 0 || si >= static_cast<int>(map_.Stations().size())) return;
    const StationDef& st = map_.Stations()[si];
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = st.x;
    markerZ_ = st.z;
    goal_ = Goal::Station;
    target_ = si;
    Approach(st.Tile());
}

void Gameplay::TapPerson(int id, bool talkOnly) {
    Cancel();
    const Person* p = people_.Find(id);
    if (!p) return;
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = p->x;
    markerZ_ = p->z;
    goal_ = Goal::Person;
    target_ = id;
    targetSub_ = talkOnly ? 1 : 0;
    Approach(p->Tile());
}

void Gameplay::TapFire(int id) {
    Cancel();
    const Fire* f = fires_.Find(id);
    if (!f || !f->Burning()) {
        Say("That fire has burnt out.", Tone::Info);
        return;
    }
    if (bag_.FindRawFood() < 0) {
        Say("You have nothing to cook. Catch some fish, or fight a chicken or a cow.", Tone::Info);
        return;
    }
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = f->tile.x + 0.5f;
    markerZ_ = f->tile.z + 0.5f;
    goal_ = Goal::Cook;
    target_ = id;
    Approach(f->tile);
}

void Gameplay::TapNpc(int id) {
    Cancel();
    const Npc* n = npcs_.Find(id);
    if (!n || n->dead) return;
    if (!WeaponReady(true)) return;   // no arrows / runes
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = n->x;
    markerZ_ = n->z;
    goal_ = Goal::Attack;
    target_ = id;
    Approach(n->Tile());
}

void Gameplay::TapFoe(int id) {
    Cancel();
    FoeView v;
    if (!foes_ || !foes_->Get(id, &v) || !v.alive) return;
    if (AbyssLevel() <= 0 || AbyssLevelAt(map_, static_cast<int>(v.x), static_cast<int>(v.z)) <= 0) {
        Say("You can only fight other adventurers in the Abyss.", Tone::Info);
        return;
    }
    if (!MayFight(v, true)) return;
    if (!WeaponReady(true)) return;
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = v.x;
    markerZ_ = v.z;
    goal_ = Goal::Attack;
    target_ = id;
    Approach({static_cast<int>(v.x), static_cast<int>(v.z)});
}

void Gameplay::TapPile(int id) {
    Cancel();
    const Pile* p = ground_.Find(id);
    if (!p) return;
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = p->tile.x + 0.5f;
    markerZ_ = p->tile.z + 0.5f;
    goal_ = Goal::Take;
    target_ = id;
    targetSub_ = -1;
    Approach(p->tile);
}

void Gameplay::TakeFromPile(int id, int index) {
    Cancel();
    const Pile* p = ground_.Find(id);
    if (!p || index < 0 || index >= static_cast<int>(p->items.size())) return;
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = p->tile.x + 0.5f;
    markerZ_ = p->tile.z + 0.5f;
    goal_ = Goal::Take;
    target_ = id;
    targetSub_ = index;
    Approach(p->tile);
}

void Gameplay::DoItemAction(int slot, ItemAction action) {
    if (slot < 0 || slot >= Inventory::kSlots) return;
    const Item item = bag_.Slot(slot).item;
    if (item == Item::None) return;
    const ItemInfo& info = GetItemInfo(item);
    switch (action) {
        case ItemAction::Light:
            if (!IsLog(item) && item != Item::Tinderbox) return;
            StartLighting();
            break;
        case ItemAction::Bonfire:
            if (!IsLog(item)) return;
            StartLighting(true, item);
            break;
        case ItemAction::Cook: {
            if (!info.raw) return;
            const Fire* f = fires_.NearestBurning(player_.Tile());
            int range = -1, rangeDist = 1 << 20;   // the nearest cooking range (if it is closer than the fire, or there is no fire)
            for (size_t i = 0; i < map_.Stations().size(); ++i) {
                if (map_.Stations()[i].kind != "range") continue;
                const int d = Chebyshev(player_.Tile(), map_.Stations()[i].Tile());
                if (d < rangeDist) {
                    rangeDist = d;
                    range = static_cast<int>(i);
                }
            }
            if (range >= 0 && (!f || rangeDist <= Chebyshev(player_.Tile(), f->tile)) && rangeDist <= 12) {
                TapStation(range);
                break;
            }
            if (!f) {
                Say("You need a fire or a cooking range to cook on. Light some logs with your tinderbox.", Tone::Info);
                return;
            }
            TapFire(f->id);
            break;
        }
        case ItemAction::Eat: {
            if (!info.food) return;
            bag_.TakeSlot(slot);
            const int healed = skills_.Heal(info.heals);
            std::string msg = "You eat the " + Lower(info.name) + ".";
            if (healed > 0) msg += " It heals " + Plural(healed, "hitpoint") + ".";
            Say(msg);
            events_.Changed();
            break;
        }
        case ItemAction::Bury:
            if (info.buryXp <= 0) return;
            StartBury(slot);
            break;
        case ItemAction::Wield:
        case ItemAction::Wear: {
            std::string need;
            const EquipResult r = EquipFromBag(&equip_, &bag_, slot, skills_, &need);
            if (r == EquipResult::Done) {
                if (IsThrown(item)) Say("You take the " + Lower(info.name) + " in your hand. You throw them when you wield no weapon.");
                else if (IsArrow(item)) Say("You load the " + Lower(info.name) + " into your quiver.");
                else Say(std::string(action == ItemAction::Wield ? "You wield the " : "You put on the ") + Lower(info.name) + ".");
                useSlot_ = -1;
                events_.Changed();
            } else if (r == EquipResult::LevelTooLow) {
                Say("You need " + need + " to use the " + Lower(info.name) + ".", Tone::Warn);
            }
            break;
        }
        case ItemAction::Use:
            BeginUse(slot);
            break;
        case ItemAction::Drop: {
            const ItemStack dropped = bag_.TakeSlot(slot);
            ground_.Drop(player_.Tile(), dropped);
            Say(std::string("You drop the ") + Lower(info.name) + ".");
            if (useSlot_ == slot) useSlot_ = -1;
            events_.Changed();
            break;
        }
        case ItemAction::Examine:
            Say(info.examine);
            break;
        case ItemAction::Read:
            ReadScroll(slot);
            break;
    }
}

namespace {
// A teleport scroll takes you to the middle of a named place (the nearest walkable tile to it).
struct ScrollDest {
    Item scroll;
    const char* area;
    const char* place;
};
constexpr ScrollDest kScrolls[] = {
    {Item::CastleScroll, "Market Square", "Castle Town"},
    {Item::MillbrookScroll, "Millbrook Square", "Millbrook"},
    {Item::LakesideScroll, "Lakeside", "Lakeside"},
    {Item::ElmswickScroll, "Elmswick", "Elmswick"},
    {Item::SaltmereScroll, "Saltmere", "Saltmere"},
};
}  // namespace

// The nearest walkable tile to the middle of a named place ({-1,-1} if there is no such place).
TileCoord Gameplay::TileNearArea(const std::string& areaName) const {
    TileCoord centre{-1, -1};
    for (const AreaDef& a : map_.Areas())
        if (a.name == areaName) centre = {(a.rect.x0 + a.rect.x1) / 2, (a.rect.z0 + a.rect.z1) / 2};
    if (centre.x < 0) return centre;
    for (int radius = 0; radius <= 8; ++radius)
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx) {
                const TileCoord t{centre.x + dx, centre.z + dz};
                if (std::max(std::abs(dx), std::abs(dz)) == radius && map_.Walkable(t)) return t;
            }
    return {-1, -1};
}

// Where a hero who has died wakes up: the nearest town he has been to (Castle Town, Millbrook, Lakeside, Elmswick or Saltmere), else the spawn point.
TileCoord Gameplay::RespawnTile(std::string* place) const {
    struct Town {
        const char* area;
        const char* name;
    };
    static const Town kTowns[] = {{"Market Square", "Castle Town"}, {"Millbrook Square", "Millbrook"}, {"Lakeside", "Lakeside"}, {"Elmswick", "Elmswick"}, {"Saltmere", "Saltmere"}};
    const TileCoord here = player_.Tile();
    long best = -1;
    TileCoord out = map_.Spawn();
    if (place) *place = "Castle Town";
    for (const Town& t : kTowns) {
        if (std::string(t.area) != "Market Square" && !visited_.count(t.area)) continue;
        const TileCoord at = TileNearArea(t.area);
        if (at.x < 0) continue;
        const long d = static_cast<long>(at.x - here.x) * (at.x - here.x) + static_cast<long>(at.z - here.z) * (at.z - here.z);
        if (best < 0 || d < best) {
            best = d;
            out = at;
            if (place) *place = t.name;
        }
    }
    return out;
}

void Gameplay::ReadScroll(int slot) {
    const Item item = bag_.Slot(slot).item;
    const ScrollDest* dest = nullptr;
    for (const ScrollDest& d : kScrolls)
        if (d.scroll == item) dest = &d;
    if (!dest) return;
    if (IsDangerousArea(map_.AreaAt(player_.Tile().x, player_.Tile().z))) {
        Say("A strange force stops the magic from working here.", Tone::Warn);
        return;
    }
    if (FightingSomething()) {
        Say("You cannot concentrate on the scroll while something is attacking you.", Tone::Warn);
        return;
    }
    if (!visited_.count(dest->area)) {
        Say(std::string("You have never been to ") + dest->place + ", so the scroll has nothing to take hold of.", Tone::Warn);
        return;
    }
    TileCoord centre{-1, -1};
    for (const AreaDef& a : map_.Areas())
        if (a.name == dest->area) centre = {(a.rect.x0 + a.rect.x1) / 2, (a.rect.z0 + a.rect.z1) / 2};
    if (centre.x < 0) return;
    for (int radius = 0; radius <= 8; ++radius)
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx) {
                const TileCoord t{centre.x + dx, centre.z + dz};
                if (std::max(std::abs(dx), std::abs(dz)) != radius || !map_.Walkable(t)) continue;
                bag_.Remove(item, 1);
                Cancel();
                npcs_.StopAllHunts();
                player_.Teleport(t);
                Say(std::string("You read the scroll. It crumbles to dust as you are whisked to ") + dest->place + ".", Tone::Good);
                events_.Changed();
                return;
            }
}

std::vector<QuestMarker> Gameplay::QuestMarkers() const {
    std::vector<QuestMarker> out;
    const std::vector<QuestInfo>& all = AllQuests();
    auto personAt = [&](const std::string& key, QuestMarker* m) {
        for (const Person& p : people_.All())
            if (p.info && key == p.info->key) {
                m->x = p.x;
                m->z = p.z;
                return true;
            }
        return false;
    };
    for (int i = 0; i < quests_.Count() && i < static_cast<int>(all.size()); ++i) {
        const QuestInfo& q = all[static_cast<size_t>(i)];
        const QuestStep* st = quests_.Current(i);
        QuestMarker m;
        m.quest = q.name;
        bool found = false;
        if (!st) {   // not on it: a person with a quest to give him?
            const QuestState& state = quests_.State(i);
            if (state.started || state.done || !quests_.CanStart(i, skills_)) continue;
            m.offer = true;
            m.what = "A quest: " + q.name;
            found = personAt(q.giver, &m);
        } else {
            m.what = st->objective;
            switch (st->kind) {
                case StepKind::Kill: {   // the nearest of that creature that is alive
                    float best = -1.0f;
                    for (const Npc& n : npcs_.All()) {
                        if (n.species != st->species || n.dead) continue;
                        const float d = (n.x - player_.X()) * (n.x - player_.X()) + (n.z - player_.Z()) * (n.z - player_.Z());
                        if (best < 0.0f || d < best) {
                            best = d;
                            m.x = n.x;
                            m.z = n.z;
                            found = true;
                        }
                    }
                    break;
                }
                case StepKind::Have:
                case StepKind::Talk: found = personAt(st->person, &m); break;
                case StepKind::Reach:
                case StepKind::Use:
                    for (const AreaDef& a : map_.Areas())
                        if (a.name == st->area) {
                            m.x = (a.rect.x0 + a.rect.x1) * 0.5f;
                            m.z = (a.rect.z0 + a.rect.z1) * 0.5f;
                            found = true;
                        }
                    break;
            }
        }
        if (found) out.push_back(std::move(m));
    }
    return out;
}

void Gameplay::BeginUse(int slot) {
    if (slot < 0 || slot >= Inventory::kSlots || bag_.Slot(slot).item == Item::None) return;
    Cancel();
    useSlot_ = slot;
    Say(std::string("Use the ") + Lower(GetItemInfo(bag_.Slot(slot).item).name) + " with what?", Tone::Info);
}

bool Gameplay::UseItemOnSlot(int slot) {
    if (useSlot_ < 0) return false;
    const int a = useSlot_;
    useSlot_ = -1;
    if (a == slot || slot < 0 || slot >= Inventory::kSlots) return true;   // tapped the same item again: cancel
    const Item ia = bag_.Slot(a).item, ib = bag_.Slot(slot).item;
    if (ia == Item::None || ib == Item::None) return true;
    if ((ia == Item::Tinderbox && IsLog(ib)) || (IsLog(ia) && ib == Item::Tinderbox)) {
        StartLighting();
        return true;
    }
    // two things that work together anywhere: a needle with leather, a chisel with a gem, flour with water, a knife with logs, a bow with its string...
    CraftKind together;
    if (CraftKindForUse(ia, ib, &together)) {
        Cancel();
        events_.OpenMake(BuildCraftMenu(together));
        return true;
    }
    Say("Nothing interesting happens.");
    return true;
}

void Gameplay::UseItemHere(int slot) {
    const Item item = (slot >= 0 && slot < Inventory::kSlots) ? bag_.Slot(slot).item : Item::None;
    useSlot_ = -1;
    if (item == Item::None) return;
    const std::string& area = map_.AreaAt(player_.Tile().x, player_.Tile().z);
    const int qi = quests_.FindUse(item, area);
    if (qi < 0) {
        Say(quests_.WantsItem(item) ? "This is not the right place to use that." : "Nothing interesting happens.", Tone::Info);
        return;
    }
    const QuestStep st = *quests_.Current(qi);
    if (st.tool != Item::None && !bag_.Has(st.tool)) {
        Say("You need " + WithArticle(Lower(GetItemInfo(st.tool).name)) + " for that.", Tone::Warn);
        return;
    }
    Cancel();
    player_.Stop();
    quests_.CompleteStep(qi, &bag_);   // (a Use step is never the last step of a quest: someone always thanks you at the end)
    if (!st.say.empty()) Say(st.say, Tone::Good);
    if (st.give != Item::None) {
        const int put = bag_.Add(st.give, st.giveCount);
        if (put < st.giveCount) ground_.Drop(player_.Tile(), {st.give, st.giveCount - put});
        Say("You find " + WithArticle(Lower(GetItemInfo(st.give).name)) + ".", Tone::Good);
    }
    QuestProgressMessage(qi);
}

void Gameplay::UseItemOnFire(int slot, int fireId) {
    const Item item = (slot >= 0 && slot < Inventory::kSlots) ? bag_.Slot(slot).item : Item::None;
    useSlot_ = -1;
    if (item != Item::None && GetItemInfo(item).raw) {
        TapFire(fireId);
        return;
    }
    Say("Nothing interesting happens.");
}

void Gameplay::UseItemOnStation(int slot, int si) {
    const Item item = (slot >= 0 && slot < Inventory::kSlots) ? bag_.Slot(slot).item : Item::None;
    useSlot_ = -1;
    if (si < 0 || si >= static_cast<int>(map_.Stations().size())) return;
    const bool furnace = map_.Stations()[si].kind == "furnace";
    if (map_.Stations()[si].kind == "range") {   // raw food goes on the range, nothing else
        if (item != Item::None && GetItemInfo(item).raw) TapStation(si);
        else Say("Nothing interesting happens.");
        return;
    }
    Tier t;
    const bool ore =item == Item::CopperOre || item == Item::TinOre || item == Item::IronOre || item == Item::Coal || item == Item::MithrilOre ||
                     item == Item::SilverOre || item == Item::GoldOre;
    CraftKind ck;
    if (StationCraft(map_.Stations()[si], &ck)) {   // a tanning rack, the jeweller's bench or the wheel takes what its recipes use
        for (const CraftRecipe& r : AllCraftRecipes())
            if (r.kind == ck)
                for (int i = 0; i < 3; ++i)
                    if (r.input[i] == item || r.tool == item) {
                        TapStation(si);
                        return;
                    }
    } else if ((furnace && ore) || (!furnace && (item == Item::Hammer || TierOfBar(item, &t)))) {
        TapStation(si);
        return;
    }
    Say("Nothing interesting happens.");
}

void Gameplay::Unequip(EquipSlot slot) {
    const Item item = equip_.Get(slot);
    const EquipResult r = ::journey::Unequip(&equip_, &bag_, slot);
    if (r == EquipResult::Done) {
        Say("You remove the " + Lower(GetItemInfo(item).name) + ".");
        events_.Changed();
    } else if (r == EquipResult::BagFull) {
        Say("You don't have enough inventory space to take that off.", Tone::Warn);
    }
}

std::vector<MenuEntry> Gameplay::BuildEquipMenu(EquipSlot slot) const {
    std::vector<MenuEntry> out;
    const Item item = equip_.Get(slot);
    if (item == Item::None) return out;
    const char* name = GetItemInfo(item).name;
    MenuEntry rm;
    rm.label = {Run("Remove "), Run(name, kMenuItem)};
    rm.act = MenuAct::RemoveWorn;
    rm.index = static_cast<int>(slot);
    out.push_back(rm);
    MenuEntry ex;
    ex.label = {Run("Examine "), Run(name, kMenuItem)};
    ex.act = MenuAct::ExamineWorn;
    ex.index = static_cast<int>(slot);
    out.push_back(ex);
    MenuEntry c;
    c.label = {Run("Cancel")};
    c.act = MenuAct::Cancel;
    out.push_back(c);
    return out;
}

void Gameplay::Cancel() {
    goal_ = Goal::None;
    walking_ = working_ = false;
    target_ = -1;
    targetSub_ = -1;
    useSlot_ = -1;
    mine_.Stop();
    makeLeft_ = 0;
    craftRecipe_ = -1;
    makeTimer_ = 0.0f;
    chop_.Stop();
    fish_.Stop();
    light_.Stop();
    cookTimer_ = 0.0f;
    buryTimer_ = 0.0f;
}

Activity Gameplay::Current() const {
    if (!working_) return Activity::None;
    switch (goal_) {
        case Goal::Chop: return Activity::Chopping;
        case Goal::Fish: return Activity::Fishing;
        case Goal::Light: return Activity::Lighting;
        case Goal::Cook: return Activity::Cooking;
        case Goal::Bury: return Activity::Burying;
        case Goal::Attack: return Activity::Fighting;
        case Goal::Mine: return Activity::Mining;
        case Goal::Smelt: return Activity::Smelting;
        case Goal::Smith: return Activity::Smithing;
        case Goal::Craft: return Activity::Crafting;
        default: return Activity::None;
    }
}

bool Gameplay::WorkSpot(float* x, float* z) const {
    if (!working_) return false;
    if (goal_ == Goal::Mine && target_ >= 0 && target_ < static_cast<int>(rocks_.All().size())) {
        *x = rocks_.All()[target_].def.x;
        *z = rocks_.All()[target_].def.z;
        return true;
    }
    if ((goal_ == Goal::Smelt || goal_ == Goal::Smith || (goal_ == Goal::Craft && target_ >= 0)) && target_ >= 0 && target_ < static_cast<int>(map_.Stations().size())) {
        *x = map_.Stations()[target_].x;
        *z = map_.Stations()[target_].z;
        return true;
    }
    return false;
}

float Gameplay::WorkPhase() const {
    if (!working_) return 0.0f;
    if (goal_ == Goal::Mine) return mine_.Phase();
    if (goal_ == Goal::Smelt) return makeTimer_ / kSmeltSeconds;
    if (goal_ == Goal::Smith) return std::fmod(makeTimer_ / kSmithSeconds * 4.0f, 1.0f);   // four blows per piece
    if (goal_ == Goal::Craft) return std::fmod(makeTimer_ / kSmithSeconds * 2.0f, 1.0f);
    return 0.0f;
}

bool Gameplay::TakeMarker(float* x, float* z, bool* red) {
    if (!markerPending_) return false;
    markerPending_ = false;
    *x = markerX_;
    *z = markerZ_;
    if (red) *red = markerRed_;
    return true;
}

// ---------------------------------------------------------------------------------------------- menus

std::vector<MenuEntry> Gameplay::BuildMenu(const std::vector<Target>& hits, bool hasGround, float gx, float gz) const {
    std::vector<MenuEntry> out;
    auto add = [&](std::vector<TextRun> label, MenuAct act, Target t) -> MenuEntry& {
        MenuEntry e;
        e.label = std::move(label);
        e.act = act;
        e.target = t;
        out.push_back(std::move(e));
        return out.back();
    };
    const Item usingItem = (useSlot_ >= 0 && useSlot_ < Inventory::kSlots) ? bag_.Slot(useSlot_).item : Item::None;
    const int playerLevel = skills_.CombatLevel();

    for (const Target& t : hits) {
        switch (t.kind) {
            case TargetKind::Tree: {
                if (t.id < 0 || t.id >= static_cast<int>(forest_.Trees().size())) break;
                const bool stump = forest_.Trees()[t.id].stump;
                const TreeInfo& tinfo = GetTreeInfo(forest_.Trees()[t.id].def.kind);
                if (!stump) add({Run("Chop down "), Run(tinfo.name, kMenuObject)}, MenuAct::Chop, t);
                add({Run("Examine "), Run(stump ? "Tree stump" : tinfo.name, kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Spot: {
                if (t.id < 0 || t.id >= static_cast<int>(map_.Spots().size())) break;
                const SpotInfo& sinfo = GetSpotInfo(map_.Spots()[t.id].kind);
                add({Run(std::string(sinfo.verb) + " "), Run(sinfo.name, kMenuObject)}, MenuAct::Fish, t);
                add({Run("Examine "), Run(sinfo.name, kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Fire: {
                const Fire* f = fires_.Find(t.id);
                if (!f || !f->Burning()) break;
                if (usingItem != Item::None) {
                    MenuEntry& e = add({Run("Use "), Run(GetItemInfo(usingItem).name, kMenuItem), Run(" -> "), Run("Fire", kMenuObject)},
                                       MenuAct::UseOnFire, t);
                    e.slot = useSlot_;
                }
                add({Run("Cook "), Run("Fire", kMenuObject)}, MenuAct::Cook, t);
                add({Run("Examine "), Run("Fire", kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Npc: {
                const Npc* n = npcs_.Find(t.id);
                if (!n || n->dead) break;
                const SpeciesInfo& info = n->Info();
                const Rgb c = LevelColor(playerLevel, info.level);
                const AttackKind kind = Kind();
                const std::string verb = kind == AttackKind::Ranged ? "Shoot " : (kind == AttackKind::Magic ? "Cast " + std::string(GetSpell(spell_).name) + " on " : "Attack ");
                add({Run(verb), Run(info.name, kMenuNpc), Run(" (level-" + std::to_string(info.level) + ")", c)}, MenuAct::Attack, t);
                add({Run("Examine "), Run(info.name, kMenuNpc)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Pile: {
                const Pile* p = ground_.Find(t.id);
                if (!p || p->items.empty()) break;
                if (p->items.size() > 1) add({Run("Take "), Run("all", kMenuItem)}, MenuAct::TakeAll, t);
                for (size_t i = 0; i < p->items.size(); ++i) {
                    MenuEntry& e = add({Run("Take "), Run(GetItemInfo(p->items[i].item).name, kMenuItem)},
                                       p->items.size() == 1 ? MenuAct::TakeAll : MenuAct::TakeItem, t);
                    e.index = static_cast<int>(i);
                }
                MenuEntry& ex = add({Run("Examine "), Run(GetItemInfo(p->items[0].item).name, kMenuItem)}, MenuAct::ExamineItem, t);
                ex.index = 0;
                break;
            }
            case TargetKind::Person: {
                const Person* p = people_.Find(t.id);
                if (!p) break;
                if (std::string(p->info->shop) == "bank") add({Run("Bank "), Run(p->info->name, kMenuNpc)}, MenuAct::Trade, t);
                else if (p->info->shop[0]) add({Run("Trade "), Run(p->info->name, kMenuNpc)}, MenuAct::Trade, t);
                add({Run("Talk-to "), Run(p->info->name, kMenuNpc)}, MenuAct::TalkTo, t);
                add({Run("Examine "), Run(p->info->name, kMenuNpc)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Rock: {
                if (t.id < 0 || t.id >= static_cast<int>(rocks_.All().size())) break;
                const RockState& r = rocks_.All()[t.id];
                const RockInfo& info = GetRockInfo(r.kind);
                if (!r.depleted) add({Run("Mine "), Run(info.name, kMenuObject)}, MenuAct::Mine, t);
                add({Run("Examine "), Run(r.depleted ? "Empty rocks" : info.name, kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Station: {
                if (t.id < 0 || t.id >= static_cast<int>(map_.Stations().size())) break;
                const StationDef& st = map_.Stations()[t.id];
                const bool furnace = st.kind == "furnace";
                if (usingItem != Item::None) {
                    MenuEntry& e = add({Run("Use "), Run(GetItemInfo(usingItem).name, kMenuItem), Run(" -> "), Run(StationLabel(st), kMenuObject)},
                                       MenuAct::UseOnStation, t);
                    e.slot = useSlot_;
                }
                CraftKind menuKind;
                add({Run(StationCraft(st, &menuKind) ? "Use " : (furnace ? "Smelt " : (st.kind == "range" ? "Cook on " : "Smith "))), Run(StationLabel(st), kMenuObject)},
                    MenuAct::UseStation, t);
                add({Run("Examine "), Run(StationLabel(st), kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Ground:
            case TargetKind::Adventurer:
                break;
        }
    }
    if (hasGround) {
        if (usingItem != Item::None && quests_.WantsItem(usingItem)) {   // a quest is waiting for this item to be used: that is what a tap on the ground does now
            MenuEntry& u = add({Run("Use "), Run(GetItemInfo(usingItem).name, kMenuItem), Run(" here")}, MenuAct::UseHere, {TargetKind::Ground, -1});
            u.slot = useSlot_;
        }
        MenuEntry& e = add({Run("Walk here")}, MenuAct::WalkHere, {TargetKind::Ground, -1});
        e.gx = gx;
        e.gz = gz;
    }
    add({Run("Cancel")}, MenuAct::Cancel, {});
    return out;
}

std::vector<MenuEntry> Gameplay::BuildItemMenu(int slot) const {
    std::vector<MenuEntry> out;
    if (slot < 0 || slot >= Inventory::kSlots) return out;
    const Item item = bag_.Slot(slot).item;
    if (item == Item::None) return out;
    const char* name = GetItemInfo(item).name;
    if (useSlot_ >= 0 && useSlot_ != slot && bag_.Slot(useSlot_).item != Item::None) {
        MenuEntry e;
        e.label = {Run("Use "), Run(GetItemInfo(bag_.Slot(useSlot_).item).name, kMenuItem), Run(" -> "), Run(name, kMenuItem)};
        e.act = MenuAct::UseOnItem;
        e.useSlot = useSlot_;
        e.slot = slot;
        out.push_back(e);
    }
    for (ItemAction a : ActionsFor(item, bag_)) {
        MenuEntry e;
        e.label = {Run(std::string(ActionName(a)) + " "), Run(name, kMenuItem)};
        e.act = a == ItemAction::Examine ? MenuAct::ExamineItem : MenuAct::ItemAct;
        e.slot = slot;
        e.itemAction = a;
        out.push_back(e);
    }
    MenuEntry c;
    c.label = {Run("Cancel")};
    c.act = MenuAct::Cancel;
    out.push_back(c);
    return out;
}

void Gameplay::Perform(const MenuEntry& e) {
    switch (e.act) {
        case MenuAct::WalkHere: WalkTo(e.gx, e.gz); break;
        case MenuAct::Chop: TapTree(e.target.id); break;
        case MenuAct::Fish: TapSpot(e.target.id); break;
        case MenuAct::Mine: TapRock(e.target.id); break;
        case MenuAct::Trade: TapPerson(e.target.id, false); break;
        case MenuAct::TalkTo: TapPerson(e.target.id, true); break;
        case MenuAct::UseStation: TapStation(e.target.id); break;
        case MenuAct::UseOnStation: UseItemOnStation(e.slot, e.target.id); break;
        case MenuAct::RemoveWorn: Unequip(static_cast<EquipSlot>(e.index)); break;
        case MenuAct::ExamineWorn:
            if (e.index >= 0 && e.index < kEquipSlotCount) Say(GetItemInfo(equip_.Get(static_cast<EquipSlot>(e.index))).examine);
            break;
        case MenuAct::Cook: TapFire(e.target.id); break;
        case MenuAct::UseOnFire: UseItemOnFire(e.slot, e.target.id); break;
        case MenuAct::Attack: TapNpc(e.target.id); break;
        case MenuAct::TakeAll: TapPile(e.target.id); break;
        case MenuAct::TakeItem: TakeFromPile(e.target.id, e.index); break;
        case MenuAct::ExamineTarget: Examine(e.target); break;
        case MenuAct::ExamineItem:
            if (e.target.kind == TargetKind::Pile) {
                const Pile* p = ground_.Find(e.target.id);
                if (p && e.index >= 0 && e.index < static_cast<int>(p->items.size())) Say(GetItemInfo(p->items[e.index].item).examine);
            } else {
                DoItemAction(e.slot, ItemAction::Examine);
            }
            break;
        case MenuAct::ItemAct: DoItemAction(e.slot, e.itemAction); break;
        case MenuAct::UseOnItem:
            useSlot_ = e.useSlot;
            UseItemOnSlot(e.slot);
            break;
        case MenuAct::UseHere: UseItemHere(e.slot); break;
        case MenuAct::Cancel: break;
    }
}

void Gameplay::Examine(Target t) {
    switch (t.kind) {
        case TargetKind::Tree: {
            if (t.id < 0 || t.id >= static_cast<int>(forest_.Trees().size())) return;
            if (forest_.Trees()[t.id].stump) {
                const int mins = static_cast<int>(std::ceil(forest_.SecondsUntilRegrow(t.id) / 60.0f));
                Say("This tree has been cut down. It will grow back in about " + Plural(mins, "minute") + ".", Tone::Info);
            } else {
                Say(GetTreeInfo(forest_.Trees()[t.id].def.kind).examine);
            }
            break;
        }
        case TargetKind::Spot: Say("I can see some fish swimming in the water."); break;
        case TargetKind::Person: {
            const Person* p = people_.Find(t.id);
            if (p) Say(std::string(p->info->name) + " (" + p->info->title + "): " + p->info->examine);
            break;
        }
        case TargetKind::Rock: {
            if (t.id < 0 || t.id >= static_cast<int>(rocks_.All().size())) return;
            const RockState& r = rocks_.All()[t.id];
            if (r.depleted) {
                const int secs = static_cast<int>(std::ceil(rocks_.SecondsUntilRespawn(t.id)));
                Say("There is no ore left in these rocks. They will refill in about " + Plural(secs, "second") + ".", Tone::Info);
            } else {
                Say(GetRockInfo(r.kind).examine);
            }
            break;
        }
        case TargetKind::Station: {
            if (t.id < 0 || t.id >= static_cast<int>(map_.Stations().size())) return;
            {
                const std::string& k = map_.Stations()[t.id].kind;
                Say(k == "anvil" ? "An anvil, for hammering metal bars into weapons and armour."
                    : k == "tanner" ? "A rack for tanning hides into leather."
                    : k == "jeweller" ? "A jeweller's bench with moulds, for rings and amulets."
                    : k == "wheel" ? "A potter's wheel, for turning clay into pots and bowls."
                    : k == "range" ? "A cooking range. Food cooked here burns far less often than on a fire."
                                   : "A roaring furnace. Ore goes in, bars come out.");
            }
            break;
        }
        case TargetKind::Fire: Say("A strongly burning fire."); break;
        case TargetKind::Npc: {
            const Npc* n = npcs_.Find(t.id);
            if (n) Say(n->Info().examine);
            break;
        }
        case TargetKind::Pile: {
            const Pile* p = ground_.Find(t.id);
            if (p && !p->items.empty()) Say(GetItemInfo(p->items[0].item).examine);
            break;
        }
        case TargetKind::Ground:
        case TargetKind::Adventurer: break;
    }
}

// ---------------------------------------------------------------------------------------------- walking to a target

TileCoord Gameplay::TargetTile() const {
    switch (goal_) {
        case Goal::Chop: return forest_.Trees()[target_].def.Tile();
        case Goal::Fish: return map_.Spots()[target_].Tile();
        case Goal::Mine: return rocks_.All()[target_].def.Tile();
        case Goal::Person: {
            const Person* p = people_.Find(target_);
            return p ? p->Tile() : player_.Tile();
        }
        case Goal::Station: return map_.Stations()[target_].Tile();
        case Goal::Cook: {
            if (targetSub_ == 1) return map_.Stations()[target_].Tile();
            const Fire* f = fires_.Find(target_);
            return f ? f->tile : player_.Tile();
        }
        case Goal::Attack: {
            float tx, tz;
            return TargetAt(target_, &tx, &tz) ? TileCoord{static_cast<int>(tx), static_cast<int>(tz)} : player_.Tile();
        }
        case Goal::Take: {
            const Pile* p = ground_.Find(target_);
            return p ? p->tile : player_.Tile();
        }
        default: return player_.Tile();
    }
}

int Gameplay::NeedDistance() const {
    // Piles you stand ON; people behind a counter you can talk to from a bit further; everything else you stand NEXT to.
    if (goal_ == Goal::Take) return map_.Walkable(TargetTile()) ? 0 : 1;
    if (goal_ == Goal::Person) {
        const Person* p = people_.Find(target_);
        return p ? p->info->reach : 1;
    }
    return 1;
}

void Gameplay::Approach(TileCoord target) {
    const int need = NeedDistance();
    const bool ranged = goal_ == Goal::Attack && Kind() != AttackKind::Melee;
    if (ranged ? InRangeOf(target) : Chebyshev(player_.Tile(), target) <= need) {
        player_.Stop();
        Arrived();
        return;
    }
    PathResult path = ranged ? ShootingPath(target) : FindPathToward(map_, player_.Tile(), target);
    if (!path.found) {
        Say("I can't reach that!", Tone::Warn);
        Cancel();
        return;
    }
    if (goal_ == Goal::Attack && !ranged && !path.tiles.empty() && path.tiles.back() == target) path.tiles.pop_back();   // stop next to it
    if (need > 1) {   // stop as soon as we are close enough (across the counter)
        for (size_t i = 0; i < path.tiles.size(); ++i)
            if (Chebyshev(path.tiles[i], target) <= need) {
                path.tiles.resize(i + 1);
                break;
            }
    }
    player_.SetPath(path.tiles);
    walking_ = true;
    repathLeft_ = 0.4f;
}

void Gameplay::Arrived() {
    walking_ = false;
    const TileCoord tt = TargetTile();
    const int need = NeedDistance();
    if (goal_ == Goal::Attack && Kind() != AttackKind::Melee) {
        float tx, tz;
        if (!TargetAt(target_, &tx, &tz) || !InRangeOf({static_cast<int>(tx), static_cast<int>(tz)})) {
            Say("I can't reach that!", Tone::Warn);
            Cancel();
            return;
        }
    } else if (Chebyshev(player_.Tile(), tt) > need) {
        Say("I can't reach that!", Tone::Warn);
        Cancel();
        return;
    }
    switch (goal_) {
        case Goal::Chop: {
            const TreeState& t = forest_.Trees()[target_];
            if (t.stump) {
                Say("Someone beat you to it - that tree is already cut down.", Tone::Info);
                Cancel();
                return;
            }
            if (bag_.Full()) {
                Say("Your inventory is too full to hold any more logs.", Tone::Warn);
                Cancel();
                return;
            }
            player_.FaceToward(t.def.x, t.def.z);
            chop_.Start(target_);
            working_ = true;
            Say("You begin to chop the tree.");
            break;
        }
        case Goal::Fish: {
            const SpotDef& s = map_.Spots()[target_];
            player_.FaceToward(s.x, s.z);
            fish_.Start(target_);
            working_ = true;
            Say(GetSpotInfo(s.kind).start);
            break;
        }
        case Goal::Mine: {
            const RockState& r = rocks_.All()[target_];
            if (r.depleted) {
                Say("There is no ore left in these rocks.", Tone::Info);
                Cancel();
                return;
            }
            if (bag_.Full()) {
                Say("Your inventory is too full to hold any more ore.", Tone::Warn);
                Cancel();
                return;
            }
            player_.FaceToward(r.def.x, r.def.z);
            mine_.Start(target_);
            working_ = true;
            Say("You swing your pickaxe at the rock...");
            break;
        }
        case Goal::Person: {
            Person* p = people_.Find(target_);
            const bool talkOnly = targetSub_ == 1;
            Cancel();
            player_.Stop();
            if (!p) return;
            player_.FaceToward(p->x, p->z);
            p->facing = std::atan2(player_.X() - p->x, player_.Z() - p->z);   // they turn to face him
            Talk(p, talkOnly);
            break;
        }
        case Goal::Station: {
            const StationDef& st = map_.Stations()[target_];
            const int si = target_;
            Cancel();
            player_.Stop();
            player_.FaceToward(st.x, st.z);
            CraftKind craftKind;
            if (st.kind == "range") {
                if (bag_.FindRawFood() < 0) {
                    Say("You have nothing to cook. Catch some fish, or fight a chicken or a cow.", Tone::Info);
                    return;
                }
                goal_ = Goal::Cook;   // cooking on a range is cooking on a fire that never goes out (targetSub_ 1 = a range, target_ = the station)
                target_ = si;
                targetSub_ = 1;
                cookTimer_ = 0.0f;
                working_ = true;
            } else if (StationCraft(st, &craftKind)) {
                events_.OpenMake(BuildCraftMenu(craftKind));
            } else if (st.kind == "furnace") {
                bool anyOre = false;
                for (Item o : {Item::CopperOre, Item::TinOre, Item::IronOre, Item::Coal, Item::MithrilOre, Item::SilverOre, Item::GoldOre}) anyOre = anyOre || bag_.Has(o);
                if (!anyOre) {
                    Say("You have no ore to smelt. Mine some copper and tin first.", Tone::Info);
                    return;
                }
                events_.OpenMake(BuildSmeltMenu());
            } else {
                if (!bag_.Has(Item::Hammer)) {
                    Say("You need a hammer to work the metal with.", Tone::Warn);
                    return;
                }
                int best = -1;
                for (int t = 0; t < kTierCount; ++t)
                    if (bag_.Has(BarFor(static_cast<Tier>(t)))) best = t;
                if (best < 0) {
                    Say("You have no metal bars to smith. Smelt some ore at the furnace first.", Tone::Info);
                    return;
                }
                events_.OpenMake(BuildSmithMenu(static_cast<Tier>(best)));
            }
            (void)si;
            break;
        }
        case Goal::Cook: {
            const Fire* f = fires_.Find(target_);
            if (!f || !f->Burning()) {
                Say("That fire has burnt out.", Tone::Info);
                Cancel();
                return;
            }
            player_.FaceToward(f->tile.x + 0.5f, f->tile.z + 0.5f);
            cookTimer_ = 0.0f;
            working_ = true;
            break;
        }
        case Goal::Attack: {
            float tx, tz;
            if (!TargetAt(target_, &tx, &tz)) {
                Cancel();
                return;
            }
            StartFight(target_, false);
            break;
        }
        case Goal::Take: {
            const int pile = target_, index = targetSub_;
            Cancel();
            player_.Stop();
            if (!ground_.Find(pile)) {
                Say("Too late - it is gone.", Tone::Info);
                return;
            }
            TakeNow(pile, index);
            break;
        }
        default:
            Cancel();
            break;
    }
}

void Gameplay::TakeNow(int pileId, int index) {
    const Pile* p = ground_.Find(pileId);
    if (!p) return;
    if (index < 0) {
        const std::vector<ItemStack> got = ground_.TakeAll(pileId, &bag_);
        for (const ItemStack& s : got) {
            const std::string name = Lower(GetItemInfo(s.item).name);
            Say(s.count > 1 ? "You take " + std::to_string(s.count) + " " + name + "." : "You take the " + name + ".");
        }
        if (ground_.Find(pileId) != nullptr) Say("Your inventory is too full to hold any more.", Tone::Warn);
        if (!got.empty()) events_.Changed();
    } else {
        const Item item = index < static_cast<int>(p->items.size()) ? p->items[index].item : Item::None;
        ItemStack got;
        if (ground_.TakeOne(pileId, index, &bag_, &got)) {
            Say("You take the " + Lower(GetItemInfo(item).name) + ".");
            events_.Changed();
        } else {
            Say("Your inventory is too full to hold any more.", Tone::Warn);
        }
    }
}

void Gameplay::StartLighting(bool bonfire, Item bonfireLog) {
    Cancel();
    if (!bag_.Has(Item::Tinderbox)) {
        Say("You need a tinderbox to light a fire.", Tone::Warn);
        return;
    }
    if (bonfire) {   // four logs of the kind he picked
        const LogInfo* li = GetLogInfo(bonfireLog);
        if (!li || bag_.Count(bonfireLog) < kBonfireLogs) {
            Say("A bonfire needs " + std::to_string(kBonfireLogs) + " logs of the same kind.", Tone::Warn);
            return;
        }
        if (skills_.Level(Skill::Firemaking) < li->fmLevel) {
            Say("You need a Firemaking level of " + std::to_string(li->fmLevel) + " to burn " + Lower(GetItemInfo(bonfireLog).name) + ".", Tone::Warn);
            return;
        }
        if (!fires_.CanLight(map_, player_.Tile())) {
            Say("You can't light a fire here.", Tone::Warn);
            return;
        }
        player_.Stop();
        goal_ = Goal::Light;
        lightLog_ = bonfireLog;
        lightBig_ = true;
        light_.Start();
        working_ = true;
        Say("You heap the logs up and try to light the bonfire.");
        return;
    }
    lightBig_ = false;
    const Item log = BurnableLog(bag_, skills_.Level(Skill::Firemaking));
    if (log == Item::None) {
        if (const Item any = AnyLog(bag_); any != Item::None)
            Say("You need a Firemaking level of " + std::to_string(GetLogInfo(any)->fmLevel) + " to burn " + Lower(GetItemInfo(any).name) + ".", Tone::Warn);
        else
            Say("You have no logs to light.", Tone::Warn);
        return;
    }
    if (!fires_.CanLight(map_, player_.Tile())) {
        Say("You can't light a fire here.", Tone::Warn);
        return;
    }
    player_.Stop();
    goal_ = Goal::Light;
    lightLog_ = log;
    light_.Start();
    working_ = true;
    Say("You attempt to light the logs.");
}

void Gameplay::StartBury(int slot) {
    const Item item = bag_.Slot(slot).item;
    Cancel();
    player_.Stop();
    goal_ = Goal::Bury;
    buryItem_ = item;
    buryTimer_ = 0.0f;
    working_ = true;
    Say(std::string("You dig a hole in the ground..."));
}

// ---------------------------------------------------------------------------------------------- fighting

int Gameplay::HeroSwingTicks() const {
    if (Kind() == AttackKind::Ranged && !IsBow(equip_.Get(EquipSlot::Weapon))) return kThrowTicks;   // darts
    return Kind() == AttackKind::Magic ? kDefaultSwingTicks : WeaponTicks(equip_.Get(EquipSlot::Weapon));   // a bow has its own pace (shortbow 3, longbow 5); a staff casts every 4
}

float Gameplay::HeroSwingSeconds() const { return static_cast<float>(HeroSwingTicks()) * kTickSeconds; }

Fighter Gameplay::PlayerNow() const {
    const EquipBonuses b = equip_.Total();
    Fighter f = PlayerFighter(skills_, style_, b.attack, b.strength, b.defence);
    f.swingScale = static_cast<float>(HeroSwingTicks()) / static_cast<float>(kDefaultSwingTicks);   // a slow weapon hits harder per swing
    return f;
}

Fighter Gameplay::NpcFighter(const Npc& n) const {
    const SpeciesInfo& s = n.Info();
    Fighter f;
    f.attack = s.attack;
    f.strength = s.strength;
    f.defence = s.defence;
    f.creature = true;
    return f;
}

bool Gameplay::CreatureCanStrike(const Npc& n) const {
    if (!n.Alive() || !n.hunting) return false;
    const SpeciesInfo& info = n.Info();
    const int d = Chebyshev(n.Tile(), player_.Tile());
    if (d <= 1) return true;
    return info.attackKind != AttackKind::Melee && info.attackRange > 1 && d <= info.attackRange &&
           map_.ProjectileClear(n.Tile(), player_.Tile());
}

void Gameplay::Poison(int damage) {
    if (damage <= 0 || poisonDamage_ >= damage) {
        if (poisonDamage_ > 0) poisonTicks_ = 0;   // bitten again: the poison starts over
        return;
    }
    if (poisonDamage_ <= 0) Say("You have been poisoned!", Tone::Warn);
    poisonDamage_ = damage;
    poisonTicks_ = 0;
    poisonTimer_ = 0.0f;
}

bool Gameplay::FightingSomething() const {
    if (goal_ == Goal::Attack && working_) return true;
    if (foeHitLeft_ > 0.0f) return true;
    for (const Npc& n : npcs_.All())
        if (CreatureCanStrike(n)) return true;
    return false;
}

void Gameplay::StartFight(int npcId, bool fromAutoRetaliate) {
    float tx, tz;
    if (!TargetAt(npcId, &tx, &tz)) return;
    const bool alreadyFighting = FightingSomething();
    goal_ = Goal::Attack;
    target_ = npcId;
    walking_ = false;
    working_ = true;
    player_.FaceToward(tx, tz);
    if (!IsFoeId(npcId)) npcs_.StartHunt(npcId);
    if (!alreadyFighting) {   // the first strike comes half a second after we reach it
        turnTimer_ = HeroSwingSeconds() - 0.5f;                    // his first swing comes half a second after he arrives...
        creatureTimer_ = kCreatureSwingSeconds - (0.5f + kTurnSeconds);   // ...and theirs a little after that
    }
    (void)fromAutoRetaliate;
}

AttackKind Gameplay::Kind() const {
    const Item w = equip_.Get(EquipSlot::Weapon);
    if (IsBow(w)) return AttackKind::Ranged;
    if (IsStaff(w)) return AttackKind::Magic;
    if (w == Item::None && IsThrown(equip_.Get(EquipSlot::Ammo)) && equip_.AmmoCount() > 0) return AttackKind::Ranged;   // darts, thrown with an empty hand
    return AttackKind::Melee;
}

constexpr int kSpellRange = 8;   // how far a spell flies, in tiles

int Gameplay::AttackReach() const {
    switch (Kind()) {
        case AttackKind::Ranged: {
            const BowInfo* bow = FindBow(equip_.Get(EquipSlot::Weapon));
            const int base = bow ? bow->range : (IsThrown(equip_.Get(EquipSlot::Ammo)) ? kThrowRange : 7);
            return base + (style_ == CombatStyle::Defensive ? kLongrangeBonus : 0);
        }
        case AttackKind::Magic: return kSpellRange;
        default: return 1;
    }
}

void Gameplay::SetSpell(int index) {
    if (index >= 0 && index < SpellCount()) spell_ = index;
}

bool Gameplay::InAttackRange(const Npc& n) const { return InRangeOf(n.Tile()); }

bool Gameplay::InRangeOf(TileCoord t) const {
    const int d = Chebyshev(player_.Tile(), t);
    if (d <= 1) return true;
    if (Kind() == AttackKind::Melee) return false;
    return d <= AttackReach() && map_.ProjectileClear(player_.Tile(), t);
}

bool Gameplay::TargetAt(int id, float* x, float* z) const {
    if (IsFoeId(id)) {
        FoeView v;
        if (!foes_ || !foes_->Get(id, &v) || !v.alive) return false;
        *x = v.x;
        *z = v.z;
        return true;
    }
    const Npc* n = npcs_.Find(id);
    if (!n || n->dead) return false;
    *x = n->x;
    *z = n->z;
    return true;
}

Fighter Gameplay::DefenderOf(int id) const {
    if (IsFoeId(id)) {
        FoeView v;
        if (foes_ && foes_->Get(id, &v)) return v.fighter;
        return Fighter{};
    }
    const Npc* n = npcs_.Find(id);
    return n ? NpcFighter(*n) : Fighter{};
}

bool Gameplay::MayFight(const FoeView& v, bool say) {
    const int mine = AbyssLevel(), theirs = AbyssLevelAt(map_, static_cast<int>(v.x), static_cast<int>(v.z));
    if (CanFightPlayers(skills_.CombatLevel(), v.combat, mine, theirs)) return true;
    if (say)
        Say("You cannot fight " + v.name + " here: at Abyss level " + std::to_string(std::max(1, std::min(mine, theirs))) + " players can only fight others within " +
                std::to_string(std::max(1, std::min(mine, theirs))) + " combat levels of their own (they are level " + std::to_string(v.combat) + ", you are level " +
                std::to_string(skills_.CombatLevel()) + ").",
            Tone::Warn);
    return false;
}

bool Gameplay::Provoked(int foeId) const {
    for (const auto& p : foeAggro_)
        if (p.first == foeId && p.second > 0.0f) return true;
    return false;
}

void Gameplay::StrikeHero(int foeId, int damage) {
    pending_.push_back({0.5f, true, foeId, damage});
    foeHitLeft_ = 8.0f;
    bool known = false;
    for (auto& p : foeAggro_)
        if (p.first == foeId) {
            p.second = 30.0f;
            known = true;
        }
    if (!known) foeAggro_.push_back({foeId, 30.0f});
    blockAge_ = 0.0f;
    blockLanded_ = damage > 0;
}

// Experience for a landed blow (the same numbers as against a creature).
void Gameplay::AwardHitXp(AttackKind kind, int damage) {
    if (damage <= 0) return;
    const CombatXp xp = kind == AttackKind::Ranged ? XpForRangedDamage(style_, damage) : kind == AttackKind::Magic ? XpForSpellDamage(damage) : XpForDamage(style_, damage);
    if (xp.attack > 0) GainXp(Skill::Attack, xp.attack);
    if (xp.strength > 0) GainXp(Skill::Strength, xp.strength);
    if (xp.defence > 0) GainXp(Skill::Defence, xp.defence);
    if (xp.ranged > 0) GainXp(Skill::Ranged, xp.ranged);
    if (xp.magic > 0) GainXp(Skill::Magic, xp.magic);
    hpXpCarry_ += damage * 4.0 / 3.0;
    const int hpXp = static_cast<int>(hpXpCarry_ + 1e-9);
    hpXpCarry_ -= hpXp;
    if (hpXp > 0) GainXp(Skill::Hitpoints, hpXp);
    events_.Changed();
}

void Gameplay::ApplyFoeHit(const PendingHit& h) {
    FoeView v;
    if (!foes_ || !foes_->Get(h.npcId, &v) || !v.alive) return;
    events_.Hit(false, h.npcId, h.damage);
    AwardHitXp(h.kind, h.damage);
    if (foes_->HeroHits(h.npcId, h.damage)) {
        ++pvpKills_;
        Say("You have defeated " + v.name + "! (Players killed: " + std::to_string(pvpKills_) + ")", Tone::Good);
        if (goal_ == Goal::Attack && target_ == h.npcId) Cancel();
    }
}

PathResult Gameplay::ShootingPath(TileCoord target) const {
    const int range = AttackReach();
    struct Cand {
        TileCoord t;
        int d2;
    };
    std::vector<Cand> cands;
    const TileCoord me = player_.Tile();
    for (int dz = -range; dz <= range; ++dz)
        for (int dx = -range; dx <= range; ++dx) {
            const TileCoord t{target.x + dx, target.z + dz};
            if (!map_.Walkable(t) || (dx == 0 && dz == 0)) continue;
            if (Chebyshev(t, target) > 1 && !map_.ProjectileClear(t, target)) continue;
            cands.push_back({t, (t.x - me.x) * (t.x - me.x) + (t.z - me.z) * (t.z - me.z)});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
    for (size_t i = 0; i < cands.size() && i < 12; ++i) {   // the nearest few that can really be walked to
        if (cands[i].t == me) return {true, {}};
        PathResult p = FindPath(map_, me, cands[i].t);
        if (p.found) return p;
    }
    return {};
}

// Does the weapon in his hand have what it needs? (arrows for a bow, runes and level for a staff)
bool Gameplay::WeaponReady(bool say) {
    const AttackKind kind = Kind();
    if (kind == AttackKind::Ranged) {
        const Item arrows = equip_.Get(EquipSlot::Ammo);
        if (arrows == Item::None || equip_.AmmoCount() <= 0) {
            if (say) Say("You have no arrows to shoot. Wield some arrows from your bag.", Tone::Warn);
            return false;
        }
        Skill skill;
        int level;
        if (ItemRequirement(arrows, &skill, &level) && skills_.Level(skill) < level) {
            if (say) Say("You need Ranged level " + std::to_string(level) + (IsThrown(arrows) ? " to throw these darts." : " to shoot these arrows."), Tone::Warn);
            return false;
        }
        return true;
    }
    if (kind == AttackKind::Magic) {
        const SpellInfo& sp = GetSpell(spell_);
        const CastCheck c = CheckCast(sp, bag_, equip_.Get(EquipSlot::Weapon), skills_.Level(Skill::Magic));
        if (c == CastCheck::LevelTooLow) {
            if (say) Say(std::string("You need Magic level ") + std::to_string(sp.level) + " to cast " + sp.name + ".", Tone::Warn);
            return false;
        }
        if (c == CastCheck::NoRunes) {
            if (say) Say(std::string("You do not have enough runes to cast ") + sp.name + ".", Tone::Warn);
            return false;
        }
    }
    return true;
}

// A shot or a spell: the missile leaves after the draw (or the gesture) and lands when it arrives.
void Gameplay::StrikeAtRange(int id, float x, float z, AttackKind kind) {
    const EquipBonuses b = equip_.Total();   // (before an arrow leaves the quiver)
    ShotFx fx;
    fx.fromX = player_.X();
    fx.fromZ = player_.Z();
    fx.toX = x;
    fx.toZ = z;
    fx.npcId = id;
    const float dist = std::hypot(x - player_.X(), z - player_.Z());
    fx.flight = 0.08f + 0.04f * dist;
    PendingHit hit{fx.delay + fx.flight, false, id, 0, kind, Item::None};
    if (kind == AttackKind::Ranged) {
        const Item arrows = equip_.Get(EquipSlot::Ammo);
        const ArrowInfo* ai = FindArrow(arrows);
        fx.metal = ai ? ai->metal : 0;
        equip_.UseAmmo(1);
        Fighter shooter = RangedFighter(skills_, style_, b.ranged, b.rangedStrength, b.defence);
        shooter.swingScale = static_cast<float>(HeroSwingTicks()) / static_cast<float>(kDefaultSwingTicks);   // a longbow hits harder per shot, a shortbow lighter
        const AttackResult r = RollAttack(shooter, DefenderOf(id), roll_);
        hit.damage = r.damage;
        if (roll_() < kArrowSurvival) hit.arrow = arrows;   // this one can be picked up again
        events_.Changed();
    } else {
        const SpellInfo& sp = GetSpell(spell_);
        PayRunes(sp, &bag_, equip_.Get(EquipSlot::Weapon));
        const AttackResult r = RollSpell(MagicFighter(skills_, b.magic, b.defence), sp.maxHit, DefenderOf(id), roll_);
        hit.damage = r.damage;
        fx.magic = true;
        fx.color = SpellColor(sp);
        GainXp(Skill::Magic, sp.xp);
        events_.Changed();
    }
    pending_.push_back(hit);
    events_.Shoot(fx);
    strikeAge_ = 0.0f;
    strikeLeft_ = !strikeLeft_;
    if (!IsFoeId(id)) npcs_.RefreshHunt(id);
}

void Gameplay::PlayerTurn() {
    if (!(goal_ == Goal::Attack && working_)) return;
    float tx, tz;
    if (!TargetAt(target_, &tx, &tz)) return;
    const TileCoord tt{static_cast<int>(tx), static_cast<int>(tz)};
    if (IsFoeId(target_)) {   // another player: the first blow at somebody who is not skulled (and did not start it) puts a skull on him
        FoeView v;
        if (foes_ && foes_->Get(target_, &v) && !MayFight(v, true)) {   // (he walked up or down the Abyss out of the gap of levels)
            Cancel();
            return;
        }
        if (foes_ && foes_->Get(target_, &v) && !v.skulled && !Provoked(target_) && (Kind() != AttackKind::Melee || Chebyshev(player_.Tile(), tt) <= 1)) {
            if (skullLeft_ <= 0.0f) Say("You attack " + v.name + ". A skull appears over your head for ten minutes: if you die now you lose everything.", Tone::Warn);
            skullLeft_ = kSkullSeconds;
        }
    }
    const AttackKind kind = Kind();
    if (kind != AttackKind::Melee) {
        if (!InRangeOf(tt)) return;   // (it is out of reach or out of sight: Update walks after it)
        if (!WeaponReady(true)) {     // out of arrows or runes: the fight goes on without him striking
            Cancel();
            return;
        }
        player_.FaceToward(tx, tz);
        StrikeAtRange(target_, tx, tz, kind);
        return;
    }
    if (Chebyshev(player_.Tile(), tt) > 1) return;
    player_.FaceToward(tx, tz);
    const AttackResult r = RollAttack(PlayerNow(), DefenderOf(target_), roll_);
    pending_.push_back({kImpactSeconds, false, target_, r.damage});
    strikeAge_ = 0.0f;
    strikeLeft_ = !strikeLeft_;
    if (!IsFoeId(target_)) npcs_.RefreshHunt(target_);
}

void Gameplay::CreatureTurn() {
    const TileCoord me = player_.Tile();
    int firstAttacker = -1;
    for (Npc& n : npcs_.All()) {
        if (!CreatureCanStrike(n)) continue;
        const SpeciesInfo& info = n.Info();
        const AttackResult r = RollAttack(NpcFighter(n), PlayerNow(), roll_);
        float delay = kImpactSeconds;
        if (Chebyshev(n.Tile(), me) > 1) {   // a shooter or a caster: the missile flies
            ShotFx fx;
            fx.magic = info.attackKind == AttackKind::Magic;
            fx.fromX = n.x;
            fx.fromZ = n.z;
            fx.toX = player_.X();
            fx.toZ = player_.Z();
            fx.npcId = n.id;
            fx.toHero = true;
            fx.color = info.spellColor;
            fx.delay = kImpactSeconds - 0.1f;
            fx.flight = 0.08f + 0.04f * std::hypot(fx.toX - fx.fromX, fx.toZ - fx.fromZ);
            delay = fx.delay + fx.flight;
            events_.Shoot(fx);
        }
        pending_.push_back({delay, true, n.id, r.damage});
        if (firstAttacker < 0) blockLanded_ = false;
        blockAge_ = 0.0f;               // the hero raises his guard (shield, weapon or fists) as the strike comes in
        blockLanded_ = blockLanded_ || r.damage > 0;
        n.attackT = 0.0f;
        n.facing = std::atan2(player_.X() - n.x, player_.Z() - n.z);
        npcs_.RefreshHunt(n.id);
        if (firstAttacker < 0) firstAttacker = n.id;
    }
    // Auto-retaliate: if we were just standing there, hit back at whoever is hitting us.
    if (firstAttacker >= 0 && goal_ == Goal::None && !player_.Moving()) {
        Cancel();
        StartFight(firstAttacker, true);
    }
}

void Gameplay::GainXp(Skill skill, int amount) {
    events_.XpDrop(skill, amount);
    int newLevel = 0;
    if (skills_.AddXp(skill, amount, &newLevel)) events_.LevelUp(skill, newLevel);
}

void Gameplay::ApplyHit(const PendingHit& h) {
    if (h.onPlayer) {
        if (skills_.Hp() <= 0) return;
        events_.Hit(true, h.npcId, h.damage);
        if (h.damage > 0) {
            skills_.Hurt(h.damage);
            if (const Npc* attacker = npcs_.Find(h.npcId)) Poison(attacker->Info().poison);
        }
        if (skills_.Hp() <= 0) {
            if (IsFoeId(h.npcId)) {   // killed by another player: a death on his record and a kill on theirs
                ++pvpDeaths_;
                if (foes_) foes_->HeroKilledBy(h.npcId);
            }
            PlayerDies();
        } else if (IsFoeId(h.npcId) && goal_ == Goal::None && !player_.Moving()) {   // a player is hitting him and he is standing about: hit back
            Cancel();
            StartFight(h.npcId, true);
        }
        return;
    }
    if (IsFoeId(h.npcId)) {
        ApplyFoeHit(h);
        return;
    }
    const Npc* n = npcs_.Find(h.npcId);
    if (h.arrow != Item::None && n) ground_.Drop(n->Tile(), {h.arrow, 1});   // hit or miss, the arrow lands there
    if (!n || n->dead) return;
    events_.Hit(false, h.npcId, h.damage);
    if (h.damage > 0) {
        const CombatXp xp = h.kind == AttackKind::Ranged ? XpForRangedDamage(style_, h.damage)
                            : h.kind == AttackKind::Magic ? XpForSpellDamage(h.damage)
                                                          : XpForDamage(style_, h.damage);
        if (xp.attack > 0) GainXp(Skill::Attack, xp.attack);
        if (xp.strength > 0) GainXp(Skill::Strength, xp.strength);
        if (xp.defence > 0) GainXp(Skill::Defence, xp.defence);
        if (xp.ranged > 0) GainXp(Skill::Ranged, xp.ranged);
        if (xp.magic > 0) GainXp(Skill::Magic, xp.magic);
        hpXpCarry_ += h.damage * 4.0 / 3.0;                 // 1.33 per point: carry the fractions so none are lost
        const int hpXp = static_cast<int>(hpXpCarry_ + 1e-9);
        hpXpCarry_ -= hpXp;
        if (hpXp > 0) GainXp(Skill::Hitpoints, hpXp);
        events_.Changed();
    }
    const bool died = h.damage > 0 && npcs_.Hurt(h.npcId, h.damage);
    npcs_.RefreshHunt(h.npcId);
    if (died) {
        Say("You have killed the " + Lower(GetSpecies(n->species).name) + ".");
        const int qi = quests_.OnKill(n->species, h.kind);
        if (qi >= 0) QuestProgressMessage(qi);
        if (goal_ == Goal::Attack && target_ == h.npcId) Cancel();
    }
}

bool IsDangerousArea(const std::string& areaName) { return areaName == "The Abyss"; }

// In a dangerous place a dying hero keeps only his three most valuable items (worn ones included; a stack counts as one);
// everything else is left on the ground where he fell, for five minutes.
void Gameplay::DropOnDeath(int keep) {
    struct Entry {
        Item item;
        int count;
        int worn;   // -1 = in the bag, otherwise the EquipSlot
    };
    std::vector<Entry> all;
    for (int s = 0; s < kEquipSlotCount; ++s) {
        const Item it = equip_.Get(static_cast<EquipSlot>(s));
        if (it == Item::None) continue;
        all.push_back({it, static_cast<EquipSlot>(s) == EquipSlot::Ammo ? std::max(1, equip_.AmmoCount()) : 1, s});
    }
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemStack& st = bag_.Slot(i);
        if (st.item != Item::None) all.push_back({st.item, st.count, -1});
    }
    std::stable_sort(all.begin(), all.end(), [](const Entry& a, const Entry& b) { return ItemValue(a.item) > ItemValue(b.item); });
    const TileCoord tile = player_.Tile();
    int lostStacks = 0;
    for (size_t i = static_cast<size_t>(keep); i < all.size(); ++i) {
        const Entry& e = all[i];
        if (e.worn >= 0) {
            if (static_cast<EquipSlot>(e.worn) == EquipSlot::Ammo) equip_.SetAmmo(e.item, 0);
            else equip_.Set(static_cast<EquipSlot>(e.worn), Item::None);
        } else {
            bag_.Remove(e.item, e.count);
        }
        ground_.Drop(tile, {e.item, e.count}, GroundItems::kDeathLifeSeconds);
        ++lostStacks;
    }
    if (lostStacks > 0 && keep == 0)
        Say("You were skulled: you lost EVERYTHING you carried. It lies where you fell for five minutes, for anyone to take.", Tone::Warn);
    else if (lostStacks > 0)
        Say("You lost everything except your three most valuable items. What you dropped lies where you fell for five minutes.", Tone::Warn);
    else
        Say("You had so little that nothing was lost.", Tone::Info);
}

void Gameplay::PlayerDies() {
    Say("Oh dear, you are dead!", Tone::Warn);
    if (IsDangerousArea(map_.AreaAt(player_.Tile().x, player_.Tile().z))) DropOnDeath(skullLeft_ > 0.0f ? 0 : kItemsKeptOnDeath);
    skullLeft_ = 0.0f;
    foeHitLeft_ = 0.0f;
    foeAggro_.clear();
    poisonDamage_ = 0;
    pending_.clear();
    Cancel();
    npcs_.StopAllHunts();
    std::string where;
    player_.Teleport(RespawnTile(&where));   // (the nearest town he has been to)
    if (where != "Castle Town") Say("You wake up in " + where + ".", Tone::Info);
    skills_.SetHp(skills_.MaxHp());
    turnTimer_ = 0.0f;
    creatureTimer_ = 0.0f;
    events_.PlayerDied();
    events_.Changed();
}

void Gameplay::UpdateLoot() {
    for (Npc& n : npcs_.All()) {
        if (!n.dead || !n.lootPending || n.deathT < Npcs::kDeathAnimSeconds) continue;
        n.lootPending = false;
        for (const ItemStack& s : RollLoot(n.species, roll_)) ground_.Drop(n.Tile(), s);
    }
}

void Gameplay::UpdateCombat(float dt) {
    strikeAge_ += dt;
    blockAge_ += dt;
    foeHitLeft_ = std::max(0.0f, foeHitLeft_ - dt);
    for (auto& p : foeAggro_) p.second -= dt;
    foeAggro_.erase(std::remove_if(foeAggro_.begin(), foeAggro_.end(), [](const std::pair<int, float>& p) { return p.second <= 0.0f; }), foeAggro_.end());
    if (skullLeft_ > 0.0f) {
        skullLeft_ -= dt;
        if (skullLeft_ <= 0.0f) {
            skullLeft_ = 0.0f;
            Say("Your skull fades away.", Tone::Info);
        }
    }
    if (poisonDamage_ > 0) {   // poison: a few hit points every 3 seconds, weakening every four ticks
        poisonTimer_ += dt;
        if (poisonTimer_ >= 3.0f) {
            poisonTimer_ -= 3.0f;
            events_.Hit(true, -1, poisonDamage_);
            skills_.Hurt(poisonDamage_);
            if (++poisonTicks_ % 4 == 0 && --poisonDamage_ <= 0) {
                poisonDamage_ = 0;
                Say("The poison has worn off.");
            }
            if (skills_.Hp() <= 0) PlayerDies();
        }
    }
    // hit points creep back
    if (skills_.Hp() < skills_.MaxHp()) {
        regenTimer_ += dt;
        if (regenTimer_ >= kRegenSeconds) {
            regenTimer_ = 0.0f;
            skills_.Heal(1);
        }
    } else {
        regenTimer_ = 0.0f;
    }
    // strikes that are in the air
    for (size_t i = 0; i < pending_.size();) {
        pending_[i].timer -= dt;
        if (pending_[i].timer <= 0.0f) {
            const PendingHit h = pending_[i];
            pending_.erase(pending_.begin() + static_cast<long>(i));
            ApplyHit(h);
        } else {
            ++i;
        }
    }
    // turns
    if (!FightingSomething()) {
        turnTimer_ = 0.0f;
        creatureTimer_ = 0.0f;
        return;
    }
    turnTimer_ += dt;
    creatureTimer_ += dt;
    const float swing = HeroSwingSeconds();
    if (turnTimer_ >= swing) {
        turnTimer_ -= swing;
        PlayerTurn();
    }
    if (creatureTimer_ >= kCreatureSwingSeconds) {
        creatureTimer_ -= kCreatureSwingSeconds;
        CreatureTurn();
    }
}

// ---------------------------------------------------------------------------------------------- per-frame

void Gameplay::Update(float dt) {
    {   // entering a named place can finish a quest step
        const TileCoord here = player_.Tile();
        const std::string& area = map_.AreaAt(here.x, here.z);
        if (area != lastArea_) {
            const bool leftDanger = IsDangerousArea(lastArea_);
            lastArea_ = area;
            if (!area.empty()) {
                // the design document: progress is saved when he walks into a new place and when he leaves the Abyss
                const bool firstVisit = visited_.insert(area).second;
                if (firstVisit || (leftDanger && !IsDangerousArea(area))) events_.Changed();
                const int qi = quests_.OnReach(area);
                if (qi >= 0) QuestProgressMessage(qi);
                if (IsDangerousArea(area))
                    Say("DANGER ZONE! The deeper you go, the wider the gap of levels players can fight across. Die and you lose all but 3 items; skulled, everything.", Tone::Warn);
            }
        }
    }
    if (goal_ != Goal::None) {
        if (walking_) {
            if (goal_ == Goal::Attack) {
                float tx, tz;
                if (!TargetAt(target_, &tx, &tz)) {
                    Cancel();
                } else if (InRangeOf({static_cast<int>(tx), static_cast<int>(tz)})) {
                    player_.Stop();
                    Arrived();
                } else {
                    const TileCoord ntile{static_cast<int>(tx), static_cast<int>(tz)};
                    repathLeft_ -= dt;
                    if (repathLeft_ <= 0.0f || !player_.Moving()) {   // it moves: keep walking to where it is now
                        repathLeft_ = 0.4f;
                        PathResult p = Kind() != AttackKind::Melee ? ShootingPath(ntile) : FindPathToward(map_, player_.Tile(), ntile);
                        if (!p.found) {
                            Say("I can't reach that!", Tone::Warn);
                            Cancel();
                        } else {
                            if (Kind() == AttackKind::Melee && !p.tiles.empty() && p.tiles.back() == ntile) p.tiles.pop_back();
                            player_.SetPath(p.tiles);
                        }
                    }
                }
            } else if (!player_.Moving()) {
                Arrived();
            }
        } else if (working_) {
            UpdateWork(dt);
        }
    }
    UpdateLoot();
    UpdateCombat(dt);
}

void Gameplay::StepAwayFromFire(TileCoord fire) {
    // Like the classic game: step west if you can, otherwise to another side.
    static const int kDx[4] = {-1, 1, 0, 0};
    static const int kDz[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) {
        const TileCoord n{fire.x + kDx[i], fire.z + kDz[i]};
        if (map_.Walkable(n)) {
            player_.SetPath({n});
            return;
        }
    }
}

void Gameplay::UpdateWork(float dt) {
    if (player_.Moving()) {   // something moved the hero: the work is over
        Cancel();
        return;
    }
    switch (goal_) {
        case Goal::Chop: {
            if (forest_.Trees()[target_].stump) {
                Cancel();
                return;
            }
            const TreeInfo& kind = GetTreeInfo(forest_.Trees()[target_].def.kind);
            const Item axe = AxeInUse();
            if (axe == Item::None) {
                Say("You need an axe to chop down this tree.", Tone::Warn);
                Cancel();
                return;
            }
            const float chance = TreeChopChance(kind, skills_.Level(Skill::Woodcutting), HatchetSpeed(axe));
            if (chop_.UpdateWithChance(dt, chance, roll_) != ChopEvent::GotLog) return;
            const int tree = target_;
            Cancel();
            if (bag_.Add(kind.logs) == 0) {
                Say("Your inventory is too full to hold any more logs.", Tone::Warn);
                return;
            }
            forest_.Fell(tree);
            Say("You get some " + Lower(GetItemInfo(kind.logs).name) + ".");
            GainXp(Skill::Woodcutting, kind.xp);
            events_.Changed();
            break;
        }
        case Goal::Fish: {
            const SpotInfo& spot = GetSpotInfo(map_.Spots()[target_].kind);
            const FishEvent ev = fish_.Update(dt, skills_.Level(Skill::Fishing), roll_);
            if (ev == FishEvent::None) return;
            if (spot.bait != Item::None && !bag_.Remove(spot.bait)) {   // every cast uses a bit of bait
                Say(std::string("You have run out of ") + Lower(GetItemInfo(spot.bait).name) + ".", Tone::Warn);
                Cancel();
                return;
            }
            if (ev != FishEvent::Caught) return;
            const Item fish = RollSpotCatch(spot, skills_.Level(Skill::Fishing), roll_());
            if (fish == Item::None) return;
            if (bag_.Add(fish) == 0) {
                Say("Your inventory is too full to hold any more fish.", Tone::Warn);
                Cancel();
                return;
            }
            Say("You catch some " + Lower(GetItemInfo(fish).name).substr(4) + ".");   // drop the "raw " prefix
            GainXp(Skill::Fishing, FishXp(fish));
            events_.Changed();
            if (bag_.Full()) {
                Say("Your inventory is too full to hold any more fish.", Tone::Warn);
                Cancel();
            }
            break;
        }
        case Goal::Mine: {
            const int ri = target_;
            if (ri < 0 || ri >= static_cast<int>(rocks_.All().size()) || rocks_.All()[ri].depleted) {
                Cancel();
                return;
            }
            const RockKind kind = rocks_.All()[ri].kind;
            const Item pick = BestPickaxe(bag_, skills_.Level(Skill::Mining));
            Tier pt = Tier::Bronze;
            if (pick == Item::None || !GearOf(pick, &pt, nullptr)) {
                Say("You need a pickaxe to mine this rock.", Tone::Warn);
                Cancel();
                return;
            }
            const MineEvent ev = mine_.Update(dt, kind, skills_.Level(Skill::Mining), PickaxeSpeed(pt), roll_);
            if (ev == MineEvent::None) return;
            rocks_.Hit(ri);   // the rock shakes at the moment the pick lands
            if (ev == MineEvent::Swing) return;
            const RockInfo& info = GetRockInfo(kind);
            const Item yield = RockYield(kind, roll_());
            Cancel();
            if (bag_.Add(yield) == 0) {
                Say("Your inventory is too full to hold any more ore.", Tone::Warn);
                return;
            }
            rocks_.Deplete(ri);
            Say(kind == RockKind::Gems ? "You find " + WithArticle(Lower(GetItemInfo(yield).name)) + "!" : std::string("You manage to mine some ") + info.key + ".");
            GainXp(Skill::Mining, info.xp);
            events_.Changed();
            break;
        }
        case Goal::Smelt:
        case Goal::Smith:
        case Goal::Craft:
            UpdateMake(dt);
            break;
        case Goal::Cook: {
            const bool onRange = targetSub_ == 1;
            const Fire* f = onRange ? nullptr : fires_.Find(target_);
            if (!onRange && (!f || !f->Burning())) {
                Say("The fire has gone out.", Tone::Info);
                Cancel();
                return;
            }
            cookTimer_ += dt;
            if (cookTimer_ < kCookSeconds) return;
            cookTimer_ -= kCookSeconds;
            const int level = skills_.Level(Skill::Cooking);
            int slot = -1, blocked = 0;
            for (int i = 0; i < Inventory::kSlots && slot < 0; ++i) {   // the first raw food his Cooking level can handle
                const Recipe* cand = RecipeFor(bag_.Slot(i).item);
                if (!cand) continue;
                if (level >= cand->level) slot = i;
                else blocked = std::max(blocked, cand->level);
            }
            const Recipe* r = slot >= 0 ? RecipeFor(bag_.Slot(slot).item) : nullptr;
            if (!r) {
                if (blocked > 0) Say("You need a Cooking level of " + std::to_string(blocked) + " to cook that.", Tone::Warn);
                else Say("You have nothing left to cook.", Tone::Info);
                Cancel();
                return;
            }
            const bool bonfire = f && f->big;   // a bonfire cooks as well as a range
            const bool burnt = roll_() < BurnChance(*r, level + (onRange || bonfire ? kRangeLevelBonus : 0));
            bag_.SetSlot(slot, {burnt ? r->burnt : r->cooked, 1});
            if (burnt) {
                Say("You accidentally burn the " + Lower(GetItemInfo(r->cooked).name) + ".", Tone::Warn);
            } else {
                Say("You successfully cook the " + Lower(GetItemInfo(r->cooked).name) + ".");
                GainXp(Skill::Cooking, r->xp);
            }
            events_.Changed();
            if (bag_.FindRawFood() < 0) {
                Say("You have nothing left to cook.", Tone::Info);
                Cancel();
            }
            break;
        }
        case Goal::Light: {
            if (light_.Update(dt, skills_.Level(Skill::Firemaking), roll_) != LightEvent::Lit) return;
            const TileCoord tile = player_.Tile();
            Cancel();
            const LogInfo* li = GetLogInfo(lightLog_);
            const bool big = lightBig_;
            lightBig_ = false;
            if (big ? !bag_.Remove(lightLog_, kBonfireLogs) : !bag_.Remove(lightLog_)) {
                Say("You can't light a fire here.", Tone::Warn);
                return;
            }
            if (fires_.Light(&map_, tile, big) < 0) {
                bag_.Add(lightLog_, big ? kBonfireLogs : 1);
                Say("You can't light a fire here.", Tone::Warn);
                return;
            }
            if (big) {
                Say("The bonfire roars into life!");
                GainXp(Skill::Firemaking, static_cast<int>((li ? li->fmXp : kLightXp) * kBonfireLogs * kBonfireXpFactor));
            } else {
                Say("The fire catches and the logs begin to burn.");
                GainXp(Skill::Firemaking, li ? li->fmXp : kLightXp);
            }
            events_.Changed();
            StepAwayFromFire(tile);
            break;
        }
        case Goal::Bury: {
            buryTimer_ += dt;
            if (buryTimer_ < kBurySeconds) return;
            const Item item = buryItem_;
            Cancel();
            if (!bag_.Remove(item)) return;
            Say(std::string("You bury the ") + Lower(GetItemInfo(item).name) + ".");
            GainXp(Skill::Prayer, GetItemInfo(item).buryXp);
            events_.Changed();
            break;
        }
        case Goal::Attack: {
            float tx, tz;
            if (!TargetAt(target_, &tx, &tz)) {
                Cancel();
                return;
            }
            if (!InRangeOf({static_cast<int>(tx), static_cast<int>(tz)})) {   // it backed off: go after it
                working_ = false;
                walking_ = true;
                repathLeft_ = 0.0f;
                return;
            }
            player_.FaceToward(tx, tz);
            break;
        }
        default:
            Cancel();
            break;
    }
}

// ---------------------------------------------------------------------------------------------- conversations and quests

void Gameplay::QuestProgressMessage(int qi) {
    const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
    if (const QuestStep* next = quests_.Current(qi)) Say("Quest updated: " + q.name + " - next: " + next->objective, Tone::Good);
    events_.Changed();
}

// Gives the reward of a finished quest: XP, coins and items (an item that does not fit in the bag is put on the ground).
void Gameplay::GiveQuestReward(int qi) {
    const QuestInfo& q = AllQuests()[static_cast<size_t>(qi)];
    for (const std::pair<Skill, int>& x : q.reward.xp) GainXp(x.first, x.second);
    if (q.reward.coins > 0 && bag_.Add(Item::Coins, q.reward.coins) < q.reward.coins) ground_.Drop(player_.Tile(), {Item::Coins, q.reward.coins});
    for (const ItemStack& s : q.reward.items) {
        const int put = bag_.Add(s.item, s.count);
        if (put < s.count) ground_.Drop(player_.Tile(), {s.item, s.count - put});
    }
}

namespace {
std::string RewardText(const QuestInfo& q) {
    std::string out;
    auto add = [&out](const std::string& s) {
        if (!out.empty()) out += ", ";
        out += s;
    };
    if (q.reward.coins > 0) add(std::to_string(q.reward.coins) + " coins");
    for (const std::pair<Skill, int>& x : q.reward.xp) add(std::to_string(x.second) + " " + SkillName(x.first) + " XP");
    for (const ItemStack& s : q.reward.items) add((s.count > 1 ? std::to_string(s.count) + " " : std::string()) + GetItemInfo(s.item).name);
    return "Reward: " + out + ".";
}
}  // namespace

namespace {
// The two ferrymen: each takes you to the other one's dock.
struct FerryRoute {
    const char* from;
    const char* to;
    const char* destination;
    int price;
};
constexpr FerryRoute kFerries[] = {
    {"barnaby", "silas", "Lakeside", 20},
    {"silas", "barnaby", "the castle town", 20},
};
}  // namespace

void Gameplay::Ferry(int route) {
    if (route < 0 || route >= static_cast<int>(sizeof(kFerries) / sizeof(kFerries[0]))) return;
    const FerryRoute& r = kFerries[route];
    if (bag_.Count(Item::Coins) < r.price) {
        Say("The ferry costs " + std::to_string(r.price) + " coins.", Tone::Warn);
        return;
    }
    const Person* dest = nullptr;
    for (const Person& p : people_.All())
        if (std::string(p.info->key) == r.to) dest = &p;
    if (!dest) return;
    // a free tile beside the other ferryman
    for (int radius = 1; radius <= 3; ++radius)
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx) {
                const TileCoord t{dest->Tile().x + dx, dest->Tile().z + dz};
                if (std::max(std::abs(dx), std::abs(dz)) != radius || !map_.Walkable(t)) continue;
                bag_.Remove(Item::Coins, r.price);
                Cancel();
                npcs_.StopAllHunts();
                player_.Teleport(t);
                Say(std::string("You pay ") + std::to_string(r.price) + " coins and sail to " + r.destination + ".", Tone::Good);
                events_.Changed();
                return;
            }
}

void Gameplay::Talk(Person* p, bool talkOnly) {
    const std::string key = p->info->key;
    const std::string name = p->info->name;
    pages_.clear();
    page_ = 0;
    const int shop = p->info->shop[0] ? ShopIndex(p->info->shop) : -1;
    afterShop_ = (shop >= 0 && !talkOnly) ? shop : -1;
    afterBank_ = std::string(p->info->shop) == "bank" && !talkOnly;
    // 1. something to hand in (an item they asked for, or just a word with them)
    for (int i = 0; i < quests_.Count(); ++i) {
        if (!quests_.StepReady(i, key, bag_)) continue;
        const QuestStep& st = *quests_.Current(i);
        pages_.push_back({name, st.say.empty() ? "Thank you." : st.say, {}});
        const QuestInfo& q = AllQuests()[static_cast<size_t>(i)];
        if (quests_.CompleteStep(i, &bag_)) {
            quests_.Finish(i);
            GiveQuestReward(i);
            pages_.push_back({"", "Quest complete: " + q.name + "!\n" + RewardText(q), {}});
            if (!q.outro.empty()) pages_.push_back({name, q.outro, {}});
            Say("Quest complete: " + q.name + "  (" + std::to_string(quests_.Points()) + " quest points)", Tone::Good);
            events_.Changed();
        } else {
            if (const QuestStep* next = quests_.Current(i)) pages_.push_back({"", "Next: " + next->objective, {}});
            QuestProgressMessage(i);
        }
    }
    // 2. a quest to offer him
    for (int i = 0; i < quests_.Count(); ++i) {
        if (AllQuests()[static_cast<size_t>(i)].giver != key || !quests_.CanStart(i, skills_)) continue;
        pages_.push_back({name, AllQuests()[static_cast<size_t>(i)].intro, {{"Accept the quest: " + AllQuests()[static_cast<size_t>(i)].name, 1, i}, {"Not now", 2, 0}}});
        break;
    }
    // 3. nothing to hand in or offer: a reminder of a quest he is on, or why he cannot have one yet
    if (pages_.empty()) {
        for (int i = 0; i < quests_.Count(); ++i) {
            if (AllQuests()[static_cast<size_t>(i)].giver != key) continue;
            const QuestState& s = quests_.State(i);
            if (s.started && !s.done) {
                pages_.push_back({name, "How are you getting on?\n" + quests_.ObjectiveText(i, bag_), {}});
                break;
            }
            const std::string missing = quests_.Missing(i, skills_);
            if (!s.started && !s.done && !missing.empty()) {
                pages_.push_back({name, "I might have a task for you, but you are not ready yet.\n" + missing, {}});
                break;
            }
        }
    }
    // 4. a ferryman: where he sails and what it costs
    for (size_t f = 0; f < sizeof(kFerries) / sizeof(kFerries[0]); ++f) {
        if (key != kFerries[f].from) continue;
        pages_.push_back({name, std::string("I can take you to ") + kFerries[f].destination + " for " + std::to_string(kFerries[f].price) + " coins.",
                          {{std::string("Sail to ") + kFerries[f].destination + " (" + std::to_string(kFerries[f].price) + " coins)", 3, static_cast<int>(f)},
                           {"Not now", 2, 0}}});
    }
    if (pages_.empty()) {   // an ordinary chat: one line in the chat box, and on to the shop or the bank
        Say(std::string(p->info->name) + ": " + p->info->talk[p->nextLine % std::max(1, p->info->talkCount)], Tone::Info);
        ++p->nextLine;
        EndDialogue();
        return;
    }
    // the first line of a conversation is also in the chat box (so it is not lost when the box closes)
    Say(name + ": " + pages_[0].text.substr(0, pages_[0].text.find('\n')), Tone::Info);
}

DialogueView Gameplay::Dialogue() const {
    DialogueView v;
    if (pages_.empty() || page_ >= pages_.size()) return v;
    const DlgPage& pg = pages_[page_];
    v.open = true;
    v.speaker = pg.speaker;
    v.text = pg.text;
    for (const DlgOption& o : pg.options) v.options.push_back(o.text);
    return v;
}

void Gameplay::EndDialogue() {
    pages_.clear();
    page_ = 0;
    const int shop = afterShop_;
    const bool bank = afterBank_;
    afterShop_ = -1;
    afterBank_ = false;
    if (shop >= 0) {
        const ShopMenu menu = BuildShopMenu(shop);
        if (!menu.buy.empty() || !menu.sell.empty()) events_.OpenShop(menu);
        else if (shops_.Def(shop).key == "masters")
            Say("The king: \"My guild's capes are for heroes who have mastered a skill (level 99) or finished every quest. Come back then.\"", Tone::Info);
        else
            Say("The shop has nothing for you right now.", Tone::Info);
    }
    if (bank) events_.OpenBank();
}

void Gameplay::DialogueClose() {
    pages_.clear();
    page_ = 0;
    afterShop_ = -1;
    afterBank_ = false;
}

void Gameplay::DialogueChoose(int option) {
    if (pages_.empty() || page_ >= pages_.size()) return;
    const DlgPage& pg = pages_[page_];
    int act = 0, arg = 0;
    if (!pg.options.empty()) {
        if (option < 0 || option >= static_cast<int>(pg.options.size())) return;   // a choice must be made
        act = pg.options[static_cast<size_t>(option)].act;
        arg = pg.options[static_cast<size_t>(option)].arg;
    }
    if (act == 1) {
        quests_.Start(arg);
        Say("Quest started: " + AllQuests()[static_cast<size_t>(arg)].name, Tone::Good);
        events_.Changed();
        if (const QuestStep* st = quests_.Current(arg)) {
            pages_.push_back({"", "Your task: " + st->objective, {}});
        }
    } else if (act == 2) {
        EndDialogue();
        return;
    } else if (act == 3) {
        Ferry(arg);
        EndDialogue();
        return;
    }
    if (++page_ >= pages_.size()) EndDialogue();
}

// ---------------------------------------------------------------------------------------------- the bank

bool Gameplay::NearBanker() const {
    for (const Person& p : people_.All())
        if (std::string(p.info->shop) == "bank" && Chebyshev(player_.Tile(), p.Tile()) <= p.info->reach) return true;
    return false;
}

void Gameplay::BankDeposit(Item item, int qty) {
    if (item == Item::None || qty <= 0) return;
    if (!NearBanker()) {
        Say("You are too far from the banker.", Tone::Warn);
        return;
    }
    const int have = bag_.Count(item);
    const int want = std::min(qty, have);
    if (want <= 0) return;
    const int put = bank_.Deposit(item, want);
    if (put <= 0) {
        Say("Your bank is full.", Tone::Warn);
        return;
    }
    bag_.Remove(item, put);
    if (put < want) Say("Your bank is full.", Tone::Warn);
    events_.Changed();
}

void Gameplay::BankDepositAll() {
    if (!NearBanker()) {
        Say("You are too far from the banker.", Tone::Warn);
        return;
    }
    bool full = false;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const Item item = bag_.Slot(i).item;
        if (item == Item::None) continue;
        const int have = bag_.Count(item);
        const int put = bank_.Deposit(item, have);
        if (put <= 0) {
            full = true;
            continue;
        }
        bag_.Remove(item, put);
        if (put < have) full = true;
    }
    if (full) Say("Your bank is full.", Tone::Warn);
    else Say("You put everything in the bank.");
    events_.Changed();
}

void Gameplay::BankWithdraw(Item item, int qty) {
    if (item == Item::None || qty <= 0) return;
    if (!NearBanker()) {
        Say("You are too far from the banker.", Tone::Warn);
        return;
    }
    const int inBank = bank_.Count(item);
    if (inBank <= 0) return;
    // how many fit: a stackable item needs a slot only when you have none of it; the rest take a slot each
    int room;
    if (GetItemInfo(item).stackable) room = (bag_.Has(item) || bag_.FreeSlots() > 0) ? qty : 0;
    else room = bag_.FreeSlots();
    const int want = std::min({qty, inBank, room});
    if (want <= 0) {
        Say("Your inventory is full.", Tone::Warn);
        return;
    }
    const int took = bank_.Withdraw(item, want);
    bag_.Add(item, took);
    if (want < qty && want < inBank) Say("Your inventory is full.", Tone::Warn);
    events_.Changed();
}

// ---------------------------------------------------------------------------------------------- smelting and smithing

int Gameplay::NearbyStation(const char* kind) const {
    for (size_t i = 0; i < map_.Stations().size(); ++i) {
        const StationDef& st = map_.Stations()[i];
        if (st.kind == kind && Chebyshev(player_.Tile(), st.Tile()) <= 1) return static_cast<int>(i);
    }
    return -1;
}

MakeMenu Gameplay::BuildSmeltMenu() const {
    MakeMenu m;
    m.kind = MakeMenu::Kind::Smelt;
    m.title = "Furnace";
    for (int t = 0; t < kSmeltCount; ++t) {
        const SmeltRecipe& r = GetSmeltRecipe(static_cast<Tier>(t));
        MakeOption o;
        o.product = r.bar;
        o.name = GetItemInfo(r.bar).name;
        o.detail = OreList(r);
        o.level = r.level;
        o.levelOk = skills_.Level(Skill::Smithing) >= r.level;
        o.canMake = CanSmelt(r, bag_);
        o.haveAll = o.canMake > 0;
        o.index = t;
        m.options.push_back(o);
    }
    return m;
}

MakeMenu Gameplay::BuildSmithMenu(Tier tier) const {
    MakeMenu m;
    m.kind = MakeMenu::Kind::Smith;
    m.title = "Anvil";
    m.tier = static_cast<int>(tier);
    for (int g = 0; g < kGearTypeCount; ++g) {
        const SmithRecipe r = GetSmithRecipe(tier, static_cast<GearType>(g));
        MakeOption o;
        o.product = r.product;
        o.name = GetItemInfo(r.product).name;
        o.detail = Plural(r.bars, "bar") + (r.yield > 1 ? " -> " + std::to_string(r.yield) : "");
        o.level = r.level;
        o.levelOk = skills_.Level(Skill::Smithing) >= r.level;
        o.canMake = CanSmith(r, bag_);
        o.haveAll = o.canMake > 0;
        o.index = g;
        m.options.push_back(o);
    }
    return m;
}

void Gameplay::StartSmelt(Tier tier, int count) {
    const int st = NearbyStation("furnace");
    if (st < 0) {
        Say("You need to be standing next to the furnace.", Tone::Warn);
        return;
    }
    const SmeltRecipe& r = GetSmeltRecipe(tier);
    const std::string barName = Lower(GetItemInfo(r.bar).name);
    if (skills_.Level(Skill::Smithing) < r.level) {
        Say("You need a Smithing level of " + std::to_string(r.level) + " to make a " + barName + ".", Tone::Warn);
        return;
    }
    const int can = CanSmelt(r, bag_);
    if (can < 1) {
        Say("You don't have the ore for a " + barName + ". You need " + OreList(r) + ".", Tone::Warn);
        return;
    }
    Cancel();
    player_.Stop();
    player_.FaceToward(map_.Stations()[st].x, map_.Stations()[st].z);
    goal_ = Goal::Smelt;
    target_ = st;
    makeTier_ = tier;
    makeLeft_ = std::clamp(count, 1, can);
    makeTimer_ = 0.0f;
    working_ = true;
    Say("You place the ore in the furnace...");
}

void Gameplay::StartSmith(Tier tier, GearType type, int count) {
    const int st = NearbyStation("anvil");
    if (st < 0) {
        Say("You need to be standing next to the anvil.", Tone::Warn);
        return;
    }
    if (!bag_.Has(Item::Hammer)) {
        Say("You need a hammer to work the metal with.", Tone::Warn);
        return;
    }
    const SmithRecipe r = GetSmithRecipe(tier, type);
    const std::string name = Lower(GetItemInfo(r.product).name);
    if (skills_.Level(Skill::Smithing) < r.level) {
        Say("You need a Smithing level of " + std::to_string(r.level) + " to make " + WithArticle(name) + ".", Tone::Warn);
        return;
    }
    const int can = CanSmith(r, bag_);
    if (can < 1) {
        Say("You need " + Plural(r.bars, "bar") + " of " + Lower(TierName(tier)) + " to make " + WithArticle(name) + ".", Tone::Warn);
        return;
    }
    Cancel();
    player_.Stop();
    player_.FaceToward(map_.Stations()[st].x, map_.Stations()[st].z);
    goal_ = Goal::Smith;
    target_ = st;
    makeTier_ = tier;
    makeType_ = type;
    makeLeft_ = std::clamp(count, 1, can);
    makeTimer_ = 0.0f;
    working_ = true;
}

// Some shelves open up when a quest is finished (ShopStockDef::quest): is this item on sale in this shop for this hero?
bool Gameplay::ShopLineUnlocked(int shop, Item item) const {
    const std::vector<ShopDef>& defs = AllShopDefs();
    if (shop < 0 || shop >= static_cast<int>(defs.size())) return true;
    for (const ShopStockDef& s : defs[static_cast<size_t>(shop)].stock) {
        if (s.item != item) continue;
        if (s.reqLevel > 0 && skills_.Level(s.reqSkill) < s.reqLevel) return false;
        const int need = s.reqQuestPoints < 0 ? static_cast<int>(AllQuests().size()) : s.reqQuestPoints;
        if (need > 0 && quests_.Points() < need) return false;
        if (s.quest[0]) {
            const int q = FindQuest(s.quest);
            return q < 0 || quests_.State(q).done;
        }
        return true;
    }
    return true;
}

MakeMenu Gameplay::BuildCraftMenu(CraftKind kind) const {
    MakeMenu m;
    m.kind = MakeMenu::Kind::Craft;
    m.title = CraftTitle(kind);
    const std::vector<CraftRecipe>& all = AllCraftRecipes();
    for (size_t i = 0; i < all.size(); ++i) {
        const CraftRecipe& r = all[i];
        if (r.kind != kind) continue;
        MakeOption o;
        o.product = r.product;
        o.name = GetItemInfo(r.product).name;
        o.detail = IngredientText(r) + (r.tool != Item::None ? "  (needs a " + Lower(GetItemInfo(r.tool).name) + ")" : std::string());
        o.level = r.level;
        o.levelOk = skills_.Level(r.skill) >= r.level;
        o.canMake = CanCraft(r, bag_);
        o.haveAll = o.canMake > 0;
        o.index = static_cast<int>(i);
        m.options.push_back(o);
    }
    return m;
}

void Gameplay::StartCraft(int recipeIndex, int count) {
    const std::vector<CraftRecipe>& all = AllCraftRecipes();
    if (recipeIndex < 0 || recipeIndex >= static_cast<int>(all.size())) return;
    const CraftRecipe& r = all[static_cast<size_t>(recipeIndex)];
    const std::string station = CraftStation(r.kind);
    int st = -1;
    if (!station.empty()) {
        st = NearbyStation(station.c_str());
        if (st < 0) {
            Say("You need to be standing next to the " + Lower(CraftTitle(r.kind)) + ".", Tone::Warn);
            return;
        }
    }
    const std::string name = Lower(GetItemInfo(r.product).name);
    if (skills_.Level(r.skill) < r.level) {
        Say(std::string("You need a ") + SkillName(r.skill) + " level of " + std::to_string(r.level) + " to make " + WithArticle(name) + ".", Tone::Warn);
        return;
    }
    if (r.tool != Item::None && !bag_.Has(r.tool)) {
        Say("You need " + WithArticle(Lower(GetItemInfo(r.tool).name)) + " to make that.", Tone::Warn);
        return;
    }
    const int can = CanCraft(r, bag_);
    if (can < 1) {
        Say("You don't have what you need for " + WithArticle(name) + ". You need " + IngredientText(r) + ".", Tone::Warn);
        return;
    }
    Cancel();
    player_.Stop();
    if (st >= 0) player_.FaceToward(map_.Stations()[st].x, map_.Stations()[st].z);
    goal_ = Goal::Craft;
    target_ = st;
    craftRecipe_ = recipeIndex;
    makeLeft_ = std::clamp(count, 1, can);
    makeTimer_ = 0.0f;
    working_ = true;
    Say("You start working...");
}

void Gameplay::UpdateMake(float dt) {
    const bool smelt = goal_ == Goal::Smelt;
    makeTimer_ += dt;
    if (goal_ == Goal::Craft) {
        if (makeTimer_ < kSmithSeconds * 0.8f) return;
        makeTimer_ = 0.0f;
        const CraftRecipe& r = AllCraftRecipes()[static_cast<size_t>(craftRecipe_)];
        if (CanCraft(r, bag_) < 1) {
            Say("You have run out of materials.", Tone::Info);
            Cancel();
            return;
        }
        for (int i = 0; i < 3; ++i)
            if (r.input[i] != Item::None && r.count[i] > 0) bag_.Remove(r.input[i], r.count[i]);
        const int put = bag_.Add(r.product, r.yield);
        if (put < r.yield) ground_.Drop(player_.Tile(), {r.product, r.yield - put});
        Say("You make " + WithArticle(Lower(GetItemInfo(r.product).name)) + ".");
        if (r.xp > 0) GainXp(r.skill, r.xp);
        events_.Changed();
        if (--makeLeft_ <= 0) {
            Cancel();
        } else if (CanCraft(r, bag_) < 1) {
            Say("You have run out of materials.", Tone::Info);
            Cancel();
        }
        return;
    }
    if (makeTimer_ < (smelt ? kSmeltSeconds : kSmithSeconds)) return;
    makeTimer_ = 0.0f;
    if (smelt) {
        const SmeltRecipe& r = GetSmeltRecipe(makeTier_);
        if (CanSmelt(r, bag_) < 1) {
            Say("You have run out of ore.", Tone::Info);
            Cancel();
            return;
        }
        for (int i = 0; i < 2; ++i)
            if (r.ore[i] != Item::None) bag_.Remove(r.ore[i], r.count[i]);
        if (roll_() < r.success) {
            bag_.Add(r.bar);
            Say("You retrieve a " + Lower(GetItemInfo(r.bar).name) + ".");
            GainXp(Skill::Smithing, r.xp);
        } else {
            Say("The ore is too impure and you fail to smelt it.", Tone::Warn);
        }
        events_.Changed();
        if (--makeLeft_ <= 0) {
            Cancel();
        } else if (CanSmelt(r, bag_) < 1) {
            Say("You have run out of ore.", Tone::Info);
            Cancel();
        }
        return;
    }
    const SmithRecipe r = GetSmithRecipe(makeTier_, makeType_);
    if (!bag_.Has(Item::Hammer) || CanSmith(r, bag_) < 1) {
        Say("You have run out of bars.", Tone::Info);
        Cancel();
        return;
    }
    bag_.Remove(BarFor(makeTier_), r.bars);
    bag_.Add(r.product, r.yield);   // (removing the bars freed at least one slot)
    Say(r.yield > 1 ? "You make " + std::to_string(r.yield) + " " + Lower(GetItemInfo(r.product).name) + "."
                    : "You make " + WithArticle(Lower(GetItemInfo(r.product).name)) + ".");
    GainXp(Skill::Smithing, r.xp);
    events_.Changed();
    if (--makeLeft_ <= 0) {
        Cancel();
    } else if (CanSmith(r, bag_) < 1) {
        Say("You have run out of bars.", Tone::Info);
        Cancel();
    }
}

// ---------------------------------------------------------------------------------------------- shops

bool Gameplay::NearShopKeeper(int shop) const {
    for (const Person& p : people_.All())
        if (p.info->shop[0] && ShopIndex(p.info->shop) == shop && Chebyshev(p.Tile(), player_.Tile()) <= p.info->reach + 1) return true;
    return false;
}

ShopMenu Gameplay::BuildShopMenu(int shop) const {
    ShopMenu m;
    m.shop = shop;
    m.name = shops_.Def(shop).name;
    for (const Person& p : people_.All())
        if (p.info->shop[0] && ShopIndex(p.info->shop) == shop) m.keeper = p.info->name;
    m.coins = bag_.Count(Item::Coins);
    const std::vector<Shops::Line>& lines = shops_.Lines(shop);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].count <= 0 || !ShopLineUnlocked(shop, lines[i].item)) continue;
        ShopLine l;
        l.item = lines[i].item;
        l.price = shops_.BuyPrice(shop, l.item);
        l.stock = lines[i].count;
        l.ok = m.coins >= l.price;
        l.index = static_cast<int>(i);
        m.buy.push_back(l);
    }
    std::vector<Item> seen;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const Item item = bag_.Slot(i).item;
        if (item == Item::None || !shops_.Buys(shop, item) || std::find(seen.begin(), seen.end(), item) != seen.end()) continue;
        seen.push_back(item);
        ShopLine l;
        l.item = item;
        l.price = shops_.SellPrice(shop, item);
        l.stock = bag_.Count(item);
        l.index = i;
        m.sell.push_back(l);
    }
    return m;
}

void Gameplay::Buy(int shop, int line, int qty) {
    if (shop < 0 || shop >= shops_.Count() || qty <= 0) return;
    if (!NearShopKeeper(shop)) {
        Say("You are too far from the shop.", Tone::Warn);
        return;
    }
    const std::vector<Shops::Line>& lines = shops_.Lines(shop);
    if (line < 0 || line >= static_cast<int>(lines.size()) || lines[line].count <= 0) {
        Say("The shop has run out of that.", Tone::Warn);
        return;
    }
    const Item item = lines[line].item;
    if (!ShopLineUnlocked(shop, item)) {
        Say("The shopkeeper does not sell that to strangers.", Tone::Warn);
        return;
    }
    const int price = shops_.BuyPrice(shop, item);
    int want = std::min(qty, lines[line].count);
    const int affordable = price > 0 ? bag_.Count(Item::Coins) / price : 0;
    if (affordable <= 0) {
        Say("You don't have enough coins.", Tone::Warn);
        return;
    }
    want = std::min(want, affordable);
    bag_.Remove(Item::Coins, want * price);   // pay first: a spent coin stack frees its slot
    const int got = bag_.Add(item, want);
    if (got < want) bag_.Add(Item::Coins, (want - got) * price);   // no room: refund the rest
    if (got <= 0) {
        Say("You don't have enough inventory space.", Tone::Warn);
        return;
    }
    shops_.Take(shop, line, got);
    Say("You buy " + std::to_string(got) + " " + Lower(GetItemInfo(item).name) + " for " + Plural(got * price, "coin") + ".");
    if (got < want) Say("You don't have enough inventory space for the rest.", Tone::Warn);
    events_.Changed();
}

void Gameplay::Sell(int shop, Item item, int qty) {
    if (shop < 0 || shop >= shops_.Count() || qty <= 0) return;
    if (!NearShopKeeper(shop)) {
        Say("You are too far from the shop.", Tone::Warn);
        return;
    }
    if (!shops_.Buys(shop, item)) {
        Say("The shop has no use for that.", Tone::Warn);
        return;
    }
    const int n = std::min(qty, bag_.Count(item));
    if (n <= 0) return;
    const int price = shops_.SellPrice(shop, item);
    bag_.Remove(item, n);
    if (bag_.Add(Item::Coins, n * price) <= 0) {   // no room for the coins: undo
        bag_.Add(item, n);
        Say("You don't have enough inventory space for the coins.", Tone::Warn);
        return;
    }
    shops_.Add(shop, item, n);
    Say("You sell " + std::to_string(n) + " " + Lower(GetItemInfo(item).name) + " for " + Plural(n * price, "coin") + ".");
    events_.Changed();
}

}  // namespace journey
