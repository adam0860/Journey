#include "game/Combat.h"

#include <algorithm>
#include <cmath>

namespace journey {

const char* StyleName(CombatStyle s) {
    switch (s) {
        case CombatStyle::Accurate: return "Accurate";
        case CombatStyle::Aggressive: return "Aggressive";
        case CombatStyle::Defensive: return "Defensive";
        case CombatStyle::Controlled: return "Controlled";
    }
    return "";
}

const char* StyleTrains(CombatStyle s) {
    switch (s) {
        case CombatStyle::Accurate: return "Attack XP";
        case CombatStyle::Aggressive: return "Strength XP";
        case CombatStyle::Defensive: return "Defence XP";
        case CombatStyle::Controlled: return "Shared XP";
    }
    return "";
}

CombatStyle StyleFromIndex(int i) {
    return static_cast<CombatStyle>(std::clamp(i, 0, kCombatStyleCount - 1));
}

Fighter PlayerFighter(const Skills& skills, CombatStyle style, int attackBonus, int strengthBonus, int defenceBonus) {
    Fighter f;
    f.attackBonus = attackBonus;
    f.strengthBonus = strengthBonus;
    f.defenceBonus = defenceBonus;
    f.attack = skills.Level(Skill::Attack);
    f.strength = skills.Level(Skill::Strength);
    f.defence = skills.Level(Skill::Defence);
    switch (style) {   // the classic "+3 to the trained skill" (or +1 to all three when controlled)
        case CombatStyle::Accurate: f.styleAttack = 3; break;
        case CombatStyle::Aggressive: f.styleStrength = 3; break;
        case CombatStyle::Defensive: f.styleDefence = 3; break;
        case CombatStyle::Controlled: f.styleAttack = f.styleStrength = f.styleDefence = 1; break;
    }
    return f;
}

namespace {
// "Effective level" = the level plus a fixed bonus. Creatures defend and hit like the classic +9, but their strikes are
// a little clumsy (+4 instead of +8 when aiming), which keeps the first fights gentle.
int EffectiveAim(const Fighter& f) {
    return f.attack + (f.creature ? 4 : 8) + f.styleAttack;
}
int EffectiveDefence(const Fighter& f) {
    return f.defence + (f.creature ? 9 : 8) + f.styleDefence;
}
int EffectiveStrength(const Fighter& f) {
    return f.strength + (f.creature ? 9 : 8) + f.styleStrength;
}
}  // namespace

int MaxHit(const Fighter& a) {
    const double eff = EffectiveStrength(a);
    return static_cast<int>(std::floor(0.5 + eff * (a.strengthBonus + 64) / 640.0));
}

double HitChance(const Fighter& a, const Fighter& d) {
    const double attackRoll = EffectiveAim(a) * (a.attackBonus + 64.0);
    const double defenceRoll = EffectiveDefence(d) * (d.defenceBonus + 64.0);
    if (attackRoll > defenceRoll) return 1.0 - (defenceRoll + 2.0) / (2.0 * (attackRoll + 1.0));
    return attackRoll / (2.0 * (defenceRoll + 1.0));
}

AttackResult RollAttack(const Fighter& a, const Fighter& d, const std::function<double()>& roll) {
    AttackResult r;
    r.landed = roll() < HitChance(a, d);
    if (r.landed) {
        const int mx = std::max(1, MaxHit(a));
        r.damage = std::clamp(1 + static_cast<int>(roll() * mx), 1, mx);
    }
    return r;
}

CombatXp XpForDamage(CombatStyle style, int damage) {
    CombatXp x;
    if (damage <= 0) return x;
    x.hitpoints = static_cast<int>(std::lround(damage * 4.0 / 3.0));
    const int main = damage * 4;
    switch (style) {
        case CombatStyle::Accurate: x.attack = main; break;
        case CombatStyle::Aggressive: x.strength = main; break;
        case CombatStyle::Defensive: x.defence = main; break;
        case CombatStyle::Controlled:
            x.attack = x.strength = x.defence = static_cast<int>(std::lround(damage * 4.0 / 3.0));
            break;
    }
    return x;
}

Rgb LevelColor(int player, int enemy) {
    const float d = static_cast<float>(std::clamp(enemy - player, -9, 9));
    // -9 or lower: green; 0: yellow; +9 or higher: red (a straight blend in between)
    const float r = d <= 0.0f ? 255.0f * (d + 9.0f) / 9.0f : 255.0f;
    const float g = d <= 0.0f ? 255.0f : 255.0f * (9.0f - d) / 9.0f;
    return {static_cast<unsigned char>(std::lround(r)), static_cast<unsigned char>(std::lround(g)), 0};
}

}  // namespace journey
