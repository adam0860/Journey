#include "game/Gameplay.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "world/Pathfinder.h"

namespace journey {

namespace {

constexpr float kBurySeconds = 1.8f;
constexpr float kRegenSeconds = 30.0f;   // one hit point back every 30 s

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
    return s.kind == "anvil" ? "Anvil" : "Furnace";
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
    if (!bag_.Has(Item::BronzeAxe)) {
        Say("You need an axe to chop down this tree.", Tone::Warn);
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
    if (!bag_.Has(Item::SmallNet)) {
        Say("You need a small fishing net to catch these fish.", Tone::Warn);
        return;
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
    markerPending_ = true;
    markerRed_ = true;
    markerX_ = n->x;
    markerZ_ = n->z;
    goal_ = Goal::Attack;
    target_ = id;
    Approach(n->Tile());
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
            if (item != Item::Logs && item != Item::Tinderbox) return;
            StartLighting();
            break;
        case ItemAction::Cook: {
            if (!info.raw) return;
            const Fire* f = fires_.NearestBurning(player_.Tile());
            if (!f) {
                Say("You need a fire to cook on. Light some logs with your tinderbox.", Tone::Info);
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
                Say(std::string(action == ItemAction::Wield ? "You wield the " : "You put on the ") + Lower(info.name) + ".");
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
            if (info.tool) {
                Say("You had better keep your " + Lower(info.name) + ".", Tone::Info);
                return;
            }
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
    }
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
    if ((ia == Item::Tinderbox && ib == Item::Logs) || (ia == Item::Logs && ib == Item::Tinderbox)) {
        StartLighting();
        return true;
    }
    Say("Nothing interesting happens.");
    return true;
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
    Tier t;
    const bool ore = item == Item::CopperOre || item == Item::TinOre || item == Item::IronOre || item == Item::Coal || item == Item::MithrilOre;
    if ((furnace && ore) || (!furnace && (item == Item::Hammer || TierOfBar(item, &t)))) {
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
    if ((goal_ == Goal::Smelt || goal_ == Goal::Smith) && target_ >= 0 && target_ < static_cast<int>(map_.Stations().size())) {
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
                if (!stump) add({Run("Chop down "), Run("Tree", kMenuObject)}, MenuAct::Chop, t);
                add({Run("Examine "), Run(stump ? "Tree stump" : "Tree", kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Spot:
                add({Run("Net "), Run("Fishing spot", kMenuObject)}, MenuAct::Fish, t);
                add({Run("Examine "), Run("Fishing spot", kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
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
                add({Run("Attack "), Run(info.name, kMenuNpc), Run(" (level-" + std::to_string(info.level) + ")", c)}, MenuAct::Attack, t);
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
                if (p->info->shop[0]) add({Run("Trade "), Run(p->info->name, kMenuNpc)}, MenuAct::Trade, t);
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
                add({Run(furnace ? "Smelt " : "Smith "), Run(StationLabel(st), kMenuObject)}, MenuAct::UseStation, t);
                add({Run("Examine "), Run(StationLabel(st), kMenuObject)}, MenuAct::ExamineTarget, t);
                break;
            }
            case TargetKind::Ground:
                break;
        }
    }
    if (hasGround) {
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
                Say("A commonly found tree.");
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
            Say(map_.Stations()[t.id].kind == "anvil" ? "An anvil, for hammering metal bars into weapons and armour."
                                                      : "A roaring furnace. Ore goes in, bars come out.");
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
        case TargetKind::Ground: break;
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
            const Fire* f = fires_.Find(target_);
            return f ? f->tile : player_.Tile();
        }
        case Goal::Attack: {
            const Npc* n = npcs_.Find(target_);
            return n ? n->Tile() : player_.Tile();
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
    if (Chebyshev(player_.Tile(), target) <= need) {
        player_.Stop();
        Arrived();
        return;
    }
    PathResult path = FindPathToward(map_, player_.Tile(), target);
    if (!path.found) {
        Say("I can't reach that!", Tone::Warn);
        Cancel();
        return;
    }
    if (goal_ == Goal::Attack && !path.tiles.empty() && path.tiles.back() == target) path.tiles.pop_back();   // stop next to it
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
    if (Chebyshev(player_.Tile(), tt) > need) {
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
            Say("You cast out your net...");
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
            Say(std::string(p->info->name) + ": " + p->info->talk[p->nextLine % std::max(1, p->info->talkCount)], Tone::Info);
            ++p->nextLine;
            const int shop = p->info->shop[0] ? ShopIndex(p->info->shop) : -1;
            if (shop >= 0 && !talkOnly) events_.OpenShop(BuildShopMenu(shop));
            break;
        }
        case Goal::Station: {
            const StationDef& st = map_.Stations()[target_];
            const int si = target_;
            Cancel();
            player_.Stop();
            player_.FaceToward(st.x, st.z);
            if (st.kind == "furnace") {
                bool anyOre = false;
                for (Item o : {Item::CopperOre, Item::TinOre, Item::IronOre, Item::Coal, Item::MithrilOre}) anyOre = anyOre || bag_.Has(o);
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
            const Npc* n = npcs_.Find(target_);
            if (!n || n->dead) {
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

void Gameplay::StartLighting() {
    Cancel();
    if (!bag_.Has(Item::Tinderbox)) {
        Say("You need a tinderbox to light a fire.", Tone::Warn);
        return;
    }
    if (!bag_.Has(Item::Logs)) {
        Say("You have no logs to light.", Tone::Warn);
        return;
    }
    if (!fires_.CanLight(map_, player_.Tile())) {
        Say("You can't light a fire here.", Tone::Warn);
        return;
    }
    player_.Stop();
    goal_ = Goal::Light;
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

Fighter Gameplay::PlayerNow() const {
    const EquipBonuses b = equip_.Total();
    return PlayerFighter(skills_, style_, b.attack, b.strength, b.defence);
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

bool Gameplay::FightingSomething() const {
    if (goal_ == Goal::Attack && working_) return true;
    const TileCoord me = player_.Tile();
    for (const Npc& n : npcs_.All())
        if (n.Alive() && n.hunting && Chebyshev(n.Tile(), me) <= 1) return true;
    return false;
}

void Gameplay::StartFight(int npcId, bool fromAutoRetaliate) {
    const Npc* n = npcs_.Find(npcId);
    if (!n || n->dead) return;
    const bool alreadyFighting = FightingSomething();
    goal_ = Goal::Attack;
    target_ = npcId;
    walking_ = false;
    working_ = true;
    player_.FaceToward(n->x, n->z);
    npcs_.StartHunt(npcId);
    if (!alreadyFighting) {   // the first strike comes half a second after we reach it
        turnTimer_ = kTurnSeconds - 0.5f;
        playerTurn_ = true;
    }
    (void)fromAutoRetaliate;
}

void Gameplay::PlayerTurn() {
    if (!(goal_ == Goal::Attack && working_)) return;
    const Npc* n = npcs_.Find(target_);
    if (!n || n->dead || Chebyshev(player_.Tile(), n->Tile()) > 1) return;
    player_.FaceToward(n->x, n->z);
    const AttackResult r = RollAttack(PlayerNow(), NpcFighter(*n), roll_);
    pending_.push_back({kImpactSeconds, false, n->id, r.damage});
    strikeAge_ = 0.0f;
    strikeLeft_ = !strikeLeft_;
    npcs_.RefreshHunt(n->id);
}

void Gameplay::CreatureTurn() {
    const TileCoord me = player_.Tile();
    int firstAttacker = -1;
    for (Npc& n : npcs_.All()) {
        if (!n.Alive() || !n.hunting || Chebyshev(n.Tile(), me) > 1) continue;
        const AttackResult r = RollAttack(NpcFighter(n), PlayerNow(), roll_);
        pending_.push_back({kImpactSeconds, true, n.id, r.damage});
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
        if (h.damage > 0) skills_.Hurt(h.damage);
        if (skills_.Hp() <= 0) PlayerDies();
        return;
    }
    const Npc* n = npcs_.Find(h.npcId);
    if (!n || n->dead) return;
    events_.Hit(false, h.npcId, h.damage);
    if (h.damage > 0) {
        const CombatXp xp = XpForDamage(style_, h.damage);
        if (xp.attack > 0) GainXp(Skill::Attack, xp.attack);
        if (xp.strength > 0) GainXp(Skill::Strength, xp.strength);
        if (xp.defence > 0) GainXp(Skill::Defence, xp.defence);
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
        if (goal_ == Goal::Attack && target_ == h.npcId) Cancel();
    }
}

void Gameplay::PlayerDies() {
    Say("Oh dear, you are dead!", Tone::Warn);
    pending_.clear();
    Cancel();
    npcs_.StopAllHunts();
    player_.Teleport(map_.Spawn());
    skills_.SetHp(skills_.MaxHp());
    turnTimer_ = 0.0f;
    playerTurn_ = true;
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
        playerTurn_ = true;
        return;
    }
    turnTimer_ += dt;
    if (turnTimer_ >= kTurnSeconds) {
        turnTimer_ -= kTurnSeconds;
        if (playerTurn_) PlayerTurn();
        else CreatureTurn();
        playerTurn_ = !playerTurn_;
    }
}

// ---------------------------------------------------------------------------------------------- per-frame

void Gameplay::Update(float dt) {
    if (goal_ != Goal::None) {
        if (walking_) {
            if (goal_ == Goal::Attack) {
                const Npc* n = npcs_.Find(target_);
                if (!n || n->dead) {
                    Cancel();
                } else if (Chebyshev(player_.Tile(), n->Tile()) <= 1) {
                    player_.Stop();
                    Arrived();
                } else {
                    repathLeft_ -= dt;
                    if (repathLeft_ <= 0.0f || !player_.Moving()) {   // it moves: keep walking to where it is now
                        repathLeft_ = 0.4f;
                        PathResult p = FindPathToward(map_, player_.Tile(), n->Tile());
                        if (!p.found) {
                            Say("I can't reach that!", Tone::Warn);
                            Cancel();
                        } else {
                            if (!p.tiles.empty() && p.tiles.back() == n->Tile()) p.tiles.pop_back();
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
            if (chop_.Update(dt, skills_.Level(Skill::Woodcutting), roll_) != ChopEvent::GotLog) return;
            const int tree = target_;
            Cancel();
            if (bag_.Add(Item::Logs) == 0) {
                Say("Your inventory is too full to hold any more logs.", Tone::Warn);
                return;
            }
            forest_.Fell(tree);
            Say("You get some logs.");
            GainXp(Skill::Woodcutting, kLogXp);
            events_.Changed();
            break;
        }
        case Goal::Fish: {
            const FishEvent ev = fish_.Update(dt, skills_.Level(Skill::Fishing), roll_);
            if (ev != FishEvent::Caught) return;
            const Item fish = RollCatch(skills_.Level(Skill::Fishing), roll_());
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
            Cancel();
            if (bag_.Add(info.ore) == 0) {
                Say("Your inventory is too full to hold any more ore.", Tone::Warn);
                return;
            }
            rocks_.Deplete(ri);
            Say(std::string("You manage to mine some ") + info.key + ".");
            GainXp(Skill::Mining, info.xp);
            events_.Changed();
            break;
        }
        case Goal::Smelt:
        case Goal::Smith:
            UpdateMake(dt);
            break;
        case Goal::Cook: {
            const Fire* f = fires_.Find(target_);
            if (!f || !f->Burning()) {
                Say("The fire has gone out.", Tone::Info);
                Cancel();
                return;
            }
            cookTimer_ += dt;
            if (cookTimer_ < kCookSeconds) return;
            cookTimer_ -= kCookSeconds;
            const int slot = bag_.FindRawFood();
            const Recipe* r = slot >= 0 ? RecipeFor(bag_.Slot(slot).item) : nullptr;
            if (!r) {
                Say("You have nothing left to cook.", Tone::Info);
                Cancel();
                return;
            }
            const int level = skills_.Level(Skill::Cooking);
            const bool burnt = roll_() < BurnChance(*r, level);
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
            if (!bag_.Remove(Item::Logs) || fires_.Light(&map_, tile) < 0) {
                Say("You can't light a fire here.", Tone::Warn);
                return;
            }
            Say("The fire catches and the logs begin to burn.");
            GainXp(Skill::Firemaking, kLightXp);
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
            const Npc* n = npcs_.Find(target_);
            if (!n || n->dead) {
                Cancel();
                return;
            }
            if (Chebyshev(player_.Tile(), n->Tile()) > 1) {   // it backed off: go after it
                working_ = false;
                walking_ = true;
                repathLeft_ = 0.0f;
                return;
            }
            player_.FaceToward(n->x, n->z);
            break;
        }
        default:
            Cancel();
            break;
    }
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
    for (int t = 0; t < kTierCount; ++t) {
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
        o.detail = Plural(r.bars, "bar");
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

void Gameplay::UpdateMake(float dt) {
    const bool smelt = goal_ == Goal::Smelt;
    makeTimer_ += dt;
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
            Say("You retrieve a bar of " + Lower(TierName(makeTier_)) + ".");
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
    bag_.Add(r.product);   // (removing the bars freed at least one slot)
    Say("You make " + WithArticle(Lower(GetItemInfo(r.product).name)) + ".");
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
        if (lines[i].count <= 0) continue;
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
