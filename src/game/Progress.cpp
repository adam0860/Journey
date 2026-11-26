#include "game/Progress.h"

#include "game/Magic.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace journey {

// Format, one value per line:
//   version=2                    the format number (a file without one is version 1)
//   skull=312.5                  seconds of skull left (the Abyss; only written while skulled)
//   pk=3:1                       players killed : times killed by a player (only written once there is a record)
//   visited=Market Square        a named place the hero has been to (one line each; teleport scrolls need it)
//   xp.woodcutting=1234          total experience per skill (the level is worked out from it)
//   hp=7                         hit points right now
//   style=1                      combat style (0 accurate, 1 aggressive, 2 defensive, 3 controlled)
//   slot.0=1:1                   bag slot -> item id : count
//   worn.3=40                    worn gear: equipment slot (0 head, 1 body, 2 legs, 3 weapon, 4 shield, 5 quiver) -> item id
//   ammo=120                     how many arrows are in the quiver
//   spell=fire_strike            the spell a staff casts
//   bank.0=1:200                 bank kind number -> item id : count
//   quest.goblin_trouble=1:3:0   quest key -> step : progress : done
int SaveVersion(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("version=", 0) == 0) return std::max(1, std::atoi(line.c_str() + 8));
    return 1;
}

std::string SerializeProgress(const Skills& skills, const Inventory& bag, CombatStyle style, const Equipment* worn, const Bank* bank,
                              int spell, const QuestLog* quests, const std::set<std::string>* visited, float skullSeconds, int pkKills, int pkDeaths) {
    std::ostringstream o;
    o << "version=" << kSaveVersion << "\n";
    o << "hp=" << skills.Hp() << "\n";
    o << "style=" << static_cast<int>(style) << "\n";
    if (spell > 0) o << "spell=" << GetSpell(spell).key << "\n";
    if (skullSeconds > 0.0f) o << "skull=" << skullSeconds << "\n";
    if (pkKills > 0 || pkDeaths > 0) o << "pk=" << pkKills << ":" << pkDeaths << "\n";
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
    if (worn && worn->AmmoCount() > 0) o << "ammo=" << worn->AmmoCount() << "\n";
    if (bank)
        for (size_t i = 0; i < bank->Slots().size(); ++i)
            o << "bank." << i << "=" << static_cast<int>(bank->Slots()[i].item) << ":" << bank->Slots()[i].count << "\n";
    if (visited)
        for (const std::string& place : *visited) o << "visited=" << place << "\n";
    if (quests) o << quests->Serialize();
    return o.str();
}

void ParseProgress(const std::string& text, Skills* skills, Inventory* bag, CombatStyle* style, Equipment* worn, Bank* bank,
                   int* spell, QuestLog* quests, std::set<std::string>* visited, float* skullSeconds, int* pkKills, int* pkDeaths) {
    std::istringstream in(text);
    std::string line;
    int ammo = 0;
    int hp = -1;   // applied last: the maximum depends on the Hitpoints XP, which may come later in the file
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "version") {
            continue;   // (nothing to convert yet: version 1 and 2 differ only by this line and the visited places)
        } else if (k == "pk") {
            const size_t colon = v.find(':');
            if (colon != std::string::npos) {
                if (pkKills) *pkKills = std::max(0, std::atoi(v.c_str()));
                if (pkDeaths) *pkDeaths = std::max(0, std::atoi(v.c_str() + colon + 1));
            }
        } else if (k == "skull") {
            if (skullSeconds) *skullSeconds = std::max(0.0f, std::min(static_cast<float>(std::atof(v.c_str())), 600.0f));
        } else if (k == "visited") {
            if (visited && !v.empty()) visited->insert(v);
        } else if (k == "hp") {
            hp = std::atoi(v.c_str());
        } else if (k == "style") {
            if (style) *style = StyleFromIndex(std::atoi(v.c_str()));
        } else if (k.rfind("xp.", 0) == 0) {
            const std::string key = k.substr(3);
            for (int i = 0; i < kSkillCount; ++i) {
                const Skill s = static_cast<Skill>(i);
                if (key == SkillKey(s)) skills->SetXp(s, std::strtoll(v.c_str(), nullptr, 10));
            }
        } else if (k == "ammo") {
            ammo = std::atoi(v.c_str());
        } else if (k == "spell") {
            const int s = FindSpell(v);
            if (spell && s >= 0) *spell = s;
        } else if (k.rfind("quest.", 0) == 0) {
            if (quests) quests->ParseLine(k.substr(6), v);
        } else if (k.rfind("bank.", 0) == 0) {
            const size_t colon = v.find(':');
            if (!bank || colon == std::string::npos) continue;
            const int item = std::atoi(v.c_str());
            const int count = std::atoi(v.c_str() + colon + 1);
            if (item > 0 && item < static_cast<int>(Item::Count) && count > 0) bank->SetSlot(static_cast<Item>(item), count);
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
    if (worn && worn->Get(EquipSlot::Ammo) != Item::None) worn->SetAmmo(worn->Get(EquipSlot::Ammo), ammo);   // (zero arrows = nothing worn)
    if (hp >= 0) skills->SetHp(hp);
}

}  // namespace journey
