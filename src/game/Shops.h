// Shops: what they sell, what they will buy, and what everything costs. Pure C++ (unit tested).
//
// A shop sells the items on its shelves at their value times its markup (a general store charges a bit more, a specialist
// less). It buys only the kinds of thing it deals in, for a share of their value, and puts what you sell on its shelves
// for others. Shelves refill by themselves: stock the shop normally carries comes back one at a time, extra stock it
// bought from you is sold on and fades away.
#pragma once
#include <string>
#include <vector>

#include "game/Inventory.h"

namespace journey {

// Kinds of goods a shop may buy (a bit mask).
enum ShopCategory : unsigned {
    kCatFood = 1, kCatWeapon = 2, kCatArmour = 4, kCatOre = 8, kCatBar = 16, kCatTool = 32, kCatMisc = 64,
    kCatEverything = 127,
};
unsigned CategoryOf(Item item);

struct ShopStockDef {
    Item item;
    int count;   // how many the shelf normally holds
};

struct ShopDef {
    std::string key;
    std::string name;
    float markup;      // selling price = value * markup
    float sellRate;    // what it pays you = value * sellRate
    unsigned buys;     // ShopCategory mask
    std::vector<ShopStockDef> stock;
};

const std::vector<ShopDef>& AllShopDefs();
int ShopIndex(const std::string& key);   // -1 if unknown

constexpr float kRestockSeconds = 12.0f;   // a missing item comes back this often
constexpr float kExcessSeconds = 40.0f;    // surplus the shop bought fades at this pace
constexpr int kMaxShopLines = 30;

class Shops {
public:
    struct Line {
        Item item = Item::None;
        int count = 0;
        int base = 0;        // how many the shop normally keeps (0 = something it bought from you)
        float timer = 0;
    };

    void Init();
    void Update(float dt);

    int Count() const { return static_cast<int>(lines_.size()); }
    const ShopDef& Def(int shop) const;
    const std::vector<Line>& Lines(int shop) const { return lines_[shop]; }

    // Price to buy one / what the shop pays for one (0 = it does not buy it).
    int BuyPrice(int shop, Item item) const;
    int SellPrice(int shop, Item item) const;
    bool Buys(int shop, Item item) const;

    // Takes `qty` from a shelf line (returns how many were really taken).
    int Take(int shop, int line, int qty);
    // Puts something the hero sold on the shelves.
    void Add(int shop, Item item, int qty);

private:
    std::vector<std::vector<Line>> lines_;
};

}  // namespace journey
