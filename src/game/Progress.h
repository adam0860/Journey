// Saving your skills, hit points, combat style, worn gear, bag, bank and chosen spell (journey_progress.cfg). Your look lives in journey_save.cfg (Appearance).
#pragma once
#include <set>
#include <string>

#include "game/Bank.h"
#include "game/Combat.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Quests.h"
#include "game/Skills.h"

namespace journey {

// The save format number, written as the first line ("version=2"). Version 1 files (no number) still load. Bump it when the
// meaning of a line changes, and let ParseProgress / SaveVersion() convert the old way.
constexpr int kSaveVersion = 2;
int SaveVersion(const std::string& text);   // 1 when the file has no "version=" line

std::string SerializeProgress(const Skills& skills, const Inventory& bag, CombatStyle style = CombatStyle::Accurate,
                              const Equipment* worn = nullptr, const Bank* bank = nullptr, int spell = 0, const QuestLog* quests = nullptr,
                              const std::set<std::string>* visited = nullptr, float skullSeconds = 0.0f,
                              int pkKills = 0, int pkDeaths = 0);
// Reads a save. Unknown or damaged lines are ignored, so an old or hand-edited file never crashes the game.
void ParseProgress(const std::string& text, Skills* skills, Inventory* bag, CombatStyle* style = nullptr, Equipment* worn = nullptr,
                   Bank* bank = nullptr, int* spell = nullptr, QuestLog* quests = nullptr, std::set<std::string>* visited = nullptr, float* skullSeconds = nullptr,
                   int* pkKills = nullptr, int* pkDeaths = nullptr);

}  // namespace journey
