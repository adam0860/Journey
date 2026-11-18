// Combat rules. Pure C++ (unit tested). Modelled on the classic game, with turns you can see.
//
// Fighting is turn based: every kTurnSeconds the turn passes between you and the things fighting you, so
// the hits alternate (you, them, you, them...). On your turn you strike once; on their turn each creature that
// is next to you strikes once. A strike either misses (a blue 0) or lands for 1..max damage (a red number).
//
//   Hit chance:  compares an attack roll with a defence roll (the Old School formula).
//   Max hit:     from the attacker's Strength level.
//   XP:          4 per damage point to the skill your combat style trains, plus 1.33 per damage to Hitpoints.
#pragma once
#include <functional>

#include "game/Skills.h"

namespace journey {

constexpr float kTurnSeconds = 1.2f;     // one turn: the hero's strike, or the creatures' strikes
constexpr float kImpactSeconds = 0.30f;  // after a strike starts, the hit lands (the animation's contact point)

enum class CombatStyle { Accurate, Aggressive, Defensive, Controlled };
constexpr int kCombatStyleCount = 4;
const char* StyleName(CombatStyle s);     // "Accurate"
const char* StyleTrains(CombatStyle s);   // "Attack XP"
CombatStyle StyleFromIndex(int i);

// Everything the hit formulas need about one fighter. The *Bonus fields come from worn weapons and armour.
struct Fighter {
    int attack = 1, strength = 1, defence = 1;
    int attackBonus = 0, strengthBonus = 0, defenceBonus = 0;
    int styleAttack = 0, styleStrength = 0, styleDefence = 0;   // the "+3 / +1" a combat style gives
    bool creature = false;                                       // creatures: +9 to defend and hit hard, but only +4 to aim (clumsy)
};

// The bonuses come from worn gear (Equipment::Total()).
Fighter PlayerFighter(const Skills& skills, CombatStyle style, int attackBonus = 0, int strengthBonus = 0, int defenceBonus = 0);

int MaxHit(const Fighter& attacker);
// Chance (0..1) that a strike by `attacker` lands on `defender`.
double HitChance(const Fighter& attacker, const Fighter& defender);

struct AttackResult {
    bool landed = false;
    int damage = 0;   // 0 for a miss
};
// `roll` returns numbers in [0,1). Uses one roll for the hit and one for the damage.
AttackResult RollAttack(const Fighter& attacker, const Fighter& defender, const std::function<double()>& roll);

struct CombatXp {
    int attack = 0, strength = 0, defence = 0, hitpoints = 0;
};
CombatXp XpForDamage(CombatStyle style, int damage);

// The colour of an enemy's level number: green well below you, yellow equal, red well above.
struct Rgb {
    unsigned char r = 255, g = 255, b = 255;
};
Rgb LevelColor(int playerCombatLevel, int enemyLevel);

}  // namespace journey
