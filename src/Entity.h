#pragma once
#include "Vec2.h"
#include "Item.h"
#include "Map.h"
#include <string>
#include <vector>

enum class PlayerClass { Warrior, Rogue, Mage, Cleric };

class Entity {
public:
    Vec2 pos;
    std::string name;
    char glyph;
    int hp, maxHp;
    int attack;
    int defense;
    int level;
    int xp;

    Entity() : glyph('.'), hp(0), maxHp(0), attack(0), defense(0), level(1), xp(0) {}
    Entity(Vec2 pos, const std::string& name, char glyph, int hp, int atk, int def)
        : pos(pos), name(name), glyph(glyph), hp(hp), maxHp(hp),
          attack(atk), defense(def), level(1), xp(0) {}

    bool isAlive() const { return hp > 0; }
    virtual ~Entity() = default;
};

class Enemy;

class Player : public Entity {
public:
    Inventory inventory;
    Item* equippedWeapon = nullptr;
    Item* equippedArmor = nullptr;
    int baseAttack;
    int baseDefense;

    PlayerClass playerClass = PlayerClass::Warrior;
    int abilityCooldown = 0;
    bool abilityBuffActive = false;
    int lvlHp, lvlAtk, lvlDef;
    int poisonTurns = 0;
    int poisonDmg = 0;
    int killCount = 0;
    int blindTurns = 0;
    int slowTurns = 0;
    int hasteTurns = 0;
    int burningTurns = 0;
    int burningDmg = 2;
    int gold = 0;

    Player();
    Player(PlayerClass cls);
    void init(Vec2 startPos);
    int totalAttack() const;
    int totalDefense() const;
    void addXP(int amount);
    void levelUp();
    int xpToNextLevel() const;
    std::string useItem(int index);
    std::string equipItem(int index);
    std::string useAbility(std::vector<Enemy>& enemies);
    void tickCooldown();
    void tickPoison();
    void tickBurning();
    void tickStatusEffects();
    int effectiveFovRadius() const;
    std::string abilityName() const;
    int abilityCooldownMax() const;

    bool hasEquippedWeapon() const { return hasWeapon; }
    bool hasEquippedArmor() const { return hasArmor; }
    const Item& getWeaponSlot() const { return weaponSlot; }
    const Item& getArmorSlot() const { return armorSlot; }
    void setWeaponSlot(const Item& item) { weaponSlot = item; hasWeapon = true; equippedWeapon = &weaponSlot; }
    void setArmorSlot(const Item& item) { armorSlot = item; hasArmor = true; equippedArmor = &armorSlot; }

private:
    Item weaponSlot;
    Item armorSlot;
    bool hasWeapon = false;
    bool hasArmor = false;
};

enum class EnemyType { Rat, Skeleton, Ghost, Demon, Dragon, Archer, Necromancer, Lich };

class Enemy : public Entity {
public:
    EnemyType type;
    int xpReward;
    bool awake;
    int stunTurns = 0;
    int summonTimer = 0;
    bool enraged = false;

    Enemy() : type(EnemyType::Rat), xpReward(0), awake(false) {}
    Enemy(Vec2 pos, EnemyType type);

    void update(const Map& map, Vec2 playerPos);
    bool shouldFlee() const;
    void moveAwayFrom(const Map& map, Vec2 target);
    bool hasLineOfSight(const Map& map, Vec2 target) const;
    int rangedAttackDamage(const Map& map, Vec2 target) const;

    static Enemy create(EnemyType type, Vec2 pos);
    static EnemyType randomForFloor(int floor);

private:
    void moveToward(const Map& map, Vec2 target);
    void wander(const Map& map);
    Vec2 astarStep(const Map& map, Vec2 target) const;
};
