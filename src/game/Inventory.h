// The player's bag: 28 slots, like the classic game. Pure C++ (unit tested).
//
// Item ids are stored in the save file, so only ever add new items at the END of the enum (before Count).
#pragma once
#include <array>
#include <vector>

namespace journey {

enum class Item : int {
    None = 0,
    Logs,
    BronzeAxe,
    SmallNet,
    Tinderbox,
    RawShrimps,
    Shrimps,
    RawAnchovies,
    Anchovies,
    BurntFish,
    Bones,
    RawChicken,
    Chicken,
    RawBeef,
    Beef,
    BurntMeat,
    Egg,
    Cowhide,
    // ---- mining and smithing ----
    CopperOre,
    TinOre,
    IronOre,
    Coal,
    MithrilOre,
    BronzeBar,
    IronBar,
    SteelBar,
    MithrilBar,
    Hammer,
    BronzePickaxe,
    IronPickaxe,
    SteelPickaxe,
    MithrilPickaxe,
    // The 32 smithed weapons and armour: 4 metals x 8 pieces, in the order of Gear.h (dagger, sword, battleaxe, mace, helm,
    // shield, legs, body for bronze, then the same for iron...). GearItem() / GearOf() turn these into (tier, type).
    GearFirst,
    GearLast = GearFirst + 31,
    // ---- money and shop food ----
    Coins,
    Bread,
    // ---- archery (game/Archery.h): six bows (wood x short/long), four kinds of arrows ----
    Shortbow,
    Longbow,
    OakShortbow,
    OakLongbow,
    YewShortbow,
    YewLongbow,
    BronzeArrows,
    IronArrows,
    SteelArrows,
    MithrilArrows,
    // ---- magic (game/Magic.h): runes, staffs and wizard robes ----
    AirRune,
    WaterRune,
    EarthRune,
    FireRune,
    MindRune,
    ChaosRune,
    Staff,
    AirStaff,
    WaterStaff,
    EarthStaff,
    FireStaff,
    WizardHat,
    WizardRobe,
    WizardSkirt,
    // ---- from the bigger monsters ----
    BigBones,
    DragonBones,
    // ---- woodcutting: better trees and axes
    OakLogs,
    WillowLogs,
    YewLogs,
    IronAxe,
    SteelAxe,
    MithrilAxe,
    // ---- fishing: more tools and fish
    FlyFishingRod,
    Harpoon,
    LobsterPot,
    Feathers,
    RawTrout,
    Trout,
    RawSalmon,
    Salmon,
    RawTuna,
    Tuna,
    RawLobster,
    Lobster,
    RawSwordfish,
    Swordfish,
    // ---- mining: more rocks
    Clay,
    SilverOre,
    GoldOre,
    UncutSapphire,
    UncutEmerald,
    UncutRuby,
    SilverBar,
    GoldBar,
    // ---- crafting: gems, leather, jewellery, pottery
    Chisel,
    Sapphire,
    Emerald,
    Ruby,
    Needle,
    Thread,
    Leather,
    LeatherCap,
    LeatherBody,
    LeatherChaps,
    RingMould,
    AmuletMould,
    GoldRing,
    SapphireRing,
    EmeraldRing,
    RubyRing,
    GoldAmulet,
    SapphireAmulet,
    EmeraldAmulet,
    RubyAmulet,
    Pot,
    PieDish,
    Bowl,
    // ---- teleport scrolls (Read one to be whisked away; see game/Teleport.h)
    CastleScroll,
    MillbrookScroll,
    LakesideScroll,
    // ---- adamant, the fifth metal (Gear.h: Tier::Adamant). The eleven pieces are consecutive, in GearType order:
    // dagger, sword, battleaxe, mace, helm, shield, legs, body, pickaxe, axe, arrows.
    AdamantOre,
    AdamantBar,
    AdamantFirst,
    AdamantLast = AdamantFirst + 10,
    // ---- more fish (raw, cooked)
    RawSardine,
    Sardine,
    RawHerring,
    Herring,
    RawPike,
    Pike,
    RawBass,
    Bass,
    RawShark,
    Shark,
    // ---- rarer gems and the jewellery made from them
    UncutDiamond,
    Diamond,
    DiamondRing,
    DiamondAmulet,
    UncutDragonstone,
    Dragonstone,
    DragonstoneAmulet,
    // ---- unique jewellery (monster drops and quest rewards)
    MummysAmulet,
    TrollToothAmulet,
    AbyssalRing,
    // ---- round 10, the workshop round. Seven more smithed types in five metals (Gear.h: Scimitar..Chainbody), tier by tier:
    // tier * 7 + (type - Scimitar). GearItem() / GearOf() turn these into (tier, type).
    Gear2First,
    Gear2Last = Gear2First + 34,
    // ---- tools and woodwork: a knife carves logs into bows, a bowstring turns them into weapons
    Knife,
    Bowstring,
    UnstrungShortbow,
    UnstrungLongbow,
    UnstrungOakShortbow,
    UnstrungOakLongbow,
    UnstrungYewShortbow,
    UnstrungYewLongbow,
    // ---- more trees
    MapleLogs,
    TeakLogs,
    MahoganyLogs,
    MagicLogs,
    // ---- cooking: ingredients, doughs, dishes, game meat
    Flour,
    JugOfWater,
    Cheese,
    Tomato,
    Potato,
    CookingApple,
    BucketOfMilk,
    BreadDough,
    PastryDough,
    PizzaBase,
    RawMeatPie,
    MeatPie,
    RawApplePie,
    ApplePie,
    RawFishPie,
    FishPie,
    UncookedStew,
    Stew,
    UncookedCake,
    Cake,
    RawPizza,
    PlainPizza,
    BurntPie,
    RawRatMeat,
    RatMeat,
    RawWolfMeat,
    WolfMeat,
    Onion,
    Cabbage,
    Redberries,
    RawRedberryPie,
    RedberryPie,
    RawGardenPie,
    GardenPie,
    // ---- more fish
    RawMackerel,
    Mackerel,
    RawCod,
    Cod,
    RawEel,
    Eel,
    RawMantaRay,
    MantaRay,
    RawSeaTurtle,
    SeaTurtle,
    // ---- leatherwork, and the plainer gems
    HardLeather,
    HardLeatherCap,
    HardLeatherBody,
    HardLeatherChaps,
    StuddedCap,
    StuddedBody,
    StuddedChaps,
    Studs,
    UncutOpal,
    Opal,
    UncutJade,
    Jade,
    UncutRedTopaz,
    RedTopaz,
    // ---- quest items
    Spade,
    Candle,
    RustyKey,
    CaptainsLog,
    // ---- jewellery from the plainer gems
    OpalRing,
    OpalAmulet,
    JadeRing,
    JadeAmulet,
    RedTopazRing,
    RedTopazAmulet,
    // ---- round 11: boots and gauntlets in five metals (Gear.h: Boots, Gauntlets; id = tier * 2 + type - 18), leather boots and gloves, capes
    Gear3First,
    Gear3Last = Gear3First + 9,
    LeatherBoots,
    HardLeatherBoots,
    StuddedBoots,
    LeatherGloves,
    HardLeatherGloves,
    AttackCape,
    StrengthCape,
    DefenceCape,
    HitpointsCape,
    RangedCape,
    MagicCape,
    PrayerCape,
    MiningCape,
    SmithingCape,
    FishingCape,
    CookingCape,
    WoodcuttingCape,
    FiremakingCape,
    CraftingCape,
    QuestCape,
    BronzeDarts, IronDarts, SteelDarts, MithrilDarts, AdamantDarts,   // round 13: thrown weapons (Archery.h)
    ElmswickScroll, SaltmereScroll,                                    // round 13: teleport scrolls for the two wild-country towns
    AbyssalAmulet,                                                     // round 13: the rarest drop of the Abyss (its champions, knights and dragon, and the best of the players who hunt there)
    Count,
    // Names for two of the adamant pieces (aliases: they come after Count on purpose, so they do not add items).
    AdamantPickaxe = AdamantFirst + 8,
    AdamantAxe = AdamantFirst + 9,
    AdamantArrows = AdamantFirst + 10
};

struct ItemInfo {
    const char* name;
    bool stackable;   // stackable items share one slot (coins, arrows...); everything else takes a slot each
    bool raw;         // can be cooked on a fire
    bool food;        // can be eaten
    int heals;        // hit points restored when eaten
    int buryXp;       // Prayer XP for burying it (0 = cannot be buried)
    const char* examine;   // what "Examine" says
    const char* model;     // ground model file: assets/models/props/drop_<model>.glb (see tools/build_items.py)
};
const ItemInfo& GetItemInfo(Item item);

// A teleport scroll (Read one to be whisked to its town; the places are in Gameplay.cpp).
inline bool IsScroll(Item i) {
    return i == Item::CastleScroll || i == Item::MillbrookScroll || i == Item::LakesideScroll || i == Item::ElmswickScroll || i == Item::SaltmereScroll;
}

// What an item is worth in coins (shops sell at about this price and buy for a share of it). 0 = worthless (burnt food).
int ItemValue(Item item);
// Can a shop buy it? (not worthless, not a starter tool, not coins)
bool ItemSellable(Item item);

struct ItemStack {
    Item item = Item::None;
    int count = 0;
};

// What you can do with a bag item (buttons under the bag grid, and the long-press menu).
enum class ItemAction { Light, Cook, Eat, Bury, Wield, Wear, Use, Drop, Examine, Read, Bonfire };   // (Bonfire: four logs at once, see Fires.h)
const char* ActionName(ItemAction a);

class Inventory {
public:
    static constexpr int kSlots = 28;

    // Adds `count` of an item. Returns how many were actually added (less than asked when the bag fills up).
    int Add(Item item, int count = 1);
    // Removes the whole stack in a slot and returns it ({None,0} for an empty slot).
    ItemStack TakeSlot(int slot);
    // Removes `count` of an item (from the first slots that hold it). Returns false, changing nothing, if there are fewer.
    bool Remove(Item item, int count = 1);

    const ItemStack& Slot(int slot) const { return slots_[slot]; }
    void SetSlot(int slot, ItemStack s) { slots_[slot] = s; }   // for loading a save / swapping raw food for cooked
    int Count(Item item) const;
    bool Has(Item item) const { return Count(item) > 0; }
    // First slot holding `item`, or -1.
    int FindSlot(Item item) const;
    // First slot holding any raw (cookable) food, or -1.
    int FindRawFood() const;
    int FreeSlots() const;
    bool Full() const { return FreeSlots() == 0; }

private:
    std::array<ItemStack, kSlots> slots_{};
};

// The buttons that make sense for this item right now, e.g. Light for logs only while you carry a tinderbox.
std::vector<ItemAction> ActionsFor(Item item, const Inventory& bag);

// Makes sure the bag holds the starting tools (bronze axe, small fishing net, tinderbox, bronze pickaxe, hammer).
// Returns how many were added.
int GiveStarterKit(Inventory* bag);

}  // namespace journey
