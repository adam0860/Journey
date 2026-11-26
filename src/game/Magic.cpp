#include "game/Magic.h"

namespace journey {

namespace {

const char* kRuneNames[kRuneCount] = {"Air rune", "Water rune", "Earth rune", "Fire rune", "Mind rune", "Chaos rune"};

const StaffInfo kStaffs[kStaffCount] = {
    {Item::Staff, "Staff", "staff", 4, 20, false, Rune::Air, "A plain wooden staff."},
    {Item::AirStaff, "Staff of air", "staff_air", 8, 150, true, Rune::Air, "A staff that gives endless air runes."},
    {Item::WaterStaff, "Staff of water", "staff_water", 8, 150, true, Rune::Water, "A staff that gives endless water runes."},
    {Item::EarthStaff, "Staff of earth", "staff_earth", 8, 150, true, Rune::Earth, "A staff that gives endless earth runes."},
    {Item::FireStaff, "Staff of fire", "staff_fire", 8, 180, true, Rune::Fire, "A staff that gives endless fire runes."},
};

const RobeInfo kRobes[kRobeCount] = {
    {Item::WizardHat, "Wizard hat", "gear_wizard_hat", 0, 4, 1, 40, "A pointy blue hat. Wizards wear them."},
    {Item::WizardRobe, "Wizard robe", "gear_wizard_robe", 1, 8, 3, 90, "A long blue robe, easy to cast in."},
    {Item::WizardSkirt, "Wizard skirt", "gear_wizard_skirt", 2, 5, 2, 70, "A blue skirt. Practical for a wizard."},
};

std::vector<SpellInfo> Build() {
    std::vector<SpellInfo> v;
    //        key              name            lvl  n  runes                                                   max xp  element     description
    v.push_back({"wind_strike", "Wind Strike", 1, 2, {{Rune::Air, 1}, {Rune::Mind, 1}, {}}, 2, 6, Rune::Air, "A gust of wind."});
    v.push_back({"water_strike", "Water Strike", 3, 3, {{Rune::Water, 1}, {Rune::Air, 1}, {Rune::Mind, 1}}, 4, 8, Rune::Water, "A bolt of water."});
    v.push_back({"earth_strike", "Earth Strike", 5, 3, {{Rune::Earth, 1}, {Rune::Air, 1}, {Rune::Mind, 1}}, 6, 10, Rune::Earth, "A fist of rock."});
    v.push_back({"fire_strike", "Fire Strike", 7, 3, {{Rune::Fire, 1}, {Rune::Air, 1}, {Rune::Mind, 1}}, 8, 12, Rune::Fire, "A ball of fire."});
    v.push_back({"wind_bolt", "Wind Bolt", 9, 2, {{Rune::Air, 2}, {Rune::Chaos, 1}, {}}, 9, 14, Rune::Air, "A strong gust of wind."});
    v.push_back({"water_bolt", "Water Bolt", 11, 3, {{Rune::Water, 2}, {Rune::Air, 2}, {Rune::Chaos, 1}}, 10, 17, Rune::Water, "A strong bolt of water."});
    v.push_back({"earth_bolt", "Earth Bolt", 13, 3, {{Rune::Earth, 3}, {Rune::Air, 2}, {Rune::Chaos, 1}}, 11, 20, Rune::Earth, "A strong blow of rock."});
    v.push_back({"fire_bolt", "Fire Bolt", 15, 3, {{Rune::Fire, 4}, {Rune::Air, 3}, {Rune::Chaos, 1}}, 12, 23, Rune::Fire, "A strong ball of fire."});
    v.push_back({"wind_blast", "Wind Blast", 17, 2, {{Rune::Air, 3}, {Rune::Chaos, 2}, {}}, 13, 25, Rune::Air, "A fierce blast of wind."});
    v.push_back({"water_blast", "Water Blast", 19, 3, {{Rune::Water, 3}, {Rune::Air, 3}, {Rune::Chaos, 2}}, 14, 28, Rune::Water, "A fierce blast of water."});
    v.push_back({"earth_blast", "Earth Blast", 21, 3, {{Rune::Earth, 4}, {Rune::Air, 3}, {Rune::Chaos, 2}}, 15, 31, Rune::Earth, "A fierce blast of rock."});
    v.push_back({"fire_blast", "Fire Blast", 23, 3, {{Rune::Fire, 5}, {Rune::Air, 4}, {Rune::Chaos, 2}}, 16, 34, Rune::Fire, "A fierce blast of fire."});
    v.push_back({"wind_wave", "Wind Wave", 31, 2, {{Rune::Air, 5}, {Rune::Chaos, 4}, {}}, 18, 38, Rune::Air, "A crushing wave of wind."});
    v.push_back({"water_wave", "Water Wave", 35, 3, {{Rune::Water, 6}, {Rune::Air, 5}, {Rune::Chaos, 4}}, 19, 42, Rune::Water, "A crushing wave of water."});
    v.push_back({"earth_wave", "Earth Wave", 39, 3, {{Rune::Earth, 7}, {Rune::Air, 5}, {Rune::Chaos, 4}}, 20, 46, Rune::Earth, "A crushing wave of rock."});
    v.push_back({"fire_wave", "Fire Wave", 43, 3, {{Rune::Fire, 8}, {Rune::Air, 6}, {Rune::Chaos, 4}}, 22, 50, Rune::Fire, "A crushing wave of fire."});
    return v;
}

}  // namespace

const char* RuneName(Rune r) { return kRuneNames[static_cast<int>(r)]; }

Item RuneItem(Rune r) { return static_cast<Item>(static_cast<int>(Item::AirRune) + static_cast<int>(r)); }

bool IsRune(Item item, Rune* r) {
    const int id = static_cast<int>(item), first = static_cast<int>(Item::AirRune);
    if (id < first || id >= first + kRuneCount) return false;
    if (r) *r = static_cast<Rune>(id - first);
    return true;
}

const StaffInfo& GetStaff(int index) { return kStaffs[index < 0 ? 0 : (index >= kStaffCount ? kStaffCount - 1 : index)]; }

const StaffInfo* FindStaff(Item item) {
    for (const StaffInfo& s : kStaffs)
        if (s.item == item) return &s;
    return nullptr;
}

const RobeInfo& GetRobe(int index) { return kRobes[index < 0 ? 0 : (index >= kRobeCount ? kRobeCount - 1 : index)]; }

const RobeInfo* FindRobe(Item item) {
    for (const RobeInfo& r : kRobes)
        if (r.item == item) return &r;
    return nullptr;
}

const std::vector<SpellInfo>& AllSpells() {
    static const std::vector<SpellInfo> spells = Build();
    return spells;
}

int SpellCount() { return static_cast<int>(AllSpells().size()); }

const SpellInfo& GetSpell(int index) {
    const std::vector<SpellInfo>& s = AllSpells();
    const int n = static_cast<int>(s.size());
    return s[static_cast<size_t>(index < 0 ? 0 : (index >= n ? n - 1 : index))];
}

int FindSpell(const std::string& key) {
    const std::vector<SpellInfo>& s = AllSpells();
    for (size_t i = 0; i < s.size(); ++i)
        if (key == s[i].key) return static_cast<int>(i);
    return -1;
}

Rgb SpellColor(const SpellInfo& s) {
    switch (s.element) {
        case Rune::Air: return {220, 232, 240};
        case Rune::Water: return {70, 140, 255};
        case Rune::Earth: return {150, 110, 60};
        case Rune::Fire: return {255, 120, 30};
        default: return {200, 80, 200};
    }
}

int RunesNeeded(const SpellInfo& s, Rune r, Item weapon) {
    const StaffInfo* staff = FindStaff(weapon);
    int need = 0;
    for (int i = 0; i < s.costCount; ++i)
        if (s.cost[i].rune == r) need += s.cost[i].count;
    if (staff && staff->infinite && staff->element == r) return 0;
    return need;
}

CastCheck CheckCast(const SpellInfo& s, const Inventory& bag, Item weapon, int magicLevel) {
    if (magicLevel < s.level) return CastCheck::LevelTooLow;
    for (int i = 0; i < s.costCount; ++i) {
        const Rune r = s.cost[i].rune;
        if (bag.Count(RuneItem(r)) < RunesNeeded(s, r, weapon)) return CastCheck::NoRunes;
    }
    return CastCheck::Ok;
}

void PayRunes(const SpellInfo& s, Inventory* bag, Item weapon) {
    for (int i = 0; i < s.costCount; ++i) {
        const Rune r = s.cost[i].rune;
        // a rune listed twice would be paid twice: RunesNeeded() already adds them up, so pay only on the first mention
        bool first = true;
        for (int j = 0; j < i; ++j) first = first && s.cost[j].rune != r;
        if (!first) continue;
        const int n = RunesNeeded(s, r, weapon);
        if (n > 0) bag->Remove(RuneItem(r), n);
    }
}

}  // namespace journey
