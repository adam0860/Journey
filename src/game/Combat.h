// Combat rules. Pure C++ (unit tested). Modelled on the classic game, with turns you can see.
//
// Fighting runs on the design document's tick of 0.6 seconds. A creature strikes every 4 ticks (2.4 s); the hero strikes every
// "weapon ticks" (Gear.h: WeaponTicks: a dagger takes 3, most weapons 4, a battleaxe 5, a warhammer 6), starting half a second after
// he reaches the target, so with a 4-tick weapon the hits alternate (you, them, you, them...). A strike either misses (a blue 0)
// or lands for 1..max damage (a red number). A slower weapon hits harder per swing (Fighter::swingScale), so no weapon is simply
// better: they differ in how their damage comes (a few big hits or many small ones) and in accuracy.
//
//   Hit chance:  compares an attack roll with a defence roll (the Old School formula).
//   Max hit:     from the attacker's Strength level.
//   XP:          4 per damage point to the skill your combat style trains, plus 1.33 per damage to Hitpoints.
#pragma once
#include <functional>

#include "game/Skills.h"

namespace journey {

constexpr float kTickSeconds = 0.6f;            // the game tick
constexpr int kDefaultSwingTicks = 4;           // an ordinary weapon, fists, bows and spells
constexpr float kCreatureSwingSeconds = 4 * kTickSeconds;   // how often a creature strikes
// Health comes back by itself at one hit point a minute, for everybody: the hero, the other adventurers and the creatures. (Food is the quick way.)
constexpr float kRegenSeconds = 60.0f;
constexpr float kTurnSeconds = 1.2f;     // (the old turn: half a swing of a 4-tick weapon)
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
    float swingScale = 1.0f;                                     // damage of one swing relative to a 4-tick weapon (ticks / 4)
};

// The bonuses come from worn gear (Equipment::Total()).
Fighter PlayerFighter(const Skills& skills, CombatStyle style, int attackBonus = 0, int strengthBonus = 0, int defenceBonus = 0);

struct AttackResult {
    bool landed = false;
    int damage = 0;   // 0 for a miss
};

// How the hero attacks: with what is in his hand. A bow shoots (Ranged), a staff casts (Magic), anything else is Melee.
enum class AttackKind { Melee, Ranged, Magic };
// Archery uses the same maths as melee with the Ranged level: the bow adds accuracy, the arrows add strength. The styles are
// Accurate (+3 aim), Rapid (= Aggressive: +3 strength) and Longrange (= Defensive: +3 defence and 2 more tiles of reach).
Fighter RangedFighter(const Skills& skills, CombatStyle style, int rangedBonus, int rangedStrengthBonus, int defenceBonus);
// A spell always hurts for its own damage (RollSpell's maxHit); the Magic level and the staff only decide whether it lands.
Fighter MagicFighter(const Skills& skills, int magicBonus, int defenceBonus);
AttackResult RollSpell(const Fighter& caster, int maxHit, const Fighter& defender, const std::function<double()>& roll);

int MaxHit(const Fighter& attacker);
// Chance (0..1) that a strike by `attacker` lands on `defender`.
double HitChance(const Fighter& attacker, const Fighter& defender);

// `roll` returns numbers in [0,1). Uses one roll for the hit and one for the damage.
AttackResult RollAttack(const Fighter& attacker, const Fighter& defender, const std::function<double()>& roll);

struct CombatXp {
    int attack = 0, strength = 0, defence = 0, hitpoints = 0;
    int ranged = 0, magic = 0;
};
CombatXp XpForDamage(CombatStyle style, int damage);
// Archery: 4 Ranged XP per damage (Longrange: 2 Ranged and 2 Defence), plus 1.33 Hitpoints.
CombatXp XpForRangedDamage(CombatStyle style, int damage);
// Spells: 2 Magic XP per damage (on top of the spell's own XP, given when it is cast), plus 1.33 Hitpoints.
CombatXp XpForSpellDamage(int damage);

// The colour of an enemy's level number: green well below you, yellow equal, red well above.
struct Rgb {
    unsigned char r = 255, g = 255, b = 255;
};
Rgb LevelColor(int playerCombatLevel, int enemyLevel);

}  // namespace journey
