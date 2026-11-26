// Small helpers shared by the parts of the adventurers' code (Adventurer*.cpp). Not part of the public interface.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "game/Adventurers.h"
#include "game/Archery.h"
#include "game/Gear.h"
#include "game/Magic.h"
#include "game/Shops.h"
#include "game/Skilling.h"
#include "game/Smithing.h"

namespace journey {
namespace advutil {

inline int Cheb(TileCoord a, TileCoord b) { return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z)); }

inline int FoodCount(const Inventory& bag) {
    int n = 0;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemStack& s = bag.Slot(i);
        if (s.item != Item::None && GetItemInfo(s.item).food && GetItemInfo(s.item).heals > 0) n += s.count;
    }
    return n;
}

// The slot of the food that heals the most (-1 = none).
inline int BestFoodSlot(const Inventory& bag) {
    int best = -1, heal = 0;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const Item it = bag.Slot(i).item;
        if (it == Item::None || !GetItemInfo(it).food) continue;
        if (GetItemInfo(it).heals > heal) {
            heal = GetItemInfo(it).heals;
            best = i;
        }
    }
    return best;
}

// How good a worn or wielded item is, for choosing between two.
inline float GearScore(Item item) {
    const GearStats s = ItemStats(item);
    return static_cast<float>(s.attack) + 1.4f * static_cast<float>(s.strength) + static_cast<float>(s.defence) + 0.2f * static_cast<float>(s.magic) +
           0.4f * static_cast<float>(s.ranged);
}

inline bool IsTool(Item item) {
    Tier t;
    GearType g;
    if (IsHatchet(item)) return true;
    if (GearOf(item, &t, &g) && (g == GearType::Pickaxe || g == GearType::Hatchet)) return true;
    switch (item) {
        case Item::SmallNet:
        case Item::Tinderbox:
        case Item::Hammer:
        case Item::BronzeAxe:
        case Item::FlyFishingRod:
        case Item::Harpoon:
        case Item::LobsterPot:
        case Item::Feathers:
        case Item::Knife:
        case Item::Spade:
        case Item::Needle:
        case Item::Chisel: return true;
        default: return false;
    }
}

inline bool IsWearable(Item item) {
    EquipSlot s;
    return Equippable(item, &s) && s != EquipSlot::Ammo && !IsBow(item) && !IsStaff(item);
}

}  // namespace advutil
}  // namespace journey
