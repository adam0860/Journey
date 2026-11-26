// The bank: a big store for everything you cannot carry. Pure C++ (unit tested).
//
// 200 slots, one per KIND of item (so a thousand coins or two hundred logs take one slot each, whatever the item). Bankers
// stand behind the counter of every bank; talk to one and the bank window opens. The bank is shared by every bank in the world.
#pragma once
#include <vector>

#include "game/Inventory.h"

namespace journey {

class Bank {
public:
    static constexpr int kSlots = 200;
    static constexpr int kMaxStack = 2000000000;

    // Puts up to `count` of an item in the bank. Returns how many went in (less when the bank is full or the stack is at its limit).
    int Deposit(Item item, int count);
    // Takes up to `count` of an item out of the bank and returns how many came out (the caller puts them in the bag).
    int Withdraw(Item item, int count);

    int Count(Item item) const;
    bool Has(Item item) const { return Count(item) > 0; }
    int Used() const { return static_cast<int>(slots_.size()); }   // how many slots are taken
    bool Full() const { return Used() >= kSlots; }
    bool Empty() const { return slots_.empty(); }
    const std::vector<ItemStack>& Slots() const { return slots_; }   // in the order the kinds were first put in
    void Clear() { slots_.clear(); }
    void SetSlot(Item item, int count);   // loading a save: sets the count of a kind (0 removes it)

private:
    int IndexOf(Item item) const;
    std::vector<ItemStack> slots_;
};

}  // namespace journey
