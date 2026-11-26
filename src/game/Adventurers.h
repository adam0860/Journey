// Other adventurers: the world is full of "players", like an online game that happens to run offline. Pure C++ (unit tested, no raylib).
//
// Two kinds share the map with the hero:
//
//  * REAL adventurers (kRealCount of them) start the game when the hero does: every skill at level 1, 10 hit points, the starter kit and nothing
//    else. They play for real, in real time, by the same rules as he does: they chop, fish, mine, burn logs, cook, smelt and smith with the same
//    skill tables, fight the real creatures with the same combat formulas and loot tables, bury bones, buy and sell in the shops, and do the quests.
//    Everything they own comes from skilling, drops, quest rewards or the shops (what they bought with coins they earned): see AdvSource.
//    They are saved with the hero's progress (journey_adventurers.cfg).
//
//  * FILLER adventurers (a few dozen) are scenery with a face: they have a fixed level and fixed gear, spread from level 3 to the high nineties,
//    and they work at the trade that suits them (trees, rocks, fishing spots, anvils, creatures), walk about and talk, but never gain a level or
//    change what they wear.
//
// Both are drawn with the hero's own modular body (main.cpp), carry a name above their heads, say things (speech bubbles and the chat box) and can be
// examined. This file is only rules and data: positions and animations are for the renderer to read.
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "game/Combat.h"
#include "game/Equipment.h"
#include "game/Fires.h"
#include "game/Fishing.h"
#include "game/Foes.h"
#include "game/Forest.h"
#include "game/GroundItems.h"
#include "game/Inventory.h"
#include "game/Mining.h"
#include "game/Npc.h"
#include "game/People.h"
#include "game/Quests.h"
#include "game/Shops.h"
#include "game/Skills.h"
#include "game/Woodcutting.h"
#include "world/TileMap.h"

namespace journey {

enum class AdvKind { Real, Filler };

// What the body is doing right now (the renderer turns this into an animation).
enum class AdvAct { Idle, Walk, Chop, Fish, Mine, Smelt, Smith, Cook, Light, Fight, Bury, Chat, Rest };

// What a real adventurer is working on. A goal is one errand; a plan is a queue of them (chop twenty logs, then burn them...).
enum class AdvTask { None, Rest, Chop, Fish, Mine, Burn, Cook, Smelt, Smith, Fight, Shop, Talk, Reach, UseHere, Abyss };   // (Abyss: an expedition: roam at a depth, fight players, take loot)

// Where an item came from. Real adventurers never get anything from thin air: the test of this file checks that everything they wear came by one of
// these roads (and not Starter, for anything better than the first kit).
enum AdvSource : unsigned { kSrcStarter = 1, kSrcSkill = 2, kSrcDrop = 4, kSrcQuest = 8, kSrcShop = 16, kSrcSmith = 32, kSrcCook = 64 };

struct AdvGoal {
    AdvTask task = AdvTask::None;
    std::string key;            // Chop: the tree kind / Fish: the spot kind / Mine: the rock key / Talk, Shop: the person / Reach, UseHere: the area
    int a = 0;                  // Fight: the Species / Smelt, Smith: the Tier / Smith: (b) the GearType / Rest: seconds
    int b = 0;
    int quota = 0;              // how many of the thing to get (0 = until the bag is full or the time is up)
    int got = 0;
    float left = 900.0f;        // seconds before the errand is given up
    int quest = -1;             // the quest this errand is for
    bool startQuest = false;    // Talk: the giver offers the quest, he accepts it
    Item want = Item::None;     // Fight / Chop...: stop when the bag holds `wantCount` of this
    int wantCount = 0;
    bool abyss = false;         // Chop / Mine: only trees and rocks inside the Abyss
    bool sellAll = false;       // Shop: sell what he does not need
    std::vector<ItemStack> buy; // Shop: what to buy
};

enum class AdvPhase { Plan, Travel, Work, Done };

// The thinking of a real adventurer (only they have it).
struct AdvMind {
    std::deque<AdvGoal> queue;
    AdvGoal goal;
    bool hasGoal = false;
    AdvPhase phase = AdvPhase::Plan;
    int target = -1;            // the tree / rock / spot / station / creature / person he is working at (an index into the world's list)
    TileCoord stand{};          // where he stands to do it
    ChopSession chop;
    MineSession mine;
    FishSession fish;
    LightSession light;
    float timer = 0;            // general purpose clock of the current errand
    float travelLeft = 0;
    float swingT = 0, creatureT = 0;   // a fight: his swing and the creature's
    float regen = 0;            // hit point regeneration (one point a minute: kRegenSeconds)
    double hpXpCarry = 0;
    float bias[kSkillCount] = {};   // how much he likes training each skill
    std::map<int, float> avoidTree, avoidRock, avoidNpc;   // things that did not work out (so he does not go back at once)
    std::string area;           // the place he last stood in (for "reach" quest steps)
    float thinkLeft = 0;
    int sub = 0;                // Cook: 1 = a fire, 2 = a range / Fight: 1 = the fight has begun
    float eatLeft = 0;
    std::map<int, int> questFails;   // quest -> how many errands for it went wrong (3 and he gives up on it)
    int failStreak = 0;
    int deaths = 0;
    float buryLeft = 0;
    Item burying = Item::None;
    float cookLeft = 0;
    std::string doing = "getting started";   // for "what are you up to?"
    double carryXp[kSkillCount] = {};
    float nextAbyss = 0;        // the clock time before which he does not set out for the Abyss again
    bool fleeing = false;       // hurt in the Abyss: heading out
    bool wasFighting = false;   // he was in a player fight last frame (afterwards he picks up his errand again)
    float roamWait = 0;         // an Abyss expedition: standing about before the next waypoint
    int abyssTrips = 0;
    int partner = 0;            // an Abyss expedition in company: the other adventurer (valid only while he points back at him)
    bool follower = false;      // ...and he is the one who follows
    float partnerUntil = 0;     // the company lasts until this clock time (a while after the expedition: they do not turn on each other at once)
    bool abyssEager = false;    // (tests: set out as soon as he plans)
};

// What a filler does all day.
enum class FillerJob { Chat, Chop, Fish, Mine, Smith, Fight, Cook, Wander, Hunt };   // (Hunt: the Abyss hunters, who pick fights with the hero)

struct FillerMind {
    FillerJob job = FillerJob::Chat;
    TileCoord hubTile{};        // the middle of where he lives
    int hubRadius = 12;
    float stateLeft = 0;        // time left in the current thing
    float workX = 0, workZ = 0; // what he works at
    int npcId = -1;             // Fight: the creature he faces
    bool working = false;       // at his work spot, doing it
    bool heading = false;       // walking to his work spot
    float flinchLeft = 0;
    bool bound = false;         // stays inside the area he lives in (the Abyss people must not wander out of it)
    std::string area;           // ...which one
};

// What an adventurer carries into a fight with another player (the Abyss).
struct AdvPvp {
    float skull = 0;            // seconds of skull left: killed with a skull, he drops everything
    float provoked = 0;         // seconds left of "somebody attacked me": he fights back, and hitting back earns no skull
    bool fightingHero = false;
    int foe = 0;                // the adventurer he is fighting (0 = none; the hero is fightingHero)
    int provokedBy = 0;         // who attacked him (-1 = the hero, otherwise an adventurer id)
    int beast = 0;              // the Abyss creature (an Npc id) he is fighting (0 = none)
    bool pk = false;            // a real adventurer on an expedition who picks fights with other players
    float cooldown = 0;         // no new fights for this long (he is running away)
    float swingT = 0, chaseT = 0, lootT = 0;
    bool dead = false;          // fillers only: away for a while (respawn), invisible
    float respawn = 0;
};

struct AdvSpeech {
    std::string text;
    float left = 0;
};

struct Adventurer {
    int id = 0;
    AdvKind kind = AdvKind::Real;
    std::string name;
    PersonLook look;
    // body
    float x = 0, z = 0, facing = 0, walkPhase = 0;
    std::deque<TileCoord> path;
    AdvAct act = AdvAct::Idle;
    float actTime = 0;          // an animation clock
    float strikeAge = 99, hurtAge = 99;
    bool strikeLeft = false;
    float workX = 0, workZ = 0; // the thing he works on (he faces it)
    float workPhase = 0;        // 0..1 through a swing
    // rules
    Skills skills;
    Inventory bag;
    Equipment equip;
    QuestLog quests;
    CombatStyle style = CombatStyle::Accurate;
    std::map<Item, unsigned> source;   // where each kind of item came from (AdvSource bits)
    AdvSpeech speech;
    float chatCooldown = 0;
    int chatPartner = -1;       // the adventurer he is talking to (-1 = none)
    int chatLines = 0;          // how many lines are left in the conversation he is in
    float chatTimer = 0;
    int level = 0;              // fillers: the level he was made with (their skills are set from it)
    AdvMind mind;               // real adventurers
    FillerMind filler;          // fillers
    AdvPvp pvp;
    int pkKills = 0, pkDeaths = 0;   // his record against other players (kept through a death)
    Equipment kitEquip;         // fillers keep the same gear for ever: what they wear and carry when they come back after a death
    Inventory kitBag;

    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
    int Combat() const { return skills.CombatLevel(); }
    bool Moving() const { return !path.empty(); }
};

// Everything in the world a real adventurer may touch. Built by the game each frame (the same Forest, Rocks, Fires and Npcs the hero uses, so
// a tree he fells is a stump for everyone).
struct AdvWorld {
    TileMap* map = nullptr;
    Forest* forest = nullptr;
    Rocks* rocks = nullptr;
    Fires* fires = nullptr;
    Npcs* npcs = nullptr;
    const People* people = nullptr;
    std::function<double()> roll;   // [0, 1)
    TileCoord hero{0, 0};
    // The Abyss fights: the ground (what the dead leave behind), and the hero as somebody to fight (the game fills these in every frame).
    GroundItems* ground = nullptr;
    float heroX = 0, heroZ = 0;
    bool heroAlive = true, heroInDanger = false, heroSkulled = false;
    int heroCombat = 3;
    Fighter heroFighter;
    std::function<void(int foeId, int damage)> hitHero;   // a blow lands on the hero (Gameplay::StrikeHero)
};

struct AdvChat {
    int id = 0;
    std::string name;
    std::string text;
    float x = 0, z = 0;
    bool major = false;   // a level-up or a quest: worth showing from further away
};

struct PkRank {   // the Abyss hiscore: players killed and times killed by a player
    std::string name;
    int kills = 0, deaths = 0;
    bool hero = false;
};

struct AdvRank {
    std::string name;
    int total = 0;
    int combat = 0;
    bool hero = false;
    bool real = false;
};

constexpr int kAdvRealCount = 8;
constexpr int kAdvFillerCount = 72;   // (eight of them live in the Abyss)

// A random but sensible look for a person (indices as in PersonLook; the renderer clamps them to the catalog). `rng` is an xorshift state.
PersonLook RandomLook(unsigned* rng, bool female);
const char* AdvFillerName(int index);

class Adventurers : public FoeSource {
public:
    // A fresh world: the real ones at level 1 near the spawn, the fillers in their places (`seed` makes the same fillers every time).
    void Init(const AdvWorld& w, unsigned seed = 0x5EED);
    // Brings the real ones back from a save (the fillers are made fresh). Returns false when the text is not a save (they are then fresh).
    bool Load(const AdvWorld& w, const std::string& text, unsigned seed = 0x5EED);
    std::string Save() const;

    void Update(float dt, AdvWorld& w);

    const std::vector<Adventurer>& All() const { return advs_; }
    const Adventurer* Find(int id) const;
    int Online() const { return static_cast<int>(advs_.size()) + 1; }   // (and the hero)

    // New public chat since the last call (the game shows the ones near the hero in the chat box).
    std::vector<AdvChat> TakeChat();

    // "Examine": who they are and what they have done. "Talk": what they say when the hero speaks to them.
    std::string ExamineText(int id) const;
    std::string TalkLine(int id, const std::function<double()>& roll);
    // What they are up to ("chopping oak trees").
    std::string Activity(const Adventurer& a) const;

    // The hiscores: everyone by total level, the hero included.
    std::vector<AdvRank> Ranking(const std::string& heroName, int heroTotal, int heroCombat) const;

    // The Abyss fights (FoeSource): the hero finds, hits and kills them through these.
    bool Get(int foeId, FoeView* out) const override;
    bool HeroHits(int foeId, int damage) override;
    void HeroKilledBy(int foeId) override;
    // The Abyss hiscore: everybody with a record, most kills first (the hero among them once he has one).
    std::vector<PkRank> PkRanking(const std::string& heroName, int heroKills, int heroDeaths) const;

    float Clock() const { return clock_; }
    // For tests: every item every real adventurer wears came from a legitimate source.
    bool Honest(std::string* why = nullptr) const;
    // Dev: simulate `seconds` at once (screenshots of what they have done by then).
    void FastForward(float seconds, AdvWorld& w, float step = 0.25f);

private:
    // ---- building (Adventurers.cpp)
    void MakeReal(const AdvWorld& w, unsigned seed);
    void MakeFillers(const AdvWorld& w, unsigned seed);
    void IndexWorld(const AdvWorld& w);
    // ---- common
    void Walk(Adventurer& a, float dt);
    bool GoTo(Adventurer& a, AdvWorld& w, TileCoord target, int radius = 2);   // a path next to a (maybe blocked) tile; false if there is none
    void Say(Adventurer& a, const std::string& text, float seconds = 4.5f, bool major = false);
    void UpdateChat(Adventurer& a, float dt, AdvWorld& w);
    std::string Line(const Adventurer& a, const std::function<double()>& roll) const;
    void Converse(Adventurer& a, Adventurer& b, AdvWorld& w);
    // ---- fillers (AdventurerFillers.cpp)
    void UpdateFiller(Adventurer& a, float dt, AdvWorld& w);
    void PickFillerWork(Adventurer& a, AdvWorld& w);
    // ---- fights with other players (AdventurerPvp.cpp)
    Fighter FighterOf(const Adventurer& a) const;
    bool PvpUpdate(Adventurer& a, float dt, AdvWorld& w);   // true when he is busy with a fight or loot and the usual life waits
    void PvpBlow(Adventurer& a, void* target, AdvWorld& w);
    // ---- the Abyss creatures and the players (AdventurerPvp.cpp)
    void UpdateBeasts(float dt, AdvWorld& w);                  // the creatures attack the players who come within reach, as they do the hero
    void BeastBlow(Adventurer& a, Npc& n, AdvWorld& w);        // a player's blow at a creature
    bool BeastOk(const Adventurer& a, const Npc& n) const;     // a real adventurer would take it on
    void AwardFightXp(Adventurer& a, int damage);
    void PvpDrop(Adventurer& a, AdvWorld& w);
    void PvpKill(Adventurer& a, AdvWorld& w, int killer = 0);   // killer: -1 the hero, an adventurer id, 0 a creature
    bool Scavenge(Adventurer& a, float dt, AdvWorld& w);
    Adventurer* FindMutable(int id);
    bool PlanAbyssTrip(Adventurer& a, AdvWorld& w);
    void QueueAbyssTrip(Adventurer& a, int level, bool pk, float seconds);
    Adventurer* PartnerOf(const Adventurer& a);   // his companion on this expedition, if both are still on it
    TileCoord AbyssWaypoint(const Adventurer& a, const AdvWorld& w, int level);
    void PvpRespawn(Adventurer& a, const AdvWorld& w);
    // ---- real adventurers (AdventurerBrain.cpp)
    void UpdateReal(Adventurer& a, float dt, AdvWorld& w);
    void Gain(Adventurer& a, Item item, int count, unsigned src);
    void GainXp(Adventurer& a, Skill s, int amount);
    void ManageGear(Adventurer& a);
    void PlanSession(Adventurer& a, AdvWorld& w);
    bool StartGoal(Adventurer& a, AdvWorld& w);
    void WorkGoal(Adventurer& a, float dt, AdvWorld& w);
    void EndGoal(Adventurer& a, AdvWorld& w, bool success);
    void Die(Adventurer& a, AdvWorld& w);
    bool EatNow(Adventurer& a);
    void BotStrike(Adventurer& a, Npc& n, AdvWorld& w);
    void CreatureStrike(Adventurer& a, Npc& n, AdvWorld& w);
    // planning helpers
    bool PlanAcquire(Adventurer& a, AdvWorld& w, Item item, int count, int quest, std::vector<AdvGoal>* out, int depth = 0);
    bool CanAcquire(const Adventurer& a, const AdvWorld& w, Item item, int count, int depth = 0) const;
    bool QuestFeasible(const Adventurer& a, const AdvWorld& w, int qi) const;
    bool PlanQuestStep(Adventurer& a, AdvWorld& w);
    bool FightSafe(const Adventurer& a, Species s) const;
    Species BestFightTarget(const Adventurer& a, const AdvWorld& w) const;
    bool PlanShopTrip(Adventurer& a, AdvWorld& w, bool sell, std::vector<ItemStack> buy, int quest);
    bool ShopLineOpen(const Adventurer& a, const ShopStockDef& d) const;
    int NearestShopFor(const Adventurer& a, const AdvWorld& w, Item item, bool selling, TileCoord* where, int* personIndex) const;
    bool WantsUpgrade(Adventurer& a, AdvWorld& w);
    int NearestTree(Adventurer& a, AdvWorld& w, const std::string& kind) const;
    int NearestRock(Adventurer& a, AdvWorld& w, const std::string& key) const;
    int NearestSpot(const Adventurer& a, const AdvWorld& w, const std::string& kind) const;
    int NearestStation(const Adventurer& a, const AdvWorld& w, const char* kind) const;
    int NearestCreature(Adventurer& a, AdvWorld& w, Species s) const;
    TileCoord TownNear(TileCoord from) const;
    void OnQuestStepDone(Adventurer& a, AdvWorld& w, int qi);
    void FinishQuest(Adventurer& a, AdvWorld& w, int qi);
    std::string ItemNameLower(Item item) const;

    // a conversation between two adventurers: the lines are spoken one after another (speech bubbles)
    struct Convo {
        int a = 0, b = 0;
        std::vector<std::pair<int, std::string>> lines;   // (speaker id, text)
        size_t next = 0;
        float timer = 0;
    };
    std::vector<Convo> convos_;
    struct BeastFight {
        int adv = 0;            // the player it is attacking
        float swingT = 0.8f;
        float left = 45.0f;     // it gives up after this long
    };
    std::map<int, BeastFight> beasts_;   // creature (Npc id) -> its fight
    AdvWorld* world_ = nullptr;      // the world of the last Update (the hero's blows land through HeroHits, outside of it)
    const TileMap* map_ = nullptr;
    std::vector<Adventurer> advs_;
    Shops shops_;                     // their own market (what they buy and sell does not touch the hero's shelves)
    float clock_ = 0;
    int nextId_ = 1;
    std::vector<AdvChat> chat_;
    unsigned rng_ = 12345;
    double Rand();
    int RandInt(int n);
    // the world, indexed once
    struct Town {
        std::string name;
        TileCoord tile;
    };
    std::vector<Town> towns_;
    std::vector<TileCoord> hubs_;
    bool indexed_ = false;
    std::vector<std::pair<std::string, int>> shopPeople_;   // shop key -> index into People::All() (the keeper)
    float saveLeft_ = 0;
};

}  // namespace journey
