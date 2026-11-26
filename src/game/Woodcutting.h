// The chopping rules. Pure C++ (unit tested) - main.cpp only feeds it time and acts on the result.
//
// Like the classic game, the world runs on 0.6 second "ticks". Every tick you swing, and the chance of
// getting a log climbs with your Woodcutting level. One log per tree: the tree becomes a stump.
#pragma once
#include <functional>

namespace journey {

constexpr float kChopTickSeconds = 0.6f;
constexpr int kLogXp = 25;   // Woodcutting XP for one log from a normal tree

// Chance (0..1) that a swing at a normal tree gets the log, for a Woodcutting level 1..99.
float ChopChance(int woodcuttingLevel);

enum class ChopEvent { None, Swing, GotLog };

// One in-progress chopping job on one tree.
class ChopSession {
public:
    void Start(int treeIndex) {
        tree_ = treeIndex;
        timer_ = 0.0f;
    }
    void Stop() { tree_ = -1; }
    bool Active() const { return tree_ >= 0; }
    int Tree() const { return tree_; }

    // Advance time. `roll` returns a random number in [0,1). Returns what happened this call
    // (at most one event; call again next frame). GotLog ends the session.
    ChopEvent Update(float dt, int woodcuttingLevel, const std::function<double()>& roll);
    // The same, with the chance of a log per swing worked out by the caller (tree kind, axe).
    ChopEvent UpdateWithChance(float dt, float chance, const std::function<double()>& roll);

private:
    int tree_ = -1;
    float timer_ = 0.0f;
};

}  // namespace journey
