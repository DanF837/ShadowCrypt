#include "Entity.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

// --- Player ---

Player::Player() : Player(PlayerClass::Warrior) {}

Player::Player(PlayerClass cls) : Entity({0,0}, "Player", '@', 30, 5, 2), playerClass(cls) {
    switch (cls) {
        case PlayerClass::Warrior:
            hp = maxHp = 40; attack = 4; defense = 4;
            lvlHp = 8; lvlAtk = 1; lvlDef = 2;
            break;
        case PlayerClass::Rogue:
            hp = maxHp = 25; attack = 7; defense = 1;
            lvlHp = 3; lvlAtk = 3; lvlDef = 1;
            break;
        case PlayerClass::Mage:
            hp = maxHp = 20; attack = 8; defense = 1;
            lvlHp = 3; lvlAtk = 3; lvlDef = 0;
            break;
        case PlayerClass::Cleric:
            hp = maxHp = 35; attack = 3; defense = 3;
            lvlHp = 6; lvlAtk = 1; lvlDef = 1;
            break;
    }
    baseAttack = attack;
    baseDefense = defense;
    abilityCooldown = 0;
    abilityBuffActive = false;
}

void Player::init(Vec2 startPos) {
    pos = startPos;
    hp = maxHp;
}

int Player::totalAttack() const {
    int atk = baseAttack;
    if (hasWeapon) atk += weaponSlot.value;
    return atk;
}

int Player::totalDefense() const {
    int def = baseDefense;
    if (hasArmor) def += armorSlot.value;
    return def;
}

void Player::addXP(int amount) {
    xp += amount;
    while (xp >= xpToNextLevel()) {
        xp -= xpToNextLevel();
        levelUp();
    }
}

void Player::levelUp() {
    level++;
    maxHp += lvlHp;
    hp = std::min(hp + lvlHp + 5, maxHp);
    baseAttack += lvlAtk;
    baseDefense += lvlDef;
}

int Player::xpToNextLevel() const {
    return level * 20;
}

std::string Player::abilityName() const {
    switch (playerClass) {
        case PlayerClass::Warrior: return "Shield Bash";
        case PlayerClass::Rogue:   return "Backstab";
        case PlayerClass::Mage:    return "Fireball";
        case PlayerClass::Cleric:  return "Divine Heal";
    }
    return "";
}

int Player::abilityCooldownMax() const {
    switch (playerClass) {
        case PlayerClass::Warrior: return 8;
        case PlayerClass::Rogue:   return 6;
        case PlayerClass::Mage:    return 10;
        case PlayerClass::Cleric:  return 12;
    }
    return 10;
}

std::string Player::useAbility(std::vector<Enemy>& enemies) {
    if (abilityCooldown > 0) {
        return abilityName() + " on cooldown (" + std::to_string(abilityCooldown) + " turns).";
    }

    switch (playerClass) {
        case PlayerClass::Warrior:
            abilityBuffActive = true;
            abilityCooldown = abilityCooldownMax();
            return "Shield Bash ready! Next attack deals bonus damage and stuns.";

        case PlayerClass::Rogue:
            abilityBuffActive = true;
            abilityCooldown = abilityCooldownMax();
            return "Backstab ready! Next attack deals triple damage.";

        case PlayerClass::Mage: {
            abilityCooldown = abilityCooldownMax();
            int dmg = 8 + level * 2;
            int hits = 0;
            for (auto& e : enemies) {
                if (!e.isAlive()) continue;
                int dist = pos.distanceSq(e.pos);
                if (dist <= 9) { // within 3 tiles
                    e.hp -= dmg;
                    if (e.hp < 0) e.hp = 0;
                    hits++;
                }
            }
            if (hits == 0) return "Fireball! No enemies in range.";
            return "Fireball hits " + std::to_string(hits) + " enemies for " + std::to_string(dmg) + " damage!";
        }

        case PlayerClass::Cleric: {
            abilityCooldown = abilityCooldownMax();
            int heal = maxHp / 2;
            hp = std::min(hp + heal, maxHp);
            return "Divine Heal restores " + std::to_string(heal) + " HP!";
        }
    }
    return "";
}

void Player::tickCooldown() {
    if (abilityCooldown > 0) abilityCooldown--;
}

void Player::tickPoison() {
    if (poisonTurns > 0) {
        hp -= poisonDmg;
        if (hp < 0) hp = 0;
        poisonTurns--;
    }
}

void Player::tickBurning() {
    if (burningTurns > 0) {
        hp -= burningDmg;
        if (hp < 0) hp = 0;
        burningTurns--;
    }
}

void Player::tickStatusEffects() {
    tickPoison();
    tickBurning();
    if (blindTurns > 0) blindTurns--;
    if (slowTurns > 0) slowTurns--;
    if (hasteTurns > 0) hasteTurns--;
}

int Player::effectiveFovRadius() const {
    return (blindTurns > 0) ? 2 : 8;
}

std::string Player::useItem(int index) {
    if (index < 0 || index >= inventory.size()) return "";
    const Item& item = inventory.get(index);

    switch (item.type) {
        case ItemType::HealthPotion: {
            int heal = item.value;
            if (playerClass == PlayerClass::Cleric) heal = heal * 3 / 2;
            hp = std::min(hp + heal, maxHp);
            std::string msg = "Used " + item.name + ", healed " + std::to_string(heal) + " HP.";
            inventory.remove(index);
            return msg;
        }
        case ItemType::AttackBoost: {
            baseAttack += item.value;
            std::string msg = "Used " + item.name + ", ATK +" + std::to_string(item.value) + "!";
            inventory.remove(index);
            return msg;
        }
        case ItemType::DefenseBoost: {
            baseDefense += item.value;
            std::string msg = "Used " + item.name + ", DEF +" + std::to_string(item.value) + "!";
            inventory.remove(index);
            return msg;
        }
        case ItemType::Weapon:
            return equipItem(index);
        case ItemType::Armor:
            return equipItem(index);
        case ItemType::Gold:
            return "";
    }
    return "";
}

std::string Player::equipItem(int index) {
    if (index < 0 || index >= inventory.size()) return "";
    Item item = inventory.get(index);

    if (item.type == ItemType::Weapon) {
        if (hasWeapon) {
            Item old = weaponSlot;
            old.onGround = false;
            inventory.remove(index);
            inventory.add(old);
        } else {
            inventory.remove(index);
        }
        weaponSlot = item;
        hasWeapon = true;
        equippedWeapon = &weaponSlot;
        return "Equipped " + item.name + ".";
    }
    if (item.type == ItemType::Armor) {
        if (hasArmor) {
            Item old = armorSlot;
            old.onGround = false;
            inventory.remove(index);
            inventory.add(old);
        } else {
            inventory.remove(index);
        }
        armorSlot = item;
        hasArmor = true;
        equippedArmor = &armorSlot;
        return "Equipped " + item.name + ".";
    }
    return "";
}

// --- Enemy ---

Enemy::Enemy(Vec2 pos, EnemyType type) : type(type), xpReward(0), awake(false) {
    this->pos = pos;
}

Enemy Enemy::create(EnemyType t, Vec2 pos) {
    Enemy e(pos, t);
    switch (t) {
        case EnemyType::Rat:
            e.name = "Rat"; e.glyph = 'r';
            e.hp = e.maxHp = 8; e.attack = 3; e.defense = 0; e.xpReward = 5;
            break;
        case EnemyType::Skeleton:
            e.name = "Skeleton"; e.glyph = 's';
            e.hp = e.maxHp = 15; e.attack = 6; e.defense = 2; e.xpReward = 12;
            break;
        case EnemyType::Ghost:
            e.name = "Ghost"; e.glyph = 'g';
            e.hp = e.maxHp = 12; e.attack = 8; e.defense = 1; e.xpReward = 15;
            break;
        case EnemyType::Demon:
            e.name = "Demon"; e.glyph = 'D';
            e.hp = e.maxHp = 25; e.attack = 10; e.defense = 4; e.xpReward = 25;
            break;
        case EnemyType::Dragon:
            e.name = "Dragon Boss"; e.glyph = 'W';
            e.hp = e.maxHp = 60; e.attack = 14; e.defense = 6; e.xpReward = 100;
            break;
        case EnemyType::Archer:
            e.name = "Archer"; e.glyph = 'a';
            e.hp = e.maxHp = 10; e.attack = 7; e.defense = 1; e.xpReward = 10;
            break;
        case EnemyType::Necromancer:
            e.name = "Necromancer"; e.glyph = 'n';
            e.hp = e.maxHp = 18; e.attack = 5; e.defense = 2; e.xpReward = 20;
            break;
        case EnemyType::Lich:
            e.name = "Lich Boss"; e.glyph = 'L';
            e.hp = e.maxHp = 80; e.attack = 16; e.defense = 8; e.xpReward = 200;
            break;
    }
    return e;
}

EnemyType Enemy::randomForFloor(int floor) {
    int roll = std::rand() % 100;
    switch (floor) {
        case 1: return EnemyType::Rat;
        case 2: return (roll < 50) ? EnemyType::Rat
                     : (roll < 80) ? EnemyType::Skeleton : EnemyType::Archer;
        case 3: return (roll < 25) ? EnemyType::Rat
                     : (roll < 55) ? EnemyType::Skeleton
                     : (roll < 80) ? EnemyType::Ghost : EnemyType::Archer;
        case 4: return (roll < 15) ? EnemyType::Skeleton
                     : (roll < 45) ? EnemyType::Ghost
                     : (roll < 75) ? EnemyType::Demon : EnemyType::Archer;
        case 5: return (roll < 35) ? EnemyType::Ghost
                     : (roll < 75) ? EnemyType::Demon : EnemyType::Archer;
        case 6: return (roll < 25) ? EnemyType::Demon
                     : (roll < 50) ? EnemyType::Necromancer
                     : (roll < 75) ? EnemyType::Ghost : EnemyType::Archer;
        case 7: return (roll < 30) ? EnemyType::Demon
                     : (roll < 55) ? EnemyType::Necromancer
                     : (roll < 80) ? EnemyType::Ghost : EnemyType::Archer;
        case 8: return (roll < 30) ? EnemyType::Demon
                     : (roll < 55) ? EnemyType::Necromancer
                     : (roll < 80) ? EnemyType::Ghost : EnemyType::Archer;
        default: return EnemyType::Rat;
    }
}

void Enemy::update(const Map& map, Vec2 playerPos) {
    if (!isAlive()) return;

    int dist = pos.distanceSq(playerPos);
    if (dist <= 64) awake = true; // within ~8 tiles

    if (awake) {
        if (shouldFlee() || type == EnemyType::Necromancer) {
            moveAwayFrom(map, playerPos);
        } else if (type == EnemyType::Archer && rangedAttackDamage(map, playerPos) > 0) {
            // Archer holds position when can shoot
        } else {
            moveToward(map, playerPos);
        }
    } else {
        wander(map);
    }
}

bool Enemy::shouldFlee() const {
    return hp * 4 < maxHp; // below 25%
}

void Enemy::moveAwayFrom(const Map& map, Vec2 target) {
    int dx = pos.x - target.x;
    int dy = pos.y - target.y;

    Vec2 next = pos;
    if (std::abs(dx) >= std::abs(dy)) {
        next.x += (dx >= 0) ? 1 : -1;
    } else {
        next.y += (dy >= 0) ? 1 : -1;
    }

    if (map.isWalkable(next.x, next.y)) {
        pos = next;
    } else {
        next = pos;
        if (std::abs(dx) < std::abs(dy)) {
            next.x += (dx >= 0) ? 1 : -1;
        } else {
            next.y += (dy >= 0) ? 1 : -1;
        }
        if (map.isWalkable(next.x, next.y)) {
            pos = next;
        }
    }
}

bool Enemy::hasLineOfSight(const Map& map, Vec2 target) const {
    // Bresenham raycast
    int x0 = pos.x, y0 = pos.y;
    int x1 = target.x, y1 = target.y;
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 == x1 && y0 == y1) return true;
        if (map.isOpaque(x0, y0) && !(x0 == pos.x && y0 == pos.y)) return false;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

int Enemy::rangedAttackDamage(const Map& map, Vec2 target) const {
    if (type != EnemyType::Archer) return 0;
    int dist = pos.distanceSq(target);
    if (dist <= 2) return 0; // adjacent — melee instead
    if (dist > 36) return 0; // beyond 6 tiles
    if (!hasLineOfSight(map, target)) return 0;
    return attack;
}

Vec2 Enemy::astarStep(const Map& map, Vec2 target) const {
    // A* with Manhattan heuristic, 50-node expansion limit
    struct Node {
        int x, y, g, f;
        int px, py; // parent
    };

    bool closed[Map::HEIGHT][Map::WIDTH] = {};
    // open list (simple array, find min f each step)
    Node open[64];
    int openCount = 0;
    int expanded = 0;

    // Parent tracking
    int parentX[Map::HEIGHT][Map::WIDTH];
    int parentY[Map::HEIGHT][Map::WIDTH];
    int gScore[Map::HEIGHT][Map::WIDTH];
    for (int yy = 0; yy < Map::HEIGHT; yy++)
        for (int xx = 0; xx < Map::WIDTH; xx++)
            gScore[yy][xx] = 99999;

    auto manhattan = [](int x1, int y1, int x2, int y2) {
        return std::abs(x1 - x2) + std::abs(y1 - y2);
    };

    gScore[pos.y][pos.x] = 0;
    open[openCount++] = {pos.x, pos.y, 0, manhattan(pos.x, pos.y, target.x, target.y), pos.x, pos.y};

    static const int dx[] = {0, 0, -1, 1};
    static const int dy[] = {-1, 1, 0, 0};

    while (openCount > 0 && expanded < 50) {
        // Find node with lowest f
        int bestIdx = 0;
        for (int i = 1; i < openCount; i++) {
            if (open[i].f < open[bestIdx].f) bestIdx = i;
        }
        Node cur = open[bestIdx];
        open[bestIdx] = open[--openCount];

        if (closed[cur.y][cur.x]) continue;
        closed[cur.y][cur.x] = true;
        parentX[cur.y][cur.x] = cur.px;
        parentY[cur.y][cur.x] = cur.py;
        expanded++;

        if (cur.x == target.x && cur.y == target.y) {
            // Trace back to first step
            int tx = target.x, ty = target.y;
            while (parentX[ty][tx] != pos.x || parentY[ty][tx] != pos.y) {
                int px = parentX[ty][tx];
                int py = parentY[ty][tx];
                tx = px; ty = py;
            }
            return {tx, ty};
        }

        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (!map.inBounds(nx, ny)) continue;
            if (closed[ny][nx]) continue;

            bool walkable = map.isWalkable(nx, ny);
            bool isWall = (map.getTile(nx, ny) == Tile::Wall);

            // Ghosts can walk through walls with a penalty
            if (type == EnemyType::Ghost) {
                if (!walkable && !isWall) continue; // non-wall non-walkable
                int moveCost = isWall ? 6 : 1; // +5 penalty for walls
                int ng = cur.g + moveCost;
                if (ng < gScore[ny][nx]) {
                    gScore[ny][nx] = ng;
                    int f = ng + manhattan(nx, ny, target.x, target.y);
                    if (openCount < 64) {
                        open[openCount++] = {nx, ny, ng, f, cur.x, cur.y};
                    }
                }
            } else {
                if (!walkable) continue;
                int ng = cur.g + 1;
                if (ng < gScore[ny][nx]) {
                    gScore[ny][nx] = ng;
                    int f = ng + manhattan(nx, ny, target.x, target.y);
                    if (openCount < 64) {
                        open[openCount++] = {nx, ny, ng, f, cur.x, cur.y};
                    }
                }
            }
        }
    }

    return {-1, -1}; // no path found
}

void Enemy::moveToward(const Map& map, Vec2 target) {
    // Try A* first
    Vec2 step = astarStep(map, target);
    if (step.x >= 0) {
        bool canMove = (type == EnemyType::Ghost) ? map.inBounds(step.x, step.y) : map.isWalkable(step.x, step.y);
        if (canMove) {
            pos = step;
            return;
        }
    }

    // Fall back to greedy pathfinding
    int dx = target.x - pos.x;
    int dy = target.y - pos.y;

    Vec2 next = pos;
    if (std::abs(dx) >= std::abs(dy)) {
        next.x += (dx > 0) ? 1 : -1;
    } else {
        next.y += (dy > 0) ? 1 : -1;
    }

    if (map.isWalkable(next.x, next.y)) {
        pos = next;
    } else {
        next = pos;
        if (std::abs(dx) < std::abs(dy)) {
            if (dx != 0) next.x += (dx > 0) ? 1 : -1;
        } else {
            if (dy != 0) next.y += (dy > 0) ? 1 : -1;
        }
        if (map.isWalkable(next.x, next.y)) {
            pos = next;
        }
    }
}

void Enemy::wander(const Map& map) {
    if (std::rand() % 3 != 0) return; // only move 1/3 of turns

    static const Vec2 dirs[] = {{0,-1},{0,1},{-1,0},{1,0}};
    Vec2 d = dirs[std::rand() % 4];
    Vec2 next = pos + d;
    bool canMove = (type == EnemyType::Ghost) ? map.inBounds(next.x, next.y) : map.isWalkable(next.x, next.y);
    if (canMove) {
        pos = next;
    }
}
