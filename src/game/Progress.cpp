#include "game/Progress.h"

#include <cstdlib>
#include <sstream>

namespace journey {

// Format, one value per line:
//   xp.woodcutting=1234          total experience per skill (the level is worked out from it)
//   slot.0=1:1                   bag slot -> item id : count
std::string SerializeProgress(const Skills& skills, const Inventory& bag) {
    std::ostringstream o;
    for (int i = 0; i < kSkillCount; ++i) {
        const Skill s = static_cast<Skill>(i);
        if (skills.Xp(s) > 0) o << "xp." << SkillKey(s) << "=" << skills.Xp(s) << "\n";
    }
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemStack& st = bag.Slot(i);
        if (st.item != Item::None && st.count > 0) o << "slot." << i << "=" << static_cast<int>(st.item) << ":" << st.count << "\n";
    }
    return o.str();
}

void ParseProgress(const std::string& text, Skills* skills, Inventory* bag) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k.rfind("xp.", 0) == 0) {
            const std::string key = k.substr(3);
            for (int i = 0; i < kSkillCount; ++i) {
                const Skill s = static_cast<Skill>(i);
                if (key == SkillKey(s)) skills->SetXp(s, std::strtoll(v.c_str(), nullptr, 10));
            }
        } else if (k.rfind("slot.", 0) == 0) {
            const int slot = std::atoi(k.c_str() + 5);
            const size_t colon = v.find(':');
            if (colon == std::string::npos || slot < 0 || slot >= Inventory::kSlots) continue;
            const int item = std::atoi(v.c_str());
            const int count = std::atoi(v.c_str() + colon + 1);
            if (item <= 0 || item >= static_cast<int>(Item::Count) || count <= 0) continue;
            bag->SetSlot(slot, {static_cast<Item>(item), count});
        }
    }
}

}  // namespace journey
