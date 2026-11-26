// Quests: a chain of steps from a giver to a reward. Pure C++ (unit tested). All of it is data (Quests.cpp).
//
// A quest starts when you talk to its giver (who offers it once you meet the requirements). Then each step is one of
//   Kill  - kill so many of a creature (optionally with a bow or a spell),
//   Have  - bring so many of an item to a person (they take it, or just look at it),
//   Reach - walk into a named place,
//   Use   - use an item in a named place (bag "Use", then tap the ground there): dig with a spade, light a candle, turn a key. The item
//           may be used up, it may need a tool (a tinderbox), and the step can hand him something (what the spade turned up),
//   Talk  - go and speak to a person.
// Kill, Reach and Use steps complete by themselves; Have and Talk steps complete when you speak to the right person. The last step of
// every quest is one of those, so the story always ends with someone thanking you. Rewards: XP, coins and items.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "game/Combat.h"
#include "game/Inventory.h"
#include "game/Npc.h"
#include "game/Skills.h"

namespace journey {

enum class StepKind { Kill, Have, Reach, Talk, Use };

constexpr int kAnyWeapon = -1;   // QuestStep::killKind: any way of killing counts

struct QuestStep {
    StepKind kind;
    std::string objective;     // "Kill 6 goblins" (the quest log)
    std::string person;        // Have / Talk: the person to go to (a key from People.cpp)
    Species species = Species::Chicken;   // Kill
    int killKind = kAnyWeapon;            // Kill: -1 any, or an AttackKind (0 melee, 1 ranged, 2 magic)
    int count = 1;                        // Kill: how many / Have: how many
    Item item = Item::None;               // Have / Use
    bool take = true;                     // Have: they keep what you bring / Use: one of the item is used up
    std::string area;                     // Reach / Use: the name of a map area
    Item tool = Item::None;               // Use: needed in the bag, not used up (a tinderbox to light a candle)
    Item give = Item::None;               // Use: what he finds when the step is done
    int giveCount = 1;
    std::string say;                      // what the person says when you finish the step (Have / Talk)
};

struct QuestReward {
    std::vector<std::pair<Skill, int>> xp;
    std::vector<ItemStack> items;
    int coins = 0;
};

struct QuestInfo {
    std::string key;           // "goblin_trouble" (the save file)
    std::string name;          // "Goblin Trouble"
    std::string giver;         // the person who offers it
    std::string intro;         // what the giver says when offering it
    std::string outro;         // what is said after the reward
    std::string after;         // a quest that must be finished first ("" = none)
    int minCombat = 0;         // combat level needed
    Skill reqSkill = Skill::Attack;
    int reqLevel = 0;          // a skill level needed (0 = none)
    int minPoints = 0;         // quest points (= quests finished) needed
    std::vector<QuestStep> steps;
    QuestReward reward;
};

const std::vector<QuestInfo>& AllQuests();
int FindQuest(const std::string& key);   // index or -1

// Where a quest wants him to go next, for the map: the person to see, the creature to hunt, the place to reach. `offer` marks a person who
// has a quest to give him now (not a step of a quest he is on).
struct QuestMarker {
    float x = 0, z = 0;
    std::string quest;   // "Goblin Trouble"
    std::string what;    // "Kill 6 goblins"
    bool offer = false;
};

struct QuestState {
    bool started = false;
    bool done = false;
    int step = 0;      // the step you are on
    int count = 0;     // progress within a Kill step
};

class QuestLog {
public:
    QuestLog();
    int Count() const { return static_cast<int>(states_.size()); }
    const QuestState& State(int i) const { return states_[static_cast<size_t>(i)]; }
    int Points() const;   // quests finished

    // Can this quest be offered to the hero right now? (not started, requirements met)
    bool CanStart(int i, const Skills& skills) const;
    // What is missing for a quest he cannot start yet ("" if he can) - for the giver to say.
    std::string Missing(int i, const Skills& skills) const;
    void Start(int i);
    const QuestStep* Current(int i) const;   // the step he is on (nullptr if not started / done)

    // Events. They return the index of a quest whose step just completed, or -1. `kind` is the AttackKind of the kill.
    int OnKill(Species s, AttackKind kind);
    int OnReach(const std::string& area);
    // Using an item in a place: the quest whose current step that completes (nothing changes), or -1. WantsItem: is any current step waiting for this item?
    int FindUse(Item item, const std::string& area) const;
    bool WantsItem(Item item) const;

    // Can the hero hand in the current step of quest i to this person right now? (Have: carries enough; Talk: right person)
    bool StepReady(int i, const std::string& person, const Inventory& bag) const;
    // Completes the current step (taking the items for a Have step). Returns true if that finished the whole quest.
    bool CompleteStep(int i, Inventory* bag);
    // Marks the quest finished (the caller gives the reward).
    void Finish(int i);

    // "Kill 6 goblins (2/6)" / "Bring 5 logs to Wilf (you have 3)"
    std::string ObjectiveText(int i, const Inventory& bag) const;

    std::string Serialize() const;                         // lines "quest.<key>=step:count:done"
    void ParseLine(const std::string& key, const std::string& value);   // one "quest.<key>" line (the part after "quest.")

private:
    std::vector<QuestState> states_;
};

}  // namespace journey
