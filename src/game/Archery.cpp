#include "game/Archery.h"

namespace journey {

namespace {

const BowInfo kBows[kBowCount] = {
    {Item::Shortbow, "Shortbow", "shortbow", 1, 7, 8, 40, "A simple bow made of wood. It shoots fast."},
    {Item::Longbow, "Longbow", "longbow", 5, 9, 10, 90, "A tall bow made of wood. It shoots further."},
    {Item::OakShortbow, "Oak shortbow", "oak_shortbow", 20, 7, 16, 200, "A shortbow made of oak."},
    {Item::OakLongbow, "Oak longbow", "oak_longbow", 25, 9, 18, 320, "A longbow made of oak."},
    {Item::YewShortbow, "Yew shortbow", "yew_shortbow", 40, 7, 40, 700, "A powerful shortbow made of yew."},
    {Item::YewLongbow, "Yew longbow", "yew_longbow", 45, 9, 46, 1000, "A powerful longbow made of yew."},
};

const ArrowInfo kArrows[kArrowKinds] = {
    {Item::BronzeArrows, "Bronze arrows", "bronze_arrows", 0, 1, 7, 2, "Arrows with bronze heads."},
    {Item::IronArrows, "Iron arrows", "iron_arrows", 1, 5, 10, 3, "Arrows with iron heads."},
    {Item::SteelArrows, "Steel arrows", "steel_arrows", 2, 20, 16, 5, "Arrows with steel heads."},
    {Item::MithrilArrows, "Mithril arrows", "mithril_arrows", 3, 30, 22, 10, "Arrows with mithril heads."},
    {Item::AdamantArrows, "Adamant arrows", "adamant_arrows", 4, 40, 30, 18, "Arrows with adamant heads."},
};

}  // namespace

const BowInfo& GetBow(int index) { return kBows[index < 0 ? 0 : (index >= kBowCount ? kBowCount - 1 : index)]; }

const BowInfo* FindBow(Item item) {
    for (const BowInfo& b : kBows)
        if (b.item == item) return &b;
    return nullptr;
}

const ArrowInfo kDarts[kDartKinds] = {
    {Item::BronzeDarts, "Bronze darts", "bronze_arrows", 0, 1, 5, 3, "Little bronze darts: throw them with an empty hand.", true},
    {Item::IronDarts, "Iron darts", "iron_arrows", 1, 5, 8, 5, "Iron darts: throw them with an empty hand.", true},
    {Item::SteelDarts, "Steel darts", "steel_arrows", 2, 20, 13, 8, "Steel darts: throw them with an empty hand.", true},
    {Item::MithrilDarts, "Mithril darts", "mithril_arrows", 3, 30, 19, 16, "Mithril darts: throw them with an empty hand.", true},
    {Item::AdamantDarts, "Adamant darts", "adamant_arrows", 4, 40, 26, 28, "Adamant darts: throw them with an empty hand.", true},
};

const ArrowInfo& GetDart(int index) { return kDarts[index < 0 ? 0 : (index >= kDartKinds ? kDartKinds - 1 : index)]; }

const ArrowInfo& GetArrow(int index) { return kArrows[index < 0 ? 0 : (index >= kArrowKinds ? kArrowKinds - 1 : index)]; }

const ArrowInfo* FindArrow(Item item) {
    for (const ArrowInfo& a : kArrows)
        if (a.item == item) return &a;
    for (const ArrowInfo& a : kDarts)
        if (a.item == item) return &a;
    return nullptr;
}

}  // namespace journey
