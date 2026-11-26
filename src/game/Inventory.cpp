#include "game/Inventory.h"

#include <cctype>
#include <deque>
#include <string>

#include "game/Archery.h"
#include "game/Gear.h"
#include "game/Magic.h"
#include "game/Skilling.h"
#include "game/Smithing.h"

namespace journey {

namespace {
// name, stackable, raw, food, heals, buryXp, examine, ground model
const ItemInfo kBase[] = {
    {"nothing",           false, false, false, 0, 0, "Nothing to see here.", "bundle"},
    {"Logs",              false, false, false, 0, 0, "A number of wooden logs.", "logs"},
    {"Bronze axe",        false, false, false, 0, 0, "A woodcutter's axe.", "bundle"},
    {"Small fishing net", false, false, false, 0, 0, "Useful for catching small fish.", "bundle"},
    {"Tinderbox",         false, false, false, 0, 0, "Useful for lighting a fire.", "bundle"},
    {"Raw shrimps",       false, true,  false, 0, 0, "I should cook this first.", "shrimp_raw"},
    {"Shrimps",           false, false, true,  3, 0, "Some nicely cooked shrimps.", "shrimp_cooked"},
    {"Raw anchovies",     false, true,  false, 0, 0, "I should cook this first.", "fish_raw"},
    {"Anchovies",         false, false, true,  1, 0, "Some nicely cooked anchovies.", "fish_cooked"},
    {"Burnt fish",        false, false, false, 0, 0, "Oops!", "fish_burnt"},
    {"Bones",             false, false, false, 0, 5, "Ew, a bone.", "bones"},
    {"Raw chicken",       false, true,  false, 0, 0, "I should cook this first.", "drumstick_raw"},
    {"Chicken",           false, false, true,  3, 0, "Mmm, this looks tasty.", "drumstick_cooked"},
    {"Raw beef",          false, true,  false, 0, 0, "I should cook this first.", "steak_raw"},
    {"Beef",              false, false, true,  3, 0, "Mmm, this looks tasty.", "steak_cooked"},
    {"Burnt meat",        false, false, false, 0, 0, "Oops!", "steak_burnt"},
    {"Egg",               false, false, true,  1, 0, "A nice fresh egg.", "egg"},
    {"Cowhide",           false, false, false, 0, 0, "Hair-covered hide from a cow.", "hide"},
    {"Copper ore",        false, false, false, 0, 0, "This could be smelted into a bronze bar.", "ore_copper"},
    {"Tin ore",           false, false, false, 0, 0, "This could be smelted into a bronze bar.", "ore_tin"},
    {"Iron ore",          false, false, false, 0, 0, "This needs to be smelted in a furnace.", "ore_iron"},
    {"Coal",              false, false, false, 0, 0, "Fuel for the furnace, and the secret of steel.", "ore_coal"},
    {"Mithril ore",       false, false, false, 0, 0, "A rare blue-grey ore. It needs a lot of coal.", "ore_mithril"},
    {"Bronze bar",        false, false, false, 0, 0, "A bar of bronze, ready for the anvil.", "bar_bronze"},
    {"Iron bar",          false, false, false, 0, 0, "A bar of iron, ready for the anvil.", "bar_iron"},
    {"Steel bar",         false, false, false, 0, 0, "A bar of steel, ready for the anvil.", "bar_steel"},
    {"Mithril bar",       false, false, false, 0, 0, "A bar of mithril, ready for the anvil.", "bar_mithril"},
    {"Hammer",            false, false, false, 0, 0, "Good for hitting things. Smiths need one.", "hammer"},
};
constexpr int kBaseCount = static_cast<int>(sizeof(kBase) / sizeof(kBase[0]));
static_assert(kBaseCount == static_cast<int>(Item::BronzePickaxe), "kBase must list every item before the pickaxes");

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The items added after the first release: see Extras().
namespace {
// The items added after the first release: name, stackable, raw, food, heals, buryXp, examine, ground model, value.
struct ExtraItem {
    Item item;
    ItemInfo info;
    int value;
};
const std::vector<ExtraItem>& Extras() {
    static const std::vector<ExtraItem> v = {
        {Item::OakLogs, {"Oak logs", false, false, false, 0, 0, "Logs cut from an oak tree.", "logs_oak"}, 12},
        {Item::WillowLogs, {"Willow logs", false, false, false, 0, 0, "Logs cut from a willow tree.", "logs_willow"}, 20},
        {Item::YewLogs, {"Yew logs", false, false, false, 0, 0, "Logs cut from a yew tree. Hard and dense.", "logs_yew"}, 55},
        {Item::IronAxe, {"Iron axe", false, false, false, 0, 0, "A woodcutter's axe: an iron one.", "iron_hatchet"}, 69},
        {Item::SteelAxe, {"Steel axe", false, false, false, 0, 0, "A woodcutter's axe: a steel one.", "steel_hatchet"}, 150},
        {Item::MithrilAxe, {"Mithril axe", false, false, false, 0, 0, "A woodcutter's axe: a mithril one.", "mithril_hatchet"}, 380},
        {Item::FlyFishingRod, {"Fly fishing rod", false, false, false, 0, 0, "A rod for catching trout and salmon. It needs feathers.", "fishing_rod"}, 18},
        {Item::Harpoon, {"Harpoon", false, false, false, 0, 0, "A barbed spear for the big fish.", "harpoon"}, 45},
        {Item::LobsterPot, {"Lobster pot", false, false, false, 0, 0, "A wicker cage for catching lobsters.", "lobster_pot"}, 25},
        {Item::Feathers, {"Feathers", true, false, false, 0, 0, "Bait for a fly fishing rod.", "feathers"}, 1},
        {Item::RawTrout, {"Raw trout", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 10},
        {Item::Trout, {"Trout", false, false, true, 7, 0, "A nicely cooked trout.", "fish_cooked"}, 20},
        {Item::RawSalmon, {"Raw salmon", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 22},
        {Item::Salmon, {"Salmon", false, false, true, 9, 0, "A nicely cooked salmon.", "fish_cooked"}, 40},
        {Item::RawTuna, {"Raw tuna", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 55},
        {Item::Tuna, {"Tuna", false, false, true, 10, 0, "A nicely cooked tuna.", "fish_cooked"}, 100},
        {Item::RawLobster, {"Raw lobster", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 90},
        {Item::Lobster, {"Lobster", false, false, true, 12, 0, "A beautifully cooked lobster.", "fish_cooked"}, 170},
        {Item::RawSwordfish, {"Raw swordfish", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 130},
        {Item::Swordfish, {"Swordfish", false, false, true, 14, 0, "A beautifully cooked swordfish.", "fish_cooked"}, 260},
        {Item::Clay, {"Clay", false, false, false, 0, 0, "Soft, workable clay.", "ore_clay"}, 5},
        {Item::SilverOre, {"Silver ore", false, false, false, 0, 0, "This could be smelted into a silver bar.", "ore_silver"}, 60},
        {Item::GoldOre, {"Gold ore", false, false, false, 0, 0, "This could be smelted into a gold bar.", "ore_gold"}, 120},
        {Item::UncutSapphire, {"Uncut sapphire", false, false, false, 0, 0, "A rough blue gem. A chisel would help.", "gem_blue"}, 150},
        {Item::UncutEmerald, {"Uncut emerald", false, false, false, 0, 0, "A rough green gem. A chisel would help.", "gem_green"}, 300},
        {Item::UncutRuby, {"Uncut ruby", false, false, false, 0, 0, "A rough red gem. A chisel would help.", "gem_red"}, 520},
        {Item::SilverBar, {"Silver bar", false, false, false, 0, 0, "A bar of silver.", "bar_silver"}, 90},
        {Item::GoldBar, {"Gold bar", false, false, false, 0, 0, "A bar of gold.", "bar_gold"}, 220},
        {Item::Chisel, {"Chisel", false, false, false, 0, 0, "For cutting gems.", "chisel"}, 8},
        {Item::Sapphire, {"Sapphire", false, false, false, 0, 0, "A cut sapphire.", "gem_blue"}, 320},
        {Item::Emerald, {"Emerald", false, false, false, 0, 0, "A cut emerald.", "gem_green"}, 600},
        {Item::Ruby, {"Ruby", false, false, false, 0, 0, "A cut ruby.", "gem_red"}, 1000},
        {Item::Needle, {"Needle", false, false, false, 0, 0, "For sewing leather.", "needle"}, 5},
        {Item::Thread, {"Thread", true, false, false, 0, 0, "Used with a needle.", "thread"}, 1},
        {Item::Leather, {"Leather", false, false, false, 0, 0, "Tanned cow hide, ready to be sewn.", "leather"}, 25},
        {Item::LeatherCap, {"Leather cap", false, false, false, 0, 0, "A light cap of boiled leather.", "leather_cap"}, 40},
        {Item::LeatherBody, {"Leather body", false, false, false, 0, 0, "A jerkin of boiled leather. An archer's favourite.", "leather_body"}, 120},
        {Item::LeatherChaps, {"Leather chaps", false, false, false, 0, 0, "Leather leg guards for an archer.", "leather_chaps"}, 80},
        {Item::RingMould, {"Ring mould", false, false, false, 0, 0, "Used with a furnace to make rings.", "mould"}, 10},
        {Item::AmuletMould, {"Amulet mould", false, false, false, 0, 0, "Used with a furnace to make amulets.", "mould"}, 10},
        {Item::GoldRing, {"Gold ring", false, false, false, 0, 0, "A plain gold ring.", "ring_gold"}, 290},
        {Item::SapphireRing, {"Sapphire ring", false, false, false, 0, 0, "A gold ring set with a sapphire.", "ring_gold"}, 700},
        {Item::EmeraldRing, {"Emerald ring", false, false, false, 0, 0, "A gold ring set with an emerald.", "ring_gold"}, 1070},
        {Item::RubyRing, {"Ruby ring", false, false, false, 0, 0, "A gold ring set with a ruby.", "ring_gold"}, 1590},
        {Item::GoldAmulet, {"Gold amulet", false, false, false, 0, 0, "A plain gold amulet.", "amulet_gold"}, 330},
        {Item::SapphireAmulet, {"Sapphire amulet", false, false, false, 0, 0, "A gold amulet set with a sapphire.", "amulet_gold"}, 800},
        {Item::EmeraldAmulet, {"Emerald amulet", false, false, false, 0, 0, "A gold amulet set with an emerald.", "amulet_gold"}, 1230},
        {Item::RubyAmulet, {"Ruby amulet", false, false, false, 0, 0, "A gold amulet set with a ruby.", "amulet_gold"}, 1830},
        {Item::Pot, {"Pot", false, false, false, 0, 0, "A fired clay pot.", "pot"}, 10},
        {Item::PieDish, {"Pie dish", false, false, false, 0, 0, "A fired clay pie dish.", "pot"}, 14},
        {Item::Bowl, {"Bowl", false, false, false, 0, 0, "A fired clay bowl.", "pot"}, 18},
        {Item::CastleScroll, {"Castle scroll", true, false, false, 0, 0, "A scroll of teleportation. Read it to be whisked to Castle Town.", "scroll"}, 90},
        {Item::MillbrookScroll, {"Millbrook scroll", true, false, false, 0, 0, "A scroll of teleportation. Read it to be whisked to Millbrook.", "scroll"}, 90},
        {Item::LakesideScroll, {"Lakeside scroll", true, false, false, 0, 0, "A scroll of teleportation. Read it to be whisked to Lakeside.", "scroll"}, 110},
        {Item::ElmswickScroll, {"Elmswick scroll", true, false, false, 0, 0, "A scroll of teleportation. Read it to be whisked to Elmswick.", "scroll"}, 140},
        {Item::SaltmereScroll, {"Saltmere scroll", true, false, false, 0, 0, "A scroll of teleportation. Read it to be whisked to Saltmere.", "scroll"}, 160},
        // ---- adamant
        {Item::AdamantOre, {"Adamant ore", false, false, false, 0, 0, "A rare green ore. It needs a great deal of coal.", "ore_adamant"}, 260},
        {Item::AdamantBar, {"Adamant bar", false, false, false, 0, 0, "A bar of adamant, ready for the anvil.", "bar_adamant"}, 640},
        {Item::AdamantAxe, {"Adamant axe", false, false, false, 0, 0, "A woodcutter's axe: an adamant one.", "adamant_hatchet"}, 900},
        // ---- more fish
        {Item::RawSardine, {"Raw sardine", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 3},
        {Item::Sardine, {"Sardine", false, false, true, 4, 0, "A nicely cooked sardine.", "fish_cooked"}, 8},
        {Item::RawHerring, {"Raw herring", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 6},
        {Item::Herring, {"Herring", false, false, true, 5, 0, "A nicely cooked herring.", "fish_cooked"}, 14},
        {Item::RawPike, {"Raw pike", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 16},
        {Item::Pike, {"Pike", false, false, true, 8, 0, "A nicely cooked pike.", "fish_cooked"}, 30},
        {Item::RawBass, {"Raw bass", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 70},
        {Item::Bass, {"Bass", false, false, true, 13, 0, "A beautifully cooked bass.", "fish_cooked"}, 130},
        {Item::RawShark, {"Raw shark", false, true, false, 0, 0, "I should cook this first. Carefully.", "fish_raw"}, 240},
        {Item::Shark, {"Shark", false, false, true, 20, 0, "A massive, beautifully cooked shark. It heals a lot.", "fish_cooked"}, 430},
        // ---- rarer gems and their jewellery
        {Item::UncutDiamond, {"Uncut diamond", false, false, false, 0, 0, "A rough, clear stone. A chisel would help.", "gem_white"}, 900},
        {Item::Diamond, {"Diamond", false, false, false, 0, 0, "A cut diamond. It sparkles.", "gem_white"}, 1800},
        {Item::DiamondRing, {"Diamond ring", false, false, false, 0, 0, "A gold ring set with a diamond.", "ring_gold"}, 2250},
        {Item::DiamondAmulet, {"Diamond amulet", false, false, false, 0, 0, "A gold amulet set with a diamond.", "amulet_gold"}, 2400},
        {Item::UncutDragonstone, {"Uncut dragonstone", false, false, false, 0, 0, "A rough purple stone that is warm to the touch.", "gem_purple"}, 2200},
        {Item::Dragonstone, {"Dragonstone", false, false, false, 0, 0, "A cut dragonstone. It glows faintly.", "gem_purple"}, 4500},
        {Item::DragonstoneAmulet, {"Dragonstone amulet", false, false, false, 0, 0, "A gold amulet set with a dragonstone. A hero's jewel.", "amulet_gold"}, 5000},
        // ---- unique jewellery
        {Item::MummysAmulet, {"Mummy's amulet", false, false, false, 0, 0, "A cursed amulet from the desert ruins. It makes you hit harder.", "amulet_gold"}, 1500},
        {Item::TrollToothAmulet, {"Troll-tooth amulet", false, false, false, 0, 0, "A huge tooth on a leather cord. Smells of troll.", "amulet_gold"}, 1200},
        {Item::AbyssalAmulet, {"Abyssal amulet", false, false, false, 0, 0, "A black amulet that was cold the moment you touched it. Only the Abyss makes them. The best in the game.", "amulet_gold"}, 15000},
        {Item::AbyssalRing, {"Abyssal ring", false, false, false, 0, 0, "A black ring that hums in the dark. It was worn in the Abyss.", "ring_gold"}, 2500},
        // ---- round 10: tools and woodwork
        {Item::Knife, {"Knife", false, false, false, 0, 0, "A sharp little knife. It carves wood.", "knife"}, 6},
        {Item::Bowstring, {"Bowstring", true, false, false, 0, 0, "A tough string. Put it on an unstrung bow.", "bowstring"}, 3},
        {Item::UnstrungShortbow, {"Unstrung shortbow", false, false, false, 0, 0, "A shortbow with no string yet.", "bow_unstrung"}, 18},
        {Item::UnstrungLongbow, {"Unstrung longbow", false, false, false, 0, 0, "A longbow with no string yet.", "bow_unstrung"}, 40},
        {Item::UnstrungOakShortbow, {"Unstrung oak shortbow", false, false, false, 0, 0, "An oak shortbow with no string yet.", "bow_unstrung"}, 90},
        {Item::UnstrungOakLongbow, {"Unstrung oak longbow", false, false, false, 0, 0, "An oak longbow with no string yet.", "bow_unstrung"}, 150},
        {Item::UnstrungYewShortbow, {"Unstrung yew shortbow", false, false, false, 0, 0, "A yew shortbow with no string yet.", "bow_unstrung"}, 330},
        {Item::UnstrungYewLongbow, {"Unstrung yew longbow", false, false, false, 0, 0, "A yew longbow with no string yet.", "bow_unstrung"}, 480},
        // ---- more trees
        {Item::MapleLogs, {"Maple logs", false, false, false, 0, 0, "Logs cut from a maple tree.", "logs_maple"}, 32},
        {Item::TeakLogs, {"Teak logs", false, false, false, 0, 0, "Pale, oily logs from a teak tree.", "logs_teak"}, 44},
        {Item::MahoganyLogs, {"Mahogany logs", false, false, false, 0, 0, "Dark red logs from a mahogany tree.", "logs_mahogany"}, 60},
        {Item::MagicLogs, {"Magic logs", false, false, false, 0, 0, "Logs from a magic tree. They shimmer faintly.", "logs_magic"}, 160},
        // ---- cooking: the shelf
        {Item::Flour, {"Flour", true, false, false, 0, 0, "Finely ground flour. Add water.", "flour"}, 4},
        {Item::JugOfWater, {"Jug of water", true, false, false, 0, 0, "A jug of clean water.", "jug"}, 2},
        {Item::Cheese, {"Cheese", true, false, false, 0, 0, "A wedge of cheese.", "cheese"}, 6},
        {Item::Tomato, {"Tomato", true, false, false, 0, 0, "A ripe tomato.", "tomato"}, 3},
        {Item::Potato, {"Potato", true, false, false, 0, 0, "A potato. It wants cooking.", "potato"}, 3},
        {Item::CookingApple, {"Cooking apple", true, false, false, 0, 0, "A tart apple, good for pies.", "apple"}, 4},
        {Item::BucketOfMilk, {"Bucket of milk", true, false, false, 0, 0, "A bucket of fresh milk.", "milk"}, 5},
        {Item::Onion, {"Onion", true, false, false, 0, 0, "An onion. It makes you tear up.", "onion"}, 3},
        {Item::Cabbage, {"Cabbage", true, false, false, 0, 0, "A leafy green cabbage.", "cabbage"}, 3},
        {Item::Redberries, {"Redberries", true, false, false, 0, 0, "A handful of sweet redberries.", "berries"}, 4},
        {Item::BreadDough, {"Bread dough", false, true, false, 0, 0, "Dough: it needs baking on a fire or a range.", "dough"}, 6},
        {Item::PastryDough, {"Pastry dough", false, false, false, 0, 0, "Pastry, ready to fill a pie dish.", "dough"}, 8},
        {Item::PizzaBase, {"Pizza base", false, false, false, 0, 0, "A flat round of dough.", "dough"}, 10},
        {Item::RawMeatPie, {"Uncooked meat pie", false, true, false, 0, 0, "I should bake this first.", "pie_raw"}, 30},
        {Item::MeatPie, {"Meat pie", false, false, true, 9, 0, "A hearty meat pie.", "pie"}, 64},
        {Item::RawApplePie, {"Uncooked apple pie", false, true, false, 0, 0, "I should bake this first.", "pie_raw"}, 26},
        {Item::ApplePie, {"Apple pie", false, false, true, 8, 0, "A sweet apple pie.", "pie"}, 56},
        {Item::RawRedberryPie, {"Uncooked redberry pie", false, true, false, 0, 0, "I should bake this first.", "pie_raw"}, 22},
        {Item::RedberryPie, {"Redberry pie", false, false, true, 6, 0, "A tangy redberry pie.", "pie"}, 44},
        {Item::RawGardenPie, {"Uncooked garden pie", false, true, false, 0, 0, "I should bake this first.", "pie_raw"}, 36},
        {Item::GardenPie, {"Garden pie", false, false, true, 10, 0, "A pie full of tomatoes and onions.", "pie"}, 80},
        {Item::RawFishPie, {"Uncooked fish pie", false, true, false, 0, 0, "I should bake this first.", "pie_raw"}, 70},
        {Item::FishPie, {"Fish pie", false, false, true, 12, 0, "A pie of trout and potato.", "pie"}, 130},
        {Item::UncookedStew, {"Uncooked stew", false, true, false, 0, 0, "I should cook this first.", "stew_raw"}, 24},
        {Item::Stew, {"Stew", false, false, true, 11, 0, "A thick, filling stew.", "stew"}, 52},
        {Item::UncookedCake, {"Uncooked cake", false, true, false, 0, 0, "I should bake this first.", "cake_raw"}, 50},
        {Item::Cake, {"Cake", false, false, true, 12, 0, "A fluffy sponge cake. It heals a good deal.", "cake"}, 100},
        {Item::RawPizza, {"Uncooked pizza", false, true, false, 0, 0, "I should bake this first.", "pizza_raw"}, 44},
        {Item::PlainPizza, {"Plain pizza", false, false, true, 14, 0, "Cheese and tomato on a crisp base.", "pizza"}, 90},
        {Item::BurntPie, {"Burnt pie", false, false, false, 0, 0, "Oops! It is a lump of charcoal.", "pie_burnt"}, 0},
        {Item::RawRatMeat, {"Raw rat meat", false, true, false, 0, 0, "I should cook this first. Ew.", "drumstick_raw"}, 2},
        {Item::RatMeat, {"Rat meat", false, false, true, 3, 0, "Stringy, but it is food.", "drumstick_cooked"}, 5},
        {Item::RawWolfMeat, {"Raw wolf meat", false, true, false, 0, 0, "I should cook this first.", "steak_raw"}, 9},
        {Item::WolfMeat, {"Wolf meat", false, false, true, 7, 0, "Dark, strong-tasting meat.", "steak_cooked"}, 24},
        // ---- more fish
        {Item::RawMackerel, {"Raw mackerel", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 8},
        {Item::Mackerel, {"Mackerel", false, false, true, 6, 0, "A nicely cooked mackerel.", "fish_cooked"}, 18},
        {Item::RawCod, {"Raw cod", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 14},
        {Item::Cod, {"Cod", false, false, true, 7, 0, "A flaky cooked cod.", "fish_cooked"}, 28},
        {Item::RawEel, {"Raw eel", false, true, false, 0, 0, "I should cook this first. It wriggles.", "fish_raw"}, 34},
        {Item::Eel, {"Eel", false, false, true, 9, 0, "A cooked eel.", "fish_cooked"}, 60},
        {Item::RawMantaRay, {"Raw manta ray", false, true, false, 0, 0, "I should cook this first. It is enormous.", "fish_raw"}, 420},
        {Item::MantaRay, {"Manta ray", false, false, true, 22, 0, "A great cooked manta ray. It heals a lot.", "fish_cooked"}, 560},
        {Item::RawSeaTurtle, {"Raw sea turtle", false, true, false, 0, 0, "I should cook this first.", "fish_raw"}, 520},
        {Item::SeaTurtle, {"Sea turtle", false, false, true, 21, 0, "A cooked sea turtle. A feast.", "fish_cooked"}, 680},
        // ---- leatherwork and the plainer gems
        {Item::HardLeather, {"Hard leather", false, false, false, 0, 0, "Stiff, boiled leather. It needs a needle.", "leather"}, 50},
        {Item::HardLeatherCap, {"Hard leather cap", false, false, false, 0, 0, "A stiff leather cap.", "leather_cap"}, 90},
        {Item::HardLeatherBody, {"Hard leather body", false, false, false, 0, 0, "A stiff leather jerkin.", "leather_body"}, 280},
        {Item::HardLeatherChaps, {"Hard leather chaps", false, false, false, 0, 0, "Stiff leather leg guards.", "leather_chaps"}, 190},
        {Item::StuddedCap, {"Studded leather cap", false, false, false, 0, 0, "A leather cap with steel studs.", "leather_cap"}, 150},
        {Item::StuddedBody, {"Studded leather body", false, false, false, 0, 0, "A leather jerkin studded with steel.", "leather_body"}, 400},
        {Item::StuddedChaps, {"Studded leather chaps", false, false, false, 0, 0, "Leather leg guards studded with steel.", "leather_chaps"}, 280},
        {Item::Studs, {"Steel studs", true, false, false, 0, 0, "Little steel studs for armour.", "studs"}, 3},
        {Item::UncutOpal, {"Uncut opal", false, false, false, 0, 0, "A rough milky stone. A chisel would help.", "gem_white"}, 25},
        {Item::Opal, {"Opal", false, false, false, 0, 0, "A cut opal.", "gem_white"}, 60},
        {Item::UncutJade, {"Uncut jade", false, false, false, 0, 0, "A rough green stone. A chisel would help.", "gem_green"}, 50},
        {Item::Jade, {"Jade", false, false, false, 0, 0, "A cut jade.", "gem_green"}, 120},
        {Item::UncutRedTopaz, {"Uncut red topaz", false, false, false, 0, 0, "A rough orange-red stone. A chisel would help.", "gem_red"}, 100},
        {Item::RedTopaz, {"Red topaz", false, false, false, 0, 0, "A cut red topaz.", "gem_red"}, 240},
        // ---- quest items
        {Item::Spade, {"Spade", false, false, false, 0, 0, "Good for digging.", "spade"}, 8},
        {Item::Candle, {"Candle", true, false, false, 0, 0, "A tallow candle. Light it with a tinderbox.", "candle"}, 2},
        {Item::RustyKey, {"Rusty key", false, false, false, 0, 0, "An old key, green with age. What does it open?", "key"}, 0},
        {Item::CaptainsLog, {"Captain's log", false, false, false, 0, 0, "A water-stained logbook in a looping hand.", "book"}, 0},
        // ---- round 11: leather boots and gloves
        {Item::LeatherBoots, {"Leather boots", false, false, false, 0, 0, "Soft leather boots. Quiet on the road.", "boots_leather"}, 36},
        {Item::HardLeatherBoots, {"Hard leather boots", false, false, false, 0, 0, "Stiff boots of boiled leather.", "boots_hard"}, 85},
        {Item::StuddedBoots, {"Studded leather boots", false, false, false, 0, 0, "Hard leather boots with steel studs.", "boots_studded"}, 130},
        {Item::LeatherGloves, {"Leather gloves", false, false, false, 0, 0, "Soft leather gloves.", "gloves_leather"}, 28},
        {Item::HardLeatherGloves, {"Hard leather gloves", false, false, false, 0, 0, "Stiff gloves of boiled leather.", "gloves_hard"}, 70},
        // ---- jewellery from the plainer gems
        {Item::OpalRing, {"Opal ring", false, false, false, 0, 0, "A gold ring set with a milky opal.", "ring_gold"}, 430},
        {Item::OpalAmulet, {"Opal amulet", false, false, false, 0, 0, "A gold amulet set with a milky opal.", "amulet_gold"}, 480},
        {Item::JadeRing, {"Jade ring", false, false, false, 0, 0, "A gold ring set with a green jade.", "ring_gold"}, 520},
        {Item::JadeAmulet, {"Jade amulet", false, false, false, 0, 0, "A gold amulet set with a green jade.", "amulet_gold"}, 570},
        {Item::RedTopazRing, {"Red topaz ring", false, false, false, 0, 0, "A gold ring set with a red topaz.", "ring_gold"}, 680},
        {Item::RedTopazAmulet, {"Red topaz amulet", false, false, false, 0, 0, "A gold amulet set with a red topaz.", "amulet_gold"}, 740},
    };
    return v;
}
const ExtraItem* FindExtra(Item item) {
    for (const ExtraItem& e : Extras())
        if (e.item == item) return &e;
    return nullptr;
}
}  // namespace

// The whole table. Smithed gear is generated (4 metals x 9 types), so the names live in a pool that never moves.
struct Table {
    std::vector<ItemInfo> info;
    std::deque<std::string> pool;
    const char* Keep(const std::string& s) {
        pool.push_back(s);
        return pool.back().c_str();
    }
    Table() {
        info.assign(static_cast<size_t>(Item::Count), kBase[0]);
        for (int i = 0; i < kBaseCount; ++i) info[i] = kBase[i];
        {   // money: the first stackable item
            ItemInfo c = kBase[0];
            c.name = "Coins";
            c.stackable = true;
            c.examine = "Lovely money!";
            c.model = "coins";
            info[static_cast<size_t>(Item::Coins)] = c;
            ItemInfo b = kBase[0];
            b.name = "Bread";
            b.food = true;
            b.heals = 5;
            b.examine = "Nice crusty bread.";
            b.model = "bread";
            info[static_cast<size_t>(Item::Bread)] = b;
            for (const ExtraItem& e : Extras()) info[static_cast<size_t>(e.item)] = e.info;
            {   // the bones of bigger monsters: more Prayer XP
                ItemInfo bb = kBase[0];
                bb.name = "Big bones";
                bb.buryXp = 15;
                bb.examine = "Ew, a BIG bone.";
                bb.model = "bones";
                info[static_cast<size_t>(Item::BigBones)] = bb;
                ItemInfo db = kBase[0];
                db.name = "Dragon bones";
                db.buryXp = 72;
                db.examine = "The bones of a dragon. Worth a lot of prayer.";
                db.model = "bones";
                info[static_cast<size_t>(Item::DragonBones)] = db;
            }
            for (int i = 0; i < kBowCount; ++i) {   // bows
                const BowInfo& bow = GetBow(i);
                ItemInfo ii = kBase[0];
                ii.name = bow.name;
                ii.examine = bow.examine;
                ii.model = bow.model;
                info[static_cast<size_t>(bow.item)] = ii;
            }
            for (int i = 0; i < kArrowKinds + kDartKinds; ++i) {   // arrows and darts (stackable)
                const ArrowInfo& a = i < kArrowKinds ? GetArrow(i) : GetDart(i - kArrowKinds);
                ItemInfo ii = kBase[0];
                ii.name = a.name;
                ii.stackable = true;
                ii.examine = a.examine;
                ii.model = a.model;
                info[static_cast<size_t>(a.item)] = ii;
            }
            for (int i = 0; i < kRuneCount; ++i) {   // runes (stackable)
                ItemInfo ii = kBase[0];
                ii.name = RuneName(static_cast<Rune>(i));
                ii.stackable = true;
                ii.examine = "Used for casting spells.";
                static const char* kModels[kRuneCount] = {"rune_air", "rune_water", "rune_earth", "rune_fire", "rune_mind", "rune_chaos"};
                ii.model = kModels[i];
                info[static_cast<size_t>(RuneItem(static_cast<Rune>(i)))] = ii;
            }
            for (int i = 0; i < kStaffCount; ++i) {
                const StaffInfo& s = GetStaff(i);
                ItemInfo ii = kBase[0];
                ii.name = s.name;
                ii.examine = s.examine;
                ii.model = s.model;
                info[static_cast<size_t>(s.item)] = ii;
            }
            for (int i = 0; i < kRobeCount; ++i) {
                const RobeInfo& r = GetRobe(i);
                ItemInfo ii = kBase[0];
                ii.name = r.name;
                ii.examine = r.examine;
                static const char* kModels[kRobeCount] = {"wizard_hat", "wizard_robe", "wizard_skirt"};
                ii.model = kModels[i];
                info[static_cast<size_t>(r.item)] = ii;
            }
        }
        for (int i = 0; i < CapeCount(); ++i) {   // the skill capes
            const CapeInfo& c = GetCape(i);
            ItemInfo ii = kBase[0];
            ii.name = c.name;
            ii.examine = c.allQuests ? "Worn by a hero who has finished every quest in the valley." : Keep(std::string("Worn by a master of ") + SkillName(c.skill) + ", a level 99 hero.");
            ii.model = Keep(std::string("cape_") + c.key);
            info[static_cast<size_t>(c.item)] = ii;
        }
        for (int ti = 0; ti < kTierCount; ++ti) {
            for (int gi = 0; gi < kGearTypeCount; ++gi) {
                const Tier t = static_cast<Tier>(ti);
                const GearType g = static_cast<GearType>(gi);
                if (g == GearType::Arrows) continue;   // (arrows are described in Archery.cpp)
                const Item item = GearItem(t, g);
                const std::string tier = Lower(TierName(t));
                const std::string type = Lower(GearTypeName(g));
                std::string text;
                switch (g) {
                    case GearType::Dagger: text = "A short, sharp " + tier + " dagger."; break;
                    case GearType::Sword: text = "A well-balanced " + tier + " sword."; break;
                    case GearType::Axe: text = "A heavy " + tier + " battleaxe."; break;
                    case GearType::Mace: text = "A spiked " + tier + " mace."; break;
                    case GearType::Helm: text = "A sturdy " + tier + " helmet."; break;
                    case GearType::Shield: text = "A large " + tier + " kiteshield."; break;
                    case GearType::Legs: text = "Some " + tier + " plate armour for my legs."; break;
                    case GearType::Body: text = "A " + tier + " breastplate that covers my chest."; break;
                    case GearType::Pickaxe: text = "Used for mining. A " + tier + " pickaxe."; break;
                    case GearType::Hatchet: text = "A woodcutter's axe: a " + tier + " one."; break;
                    case GearType::Arrows: break;
                    case GearType::Scimitar: text = "A curved " + tier + " scimitar. Quick and sharp."; break;
                    case GearType::Longsword: text = "A long " + tier + " sword with a good reach."; break;
                    case GearType::Spear: text = "A " + tier + " spear. It is all about the point."; break;
                    case GearType::Warhammer: text = "A huge " + tier + " warhammer. It takes strength to swing."; break;
                    case GearType::Fullhelm: text = "A " + tier + " helmet that covers the whole face."; break;
                    case GearType::SquareShield: text = "A broad " + tier + " square shield."; break;
                    case GearType::Chainbody: text = "A shirt of " + tier + " chain mail."; break;
                    case GearType::Boots: text = "A pair of sturdy " + tier + " boots."; break;
                    case GearType::Gauntlets: text = "Gloves of " + tier + " plates, for hands that fight."; break;
                }
                ItemInfo ii = kBase[0];
                ii.name = Keep(std::string(TierName(t)) + " " + type);
                ii.examine = Keep(text);
                ii.model = Keep(std::string(TierKey(t)) + "_" + GearTypeKey(g));
                if (g == GearType::Hatchet && t != Tier::Bronze) continue;   // (the iron, steel and mithril axes are in Extras())
                info[static_cast<size_t>(item)] = ii;
            }
        }
    }
};
const Table& GetTable() {
    static const Table table;
    return table;
}
}  // namespace

const ItemInfo& GetItemInfo(Item item) {
    const int i = static_cast<int>(item);
    const Table& t = GetTable();
    return t.info[(i >= 0 && i < static_cast<int>(Item::Count)) ? i : 0];
}

int ItemValue(Item item) {
    switch (item) {
        case Item::Logs: return 4;
        case Item::BronzeAxe: return 16;
        case Item::SmallNet: return 6;
        case Item::Tinderbox: return 4;
        case Item::RawShrimps: return 4;
        case Item::Shrimps: return 8;
        case Item::RawAnchovies: return 10;
        case Item::Anchovies: return 18;
        case Item::Bones: return 2;
        case Item::BigBones: return 8;
        case Item::DragonBones: return 300;
        case Item::RawChicken: return 3;
        case Item::Chicken: return 9;
        case Item::RawBeef: return 4;
        case Item::Beef: return 12;
        case Item::Egg: return 3;
        case Item::Cowhide: return 14;
        case Item::CopperOre: return 8;
        case Item::TinOre: return 8;
        case Item::IronOre: return 28;
        case Item::Coal: return 34;
        case Item::MithrilOre: return 130;
        case Item::BronzeBar: return 22;
        case Item::IronBar: return 60;
        case Item::SteelBar: return 130;
        case Item::MithrilBar: return 330;
        case Item::Hammer: return 6;
        case Item::Bread: return 10;
        case Item::Coins: return 1;
        default: break;
    }
    if (const ExtraItem* extra = FindExtra(item)) return extra->value;
    if (FindCape(item)) return kCapePrice;
    if (const BowInfo* bow = FindBow(item)) return bow->value;
    if (const ArrowInfo* arrow = FindArrow(item)) return arrow->value;
    if (const StaffInfo* staff = FindStaff(item)) return staff->value;
    if (const RobeInfo* robe = FindRobe(item)) return robe->value;
    {
        static const int kRuneValue[kRuneCount] = {2, 3, 3, 4, 2, 8};
        Rune r;
        if (IsRune(item, &r)) return kRuneValue[static_cast<int>(r)];
    }
    Tier t;
    GearType g;
    if (GearOf(item, &t, &g)) {   // worth the bars it is made of, plus a little for the smith's work
        static const int kBar[kTierCount] = {22, 60, 130, 330, 640};
        return static_cast<int>(GetSmithRecipe(t, g).bars * kBar[static_cast<int>(t)] * 1.15 + 0.5);
    }
    return 0;
}

bool ItemSellable(Item item) {
    return item != Item::None && item != Item::Coins && ItemValue(item) > 0;
}

const char* ActionName(ItemAction a) {
    switch (a) {
        case ItemAction::Light: return "Light";
        case ItemAction::Bonfire: return "Bonfire";
        case ItemAction::Cook: return "Cook";
        case ItemAction::Eat: return "Eat";
        case ItemAction::Bury: return "Bury";
        case ItemAction::Wield: return "Wield";
        case ItemAction::Wear: return "Wear";
        case ItemAction::Use: return "Use";
        case ItemAction::Drop: return "Drop";
        case ItemAction::Examine: return "Examine";
        case ItemAction::Read: return "Read";
    }
    return "";
}

int Inventory::Add(Item item, int count) {
    if (item == Item::None || count <= 0) return 0;
    int added = 0;
    if (GetItemInfo(item).stackable) {
        for (ItemStack& s : slots_)
            if (s.item == item) {
                s.count += count;
                return count;
            }
    }
    for (ItemStack& s : slots_) {
        if (added >= count) break;
        if (s.item != Item::None) continue;
        if (GetItemInfo(item).stackable) {
            s = {item, count};
            return count;
        }
        s = {item, 1};
        ++added;
    }
    return added;
}

ItemStack Inventory::TakeSlot(int slot) {
    if (slot < 0 || slot >= kSlots) return {};
    const ItemStack s = slots_[slot];
    slots_[slot] = {};
    return s;
}

bool Inventory::Remove(Item item, int count) {
    if (item == Item::None || count <= 0 || Count(item) < count) return false;
    for (ItemStack& s : slots_) {
        if (count <= 0) break;
        if (s.item != item) continue;
        const int take = s.count < count ? s.count : count;
        s.count -= take;
        count -= take;
        if (s.count <= 0) s = {};
    }
    return true;
}

int Inventory::Count(Item item) const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == item) n += s.count;
    return n;
}

int Inventory::FindSlot(Item item) const {
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i].item == item) return i;
    return -1;
}

int Inventory::FindRawFood() const {
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i].item != Item::None && GetItemInfo(slots_[i].item).raw) return i;
    return -1;
}

int Inventory::FreeSlots() const {
    int n = 0;
    for (const ItemStack& s : slots_)
        if (s.item == Item::None) ++n;
    return n;
}

std::vector<ItemAction> ActionsFor(Item item, const Inventory& bag) {
    std::vector<ItemAction> out;
    if (item == Item::None) return out;
    const ItemInfo& info = GetItemInfo(item);
    // The most useful button first (a tap on the item's first button is the "default" action), then the common ones.
    if (info.food) out.push_back(ItemAction::Eat);
    if (info.buryXp > 0) out.push_back(ItemAction::Bury);
    if (info.raw) out.push_back(ItemAction::Cook);
    if (IsScroll(item)) out.push_back(ItemAction::Read);
    EquipSlot slot;
    if (Equippable(item, &slot)) out.push_back((slot == EquipSlot::Weapon || slot == EquipSlot::Shield) ? ItemAction::Wield : ItemAction::Wear);
    if (IsLog(item) && bag.Has(Item::Tinderbox)) out.push_back(ItemAction::Light);
    if (IsLog(item) && bag.Has(Item::Tinderbox) && bag.Count(item) >= 4) out.push_back(ItemAction::Bonfire);   // (kBonfireLogs)
    if (item == Item::Tinderbox && AnyLog(bag) != Item::None) out.push_back(ItemAction::Light);
    out.push_back(ItemAction::Use);
    out.push_back(ItemAction::Drop);
    out.push_back(ItemAction::Examine);
    return out;
}

int GiveStarterKit(Inventory* bag) {
    int added = 0;
    for (Item tool : {Item::BronzeAxe, Item::SmallNet, Item::Tinderbox, Item::BronzePickaxe, Item::Hammer})
        if (!bag->Has(tool)) added += bag->Add(tool);
    return added;
}

}  // namespace journey
