#pragma once
#include "Vec2.h"
#include <string>
#include <vector>

enum class ItemType { HealthPotion, AttackBoost, DefenseBoost, Weapon, Armor, Gold };
enum class Enchantment { None, Rusty, Sharp, Flaming, Frozen, Vampiric, Blessed, Legendary };

class Item {
public:
    Vec2 pos;
    std::string name;
    char glyph;
    ItemType type;
    int value;
    bool onGround;
    Enchantment enchantment = Enchantment::None;

    Item() : glyph('?'), type(ItemType::HealthPotion), value(0), onGround(true) {}
    Item(Vec2 pos, const std::string& name, char glyph, ItemType type, int value,
         Enchantment ench = Enchantment::None);

    std::string description() const;
    static Enchantment rollEnchantment(int floor, bool legendary = false);
    static Item loadItem(Vec2 pos, const std::string& name, char glyph, ItemType type, int value, Enchantment ench);
};

struct ShopItem {
    Item item;
    int price;
    bool sold = false;
};

class Inventory {
public:
    static const int MAX_ITEMS = 10;

    bool add(const Item& item);
    void remove(int index);
    const Item& get(int index) const { return items[index]; }
    int size() const { return (int)items.size(); }
    bool isFull() const { return (int)items.size() >= MAX_ITEMS; }
    const std::vector<Item>& getItems() const { return items; }

private:
    std::vector<Item> items;
};
