#include "game/Equipment.h"

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
    }
    return b;
}

WeaponAnim Equipment::Anim() const {
    GearType g;
    if (!GearOf(Get(EquipSlot::Weapon), nullptr, &g)) return WeaponAnim::Unarmed;
    return AnimOf(g);
}

EquipResult EquipFromBag(Equipment* eq, Inventory* bag, int bagSlot, const Skills& skills, std::string* needed) {
    if (bagSlot < 0 || bagSlot >= Inventory::kSlots) return EquipResult::NotWearable;
    const Item item = bag->Slot(bagSlot).item;
    EquipSlot slot;
    Tier tier;
    GearType type;
    if (!Equippable(item, &slot) || !GearOf(item, &tier, &type)) return EquipResult::NotWearable;
    const Skill skill = RequirementSkill(type);
    const int level = TierRequirement(tier);
    if (skills.Level(skill) < level) {
        if (needed) *needed = std::string(SkillName(skill)) + " level " + std::to_string(level);
        return EquipResult::LevelTooLow;
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
    if (bag->Add(item) == 0) return EquipResult::BagFull;
    eq->Set(slot, Item::None);
    return EquipResult::Done;
}

}  // namespace journey
