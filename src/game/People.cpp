#include "game/People.h"

#include "game/Gear.h"

#include <cmath>

namespace journey {

namespace {

constexpr float kPi = 3.14159265f;

// look: female, skin, hairPick, hairColor, shirt, shirtColor, pants, pantsColor, shoes, shoesColor
std::vector<PersonInfo> BuildTable() {
    std::vector<PersonInfo> v;
    auto gear = [](Tier t, GearType g) { return GearItem(t, g); };
    // ---- Journey (the castle town)
    v.push_back({"aubrey", "Aubrey", "Shopkeeper", "He runs the general store.", "general_town", 2,
                 {false, 2, 0, 1, 4, 3, 0, 1, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Welcome to the Journey General Store! Take a look.", "Food, tools, and a fair price for your odds and ends.", "Keep an eye on the road south - goblins."}});
    v.push_back({"torvald", "Torvald", "Smith", "A burly smith with soot on his beard.", "smith_town", 2,
                 {false, 3, 3, 4, 3, 13, 0, 13, 1, 1}, PersonWork::Smith, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Need a pickaxe or some bars? Have a look.", "Bring me your ore and I'll give you a fair price.", "Hot work, smithing. Good work, too."}});
    v.push_back({"guard_gate", "Castle guard", "Guard", "A guard in the king's colours.", "", 1,
                 {false, 2, 4, 0, 0, 9, 0, 10, 1, 2}, PersonWork::Idle, gear(Tier::Iron, GearType::Sword), gear(Tier::Iron, GearType::Shield),
                 gear(Tier::Iron, GearType::Helm), gear(Tier::Iron, GearType::Body), gear(Tier::Iron, GearType::Legs),
                 3, {"Halt. Oh, it's you. Carry on.", "There's a village a long walk south down the road. Good shops there.", "Goblins have made camp on the way. They're weak, but they bite."}});
    // ---- Millbrook (the village down the road)
    v.push_back({"marta", "Marta", "Shopkeeper", "She runs Millbrook's general store.", "village_general", 2,
                 {true, 1, 1, 5, 2, 6, 0, 4, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Welcome to Millbrook General! What can I get you?", "Bread's fresh this morning.", "I'll take your spare odds and ends, too."}});
    v.push_back({"brannock", "Brannock", "Weaponsmith", "He sells blades and bashers.", "village_weapons", 2,
                 {false, 4, 2, 3, 3, 12, 0, 3, 1, 1}, PersonWork::Idle, gear(Tier::Steel, GearType::Sword), Item::None, Item::None, Item::None, Item::None,
                 3, {"Finest blades in the valley. Daggers for speed, axes for power.", "A good weapon is half the fight.", "Steel needs Attack 20, mind."}});
    v.push_back({"mira", "Mira", "Armourer", "She sells plate armour.", "village_armour", 2,
                 {true, 3, 4, 8, 3, 12, 0, 13, 1, 2}, PersonWork::Idle, Item::None, Item::None, gear(Tier::Iron, GearType::Helm),
                 gear(Tier::Iron, GearType::Body), gear(Tier::Iron, GearType::Legs),
                 3, {"Armour keeps you alive. Have a look.", "A full set of iron will stop most of what a goblin throws at you.", "I buy armour, too."}});
    v.push_back({"dunmore", "Dunmore", "Ore trader", "He deals in ore and metal bars.", "village_ore", 2,
                 {false, 2, 5, 5, 3, 7, 0, 5, 1, 0}, PersonWork::Idle, Item::BronzePickaxe, Item::None, Item::None, Item::None, Item::None,
                 3, {"Ore, bars - I buy and sell. Save yourself a trip to the quarry.", "Coal's dear this week.", "Mithril? Not often. Mine it yourself!"}});
    v.push_back({"hilda", "Hilda", "Cook", "She smells wonderfully of baking.", "village_cook", 2,
                 {true, 2, 6, 6, 2, 0, 0, 1, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Hot food, ready to eat. Heals you right up.", "I'll buy raw fish and meat, if you'd rather not cook it yourself.", "Eggs, anyone? I always need eggs."}});
    v.push_back({"wilf", "Wilf", "Smith", "The village smith.", "", 1,
                 {false, 4, 1, 2, 3, 6, 0, 12, 1, 1}, PersonWork::Smith, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Furnace and anvils are free for anyone with a hammer.", "Copper and tin make bronze. Iron takes coal for steel.", "Mind the sparks!"}});
    v.push_back({"old_tom", "Old Tom", "Farmer", "A weathered old farmer.", "", 1,
                 {false, 1, 8, 8, 3, 2, 0, 3, 1, 1}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Fine weather for it.", "Chickens and cows are good practice for a young fighter.", "The goblins up the road never used to bother us."}});
    v.push_back({"gwen", "Gwen", "Villager", "A cheerful villager.", "", 1,
                 {true, 1, 2, 10, 1, 9, 1, 11, 2, 3}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Hello! Have you been to the mine yet?", "The fishing's good by the river.", "Dunmore pays well for coal."}});
    v.push_back({"guard_village", "Village guard", "Guard", "He keeps watch over Millbrook.", "", 1,
                 {false, 3, 3, 1, 0, 12, 0, 13, 1, 2}, PersonWork::Idle, gear(Tier::Bronze, GearType::Sword), gear(Tier::Bronze, GearType::Shield),
                 gear(Tier::Bronze, GearType::Helm), gear(Tier::Bronze, GearType::Body), Item::None,
                 3, {"Welcome to Millbrook. Keep the peace.", "Goblins to the north, a quiet quarry to the east.", "The shops are round the square."}});
    v.push_back({"fisher", "Finn", "Fisherman", "He smells of fish.", "", 1,
                 {false, 3, 1, 3, 3, 9, 0, 3, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Anchovies from level 15. Patience!", "Cook them over a fire before you eat them.", "Nothing like a quiet morning on the river."}});
    (void)kPi;
    return v;
}

}  // namespace

const std::vector<PersonInfo>& AllPersonInfos() {
    static const std::vector<PersonInfo> table = BuildTable();
    return table;
}

const PersonInfo* FindPersonInfo(const std::string& key) {
    for (const PersonInfo& p : AllPersonInfos())
        if (key == p.key) return &p;
    return nullptr;
}

void People::Init(const TileMap& map) {
    people_.clear();
    int id = 1;
    for (const PersonDef& d : map.Persons()) {
        const PersonInfo* info = FindPersonInfo(d.key);
        if (!info) continue;
        Person p;
        p.id = id++;
        p.info = info;
        p.x = d.x;
        p.z = d.z;
        p.facing = d.yawDegrees * kPi / 180.0f;
        people_.push_back(p);
    }
}

const Person* People::Find(int id) const {
    for (const Person& p : people_)
        if (p.id == id) return &p;
    return nullptr;
}

Person* People::Find(int id) {
    for (Person& p : people_)
        if (p.id == id) return &p;
    return nullptr;
}

int People::AtTile(TileCoord t) const {
    for (const Person& p : people_)
        if (p.Tile() == t) return p.id;
    return -1;
}

}  // namespace journey
