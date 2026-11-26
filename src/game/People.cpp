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
    // ---- bankers: every bank has two behind its counter (the bank is shared by all of them)
    auto banker = [&](const char* key, const char* name, bool female, int skin, int hair, int hairColor, int shirtColor) {
        v.push_back({key, name, "Banker", "A banker in a neat uniform.", "bank", 2,
                     {female, skin, hair, hairColor, 3, shirtColor, 0, 13, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None,
                     Item::None, 3,
                     {"Good day. Would you like to look at your bank?", "Everything you leave with us is safe, and the same in every town.", "Coins, ore, logs - we keep it all. Take it out any time."}});
    };
    banker("oswin", "Oswin", false, 2, 5, 3, 9);
    banker("ysolde", "Ysolde", true, 1, 3, 6, 9);
    banker("cedric", "Cedric", false, 3, 2, 1, 9);
    banker("wren", "Wren", true, 2, 4, 2, 9);
    // ---- the archery and magic shops of Millbrook
    v.push_back({"larson", "Larson", "Bowyer", "He sells bows and arrows.", "village_archery", 2,
                 {false, 2, 6, 5, 3, 4, 0, 12, 1, 1}, PersonWork::Idle, Item::Shortbow, Item::None, Item::None, Item::None, Item::None,
                 3, {"Bows and arrows! Shoot over a fence and nothing can bite you.", "Shortbow from Ranged level 1, longbow from 5.", "Arrows drop where they land. Pick them up!"}});
    v.push_back({"aldric", "Aldric", "Wizard", "An old wizard with a long white beard.", "village_magic", 2,
                 {false, 1, 7, 7, 1, 8, 0, 8, 1, 2}, PersonWork::Idle, Item::AirStaff, Item::None, Item::WizardHat, Item::WizardRobe, Item::None,
                 3, {"Runes, staffs and robes. Magic is just the right words and the right stones.", "A staff of fire gives you endless fire runes. Hold one and attack.", "Fire Strike needs Magic level 7. Wind Strike is for beginners."}});
    // ---- people with quests (and places to find them)
    v.push_back({"king", "King Aldwin", "King", "The king of the valley, in a crown and a tired expression.", "masters", 1,
                 {false, 2, 4, 6, 4, 9, 0, 9, 1, 2}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Welcome to my hall, traveller.", "The realm needs heroes. It always does. A hero who masters a skill may buy its cape from me.", "There is a dragon in the desert ruins. Nobody comes back."}});
    v.push_back({"priest", "Father Eamon", "Priest", "A gentle priest in a plain robe.", "", 1,
                 {false, 1, 7, 7, 1, 8, 0, 8, 1, 4}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Peace be with you.", "Bury the bones of the creatures you fight, and the prayers will come.", "The crypt beneath the castle is no place for the living."}});
    v.push_back({"bram", "Bram", "Miner", "A grizzled miner with a lantern on his helmet.", "", 1,
                 {false, 3, 2, 3, 3, 12, 0, 5, 1, 1}, PersonWork::Idle, Item::BronzePickaxe, Item::None, Item::None, Item::None, Item::None,
                 3, {"Mind your step down here.", "There is mithril in the deeper tunnels, if you can reach it.", "The spiders came out of nowhere. Nasty things."}});
    // ---- the ferrymen (one at the castle's pier, one at Lakeside's dock), the crafter and the fishing-supplies seller
    v.push_back({"barnaby", "Barnaby", "Ferryman", "A ferryman with strong arms and a straw hat.", "", 1,
                 {false, 3, 1, 3, 3, 9, 0, 5, 1, 1}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Sailing to Lakeside? Twenty coins and I'll have you there before you can say 'seasick'.", "The river widens into a lovely lake down south.",
                     "I have never lost a passenger. Not one that mattered."}});
    v.push_back({"silas", "Silas", "Ferryman", "An old ferryman who knows every ripple of the lake.", "", 1,
                 {false, 1, 8, 8, 3, 2, 0, 3, 1, 1}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Back to the castle town? Twenty coins.", "Beware the wolf wood to the east. They hunt in packs.", "The best fish in the valley live in this lake."}});
    v.push_back({"ellis", "Ellis", "Crafter", "A crafter with a leather apron and ink-stained fingers.", "village_crafting", 2,
                 {true, 2, 4, 4, 2, 11, 0, 7, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Needles, thread, chisels and moulds - everything a crafter needs.", "Tan your hides on the rack, spin clay on the wheel, cast jewellery at the bench.",
                     "I buy leather, pots and jewellery, if you have any to sell."}});
    v.push_back({"gerrant", "Gerrant", "Angler", "He sells fishing gear and smells faintly of the lake.", "lakeside_fishing", 2,
                 {false, 3, 2, 2, 3, 9, 0, 4, 1, 0}, PersonWork::Idle, Item::SmallNet, Item::None, Item::None, Item::None, Item::None,
                 3, {"Nets, rods, harpoons and pots! Tuna and swordfish are big fish for big anglers.", "Feathers for the fly rod, a rod for the feathers.",
                     "I buy fish, raw or cooked, and tools, too."}});
    // ---- round 12: the wider world (Elmswick, Saltmere, Sunfield Farm and the wild places between)
    v.push_back({"perrin", "Perrin", "Shopkeeper", "He runs Elmswick's general store and smells of sawdust.", "elm_general", 2,
                 {false, 2, 2, 2, 3, 10, 0, 3, 1, 1}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Welcome to Elmswick! Logs, tools and a loaf of bread.", "Every house here is built from elm. The good stuff grows north of the village.", "Wolves have been coming down from the pines. Be careful out there."}});
    v.push_back({"hobb", "Hobb", "Woodsman", "A woodsman with arms like tree trunks.", "elm_woodcraft", 2,
                 {false, 3, 1, 3, 3, 12, 0, 12, 1, 1}, PersonWork::Idle, gear(Tier::Iron, GearType::Hatchet), Item::None, Item::None, Item::None, Item::None,
                 3, {"Axes, logs and bowstaves. Everything a woodcutter needs.", "Oak from level 15, willow from 30, yew from 60. Keep the axe sharp.", "A good axe is a hero's best friend."}});
    v.push_back({"brine", "Brine", "Fishmonger", "A fishmonger with a salt-stained apron and a loud voice.", "salt_market", 2,
                 {true, 2, 6, 3, 2, 9, 0, 4, 1, 0}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Fresh from the sea this morning! Sardines, cod, mackerel, whatever the nets caught.", "I buy fish, raw or cooked, and the gear to catch more of it.", "The big ones live far out on the pier. Bring a harpoon."}});
    v.push_back({"captain", "Captain Redd", "Harbour master", "The harbour master, a one-eyed veteran of the sea.", "", 1,
                 {false, 3, 4, 3, 4, 1, 0, 5, 1, 2}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Welcome to Saltmere. Mind the gulls, they steal.", "The sea runs east for ever. Nobody has ever sailed to the other side.", "Redbeard's old logbook? He fished these very waters."}});
    v.push_back({"ada", "Ada", "Farmer", "A farmer with straw in her hair and a bucket of milk.", "sunfield_farm", 2,
                 {true, 1, 8, 4, 2, 7, 1, 4, 2, 1}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"Eggs, milk, cheese and flour, straight from the Sunfields.", "The cows are gentle. The moles, less so.", "Wheat goes golden in the autumn and the whole valley smells of bread."}});
    v.push_back({"watchman", "Watchman Corwin", "Watchman", "A lone watchman who keeps the old tower lit.", "", 1,
                 {false, 2, 3, 2, 0, 9, 0, 10, 1, 2}, PersonWork::Idle, gear(Tier::Iron, GearType::Sword), gear(Tier::Iron, GearType::Shield),
                 gear(Tier::Iron, GearType::Helm), gear(Tier::Iron, GearType::Body), Item::None,
                 3, {"Northwatch has stood here since before the castle.", "Wolves in the moor, giants in the hills. The Frostpeaks are no place for a beginner.", "A long road runs back south to the castle. Take it if you can."}});
    v.push_back({"maggie", "Old Maggie", "Witch", "A hunched old woman who stirs a bubbling pot.", "swamp_witch", 2,
                 {true, 1, 7, 7, 1, 8, 1, 8, 1, 2}, PersonWork::Idle, Item::None, Item::None, Item::WizardHat, Item::WizardRobe, Item::None,
                 3, {"Hee hee. A customer, in my mire!", "Runes and candles, dearie. The dead walk the downs to the south, and they do not like the light.", "Mind the water. It is not water."}});
    v.push_back({"druid", "Brother Elwyn", "Druid", "A druid in grey, sitting among the standing stones.", "", 1,
                 {false, 1, 8, 8, 1, 2, 0, 5, 1, 4}, PersonWork::Idle, Item::None, Item::None, Item::None, Item::None, Item::None,
                 3, {"The stones were old when the giants were young.", "Eight hills I have crossed to sit here and listen.", "Giants roam to the east. Ogres and trolls to the south. Walk softly."}});
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
