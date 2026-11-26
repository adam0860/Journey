// The weapon table behind the balance pass: the damage per second of every weapon (melee, bows, darts, spells) against a creature of
// about the hero's own level, and what the shot costs in coins. Shared by tests/balance_report.cpp (prints it) and tests/test_core.cpp
// (TestWeaponBalance: guard rails, so new gear or spells cannot quietly break the balance).
#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "game/Archery.h"
#include "game/Combat.h"
#include "game/Gear.h"
#include "game/Magic.h"
#include "game/Skills.h"

namespace balance {
using namespace journey;

struct Row {
    std::string cls, name;
    int level = 1;
    int type = -1;     // melee: the GearType; bows: the bow; everything else -1
    int tier = -1;     // melee: the Tier; bows: the arrow metal; darts: the metal
    double maxHit = 0, hit = 0, seconds = 0, dps = 0;
    double cost = 0;   // coins per shot (arrows and runes that are used up; melee costs nothing)
    double CoinsPerMinute() const { return cost * 60.0 / seconds; }
};

inline Skills Hero(int level) {
    Skills s;
    for (Skill k : {Skill::Attack, Skill::Strength, Skill::Defence, Skill::Ranged, Skill::Magic}) s.SetXp(k, XpForLevel(level));
    return s;
}

// A creature of the given level (no armour bonus, like the real ones).
inline Fighter Creature(int level) {
    Fighter f;
    f.attack = f.strength = f.defence = level;
    f.creature = true;
    return f;
}

inline Row Make(const char* cls, const std::string& name, int level, const Fighter& a, const Fighter& d, int ticks, int fixedMax = -1) {
    Row r;
    r.cls = cls;
    r.name = name;
    r.level = level;
    r.maxHit = fixedMax >= 0 ? fixedMax : std::max(1, MaxHit(a));
    r.hit = HitChance(a, d);
    r.seconds = ticks * 0.6;
    r.dps = r.hit * (r.maxHit + 1.0) / 2.0 / r.seconds;   // (a landed hit is 1..max, so (max + 1) / 2 on average)
    return r;
}

// Every weapon, with a hero whose combat skills are the level the weapon needs; the better of the two usual styles is used.
inline std::vector<Row> WeaponTable() {
    std::vector<Row> rows;
    const GearType kMelee[] = {GearType::Dagger, GearType::Sword, GearType::Scimitar, GearType::Longsword, GearType::Spear, GearType::Mace, GearType::Axe, GearType::Warhammer};
    for (int t = 0; t < kTierCount; ++t)
        for (GearType g : kMelee) {
            const Item item = GearItem(static_cast<Tier>(t), g);
            Skill sk;
            int level = 1;
            ItemRequirement(item, &sk, &level);
            const Skills s = Hero(level);
            const GearStats st = ItemStats(item);
            const int ticks = WeaponTicks(item);
            Row best;
            best.dps = -1;
            for (CombatStyle style : {CombatStyle::Accurate, CombatStyle::Aggressive}) {
                Fighter a = PlayerFighter(s, style, st.attack, st.strength, 0);
                a.swingScale = static_cast<float>(ticks) / 4.0f;
                Row r = Make("melee", std::string(TierName(static_cast<Tier>(t))) + " " + GearTypeName(g), level, a, Creature(level), ticks);
                r.type = static_cast<int>(g);
                r.tier = t;
                if (r.dps > best.dps) best = r;
            }
            rows.push_back(best);
        }
    for (int b = 0; b < kBowCount; ++b)   // bows with each metal of arrows
        for (int k = 0; k < kArrowKinds; ++k) {
            const BowInfo& bow = GetBow(b);
            const ArrowInfo& arrow = GetArrow(k);
            const int level = std::max(bow.level, arrow.level);
            const Skills s = Hero(level);
            const int ticks = WeaponTicks(bow.item);
            Row best;
            best.dps = -1;
            for (CombatStyle style : {CombatStyle::Accurate, CombatStyle::Aggressive}) {
                Fighter a = RangedFighter(s, style, bow.accuracy, arrow.strength, 0);
                a.swingScale = static_cast<float>(ticks) / 4.0f;
                Row r = Make("bow", std::string(bow.name) + " + " + arrow.name, level, a, Creature(level), ticks);
                r.type = b;
                r.tier = k;
                r.cost = arrow.value * (1.0 - kArrowSurvival);
                if (r.dps > best.dps) best = r;
            }
            rows.push_back(best);
        }
    for (int k = 0; k < kDartKinds; ++k) {   // thrown darts
        const ArrowInfo& d = GetDart(k);
        const Skills s = Hero(d.level);
        const GearStats st = ItemStats(d.item);
        Row best;
        best.dps = -1;
        for (CombatStyle style : {CombatStyle::Accurate, CombatStyle::Aggressive}) {
            Fighter a = RangedFighter(s, style, st.ranged, st.rangedStrength, 0);
            a.swingScale = static_cast<float>(kThrowTicks) / 4.0f;
            Row r = Make("dart", d.name, d.level, a, Creature(d.level), kThrowTicks);
            r.tier = k;
            r.cost = d.value * (1.0 - kArrowSurvival);
            if (r.dps > best.dps) best = r;
        }
        rows.push_back(best);
    }
    for (const SpellInfo& sp : AllSpells()) {   // spells, cast with the staff of their own element (it pays for that element's runes)
        const Skills s = Hero(sp.level);
        const StaffInfo& staff = GetStaff(1 + (sp.element == Rune::Air ? 0 : sp.element == Rune::Water ? 1 : sp.element == Rune::Earth ? 2 : 3));
        Row r = Make("spell", sp.name, sp.level, MagicFighter(s, staff.accuracy, 0), Creature(sp.level), 4, sp.maxHit);
        for (int k = 0; k < kRuneCount; ++k) r.cost += RunesNeeded(sp, static_cast<Rune>(k), staff.item) * ItemValue(RuneItem(static_cast<Rune>(k)));
        rows.push_back(r);
    }
    return rows;
}

// Damage per second of one bow with one metal of arrows for a hero of this Ranged level (the better of the two usual styles).
inline double BowDps(int bow, int arrow, int level) {
    const BowInfo& b = GetBow(bow);
    const ArrowInfo& a = GetArrow(arrow);
    const Skills s = Hero(level);
    const int ticks = WeaponTicks(b.item);
    double best = 0.0;
    for (CombatStyle style : {CombatStyle::Accurate, CombatStyle::Aggressive}) {
        Fighter f = RangedFighter(s, style, b.accuracy, a.strength, 0);
        f.swingScale = static_cast<float>(ticks) / 4.0f;
        best = std::max(best, Make("bow", b.name, level, f, Creature(level), ticks).dps);
    }
    return best;
}

// The best damage per second of a class among the weapons usable at this level (0 if there are none).
inline double BestDps(const std::vector<Row>& rows, const char* cls, int level) {
    double best = 0.0;
    for (const Row& r : rows)
        if (r.cls == cls && r.level <= level) best = std::max(best, r.dps);
    return best;
}
}  // namespace balance
