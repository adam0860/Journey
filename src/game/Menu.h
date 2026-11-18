// Context menus: what tapping (or long-pressing, or right-clicking) something offers. Pure data, no screen code.
//
// Gameplay::BuildMenu() turns "what is under the finger" into a list of entries; the first entry is the default
// (what a plain tap does), the last is always Cancel. The HUD draws the coloured text runs.
#pragma once
#include <string>
#include <vector>

#include "game/Combat.h"
#include "game/Gear.h"
#include "game/Inventory.h"

namespace journey {

// Something under the finger: a thing in the world (id = its index/id in its list) or nothing but ground.
enum class TargetKind { Ground, Tree, Spot, Fire, Npc, Pile, Rock, Station, Person };
struct Target {
    TargetKind kind = TargetKind::Ground;
    int id = -1;
};

enum class MenuAct {
    WalkHere,
    Chop,        // target = tree
    Fish,        // target = spot
    Cook,        // target = fire (cooks the raw food in your bag)
    UseOnFire,   // slot = the item being used
    Mine,        // target = rock
    Trade,       // target = person who runs a shop
    TalkTo,      // target = person
    UseStation,  // target = station (a furnace smelts, an anvil smiths: opens the make menu)
    UseOnStation,// slot = the item being used, target = station
    RemoveWorn,  // index = the EquipSlot to take off
    ExamineWorn, // index = the EquipSlot
    Attack,      // target = npc
    TakeAll,     // target = pile
    TakeItem,    // target = pile, index = item index in the pile
    ExamineTarget,
    ExamineItem, // slot or pile item
    ItemAct,     // a bag item action: slot + itemAction
    UseOnItem,   // use `useSlot` with `slot`
    Cancel,
};

// Colours of the words in a menu line (the classic client's scheme).
struct TextRun {
    std::string text;
    Rgb color = {255, 255, 255};
};
constexpr Rgb kMenuWhite = {255, 255, 255};
constexpr Rgb kMenuNpc = {255, 255, 0};
constexpr Rgb kMenuObject = {0, 255, 255};
constexpr Rgb kMenuItem = {255, 152, 31};

// The "what would you like to make?" panel of a furnace or an anvil. Pure data: Gameplay builds it, the HUD draws it.
struct MakeOption {
    Item product = Item::None;
    std::string name;      // "Bronze dagger"
    std::string detail;    // "1 copper ore + 1 tin ore" / "3 bars"
    int level = 1;         // Smithing level needed
    bool levelOk = true;
    bool haveAll = true;   // you carry everything for at least one
    int canMake = 0;       // how many you could make from what you carry
    int index = 0;         // Tier (smelting) or GearType (smithing)
};
struct MakeMenu {
    enum class Kind { Smelt, Smith };
    Kind kind = Kind::Smelt;
    std::string title;     // "Furnace" / "Anvil"
    int tier = 0;          // smithing: the metal being shown
    std::vector<MakeOption> options;
};

// What a shop window shows. Pure data: Gameplay builds it, the HUD draws it.
struct ShopLine {
    Item item = Item::None;
    int price = 0;         // coins for one (buy: what it costs; sell: what the shop pays)
    int stock = 0;         // buy: how many the shop has;  sell: how many you carry
    bool ok = true;        // buy: you can afford one;  sell: (always true)
    int index = 0;         // buy: the shelf line
};
struct ShopMenu {
    int shop = -1;
    std::string name;      // "Millbrook General Store"
    std::string keeper;    // "Marta"
    int coins = 0;         // what you carry
    std::vector<ShopLine> buy;
    std::vector<ShopLine> sell;
};

struct MenuEntry {
    std::vector<TextRun> label;
    MenuAct act = MenuAct::Cancel;
    Target target;
    int slot = -1;                 // bag slot (item actions, use-on)
    int useSlot = -1;              // the item being used (UseOnItem)
    int index = 0;                 // item index in a pile (TakeItem / ExamineItem)
    ItemAction itemAction = ItemAction::Examine;
    float gx = 0, gz = 0;          // ground point (WalkHere)

    std::string Text() const {
        std::string s;
        for (const TextRun& r : label) s += r.text;
        return s;
    }
};

}  // namespace journey
