// Skills and experience. Pure C++ (no raylib) so it is unit tested.
// Every skill starts at level 1 and tops out at 99, using the classic RuneScape experience curve.
#pragma once
#include <array>
#include <cstdint>

namespace journey {

enum class Skill : int {
    Attack, Strength, Defence, Hitpoints, Ranged, Magic,
    Woodcutting, Mining, Fishing, Cooking, Smithing, Crafting, Firemaking,
    Count
};
constexpr int kSkillCount = static_cast<int>(Skill::Count);
constexpr int kMaxLevel = 99;
constexpr int64_t kMaxXp = 200000000;

const char* SkillName(Skill s);  // "Woodcutting"
const char* SkillKey(Skill s);   // "woodcutting" (used in the save file)

// Total XP needed to reach `level` (1..99). Level 1 = 0, level 2 = 83, level 99 = 13,034,431.
int64_t XpForLevel(int level);
// Level (1..99) for an XP total.
int LevelForXp(int64_t xp);

class Skills {
public:
    int Level(Skill s) const { return level_[Index(s)]; }
    int64_t Xp(Skill s) const { return xp_[Index(s)]; }
    // XP into the current level and how much that level needs (for progress bars). At 99: 1/1.
    void Progress(Skill s, int64_t* into, int64_t* needed) const;

    // Adds XP (clamped to 200M). Returns true if the level went up; *newLevel is set when it did.
    bool AddXp(Skill s, int64_t amount, int* newLevel = nullptr);
    // Loading a save: set the XP directly and recompute the level.
    void SetXp(Skill s, int64_t xp);

    int TotalLevel() const;
    int64_t TotalXp() const;

private:
    static int Index(Skill s) { return static_cast<int>(s); }
    std::array<int, kSkillCount> level_ = [] {
        std::array<int, kSkillCount> a{};
        a.fill(1);
        return a;
    }();
    std::array<int64_t, kSkillCount> xp_{};
};

}  // namespace journey
