// Saving your skills and bag (journey_progress.cfg). Your look lives in journey_save.cfg (Appearance).
#pragma once
#include <string>

#include "game/Inventory.h"
#include "game/Skills.h"

namespace journey {

std::string SerializeProgress(const Skills& skills, const Inventory& bag);
// Reads a save. Unknown or damaged lines are ignored, so an old or hand-edited file never crashes the game.
void ParseProgress(const std::string& text, Skills* skills, Inventory* bag);

}  // namespace journey
