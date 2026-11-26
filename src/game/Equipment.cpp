#include "game/Equipment.h"

#include "game/Archery.h"
#include "game/Magic.h"

namespace journey {

bool Equipment::Empty() const {
    for (Item i : slots_)
        if (i != Item::None) return false;
    return true;
}

bool Equipment::IsWorn(Item item) const {
    for (Item i : slots_)
        if (i == item && item != Item::None) return true;
    return false;
}

EquipBonuses Equipment::Total() const {
    EquipBonuses b;
    for (Item i : slots_) {
        const GearStats s = ItemStats(i);
        b.attack += s.attack;
        b.strength += s.strength;
        b.defence += s.defence;
        b.ranged += s.ranged;
        b.rangedStrength += s.rangedStrength;
        b.magic += s.magic;
    }
    return b;
}

void Equipment::SetAmmo(Item arrows, int count) {
    if (arrows == Item::None || count <= 0) {
        slots_[static_cast<int>(EquipSlot::Ammo)] = Item::None;
        ammoCount_ = 0;
        return;
    }
    slots_[static_cast<int>(EquipSlot::Ammo)] = arrows;
    ammoCount_ = count;
}

int Equipment::UseAmmo(int n) {
    const int taken = n < ammoCount_ ? n : ammoCount_;
    ammoCount_ -= taken;
    if (ammoCount_ <= 0) SetAmmo(Item::None, 0);
    return taken;
}

WeaponAnim Equipment::Anim() const {
    if (IsBow(Get(EquipSlot::Weapon))) return WeaponAnim::Shoot;
    if (IsStaff(Get(EquipSlot::Weapon))) return WeaponAnim::Cast;
    GearType g;
    if (!GearOf(Get(EquipSlot::Weapon), nullptr, &g)) return WeaponAnim::Unarmed;
    return AnimOf(g);
}

EquipResult EquipFromBag(Equipment* eq, Inventory* bag, int bagSlot, const Skills& skills, std::string* needed) {
    if (bagSlot < 0 || bagSlot >= Inventory::kSlots) return EquipResult::NotWearable;
    const Item item = bag->Slot(bagSlot).item;
    EquipSlot slot;
    if (!Equippable(item, &slot)) return EquipResult::NotWearable;
    Skill skill;
    int level;
    if (ItemRequirement(item, &skill, &level) && skills.Level(skill) < level) {
        if (needed) *needed = std::string(SkillName(skill)) + " level " + std::to_string(level);
        return EquipResult::LevelTooLow;
    }
    if (slot == EquipSlot::Ammo) {   // arrows: the whole stack goes in the quiver (arrows of the same kind join it)
        const ItemStack stack = bag->Slot(bagSlot);
        const Item old = eq->Get(slot);
        const int oldCount = eq->AmmoCount();
        bag->TakeSlot(bagSlot);
        if (old == item) {
            eq->SetAmmo(item, oldCount + stack.count);
        } else {
            eq->SetAmmo(item, stack.count);
            if (old != Item::None) bag->SetSlot(bagSlot, {old, oldCount});   // the other arrows go back in the bag
        }
        return EquipResult::Done;
    }
    // the old piece takes the bag slot the new one leaves (so nothing needs a free slot)
    const Item old = eq->Get(slot);
    bag->TakeSlot(bagSlot);
    eq->Set(slot, item);
    if (old != Item::None) bag->SetSlot(bagSlot, {old, 1});
    return EquipResult::Done;
}

EquipResult Unequip(Equipment* eq, Inventory* bag, EquipSlot slot) {
    const Item item = eq->Get(slot);
    if (item == Item::None) return EquipResult::NotWearable;
    if (slot == EquipSlot::Ammo) {   // the arrows go back in the bag (as many as fit)
        const int count = eq->AmmoCount();
        const int added = bag->Add(item, count);
        if (added == 0) return EquipResult::BagFull;
        eq->SetAmmo(item, count - added);
        return added == count ? EquipResult::Done : EquipResult::BagFull;
    }
    if (bag->Add(item) == 0) return EquipResult::BagFull;
    eq->Set(slot, Item::None);
    return EquipResult::Done;
}

}  // namespace journey
