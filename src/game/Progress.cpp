#include "game/Progress.h"

#include <cstdlib>
#include <sstream>

namespace journey {

// Format, one value per line:
//   xp.woodcutting=1234          total experience per skill (the level is worked out from it)
//   hp=7                         hit points right now
//   style=1                      combat style (0 accurate, 1 aggressive, 2 defensive, 3 controlled)
//   slot.0=1:1                   bag slot -> item id : count
//   worn.3=40                    worn gear: equipment slot (0 head, 1 body, 2 legs, 3 weapon, 4 shield) -> item id
std::string SerializeProgress(const Skills& skills, const Inventory& bag, CombatStyle style, const Equipment* worn) {
    std::ostringstream o;
    o << "hp=" << skills.Hp() << "\n";
    o << "style=" << static_cast<int>(style) << "\n";
    for (int i = 0; i < kSkillCount; ++i) {
        const Skill s = static_cast<Skill>(i);
        if (skills.Xp(s) > 0) o << "xp." << SkillKey(s) << "=" << skills.Xp(s) << "\n";
    }
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemStack& st = bag.Slot(i);
        if (st.item != Item::None && st.count > 0) o << "slot." << i << "=" << static_cast<int>(st.item) << ":" << st.count << "\n";
    }
    if (worn)
        for (int i = 0; i < kEquipSlotCount; ++i) {
            const Item it = worn->Get(static_cast<EquipSlot>(i));
            if (it != Item::None) o << "worn." << i << "=" << static_cast<int>(it) << "\n";
        }
    return o.str();
}

void ParseProgress(const std::string& text, Skills* skills, Inventory* bag, CombatStyle* style, Equipment* worn) {
    std::istringstream in(text);
    std::string line;
    int hp = -1;   // applied last: the maximum depends on the Hitpoints XP, which may come later in the file
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "hp") {
            hp = std::atoi(v.c_str());
        } else if (k == "style") {
            if (style) *style = StyleFromIndex(std::atoi(v.c_str()));
        } else if (k.rfind("xp.", 0) == 0) {
            const std::string key = k.substr(3);
            for (int i = 0; i < kSkillCount; ++i) {
                const Skill s = static_cast<Skill>(i);
                if (key == SkillKey(s)) skills->SetXp(s, std::strtoll(v.c_str(), nullptr, 10));
            }
        } else if (k.rfind("worn.", 0) == 0) {
            const int slot = std::atoi(k.c_str() + 5);
            const int item = std::atoi(v.c_str());
            EquipSlot es;
            // only accept an item that really belongs in that slot (a hand-edited file cannot wear a pickaxe on the head)
            if (worn && slot >= 0 && slot < kEquipSlotCount && item > 0 && item < static_cast<int>(Item::Count) &&
                Equippable(static_cast<Item>(item), &es) && static_cast<int>(es) == slot)
                worn->Set(es, static_cast<Item>(item));
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
    if (hp >= 0) skills->SetHp(hp);
}

}  // namespace journey
