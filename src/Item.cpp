#include "Item.h"
#include <algorithm>
#include <cstdlib>

static std::string enchantmentPrefix(Enchantment e) {
    switch (e) {
        case Enchantment::None: return "";
        case Enchantment::Rusty: return "Rusty ";
        case Enchantment::Sharp: return "Sharp ";
        case Enchantment::Flaming: return "Flaming ";
        case Enchantment::Frozen: return "Frozen ";
        case Enchantment::Vampiric: return "Vampiric ";
        case Enchantment::Blessed: return "Blessed ";
        case Enchantment::Legendary: return "Legendary ";
    }
    return "";
}

static int enchantmentValueMod(Enchantment e) {
    switch (e) {
        case Enchantment::None: return 0;
        case Enchantment::Rusty: return -1;
        case Enchantment::Sharp: return 2;
        case Enchantment::Flaming: return 1;
        case Enchantment::Frozen: return 1;
        case Enchantment::Vampiric: return 1;
        case Enchantment::Blessed: return 3;
        case Enchantment::Legendary: return 5;
    }
    return 0;
}

Item::Item(Vec2 pos, const std::string& name, char glyph, ItemType type, int value, Enchantment ench)
    : pos(pos), glyph(glyph), type(type), value(value), onGround(true), enchantment(ench) {
    if (ench != Enchantment::None) {
        this->name = enchantmentPrefix(ench) + name;
        this->value = std::max(1, value + enchantmentValueMod(ench));
    } else {
        this->name = name;
    }
}

Item Item::loadItem(Vec2 pos, const std::string& name, char glyph, ItemType type, int value, Enchantment ench) {
    Item item;
    item.pos = pos;
    item.name = name; // name already has prefix from save
    item.glyph = glyph;
    item.type = type;
    item.value = value;
    item.onGround = true;
    item.enchantment = ench;
    return item;
}

Enchantment Item::rollEnchantment(int floor, bool legendary) {
    if (legendary) return Enchantment::Legendary;
    int roll = std::rand() % 100;
    // Higher floors = better enchantments
    if (floor >= 6) {
        if (roll < 5) return Enchantment::Rusty;
        if (roll < 20) return Enchantment::Sharp;
        if (roll < 35) return Enchantment::Flaming;
        if (roll < 50) return Enchantment::Frozen;
        if (roll < 65) return Enchantment::Vampiric;
        if (roll < 80) return Enchantment::Blessed;
        return Enchantment::None;
    } else if (floor >= 3) {
        if (roll < 10) return Enchantment::Rusty;
        if (roll < 30) return Enchantment::Sharp;
        if (roll < 45) return Enchantment::Flaming;
        if (roll < 55) return Enchantment::Frozen;
        if (roll < 60) return Enchantment::Vampiric;
        return Enchantment::None;
    } else {
        if (roll < 20) return Enchantment::Rusty;
        if (roll < 35) return Enchantment::Sharp;
        return Enchantment::None;
    }
}

std::string Item::description() const {
    std::string desc;
    switch (type) {
        case ItemType::HealthPotion:  desc = name + " (heals " + std::to_string(value) + " HP)"; break;
        case ItemType::AttackBoost:   desc = name + " (+" + std::to_string(value) + " ATK permanently)"; break;
        case ItemType::DefenseBoost:  desc = name + " (+" + std::to_string(value) + " DEF permanently)"; break;
        case ItemType::Weapon:        desc = name + " (" + std::to_string(value) + " ATK)"; break;
        case ItemType::Armor:         desc = name + " (" + std::to_string(value) + " DEF)"; break;
        case ItemType::Gold:            desc = name + " (" + std::to_string(value) + " gold)"; break;
        case ItemType::TeleportScroll:  desc = name + " (teleport)"; break;
        case ItemType::Bomb:            desc = name + " (" + std::to_string(value) + " dmg AoE)"; break;
        case ItemType::ShieldPotion:    desc = name + " (+" + std::to_string(value) + " DEF, 10 turns)"; break;
    }
    // Append enchantment tags
    if (enchantment == Enchantment::Flaming) desc += " [Burns]";
    else if (enchantment == Enchantment::Vampiric) desc += " [Lifesteal]";
    else if (enchantment == Enchantment::Frozen) desc += " [Chills]";
    else if (enchantment == Enchantment::Blessed) desc += " [Holy]";
    else if (enchantment == Enchantment::Legendary) desc += " [Legendary]";
    return desc;
}

int Item::sellPrice() const {
    switch (type) {
        case ItemType::HealthPotion:    return std::max(1, value);
        case ItemType::AttackBoost:     return std::max(1, value * 15);
        case ItemType::DefenseBoost:    return std::max(1, value * 15);
        case ItemType::Weapon:          return std::max(1, 8 + value * 5);
        case ItemType::Armor:           return std::max(1, 6 + value * 5);
        case ItemType::TeleportScroll:  return 10;
        case ItemType::Bomb:            return 12;
        case ItemType::ShieldPotion:    return std::max(1, value);
        default:                        return 0;
    }
}

bool Inventory::add(const Item& item) {
    if (isFull()) return false;
    items.push_back(item);
    items.back().onGround = false;
    return true;
}

void Inventory::remove(int index) {
    if (index >= 0 && index < (int)items.size())
        items.erase(items.begin() + index);
}
