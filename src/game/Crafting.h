// Crafting: tanning hides, sewing leather, cutting gems, making jewellery and pottery. Pure C++ (unit tested). All data (Crafting.cpp).
//
// Every recipe turns some ingredients into a product. Some need a tool that is not used up (a needle, a chisel, a mould) and
// some need a place: the tanner's rack, the jeweller's bench by the furnace, the potter's wheel. Leather and gems can be worked
// anywhere ("use" the needle on leather, the chisel on a gem).
#pragma once
#include <string>
#include <vector>

#include "game/Inventory.h"
#include "game/Skills.h"

namespace journey {

// Kitchen and Fletching can be done anywhere (use the ingredient with the other); Kitchen trains Cooking, Fletching (carving wood) trains Woodcutting.
enum class CraftKind { Tan, Leather, Gem, Jewellery, Pottery, Kitchen, Fletching };

struct CraftRecipe {
    CraftKind kind;
    Item product;
    int yield;
    int level;           // level needed (in `skill`)
    int xp;
    Item input[3];       // ingredients (Item::None = unused slot)
    int count[3];
    Item tool;           // needed but not used up (Item::None = nothing)
    Skill skill = Skill::Crafting;   // the skill that is needed and trained
};

const std::vector<CraftRecipe>& AllCraftRecipes();
// The station a kind of crafting needs ("" = none, anywhere): "tanner", "jeweller", "wheel".
const char* CraftStation(CraftKind k);
const char* CraftTitle(CraftKind k);          // "Tanner", "Jeweller's bench", "Potter's wheel", "Leatherwork", "Gem cutting", "Kitchen", "Woodwork"
// Two items used together: the craft menu they belong to (a recipe that takes both, or a tool and its material). false if none.
bool CraftKindForUse(Item a, Item b, CraftKind* kind);
// How many of a recipe he could make now from what the bag holds (ingredients and the tool).
int CanCraft(const CraftRecipe& r, const Inventory& bag);
// "1 leather + 1 thread" / "1 gold bar + 1 sapphire"
std::string IngredientText(const CraftRecipe& r);

// Leather armour: worn like any armour, light and good for archers.
struct LeatherInfo {
    Item item;
    int slot;          // an EquipSlot (head, body, legs, feet or hands)
    int defence;
    int ranged;        // a little ranged accuracy
    const char* model; // worn model: gear_<model> (+ _torso_m ...)
};
const LeatherInfo* FindLeather(Item item);

}  // namespace journey
