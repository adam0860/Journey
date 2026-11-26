#include "game/Skilling.h"

#include <algorithm>

#include "game/Woodcutting.h"

namespace journey {

namespace {

const TreeInfo kTrees[] = {
    {"round", "Tree", 1, 25, Item::Logs, 180.0f, 1.0f, "A commonly found tree."},
    {"pine", "Tree", 1, 25, Item::Logs, 180.0f, 1.0f, "A commonly found tree."},
    {"tall", "Tree", 1, 25, Item::Logs, 180.0f, 1.0f, "A commonly found tree."},
    {"oak", "Oak tree", 15, 38, Item::OakLogs, 240.0f, 0.90f, "A large oak tree. Its wood is strong."},
    {"willow", "Willow tree", 30, 68, Item::WillowLogs, 300.0f, 0.85f, "A drooping willow tree. It likes water."},
    {"yew", "Yew tree", 60, 175, Item::YewLogs, 480.0f, 0.75f, "An old yew tree. The best wood for bows."},
    {"teak", "Teak tree", 35, 85, Item::TeakLogs, 360.0f, 0.80f, "A tall teak tree with pale, oily wood."},
    {"maple", "Maple tree", 45, 100, Item::MapleLogs, 420.0f, 0.78f, "A maple tree with flame-coloured leaves."},
    {"mahogany", "Mahogany tree", 50, 125, Item::MahoganyLogs, 480.0f, 0.72f, "A vast mahogany tree. Its wood is dark and dense."},
    {"magic", "Magic tree", 75, 250, Item::MagicLogs, 720.0f, 0.60f, "A tree that hums softly. Its leaves are violet."},
};

const LogInfo kLogs[] = {
    {Item::Logs, 1, 40},
    {Item::OakLogs, 15, 60},
    {Item::WillowLogs, 30, 90},
    {Item::YewLogs, 60, 202},
    {Item::TeakLogs, 35, 105},
    {Item::MapleLogs, 45, 135},
    {Item::MahoganyLogs, 50, 158},
    {Item::MagicLogs, 75, 303},
};

constexpr Item kHatchets[kTierCount] = {Item::BronzeAxe, Item::IronAxe, Item::SteelAxe, Item::MithrilAxe, Item::AdamantAxe};
constexpr int kHatchetLevel[kTierCount] = {1, 6, 21, 31, 41};
constexpr float kHatchetSpeed[kTierCount] = {1.0f, 1.25f, 1.55f, 1.9f, 2.3f};

const std::vector<SpotInfo>& Spots() {
    static const std::vector<SpotInfo> spots = {
        {"net", "Fishing spot", "Net", Item::SmallNet, Item::None, "You cast out your net...",
         {{Item::RawShrimps, 1, 10, 100}, {Item::RawSardine, 5, 20, 60}, {Item::RawHerring, 10, 30, 45}, {Item::RawAnchovies, 15, 40, 30},
          {Item::RawMackerel, 16, 20, 40}, {Item::RawCod, 23, 45, 35}}},
        {"rod", "Fly fishing spot", "Lure", Item::FlyFishingRod, Item::Feathers, "You cast out your line...",
         {{Item::RawTrout, 20, 50, 100}, {Item::RawPike, 25, 60, 50}, {Item::RawEel, 28, 65, 25}, {Item::RawSalmon, 30, 70, 40}}},
        {"cage", "Cage fishing spot", "Cage", Item::LobsterPot, Item::None, "You lower your lobster pot...",
         {{Item::RawLobster, 40, 90, 100}, {Item::RawBass, 46, 100, 40}, {Item::RawSeaTurtle, 79, 160, 12}}},
        {"harpoon", "Harpoon fishing spot", "Harpoon", Item::Harpoon, Item::None, "You raise your harpoon...",
         {{Item::RawTuna, 35, 80, 100}, {Item::RawSwordfish, 50, 100, 30}, {Item::RawShark, 76, 150, 20}, {Item::RawMantaRay, 81, 180, 10}}},
    };
    return spots;
}

}  // namespace

const TreeInfo& GetTreeInfo(const std::string& kind) {
    for (const TreeInfo& t : kTrees)
        if (kind == t.kind) return t;
    return kTrees[0];
}

const LogInfo* GetLogInfo(Item logs) {
    for (const LogInfo& l : kLogs)
        if (l.logs == logs) return &l;
    return nullptr;
}

Item BurnableLog(const Inventory& bag, int firemakingLevel) {
    for (const LogInfo& l : kLogs)
        if (bag.Has(l.logs) && firemakingLevel >= l.fmLevel) return l.logs;
    return Item::None;
}

Item AnyLog(const Inventory& bag) {
    for (const LogInfo& l : kLogs)
        if (bag.Has(l.logs)) return l.logs;
    return Item::None;
}

bool IsHatchet(Item item) {
    for (Item h : kHatchets)
        if (h == item) return true;
    return false;
}

int HatchetLevel(Item item) {
    for (int i = 0; i < kTierCount; ++i)
        if (kHatchets[i] == item) return kHatchetLevel[i];
    return 1;
}

float HatchetSpeed(Item item) {
    for (int i = 0; i < kTierCount; ++i)
        if (kHatchets[i] == item) return kHatchetSpeed[i];
    return 1.0f;
}

Item BestHatchet(const Inventory& bag, int woodcuttingLevel) {
    for (int i = kTierCount - 1; i >= 0; --i)
        if (bag.Has(kHatchets[i]) && woodcuttingLevel >= kHatchetLevel[i]) return kHatchets[i];
    return Item::None;
}

float TreeChopChance(const TreeInfo& tree, int woodcuttingLevel, float hatchetSpeed) {
    const int eff = std::max(1, woodcuttingLevel - tree.level + 1);   // levels above the tree's own requirement
    return std::clamp(ChopChance(eff) * tree.difficulty * hatchetSpeed, 0.05f, 0.95f);
}

const SpotInfo& GetSpotInfo(const std::string& kind) {
    for (const SpotInfo& s : Spots())
        if (kind == s.kind) return s;
    return Spots()[0];
}

Item RollSpotCatch(const SpotInfo& spot, int fishingLevel, double roll) {
    int total = 0;
    for (const FishDef& f : spot.fish)
        if (fishingLevel >= f.level) total += f.weight;
    if (total <= 0) return Item::None;
    double pick = roll * total;
    for (const FishDef& f : spot.fish) {
        if (fishingLevel < f.level) continue;
        if (pick < f.weight) return f.raw;
        pick -= f.weight;
    }
    return Item::None;
}

const FishDef* FindFish(Item raw) {
    for (const SpotInfo& s : Spots())
        for (const FishDef& f : s.fish)
            if (f.raw == raw) return &f;
    return nullptr;
}

}  // namespace journey
