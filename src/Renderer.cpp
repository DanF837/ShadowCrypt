#include "Renderer.h"
#include "Item.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
static void enableANSI() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
static bool ansiEnabled = (enableANSI(), true);
#endif

void Renderer::clearScreen() {
    buffer.clear();
    buffer += "\033[2J\033[H";
}

void Renderer::moveCursor(int x, int y) {
    buffer += "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

void Renderer::setColor(int fg, int bg) {
    buffer += "\033[";
    if (bg >= 0) {
        buffer += std::to_string(40 + bg) + ";";
    }
    buffer += std::to_string(30 + fg) + "m";
}

void Renderer::resetColor() {
    buffer += "\033[0m";
}

char Renderer::tileChar(Tile t, Biome biome) const {
    switch (t) {
        case Tile::Wall:
            if (biome == Biome::Cave) return '%';
            return '#';
        case Tile::Floor:
            if (biome == Biome::Cave) return ',';
            return '.';
        case Tile::StairsDown: return '>';
        case Tile::StairsUp:   return '<';
        case Tile::Water:      return '~';
        case Tile::Lava:       return '~';
        case Tile::Fountain:   return '*';
    }
    return '?';
}

void Renderer::getTileColor(Tile t, bool visible, int& fg, Biome biome) const {
    if (!visible) {
        fg = 0;
        return;
    }
    switch (t) {
        case Tile::Wall:
            if (biome == Biome::Inferno) fg = 1; // red
            else fg = 7;
            break;
        case Tile::Floor:
            if (biome == Biome::Cave) fg = 3; // yellow
            else fg = 7;
            break;
        case Tile::StairsDown: fg = 3; break;
        case Tile::StairsUp:   fg = 3; break;
        case Tile::Water:      fg = 4; break;
        case Tile::Lava:       fg = 1; break;
        case Tile::Fountain:   fg = 6; break;
    }
}

void Renderer::render(const Map& map, const Player& player,
                      const std::vector<Enemy>& enemies,
                      const std::vector<Item>& items,
                      const std::vector<Trap>& traps,
                      const std::vector<std::string>& log,
                      int floor, Biome biome,
                      Vec2 merchantPos) {
    clearScreen();

    // Draw map
    for (int y = 0; y < Map::HEIGHT; y++) {
        moveCursor(0, y);
        for (int x = 0; x < Map::WIDTH; x++) {
            if (!map.isExplored(x, y)) {
                buffer += ' ';
                continue;
            }
            bool vis = map.isVisible(x, y);
            Tile t = map.getTile(x, y);
            int fg = 7;
            getTileColor(t, vis, fg, biome);

            if (!vis) {
                buffer += "\033[90m";
            } else {
                setColor(fg);
            }
            buffer += tileChar(t, biome);
            resetColor();
        }
    }

    // Draw items (only visible)
    for (auto& item : items) {
        if (!item.onGround) continue;
        if (!map.isVisible(item.pos.x, item.pos.y)) continue;
        moveCursor(item.pos.x, item.pos.y);
        switch (item.type) {
            case ItemType::HealthPotion: setColor(1); break;
            case ItemType::AttackBoost:  setColor(6); break;
            case ItemType::DefenseBoost: setColor(6); break;
            case ItemType::Weapon:       setColor(3); break;
            case ItemType::Armor:        setColor(2); break;
            case ItemType::Gold:         setColor(3); break;
        }
        buffer += item.glyph;
        resetColor();
    }

    // Draw revealed traps (only visible)
    for (auto& trap : traps) {
        if (!trap.revealed) continue;
        if (!map.isVisible(trap.pos.x, trap.pos.y)) continue;
        moveCursor(trap.pos.x, trap.pos.y);
        setColor(5);
        buffer += '^';
        resetColor();
    }

    // Draw merchant
    if (merchantPos.x >= 0 && map.isVisible(merchantPos.x, merchantPos.y)) {
        moveCursor(merchantPos.x, merchantPos.y);
        buffer += "\033[1;33m"; // bold yellow
        buffer += 'M';
        resetColor();
    }

    // Draw enemies (only visible)
    for (auto& e : enemies) {
        if (!e.isAlive()) continue;
        if (!map.isVisible(e.pos.x, e.pos.y)) continue;
        moveCursor(e.pos.x, e.pos.y);
        switch (e.type) {
            case EnemyType::Rat:         setColor(2); break;
            case EnemyType::Skeleton:    setColor(7); break;
            case EnemyType::Ghost:       setColor(5); break;
            case EnemyType::Demon:       setColor(1); break;
            case EnemyType::Dragon:      setColor(1); break;
            case EnemyType::Archer:      setColor(3); break;
            case EnemyType::Necromancer: buffer += "\033[35m"; break; // magenta
            case EnemyType::Lich:        buffer += "\033[1;35m"; break; // bold magenta
        }
        buffer += e.glyph;
        resetColor();
    }

    // Draw player
    moveCursor(player.pos.x, player.pos.y);
    buffer += "\033[1;33m";
    buffer += '@';
    resetColor();

    // HUD area
    int hudY = Map::HEIGHT;

    // HP bar
    moveCursor(0, hudY);
    buffer += "\033[1;37m";
    buffer += "HP: ";
    int barLen = 20;
    int filled = (player.hp * barLen) / std::max(1, player.maxHp);
    buffer += "\033[41m";
    for (int i = 0; i < filled; i++) buffer += ' ';
    buffer += "\033[40m";
    for (int i = filled; i < barLen; i++) buffer += ' ';
    resetColor();
    buffer += " " + std::to_string(player.hp) + "/" + std::to_string(player.maxHp);

    // Stats line
    moveCursor(0, hudY + 1);
    buffer += "\033[1;37m";
    buffer += "ATK:" + std::to_string(player.totalAttack()) +
              " DEF:" + std::to_string(player.totalDefense()) +
              " LVL:" + std::to_string(player.level) +
              " XP:" + std::to_string(player.xp) + "/" + std::to_string(player.xpToNextLevel()) +
              " Floor:" + std::to_string(floor) +
              " Gold:" + std::to_string(player.gold);
    if (player.poisonTurns > 0) {
        buffer += " \033[32m[POISON:" + std::to_string(player.poisonTurns) + "]\033[1;37m";
    }
    if (player.burningTurns > 0) {
        buffer += " \033[31m[BURN:" + std::to_string(player.burningTurns) + "]\033[1;37m";
    }
    if (player.blindTurns > 0) {
        buffer += " \033[35m[BLIND:" + std::to_string(player.blindTurns) + "]\033[1;37m";
    }
    if (player.slowTurns > 0) {
        buffer += " \033[36m[SLOW:" + std::to_string(player.slowTurns) + "]\033[1;37m";
    }
    if (player.hasteTurns > 0) {
        buffer += " \033[33m[HASTE:" + std::to_string(player.hasteTurns) + "]\033[1;37m";
    }
    resetColor();

    // Ability status + controls
    moveCursor(0, hudY + 2);
    buffer += "\033[90m";
    buffer += "[R] " + player.abilityName();
    if (player.abilityBuffActive) {
        buffer += " \033[1;32m[READY]\033[90m";
    } else if (player.abilityCooldown > 0) {
        buffer += " (" + std::to_string(player.abilityCooldown) + " turns)";
    } else {
        buffer += " \033[33m(Ready)\033[90m";
    }
    buffer += "  [WASD]Move [E]Pick up [I]Inv [>]Stairs [X]Look [Z]Explore [F]Save [Q]Quit";
    resetColor();

    // Message log (last 5 messages)
    int logStart = hudY + 3;
    int logCount = std::min((int)log.size(), 5);
    for (int i = 0; i < logCount; i++) {
        moveCursor(0, logStart + i);
        if (i == 0) {
            buffer += "\033[1;37m";
        } else {
            buffer += "\033[90m";
        }
        std::string msg = log[log.size() - logCount + i];
        if ((int)msg.size() > Map::WIDTH) msg = msg.substr(0, Map::WIDTH);
        buffer += msg;
        resetColor();
    }

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderWithCursor(const Map& map, const Player& player,
                const std::vector<Enemy>& enemies,
                const std::vector<Item>& items,
                const std::vector<Trap>& traps,
                const std::vector<std::string>& log,
                int floor, Vec2 cursor, const std::string& desc,
                Biome biome, Vec2 merchantPos) {
    render(map, player, enemies, items, traps, log, floor, biome, merchantPos);

    // Overlay cursor with reverse video
    moveCursor(cursor.x, cursor.y);
    buffer += "\033[7m"; // reverse video

    // Show what's at cursor position
    bool found = false;
    for (auto& e : enemies) {
        if (e.isAlive() && e.pos == cursor && map.isVisible(cursor.x, cursor.y)) {
            buffer += e.glyph; found = true; break;
        }
    }
    if (!found && cursor == player.pos) {
        buffer += '@'; found = true;
    }
    if (!found) {
        for (auto& item : items) {
            if (item.onGround && item.pos == cursor && map.isVisible(cursor.x, cursor.y)) {
                buffer += item.glyph; found = true; break;
            }
        }
    }
    if (!found && merchantPos.x >= 0 && cursor.x == merchantPos.x && cursor.y == merchantPos.y
        && map.isVisible(cursor.x, cursor.y)) {
        buffer += 'M'; found = true;
    }
    if (!found) {
        if (map.isExplored(cursor.x, cursor.y)) {
            buffer += tileChar(map.getTile(cursor.x, cursor.y), biome);
        } else {
            buffer += ' ';
        }
    }
    resetColor();

    // Show description on HUD
    int hudY = Map::HEIGHT;
    moveCursor(0, hudY + 2);
    buffer += "\033[K"; // clear line
    buffer += "\033[1;36m";
    buffer += "[LOOK] " + desc + " (ESC/X=exit)";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderTitle(const std::vector<std::string>& topScores, bool hasSaveFile) {
    clearScreen();

    moveCursor(20, 2);
    buffer += "\033[1;31m";
    buffer += R"( _____  _               _                              _   )";
    moveCursor(20, 3);
    buffer += R"(/ ____|| |             | |                            | |  )";
    moveCursor(20, 4);
    buffer += R"(| (___ | |__   __ _  __| | _____      _____ _ __ _   _| |_ )";
    moveCursor(20, 5);
    buffer += R"( \___ \| '_ \ / _` |/ _` |/ _ \ \ /\ / / __|  __| | | | __|)";
    moveCursor(20, 6);
    buffer += R"( ____) | | | | (_| | (_| | (_) \ V  V / (__| |  | |_| | |_ )";
    moveCursor(20, 7);
    buffer += R"(|_____/|_| |_|\__,_|\__,_|\___/ \_/\_/ \___|_|   \__, |\__|)";
    moveCursor(20, 8);
    buffer += R"(                                                  __/ |    )";
    moveCursor(20, 9);
    buffer += R"(                                                 |___/     )";
    resetColor();

    moveCursor(30, 11);
    buffer += "\033[1;33m";
    buffer += "A Roguelike Dungeon Crawler";
    resetColor();

    if (!topScores.empty()) {
        moveCursor(30, 13);
        buffer += "\033[1;37m";
        buffer += "--- HIGH SCORES ---";
        resetColor();
        for (int i = 0; i < (int)topScores.size(); i++) {
            moveCursor(28, 14 + i);
            buffer += "\033[33m";
            buffer += topScores[i];
            resetColor();
        }
    }

    int menuY = topScores.empty() ? 15 : 18;
    moveCursor(22, menuY);
    buffer += "\033[1;37m";
    if (hasSaveFile) {
        buffer += "[ENTER] New Game  [L] Continue  [H] Help  [Q] Quit";
    } else {
        buffer += "[ENTER] Play   [H] How to Play   [Q] Quit";
    }
    resetColor();

    moveCursor(25, menuY + 2);
    buffer += "\033[90m";
    buffer += "Descend 8 floors. Slay the Lich.";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderGameOver(int floor, int level) {
    clearScreen();

    moveCursor(30, 8);
    buffer += "\033[1;31m";
    buffer += "=== YOU HAVE DIED ===";
    resetColor();

    moveCursor(28, 11);
    buffer += "\033[37m";
    buffer += "Reached Floor " + std::to_string(floor) + ", Level " + std::to_string(level);
    resetColor();

    moveCursor(25, 14);
    buffer += "\033[90m";
    buffer += "The dungeon claims another soul...";
    resetColor();

    moveCursor(24, 17);
    buffer += "\033[1;37m";
    buffer += "Press [R] to retry or [Q] to quit";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderWin(int level) {
    clearScreen();

    moveCursor(28, 8);
    buffer += "\033[1;33m";
    buffer += "=== VICTORY! ===";
    resetColor();

    moveCursor(22, 11);
    buffer += "\033[1;37m";
    buffer += "You have slain the Lich and conquered the dungeon!";
    resetColor();

    moveCursor(28, 13);
    buffer += "\033[37m";
    buffer += "Final Level: " + std::to_string(level);
    resetColor();

    moveCursor(20, 16);
    buffer += "\033[33m";
    buffer += "The dungeon trembles as light returns...";
    resetColor();

    moveCursor(24, 19);
    buffer += "\033[1;37m";
    buffer += "Press [R] to play again or [Q] to quit";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderHelp() {
    clearScreen();

    moveCursor(28, 1);
    buffer += "\033[1;33m";
    buffer += "=== HOW TO PLAY ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Movement:";
    resetColor();
    moveCursor(7, 4);
    buffer += "\033[37m";
    buffer += "WASD or Arrow Keys to move";
    resetColor();

    moveCursor(5, 6);
    buffer += "\033[1;37m";
    buffer += "Actions:";
    resetColor();
    moveCursor(7, 7);
    buffer += "\033[37m";
    buffer += "E = Pick up/Trade    I = Open inventory    R = Use ability";
    moveCursor(7, 8);
    buffer += "> or . = Descend stairs    X = Examine    Z = Auto-explore    Q = Quit";
    resetColor();

    moveCursor(5, 10);
    buffer += "\033[1;37m";
    buffer += "Inventory:";
    resetColor();
    moveCursor(7, 11);
    buffer += "\033[37m";
    buffer += "1-9 to use/equip item    ESC or I to close";
    resetColor();

    moveCursor(5, 13);
    buffer += "\033[1;37m";
    buffer += "Combat:";
    resetColor();
    moveCursor(7, 14);
    buffer += "\033[37m";
    buffer += "Bump into enemies to attack. They strike back when adjacent.";
    resetColor();

    moveCursor(5, 16);
    buffer += "\033[1;37m";
    buffer += "Goal:";
    resetColor();
    moveCursor(7, 17);
    buffer += "\033[37m";
    buffer += "Descend 8 floors and defeat the Lich!";
    resetColor();

    moveCursor(5, 19);
    buffer += "\033[1;37m";
    buffer += "Symbols:";
    resetColor();
    moveCursor(7, 20);
    buffer += "\033[1;33m@\033[37m You   ";
    buffer += "\033[32mr\033[37m Rat   ";
    buffer += "\033[37ms\033[37m Skeleton   ";
    buffer += "\033[35mg\033[37m Ghost   ";
    buffer += "\033[31mD\033[37m Demon/Dragon   ";
    buffer += "\033[33ma\033[37m Archer";
    moveCursor(7, 21);
    buffer += "\033[35mn\033[37m Necromancer   ";
    buffer += "\033[1;35mL\033[37m Lich   ";
    buffer += "\033[1;33mM\033[37m Merchant   ";
    buffer += "\033[33m$\033[37m Gold";
    moveCursor(7, 22);
    buffer += "\033[31m!\033[37m Potion   ";
    buffer += "\033[36m?\033[37m Scroll   ";
    buffer += "\033[33m/\033[37m Weapon   ";
    buffer += "\033[32m[\033[37m Armor   ";
    buffer += "\033[33m>\033[37m Stairs";
    resetColor();

    moveCursor(24, 24);
    buffer += "\033[90m";
    buffer += "Press any key to return...";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderClassSelect() {
    clearScreen();

    moveCursor(26, 1);
    buffer += "\033[1;33m";
    buffer += "=== CHOOSE YOUR CLASS ===";
    resetColor();

    moveCursor(5, 4);
    buffer += "\033[1;31m";
    buffer += "[1] Warrior";
    resetColor();
    moveCursor(9, 5);
    buffer += "\033[37m";
    buffer += "HP: 40  ATK: 4  DEF: 4";
    moveCursor(9, 6);
    buffer += "Ability: Shield Bash - Bonus damage + stun enemy (8 turn CD)";
    resetColor();

    moveCursor(5, 8);
    buffer += "\033[1;32m";
    buffer += "[2] Rogue";
    resetColor();
    moveCursor(9, 9);
    buffer += "\033[37m";
    buffer += "HP: 25  ATK: 7  DEF: 1";
    moveCursor(9, 10);
    buffer += "Ability: Backstab - Next attack deals 3x damage (6 turn CD)";
    resetColor();

    moveCursor(5, 12);
    buffer += "\033[1;34m";
    buffer += "[3] Mage";
    resetColor();
    moveCursor(9, 13);
    buffer += "\033[37m";
    buffer += "HP: 20  ATK: 8  DEF: 1";
    moveCursor(9, 14);
    buffer += "Ability: Fireball - AoE damage to nearby enemies (10 turn CD)";
    resetColor();

    moveCursor(5, 16);
    buffer += "\033[1;33m";
    buffer += "[4] Cleric";
    resetColor();
    moveCursor(9, 17);
    buffer += "\033[37m";
    buffer += "HP: 35  ATK: 3  DEF: 3";
    moveCursor(9, 18);
    buffer += "Ability: Divine Heal - Restore 50% HP (12 turn CD)";
    moveCursor(9, 19);
    buffer += "Passive: Potions heal 50% more";
    resetColor();

    moveCursor(22, 22);
    buffer += "\033[90m";
    buffer += "Press [1-4] to select, [Q] to go back";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderInventory(const Player& player) {
    clearScreen();

    moveCursor(5, 1);
    buffer += "\033[1;33m";
    buffer += "=== INVENTORY ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Equipped:";
    resetColor();

    moveCursor(7, 4);
    buffer += "\033[37m";
    if (player.equippedWeapon) {
        buffer += "Weapon: " + player.equippedWeapon->name +
                  " (+" + std::to_string(player.equippedWeapon->value) + " ATK)";
    } else {
        buffer += "Weapon: (none)";
    }

    moveCursor(7, 5);
    if (player.equippedArmor) {
        buffer += "Armor:  " + player.equippedArmor->name +
                  " (+" + std::to_string(player.equippedArmor->value) + " DEF)";
    } else {
        buffer += "Armor:  (none)";
    }
    resetColor();

    moveCursor(5, 7);
    buffer += "\033[1;37m";
    buffer += "Items (" + std::to_string(player.inventory.size()) + "/" +
              std::to_string(Inventory::MAX_ITEMS) + "):";
    resetColor();

    if (player.inventory.size() == 0) {
        moveCursor(7, 8);
        buffer += "\033[90m";
        buffer += "(empty)";
        resetColor();
    } else {
        for (int i = 0; i < player.inventory.size(); i++) {
            moveCursor(7, 8 + i);
            const Item& item = player.inventory.get(i);
            buffer += "\033[37m";
            buffer += "[" + std::to_string(i + 1) + "] " + item.description();
            resetColor();
        }
    }

    moveCursor(5, 20);
    buffer += "\033[90m";
    buffer += "Press [1-9] to use/equip item, [ESC/I] to close";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderShop(const std::vector<ShopItem>& shopItems, int playerGold) {
    clearScreen();

    moveCursor(5, 1);
    buffer += "\033[1;33m";
    buffer += "=== MERCHANT'S WARES ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Your Gold: " + std::to_string(playerGold);
    resetColor();

    for (int i = 0; i < (int)shopItems.size(); i++) {
        moveCursor(7, 5 + i);
        if (shopItems[i].sold) {
            buffer += "\033[90m";
            buffer += "[" + std::to_string(i + 1) + "] (SOLD)";
        } else {
            buffer += "\033[37m";
            buffer += "[" + std::to_string(i + 1) + "] " +
                      shopItems[i].item.description() +
                      " - \033[33m" + std::to_string(shopItems[i].price) + " gold\033[37m";
        }
        resetColor();
    }

    moveCursor(5, 18);
    buffer += "\033[90m";
    buffer += "Press [1-5] to buy, [ESC] to leave";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}
