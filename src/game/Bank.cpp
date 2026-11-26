#include "game/Bank.h"

#include <algorithm>

namespace journey {

int Bank::IndexOf(Item item) const {
    for (size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].item == item) return static_cast<int>(i);
    return -1;
}

int Bank::Count(Item item) const {
    const int i = IndexOf(item);
    return i < 0 ? 0 : slots_[static_cast<size_t>(i)].count;
}

int Bank::Deposit(Item item, int count) {
    if (item == Item::None || count <= 0) return 0;
    const int i = IndexOf(item);
    if (i < 0) {
        if (Full()) return 0;
        const int put = std::min(count, kMaxStack);
        slots_.push_back({item, put});
        return put;
    }
    ItemStack& s = slots_[static_cast<size_t>(i)];
    const int put = std::min(count, kMaxStack - s.count);
    if (put <= 0) return 0;
    s.count += put;
    return put;
}

int Bank::Withdraw(Item item, int count) {
    const int i = IndexOf(item);
    if (i < 0 || count <= 0) return 0;
    ItemStack& s = slots_[static_cast<size_t>(i)];
    const int taken = std::min(count, s.count);
    s.count -= taken;
    if (s.count <= 0) slots_.erase(slots_.begin() + i);
    return taken;
}

void Bank::SetSlot(Item item, int count) {
    if (item == Item::None) return;
    const int i = IndexOf(item);
    if (count <= 0) {
        if (i >= 0) slots_.erase(slots_.begin() + i);
        return;
    }
    if (i >= 0) {
        slots_[static_cast<size_t>(i)].count = std::min(count, kMaxStack);
    } else if (!Full()) {
        slots_.push_back({item, std::min(count, kMaxStack)});
    }
}

}  // namespace journey
