#include "game/Skills.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace journey {

namespace {
const char* kNames[kSkillCount] = {"Attack", "Strength", "Defence", "Hitpoints", "Ranged", "Magic", "Woodcutting",
                                   "Mining", "Fishing",  "Cooking", "Smithing",  "Crafting", "Firemaking"};
const char* kKeys[kSkillCount] = {"attack", "strength", "defence", "hitpoints", "ranged", "magic", "woodcutting",
                                  "mining", "fishing",  "cooking", "smithing",  "crafting", "firemaking"};

// xp[level-1] = total XP for that level. Built once from the RuneScape formula:
//   points(L) = sum_{i=1}^{L-1} floor(i + 300 * 2^(i/7));   xp = floor(points / 4)
const std::vector<int64_t>& Table() {
    static const std::vector<int64_t> t = [] {
        std::vector<int64_t> v(kMaxLevel);
        double points = 0;
        for (int level = 1; level <= kMaxLevel; ++level) {
            v[level - 1] = static_cast<int64_t>(std::floor(points / 4.0));
            points += std::floor(level + 300.0 * std::pow(2.0, level / 7.0));
        }
        return v;
    }();
    return t;
}
}  // namespace

const char* SkillName(Skill s) { return kNames[static_cast<int>(s)]; }
const char* SkillKey(Skill s) { return kKeys[static_cast<int>(s)]; }

int64_t XpForLevel(int level) {
    level = std::clamp(level, 1, kMaxLevel);
    return Table()[level - 1];
}

int LevelForXp(int64_t xp) {
    const auto& t = Table();
    // last level whose threshold is <= xp
    const auto it = std::upper_bound(t.begin(), t.end(), xp);
    return std::max(1, static_cast<int>(it - t.begin()));
}

void Skills::Progress(Skill s, int64_t* into, int64_t* needed) const {
    const int lv = Level(s);
    if (lv >= kMaxLevel) {
        *into = 1;
        *needed = 1;
        return;
    }
    *into = Xp(s) - XpForLevel(lv);
    *needed = XpForLevel(lv + 1) - XpForLevel(lv);
}

bool Skills::AddXp(Skill s, int64_t amount, int* newLevel) {
    if (amount <= 0) return false;
    const int i = Index(s);
    xp_[i] = std::min(kMaxXp, xp_[i] + amount);
    const int lv = LevelForXp(xp_[i]);
    if (lv > level_[i]) {
        level_[i] = lv;
        if (newLevel) *newLevel = lv;
        return true;
    }
    return false;
}

void Skills::SetXp(Skill s, int64_t xp) {
    const int i = Index(s);
    xp_[i] = std::clamp<int64_t>(xp, 0, kMaxXp);
    level_[i] = LevelForXp(xp_[i]);
}

int Skills::TotalLevel() const {
    int n = 0;
    for (int l : level_) n += l;
    return n;
}

int64_t Skills::TotalXp() const {
    int64_t n = 0;
    for (int64_t x : xp_) n += x;
    return n;
}

}  // namespace journey
