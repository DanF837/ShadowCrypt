#include "Game.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static std::string resolveCombat(Entity& attacker, Entity& defender, int atkBonus = 0, int defBonus = 0, int* outDmg = nullptr) {
    bool crit = (std::rand() % 10 == 0); // 10% critical hit chance
    int atk = attacker.attack + atkBonus;
    int def = defender.defense + defBonus;
    int variance = (std::rand() % 5) - 2; // -2 to +2
    int damage = std::max(1, atk - def + variance);
    if (crit) damage *= 2;

    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    if (outDmg) *outDmg = damage;

    std::string msg = attacker.name;
    if (crit) msg += " CRITICALLY";
    msg += " hits " + defender.name +
           " for " + std::to_string(damage) + " damage.";
    if (crit) msg += " Critical hit!";
    if (!defender.isAlive()) {
        msg += " " + defender.name + " dies!";
    }
    return msg;
}

void Game::run() {
    std::srand((unsigned)std::time(nullptr));
    loadHighScores();
    state = GameState::Menu;
    running = true;

    while (running) {
        switch (state) {
            case GameState::Menu:             handleMenu(); break;
            case GameState::DifficultySelect: handleDifficultySelect(); break;
            case GameState::ClassSelect:      handleClassSelect(); break;
            case GameState::Playing:          handlePlaying(); break;
            case GameState::Inventory:        handleInventory(); break;
            case GameState::Looking:          handleLooking(); break;
            case GameState::LevelUp:          handleLevelUp(); break;
            case GameState::MessageLog:       handleMessageLog(); break;
            case GameState::GameOver:         handleGameOver(); break;
            case GameState::Win:              handleWin(); break;
        }
    }

    // Clear screen on exit
    renderer.clearScreen();
    std::fputs("\033[2J\033[H", stdout);
    std::fflush(stdout);
}

void Game::newGame(PlayerClass cls) {
    currentFloor = 1;
    player = Player(cls);
    player.damageDealt = 0;
    player.damageTaken = 0;
    player.potionsUsed = 0;
    player.turnsPlayed = 0;
    player.lastDamageSource = "the dungeon";
    messageLog.clear();
    scoreRecorded = false;
    addMessage("You enter the dungeon as a " + std::string(
        cls == PlayerClass::Warrior ? "Warrior" :
        cls == PlayerClass::Rogue   ? "Rogue" :
        cls == PlayerClass::Mage    ? "Mage" : "Cleric") + "...");
    generateFloor();
}

void Game::generateFloor() {
    enemies.clear();
    items.clear();
    traps.clear();
    dunGen.generate(map, currentFloor);

    // Place player in first room
    auto& rooms = dunGen.getRooms();
    if (!rooms.empty()) {
        Vec2 start = rooms.front().center();
        // If there are stairs up, place player there
        if (currentFloor > 1) {
            Vec2 up = map.stairsUpPos();
            if (up.x >= 0) start = up;
        }
        player.init(start);
    }

    spawnEnemies();
    spawnItems();
    spawnTraps();

    // Generate shop if there's a Shop room
    merchantPos = {-1, -1};
    shopInventory.clear();
    shopActive = false;
    for (auto& r : dunGen.getRooms()) {
        if (r.theme == RoomTheme::Shop) {
            generateShop(r);
            break;
        }
    }

    if (currentFloor == 5) {
        spawnBoss();
    }
    if (currentFloor == 8) {
        spawnBoss8();
    }

    FOV::compute(map, player.pos, player.effectiveFovRadius());
    addMessage("Floor " + std::to_string(currentFloor) + ".");
}

void Game::applyDifficulty(Enemy& e) {
    if (difficulty == Difficulty::Easy) {
        e.hp = e.maxHp = std::max(1, e.maxHp * 3 / 4);
        e.attack = std::max(1, e.attack * 3 / 4);
    } else if (difficulty == Difficulty::Hard) {
        e.hp = e.maxHp = e.maxHp * 3 / 2;
        e.attack = e.attack * 5 / 4;
    }
}

void Game::spawnEnemies() {
    auto& rooms = dunGen.getRooms();
    int count = 3 + currentFloor * 2;
    if (difficulty == Difficulty::Hard) count += 2;

    for (int i = 0; i < count && rooms.size() > 1; i++) {
        // Pick a random room (not the first one where player spawns)
        int ri = 1 + std::rand() % ((int)rooms.size() - 1);
        const Room& r = rooms[ri];
        int ex = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int ey = r.y + 1 + std::rand() % std::max(1, r.h - 2);

        if (map.isWalkable(ex, ey)) {
            EnemyType et;
            if (rooms[ri].theme == RoomTheme::Crypt) {
                et = (std::rand() % 2 == 0) ? EnemyType::Skeleton : EnemyType::Ghost;
            } else {
                et = Enemy::randomForFloor(currentFloor);
            }
            enemies.push_back(Enemy::create(et, {ex, ey}));
            applyDifficulty(enemies.back());
        }
    }
}

void Game::spawnItems() {
    auto& rooms = dunGen.getRooms();
    int count = 2 + std::rand() % 3;

    struct ItemTemplate { std::string name; char glyph; ItemType type; int value; Enchantment ench = Enchantment::None; };
    std::vector<ItemTemplate> pool = {
        {"Health Potion",  '!', ItemType::HealthPotion, 10 + currentFloor * 2},
        {"Health Potion",  '!', ItemType::HealthPotion, 10 + currentFloor * 2},
        {"Attack Scroll",  '?', ItemType::AttackBoost,  1},
        {"Defense Scroll", '?', ItemType::DefenseBoost, 1},
        {"Teleport Scroll",'?', ItemType::TeleportScroll, 0},
        {"Bomb",           'o', ItemType::Bomb, 10 + currentFloor * 2},
        {"Shield Potion",  '!', ItemType::ShieldPotion, 3 + currentFloor},
    };

    // Floor-scaled weapons and armor
    std::vector<std::string> weaponNames = {"Dagger", "Short Sword", "Long Sword", "War Axe", "Flame Blade"};
    std::vector<std::string> armorNames  = {"Leather Armor", "Chain Mail", "Scale Mail", "Plate Armor", "Dragon Armor"};

    if (std::rand() % 3 == 0) {
        int tier = std::min(currentFloor - 1, 4);
        Enchantment ench = (currentFloor >= 3 && std::rand() % 3 == 0) ? Item::rollEnchantment(currentFloor) : Enchantment::None;
        pool.push_back({weaponNames[tier], '/', ItemType::Weapon, 3 + tier * 2});
        pool.back().ench = ench;
    }
    if (std::rand() % 3 == 0) {
        int tier = std::min(currentFloor - 1, 4);
        Enchantment ench = (currentFloor >= 3 && std::rand() % 3 == 0) ? Item::rollEnchantment(currentFloor) : Enchantment::None;
        pool.push_back({armorNames[tier],  '[', ItemType::Armor, 2 + tier * 2});
        pool.back().ench = ench;
    }

    for (int i = 0; i < count && !pool.empty(); i++) {
        int ri = 1 + std::rand() % std::max(1, (int)rooms.size() - 1);
        const Room& r = rooms[ri];
        int ix = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int iy = r.y + 1 + std::rand() % std::max(1, r.h - 2);

        if (map.isWalkable(ix, iy)) {
            int pi = std::rand() % (int)pool.size();
            auto& t = pool[pi];
            items.emplace_back(Vec2{ix, iy}, t.name, t.glyph, t.type, t.value, t.ench);
        }
    }

    // Themed room spawns
    for (size_t ri = 1; ri < rooms.size(); ri++) {
        const Room& r = rooms[ri];
        int ix = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int iy = r.y + 1 + std::rand() % std::max(1, r.h - 2);
        if (!map.isWalkable(ix, iy)) continue;
        if (r.theme == RoomTheme::Armory) {
            int tier = std::min(currentFloor - 1, 4);
            if (std::rand() % 2 == 0) {
                items.emplace_back(Vec2{ix, iy}, weaponNames[tier], '/', ItemType::Weapon, 3 + tier * 2);
            } else {
                items.emplace_back(Vec2{ix, iy}, armorNames[tier], '[', ItemType::Armor, 2 + tier * 2);
            }
        } else if (r.theme == RoomTheme::Library) {
            if (std::rand() % 2 == 0) {
                items.emplace_back(Vec2{ix, iy}, "Attack Scroll", '?', ItemType::AttackBoost, 1);
            } else {
                items.emplace_back(Vec2{ix, iy}, "Defense Scroll", '?', ItemType::DefenseBoost, 1);
            }
        }
    }
}

void Game::spawnBoss() {
    auto& rooms = dunGen.getRooms();
    if (rooms.size() < 2) return;
    const Room& r = rooms.back();
    Vec2 bossPos = r.center();
    // Offset so boss doesn't overlap stairs
    bossPos.x += 1;
    if (!map.isWalkable(bossPos.x, bossPos.y)) bossPos.x -= 2;
    enemies.push_back(Enemy::create(EnemyType::Dragon, bossPos));
    applyDifficulty(enemies.back());
    addMessage("You sense a powerful presence on this floor...");
}

void Game::spawnBoss8() {
    auto& rooms = dunGen.getRooms();
    if (rooms.size() < 2) return;
    const Room& r = rooms.back();
    Vec2 bossPos = r.center();
    bossPos.x += 1;
    if (!map.isWalkable(bossPos.x, bossPos.y)) bossPos.x -= 2;
    enemies.push_back(Enemy::create(EnemyType::Lich, bossPos));
    applyDifficulty(enemies.back());
    addMessage("An ancient evil stirs... The Lich awaits!");
}

void Game::generateShop(const Room& room) {
    merchantPos = room.center();
    shopInventory.clear();
    shopActive = true;

    int numItems = 3 + std::rand() % 3; // 3-5 items
    std::vector<std::string> weaponNames = {"Dagger", "Short Sword", "Long Sword", "War Axe", "Flame Blade"};
    std::vector<std::string> armorNames = {"Leather Armor", "Chain Mail", "Scale Mail", "Plate Armor", "Dragon Armor"};

    for (int i = 0; i < numItems; i++) {
        ShopItem si;
        int roll = std::rand() % 6;
        int tier = std::min(currentFloor - 1, 4);
        if (roll == 0) {
            si.item = Item({0,0}, "Health Potion", '!', ItemType::HealthPotion, 10 + currentFloor * 3);
            si.price = 15 + currentFloor * 5;
        } else if (roll == 1) {
            si.item = Item({0,0}, (std::rand() % 2 == 0) ? "Attack Scroll" : "Defense Scroll", '?',
                           (std::rand() % 2 == 0) ? ItemType::AttackBoost : ItemType::DefenseBoost, 1);
            si.price = 30 + currentFloor * 10;
        } else if (roll == 2) {
            Enchantment ench = (currentFloor >= 3) ? Item::rollEnchantment(currentFloor) : Enchantment::None;
            si.item = Item({0,0}, weaponNames[tier], '/', ItemType::Weapon, 3 + tier * 2, ench);
            si.price = 40 + tier * 15;
        } else if (roll == 4) {
            si.item = Item({0,0}, "Bomb", 'o', ItemType::Bomb, 10 + currentFloor * 2);
            si.price = 20 + currentFloor * 5;
        } else {
            si.item = Item({0,0}, "Shield Potion", '!', ItemType::ShieldPotion, 3 + currentFloor);
            si.price = 25 + currentFloor * 5;
        }
        si.sold = false;
        shopInventory.push_back(si);
    }
}

void Game::handleShopInteraction() {
    while (true) {
        renderer.renderShop(shopInventory, player.gold);
        Key key = Input::getKey();
        if (key == Key::Escape) {
            addMessage("You leave the merchant.");
            return;
        }

        // 'S' key maps to Key::Down — use it for sell mode in shop context
        if (key == Key::Down) {
            // Sell sub-loop
            while (true) {
                renderer.renderSellMenu(player);
                Key sellKey = Input::getKey();
                if (sellKey == Key::Escape) break;

                int si = -1;
                switch (sellKey) {
                    case Key::Num1: si = 0; break;
                    case Key::Num2: si = 1; break;
                    case Key::Num3: si = 2; break;
                    case Key::Num4: si = 3; break;
                    case Key::Num5: si = 4; break;
                    case Key::Num6: si = 5; break;
                    case Key::Num7: si = 6; break;
                    case Key::Num8: si = 7; break;
                    case Key::Num9: si = 8; break;
                    default: break;
                }

                if (si >= 0 && si < player.inventory.size()) {
                    const Item& item = player.inventory.get(si);
                    int price = item.sellPrice();
                    if (price <= 0) {
                        addMessage("Can't sell that.");
                    } else {
                        player.gold += price;
                        addMessage("Sold " + item.name + " for " + std::to_string(price) + " gold.");
                        player.inventory.remove(si);
                    }
                }
            }
            continue;
        }

        int idx = -1;
        switch (key) {
            case Key::Num1: idx = 0; break;
            case Key::Num2: idx = 1; break;
            case Key::Num3: idx = 2; break;
            case Key::Num4: idx = 3; break;
            case Key::Num5: idx = 4; break;
            default: break;
        }

        if (idx >= 0 && idx < (int)shopInventory.size()) {
            auto& si = shopInventory[idx];
            if (si.sold) {
                addMessage("Already sold.");
            } else if (player.gold < si.price) {
                addMessage("Not enough gold!");
            } else if (player.inventory.isFull()) {
                addMessage("Inventory full!");
            } else {
                player.gold -= si.price;
                player.inventory.add(si.item);
                si.sold = true;
                addMessage("Bought " + si.item.name + "!");
            }
        }
    }
}

void Game::handleMenu() {
    std::vector<std::string> topScores;
    auto classStr = [](PlayerClass c) -> std::string {
        switch (c) {
            case PlayerClass::Warrior: return "WAR";
            case PlayerClass::Rogue:   return "ROG";
            case PlayerClass::Mage:    return "MAG";
            case PlayerClass::Cleric:  return "CLR";
        }
        return "???";
    };
    auto diffStr = [](Difficulty d) -> std::string {
        switch (d) {
            case Difficulty::Easy: return "E";
            case Difficulty::Normal: return "N";
            case Difficulty::Hard: return "H";
        }
        return "?";
    };
    int showCount = std::min((int)highScores.size(), 3);
    for (int i = 0; i < showCount; i++) {
        auto& s = highScores[i];
        topScores.push_back(std::to_string(i + 1) + ". " +
            std::to_string(s.score) + " pts - " + classStr(s.playerClass) +
            " F" + std::to_string(s.floor) + " L" + std::to_string(s.level) +
            " K" + std::to_string(s.kills) + " [" + diffStr(s.difficulty) + "]");
    }
    // Check for save file
    {
        std::ifstream sf("shadowcrypt.sav");
        saveFileExists = sf.good();
    }
    renderer.renderTitle(topScores, saveFileExists);
    Key key = Input::getKey();
    if (key == Key::Help) {
        renderer.renderHelp();
        Input::getKey();
        return;
    }
    if (key == Key::Quit) {
        running = false;
        return;
    }
    if (key == Key::Load && saveFileExists) {
        if (loadGame()) {
            state = GameState::Playing;
        }
        return;
    }
    state = GameState::DifficultySelect;
}

void Game::handleDifficultySelect() {
    renderer.renderDifficultySelect();
    Key key = Input::getKey();
    switch (key) {
        case Key::Num1: difficulty = Difficulty::Easy; break;
        case Key::Num2: difficulty = Difficulty::Normal; break;
        case Key::Num3: difficulty = Difficulty::Hard; break;
        case Key::Quit: state = GameState::Menu; return;
        default: return;
    }
    state = GameState::ClassSelect;
}

void Game::handleClassSelect() {
    renderer.renderClassSelect();
    Key key = Input::getKey();
    PlayerClass cls;
    switch (key) {
        case Key::Num1: cls = PlayerClass::Warrior; break;
        case Key::Num2: cls = PlayerClass::Rogue;   break;
        case Key::Num3: cls = PlayerClass::Mage;    break;
        case Key::Num4: cls = PlayerClass::Cleric;  break;
        case Key::Quit: state = GameState::Menu; return;
        default: return; // invalid key, re-render
    }
    newGame(cls);
    state = GameState::Playing;
}

void Game::handlePlaying() {
    // Auto-explore loop
    if (autoExploring) {
        if (shouldStopAutoExplore() || Input::keyPending()) {
            autoExploring = false;
            addMessage("Auto-explore stopped.");
        } else {
            Vec2 step = bfsNextStep();
            if (step.x < 0) {
                autoExploring = false;
                addMessage("Nothing left to explore.");
            } else {
                int dx = step.x - player.pos.x;
                int dy = step.y - player.pos.y;
                movePlayer(dx, dy);
                processTurn();
                player.turnsPlayed++;
                if (state != GameState::Playing) return;
                if (player.pendingLevelUps > 0) {
                    autoExploring = false;
                    state = GameState::LevelUp;
                    return;
                }
                renderer.render(map, player, enemies, items, traps, messageLog, currentFloor,
                                dunGen.currentBiome, merchantPos, static_cast<int>(difficulty));
                // Small delay for visual feedback
#ifdef _WIN32
                Sleep(50);
#else
                usleep(50000);
#endif
                return;
            }
        }
    }

    renderer.render(map, player, enemies, items, traps, messageLog, currentFloor,
                    dunGen.currentBiome, merchantPos, static_cast<int>(difficulty));

    Key key = Input::getKey();

    // Slow effect: 50% chance to skip turn
    if (player.slowTurns > 0 && std::rand() % 2 == 0) {
        addMessage("You are slowed!");
        player.tickCooldown();
        player.tickStatusEffects();
        updateEnemies();
        if (!player.isAlive()) { state = GameState::GameOver; return; }
        FOV::compute(map, player.pos, player.effectiveFovRadius());
        return;
    }

    switch (key) {
        case Key::Up:    movePlayer(0, -1); break;
        case Key::Down:  movePlayer(0,  1); break;
        case Key::Left:  movePlayer(-1, 0); break;
        case Key::Right: movePlayer(1,  0); break;
        case Key::Use:   pickUpItem(); break;
        case Key::Inventory: state = GameState::Inventory; return;
        case Key::Look:
            lookCursor = player.pos;
            state = GameState::Looking;
            return;
        case Key::Stairs: descendStairs(); break;
        case Key::Ability: {
            std::string msg = player.useAbility(enemies);
            if (!msg.empty()) addMessage(msg);
            // Check for fireball kills
            for (auto& e : enemies) {
                if (!e.isAlive() && e.xpReward > 0) {
                    player.killCount++;
                    player.addXP(e.xpReward);
                    addMessage("Gained " + std::to_string(e.xpReward) + " XP.");
                    // Drop gold
                    int goldAmt = e.xpReward / 2 + std::rand() % 5;
                    items.emplace_back(e.pos, std::to_string(goldAmt) + " Gold", '$', ItemType::Gold, goldAmt);

                    // Enchanted loot drop
                    bool isBoss = (e.type == EnemyType::Dragon || e.type == EnemyType::Lich);
                    if (isBoss || std::rand() % 10 == 0) {
                        Enchantment ench = Item::rollEnchantment(currentFloor, isBoss);
                        if (std::rand() % 2 == 0) {
                            std::vector<std::string> wNames = {"Dagger", "Short Sword", "Long Sword", "War Axe", "Flame Blade"};
                            int tier = std::min(currentFloor - 1, 4);
                            items.emplace_back(e.pos, wNames[tier], '/', ItemType::Weapon, 3 + tier * 2, ench);
                        } else {
                            std::vector<std::string> aNames = {"Leather Armor", "Chain Mail", "Scale Mail", "Plate Armor", "Dragon Armor"};
                            int tier = std::min(currentFloor - 1, 4);
                            items.emplace_back(e.pos, aNames[tier], '[', ItemType::Armor, 2 + tier * 2, ench);
                        }
                    }

                    e.xpReward = 0;
                    if (e.type == EnemyType::Dragon && currentFloor == 5) {
                        addMessage("The Dragon falls! But darker forces lurk below...");
                    }
                    if (e.type == EnemyType::Lich && currentFloor == 8) {
                        state = GameState::Win;
                    }
                }
            }
            break;
        }
        case Key::Wait:
            addMessage("You wait...");
            break;
        case Key::AutoExplore:
            autoExploring = true;
            return;
        case Key::MessageLog:
            logScrollOffset = 0;
            state = GameState::MessageLog;
            return;
        case Key::Save:
            saveGame();
            addMessage("Game saved.");
            return;
        case Key::Quit:  running = false; return;
        default: return; // don't update enemies on invalid input
    }

    player.turnsPlayed++;
    player.tickCooldown();
    if (player.poisonTurns > 0) {
        player.damageTaken += player.poisonDmg;
        player.lastDamageSource = "poison";
        addMessage("Poison deals " + std::to_string(player.poisonDmg) + " damage! (" + std::to_string(player.poisonTurns) + " turns left)");
    }
    if (player.burningTurns > 0) {
        player.damageTaken += player.burningDmg;
        player.lastDamageSource = "fire";
        addMessage("Burning deals " + std::to_string(player.burningDmg) + " damage! (" + std::to_string(player.burningTurns) + " turns left)");
    }
    player.tickStatusEffects();

    // Update enemies (they move after the player)
    updateEnemies();

    // Check player death
    if (!player.isAlive()) {
        state = GameState::GameOver;
        return;
    }

    // Haste: bonus movement-only action
    if (player.hasteTurns > 0) {
        renderer.render(map, player, enemies, items, traps, messageLog, currentFloor,
                        dunGen.currentBiome, merchantPos, static_cast<int>(difficulty));
        Key hasteKey = Input::getKey();
        switch (hasteKey) {
            case Key::Up:    movePlayer(0, -1); break;
            case Key::Down:  movePlayer(0,  1); break;
            case Key::Left:  movePlayer(-1, 0); break;
            case Key::Right: movePlayer(1,  0); break;
            default: break;
        }
        if (!player.isAlive()) { state = GameState::GameOver; return; }
    }

    // Recompute FOV
    FOV::compute(map, player.pos, player.effectiveFovRadius());

    // Check for pending level ups
    if (player.pendingLevelUps > 0) {
        state = GameState::LevelUp;
    }
}

void Game::handleLevelUp() {
    renderer.renderLevelUp(player);
    Key key = Input::getKey();
    int choice = -1;
    switch (key) {
        case Key::Num1: choice = 0; break;
        case Key::Num2: choice = 1; break;
        case Key::Num3: choice = 2; break;
        default: return;
    }
    player.applyLevelChoice(choice);
    player.pendingLevelUps--;
    addMessage("Level up! You are now level " + std::to_string(player.level) + ".");
    if (player.pendingLevelUps <= 0) {
        state = GameState::Playing;
    }
}

void Game::handleMessageLog() {
    renderer.renderMessageLog(messageLog, logScrollOffset);
    Key key = Input::getKey();
    switch (key) {
        case Key::Up:
            if (logScrollOffset < (int)messageLog.size() - 1) logScrollOffset++;
            break;
        case Key::Down:
            if (logScrollOffset > 0) logScrollOffset--;
            break;
        case Key::Escape:
        case Key::MessageLog:
            state = GameState::Playing;
            break;
        default: break;
    }
}

void Game::handleInventory() {
    renderer.renderInventory(player);

    Key key = Input::getKey();
    if (key == Key::Escape || key == Key::Inventory) {
        state = GameState::Playing;
        return;
    }

    // Use item by number key
    int index = -1;
    switch (key) {
        case Key::Num1: index = 0; break;
        case Key::Num2: index = 1; break;
        case Key::Num3: index = 2; break;
        case Key::Num4: index = 3; break;
        case Key::Num5: index = 4; break;
        case Key::Num6: index = 5; break;
        case Key::Num7: index = 6; break;
        case Key::Num8: index = 7; break;
        case Key::Num9: index = 8; break;
        case Key::Num0: index = 9; break;
        default: break;
    }

    if (index >= 0 && index < player.inventory.size()) {
        const Item& item = player.inventory.get(index);

        // Special items that need game state
        if (item.type == ItemType::Bomb) {
            int dmg = item.value;
            int hits = 0;
            for (auto& e : enemies) {
                if (e.isAlive() && player.pos.distanceSq(e.pos) <= 4) {
                    e.hp -= dmg;
                    if (e.hp < 0) e.hp = 0;
                    player.damageDealt += dmg;
                    hits++;
                    if (!e.isAlive() && e.xpReward > 0) {
                        player.killCount++;
                        player.addXP(e.xpReward);
                        addMessage("Gained " + std::to_string(e.xpReward) + " XP.");
                        int goldAmt = e.xpReward / 2 + std::rand() % 5;
                        items.emplace_back(e.pos, std::to_string(goldAmt) + " Gold", '$', ItemType::Gold, goldAmt);
                        e.xpReward = 0;
                    }
                }
            }
            player.inventory.remove(index);
            if (hits > 0) addMessage("BOOM! Bomb hits " + std::to_string(hits) + " enemies for " + std::to_string(dmg) + " damage!");
            else addMessage("BOOM! The bomb explodes but hits nothing.");
            state = GameState::Playing;
            return;
        }

        if (item.type == ItemType::TeleportScroll) {
            for (int attempts = 0; attempts < 100; attempts++) {
                int tx = std::rand() % Map::WIDTH;
                int ty = std::rand() % Map::HEIGHT;
                if (map.isWalkable(tx, ty) && !map.isDangerous(tx, ty)) {
                    player.pos = {tx, ty};
                    FOV::compute(map, player.pos, player.effectiveFovRadius());
                    break;
                }
            }
            player.inventory.remove(index);
            addMessage("You vanish and reappear elsewhere!");
            state = GameState::Playing;
            return;
        }

        std::string msg = player.useItem(index);
        if (!msg.empty()) addMessage(msg);
    }
}

void Game::handleGameOver() {
    checkHighScore();
    deleteSaveFile();
    renderer.renderDeathRecap(player, currentFloor, computeScore());
    Key key = Input::getKey();
    if (key == Key::Quit) {
        running = false;
    } else {
        state = GameState::DifficultySelect;
    }
}

void Game::handleWin() {
    checkHighScore();
    deleteSaveFile();
    renderer.renderWin(player, currentFloor, computeScore());
    Key key = Input::getKey();
    if (key == Key::Quit) {
        running = false;
    } else {
        state = GameState::DifficultySelect;
    }
}

void Game::movePlayer(int dx, int dy) {
    int nx = player.pos.x + dx;
    int ny = player.pos.y + dy;

    // Check for enemy at target position — bump to attack
    Enemy* target = enemyAt(nx, ny);
    if (target) {
        int atkBonus = player.totalAttack() - player.attack;

        // Flaming weapon bonus
        if (player.hasEquippedWeapon() && player.getWeaponSlot().enchantment == Enchantment::Flaming) {
            atkBonus += 2;
        }

        // Apply ability buffs
        if (player.abilityBuffActive) {
            if (player.playerClass == PlayerClass::Warrior) {
                atkBonus += 5;
            } else if (player.playerClass == PlayerClass::Rogue) {
                atkBonus += player.totalAttack() * 2; // 3x total (base + 2x bonus)
            }
        }

        int dmgOut = 0;
        std::string msg = resolveCombat(player, *target, atkBonus, 0, &dmgOut);
        player.damageDealt += dmgOut;
        addMessage(msg);

        // Apply stun from Warrior's Shield Bash
        if (player.abilityBuffActive && player.playerClass == PlayerClass::Warrior && target->isAlive()) {
            target->stunTurns = 2;
            addMessage(target->name + " is stunned!");
        }
        player.abilityBuffActive = false;

        // Vampiric weapon heal
        if (player.hasEquippedWeapon() && player.getWeaponSlot().enchantment == Enchantment::Vampiric) {
            int heal = 2;
            player.hp = std::min(player.hp + heal, player.maxHp);
            addMessage("Your vampiric weapon drains life! +" + std::to_string(heal) + " HP");
        }

        if (!target->isAlive()) {
            player.killCount++;
            player.addXP(target->xpReward);
            addMessage("Gained " + std::to_string(target->xpReward) + " XP.");

            // Drop gold
            int goldAmt = target->xpReward / 2 + std::rand() % 5;
            items.emplace_back(target->pos, std::to_string(goldAmt) + " Gold", '$', ItemType::Gold, goldAmt);

            // Enchanted loot drop (10% chance, bosses 100% legendary)
            bool isBoss = (target->type == EnemyType::Dragon || target->type == EnemyType::Lich);
            if (isBoss || std::rand() % 10 == 0) {
                Enchantment ench = Item::rollEnchantment(currentFloor, isBoss);
                if (std::rand() % 2 == 0) {
                    std::vector<std::string> wNames = {"Dagger", "Short Sword", "Long Sword", "War Axe", "Flame Blade"};
                    int tier = std::min(currentFloor - 1, 4);
                    items.emplace_back(target->pos, wNames[tier], '/', ItemType::Weapon, 3 + tier * 2, ench);
                } else {
                    std::vector<std::string> aNames = {"Leather Armor", "Chain Mail", "Scale Mail", "Plate Armor", "Dragon Armor"};
                    int tier = std::min(currentFloor - 1, 4);
                    items.emplace_back(target->pos, aNames[tier], '[', ItemType::Armor, 2 + tier * 2, ench);
                }
                addMessage("Something enchanted drops!");
            }

            if (target->type == EnemyType::Dragon && currentFloor == 5) {
                addMessage("The Dragon falls! But darker forces lurk below...");
            }
            if (target->type == EnemyType::Lich && currentFloor == 8) {
                state = GameState::Win;
            }
        }
        return;
    }

    // Normal movement
    if (map.isWalkable(nx, ny)) {
        player.pos.x = nx;
        player.pos.y = ny;

        // Auto-pickup items on walk
        for (auto& item : items) {
            if (!item.onGround || item.pos.x != nx || item.pos.y != ny) continue;
            if (item.type == ItemType::Gold) {
                player.gold += item.value;
                addMessage("Picked up " + std::to_string(item.value) + " gold.");
                item.onGround = false;
            } else if (!player.inventory.isFull()) {
                std::string pickMsg = "Picked up " + item.name + ".";
                if (item.type == ItemType::Weapon) {
                    int diff = item.value - (player.hasEquippedWeapon() ? player.getWeaponSlot().value : 0);
                    if (!player.hasEquippedWeapon()) pickMsg += " [new weapon!]";
                    else if (diff > 0) pickMsg += " [+" + std::to_string(diff) + " ATK vs equipped]";
                    else if (diff < 0) pickMsg += " [" + std::to_string(diff) + " ATK vs equipped]";
                } else if (item.type == ItemType::Armor) {
                    int diff = item.value - (player.hasEquippedArmor() ? player.getArmorSlot().value : 0);
                    if (!player.hasEquippedArmor()) pickMsg += " [new armor!]";
                    else if (diff > 0) pickMsg += " [+" + std::to_string(diff) + " DEF vs equipped]";
                    else if (diff < 0) pickMsg += " [" + std::to_string(diff) + " DEF vs equipped]";
                }
                addMessage(pickMsg);
                player.inventory.add(item);
                item.onGround = false;
            }
        }

        // Environmental tile effects
        Tile stepped = map.getTile(nx, ny);
        if (stepped == Tile::Lava) {
            int dmg = 5;
            player.hp -= dmg;
            if (player.hp < 0) player.hp = 0;
            player.damageTaken += dmg;
            player.lastDamageSource = "lava";
            addMessage("The lava burns you for " + std::to_string(dmg) + " damage!");
            player.burningTurns = 3;
            player.burningDmg = 2;
        } else if (stepped == Tile::Fountain) {
            // Find the room this fountain belongs to, check if used
            auto& rooms = dunGen.getRooms();
            bool healed = false;
            for (auto& r : rooms) {
                if (r.theme == RoomTheme::Shrine && !r.fountainUsed &&
                    r.center().x == nx && r.center().y == ny) {
                    int heal = 10;
                    player.hp = std::min(player.hp + heal, player.maxHp);
                    addMessage("The fountain restores " + std::to_string(heal) + " HP!");
                    r.fountainUsed = true;
                    map.setTile(nx, ny, Tile::Floor);
                    healed = true;
                    break;
                }
            }
            if (!healed) {
                // Fountain not tied to a shrine room, just heal anyway
                int heal = 10;
                player.hp = std::min(player.hp + heal, player.maxHp);
                addMessage("The fountain restores " + std::to_string(heal) + " HP!");
                map.setTile(nx, ny, Tile::Floor);
            }
        }

        // Check traps after moving
        checkTraps();
    }
}

void Game::descendStairs() {
    Tile t = map.getTile(player.pos.x, player.pos.y);
    if (t == Tile::StairsUp) {
        addMessage("These stairs lead up. You can't go back.");
        return;
    }
    if (t != Tile::StairsDown) {
        addMessage("No stairs here.");
        return;
    }
    if (currentFloor >= 8) {
        addMessage("This is the deepest floor.");
        return;
    }
    // Confirmation prompt
    addMessage("Descend to floor " + std::to_string(currentFloor + 1) + "? Press > again to confirm.");
    renderer.render(map, player, enemies, items, traps, messageLog, currentFloor,
                    dunGen.currentBiome, merchantPos, static_cast<int>(difficulty));
    Key confirm = Input::getKey();
    if (confirm != Key::Stairs) {
        addMessage("Staying on this floor.");
        return;
    }
    currentFloor++;
    addMessage("You descend deeper...");
    generateFloor();
}

void Game::pickUpItem() {
    // Check if adjacent to merchant
    if (merchantPos.x >= 0) {
        int dx = std::abs(player.pos.x - merchantPos.x);
        int dy = std::abs(player.pos.y - merchantPos.y);
        if (dx <= 1 && dy <= 1) {
            handleShopInteraction();
            return;
        }
    }

    for (auto& item : items) {
        if (!item.onGround) continue;
        if (item.pos == player.pos) {
            // Gold goes directly to wallet
            if (item.type == ItemType::Gold) {
                player.gold += item.value;
                addMessage("Picked up " + std::to_string(item.value) + " gold.");
                item.onGround = false;
                return;
            }
            if (player.inventory.isFull()) {
                addMessage("Inventory full!");
                return;
            }
            addMessage("Picked up " + item.name + ".");
            player.inventory.add(item);
            item.onGround = false;
            return;
        }
    }
    addMessage("Nothing to pick up.");
}

void Game::updateEnemies() {
    int count = (int)enemies.size(); // snapshot to avoid invalidation from summons
    for (int idx = 0; idx < count; idx++) {
        Enemy& e = enemies[idx];
        if (!e.isAlive()) continue;

        // Handle stun
        if (e.stunTurns > 0) {
            e.stunTurns--;
            continue;
        }

        // Dragon: skip every other turn unless enraged
        if (e.type == EnemyType::Dragon && !e.enraged) {
            e.summonTimer++;
            if (e.summonTimer % 2 == 0) continue;
        }
        // Dragon enrage at 50% HP
        if (e.type == EnemyType::Dragon && !e.enraged && e.hp <= e.maxHp / 2) {
            e.enraged = true;
            e.attack += 4;
            addMessage("The Dragon is enraged! Its attacks grow fiercer!");
        }

        // Necromancer summoning: every 5 turns when awake
        if (e.type == EnemyType::Necromancer && e.awake) {
            e.summonTimer++;
            if (e.summonTimer >= 5) {
                e.summonTimer = 0;
                // Spawn skeleton on adjacent tile
                static const Vec2 dirs[] = {{0,-1},{0,1},{-1,0},{1,0}};
                for (int d = 0; d < 4; d++) {
                    Vec2 sp = e.pos + dirs[d];
                    if (map.isWalkable(sp.x, sp.y) && !enemyAt(sp.x, sp.y) && sp != player.pos) {
                        enemies.push_back(Enemy::create(EnemyType::Skeleton, sp));
                        applyDifficulty(enemies.back());
                        enemies.back().awake = true;
                        addMessage("The Necromancer raises a Skeleton!");
                        count = (int)enemies.size(); // update snapshot
                        break;
                    }
                }
            }
        }

        // Lich phase change at 50% HP: teleport + mass summon
        if (e.type == EnemyType::Lich && !e.enraged && e.hp <= e.maxHp / 2) {
            e.enraged = true;
            e.attack += 4;
            // Teleport to random walkable tile
            for (int attempts = 0; attempts < 100; attempts++) {
                int tx = std::rand() % Map::WIDTH;
                int ty = std::rand() % Map::HEIGHT;
                if (map.isWalkable(tx, ty) && !enemyAt(tx, ty) && !(tx == player.pos.x && ty == player.pos.y)) {
                    e.pos = {tx, ty};
                    break;
                }
            }
            // Summon a wave of 3 undead
            static const Vec2 waveDirs[] = {{0,-1},{0,1},{-1,0},{1,0}};
            int summoned = 0;
            for (int d = 0; d < 4 && summoned < 3; d++) {
                Vec2 sp = e.pos + waveDirs[d];
                if (map.isWalkable(sp.x, sp.y) && !enemyAt(sp.x, sp.y) && sp != player.pos) {
                    EnemyType st = (std::rand() % 2 == 0) ? EnemyType::Skeleton : EnemyType::Ghost;
                    enemies.push_back(Enemy::create(st, sp));
                    applyDifficulty(enemies.back());
                    enemies.back().awake = true;
                    count = (int)enemies.size();
                    summoned++;
                }
            }
            addMessage("The Lich shrieks! Dark energy erupts as it vanishes into shadow!");
        }

        // Lich summoning: every 4 turns when awake
        if (e.type == EnemyType::Lich && e.awake) {
            e.summonTimer++;
            if (e.summonTimer >= 4) {
                e.summonTimer = 0;
                static const Vec2 dirs[] = {{0,-1},{0,1},{-1,0},{1,0}};
                for (int d = 0; d < 4; d++) {
                    Vec2 sp = e.pos + dirs[d];
                    if (map.isWalkable(sp.x, sp.y) && !enemyAt(sp.x, sp.y) && sp != player.pos) {
                        EnemyType summonType = (std::rand() % 2 == 0) ? EnemyType::Skeleton : EnemyType::Ghost;
                        enemies.push_back(Enemy::create(summonType, sp));
                        applyDifficulty(enemies.back());
                        enemies.back().awake = true;
                        addMessage("The Lich summons the undead!");
                        count = (int)enemies.size();
                        break;
                    }
                }
            }
        }

        // Ranged attack check (before movement)
        int rangedDmg = e.rangedAttackDamage(map, player.pos);
        if (rangedDmg > 0) {
            int variance = (std::rand() % 5) - 2;
            int damage = std::max(1, rangedDmg - (player.totalDefense()) + variance);
            player.hp -= damage;
            if (player.hp < 0) player.hp = 0;
            player.damageTaken += damage;
            player.lastDamageSource = "a " + e.name;
            addMessage(e.name + " shoots you for " + std::to_string(damage) + " damage!");
            continue;
        }

        Vec2 oldPos = e.pos;

        // Check if enemy is adjacent to player before moving
        int dist = e.pos.distanceSq(player.pos);
        if (dist <= 2) {
            int dmgIn = 0;
            std::string msg = resolveCombat(e, player, 0,
                                            player.totalDefense() - player.defense, &dmgIn);
            player.damageTaken += dmgIn;
            player.lastDamageSource = "a " + e.name;
            addMessage(msg);
            if (e.type == EnemyType::Ghost && player.isAlive()) {
                player.blindTurns = 3;
                addMessage("The Ghost's touch blinds you!");
            }
            if (e.type == EnemyType::Demon && player.isAlive()) {
                player.burningTurns = 3;
                player.burningDmg = 2;
                addMessage("The Demon sets you ablaze!");
            }
            continue;
        }

        // Water tile slows enemies
        if (map.getTile(e.pos.x, e.pos.y) == Tile::Water && std::rand() % 2 == 0) {
            continue;
        }

        e.update(map, player.pos);

        // Collision check
        if (e.pos == player.pos) {
            e.pos = oldPos;
            continue;
        }
        for (int j = 0; j < (int)enemies.size(); j++) {
            if (j == idx || !enemies[j].isAlive()) continue;
            if (e.pos == enemies[j].pos) {
                e.pos = oldPos;
                break;
            }
        }
    }
}

Enemy* Game::enemyAt(int x, int y) {
    for (auto& e : enemies) {
        if (e.isAlive() && e.pos.x == x && e.pos.y == y)
            return &e;
    }
    return nullptr;
}

void Game::addMessage(const std::string& msg) {
    messageLog.push_back(msg);
    if ((int)messageLog.size() > 50) {
        messageLog.erase(messageLog.begin());
    }
}

void Game::spawnTraps() {
    auto& rooms = dunGen.getRooms();
    int count = 1 + std::rand() % 3;
    for (int i = 0; i < count && rooms.size() > 1; i++) {
        int ri = 1 + std::rand() % ((int)rooms.size() - 1);
        const Room& r = rooms[ri];
        int tx = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int ty = r.y + 1 + std::rand() % std::max(1, r.h - 2);
        if (!map.isWalkable(tx, ty)) continue;

        Trap trap;
        trap.pos = {tx, ty};
        int roll = std::rand() % 4;
        trap.type = (roll == 0) ? TrapType::Spike : (roll == 1) ? TrapType::Poison : (roll == 2) ? TrapType::Teleport : TrapType::Slow;
        trap.revealed = false;
        traps.push_back(trap);
    }
}

void Game::checkTraps() {
    for (auto& trap : traps) {
        if (trap.pos != player.pos) continue;
        trap.revealed = true;
        switch (trap.type) {
            case TrapType::Spike: {
                int dmg = 8 + currentFloor * 2;
                player.hp -= dmg;
                if (player.hp < 0) player.hp = 0;
                player.damageTaken += dmg;
                player.lastDamageSource = "a spike trap";
                addMessage("A spike trap deals " + std::to_string(dmg) + " damage!");
                break;
            }
            case TrapType::Poison: {
                player.poisonTurns = 5;
                player.poisonDmg = 3;
                addMessage("A poison trap! You are poisoned for 5 turns!");
                break;
            }
            case TrapType::Teleport: {
                // Teleport to random walkable tile
                for (int attempts = 0; attempts < 100; attempts++) {
                    int tx = std::rand() % Map::WIDTH;
                    int ty = std::rand() % Map::HEIGHT;
                    if (map.isWalkable(tx, ty) && !map.isDangerous(tx, ty)) {
                        player.pos = {tx, ty};
                        addMessage("A teleport trap! You are whisked away!");
                        FOV::compute(map, player.pos, player.effectiveFovRadius());
                        return;
                    }
                }
                addMessage("A teleport trap fizzles...");
                break;
            }
            case TrapType::Slow: {
                player.slowTurns = 5;
                addMessage("A slow trap! Your movements are sluggish for 5 turns!");
                break;
            }
        }
        return; // only trigger one trap per step
    }
}

void Game::processTurn() {
    player.tickCooldown();
    if (player.poisonTurns > 0) {
        addMessage("Poison deals " + std::to_string(player.poisonDmg) + " damage! (" + std::to_string(player.poisonTurns) + " turns left)");
    }
    if (player.burningTurns > 0) {
        addMessage("Burning deals " + std::to_string(player.burningDmg) + " damage! (" + std::to_string(player.burningTurns) + " turns left)");
    }
    player.tickStatusEffects();
    updateEnemies();
    if (!player.isAlive()) {
        state = GameState::GameOver;
    }
    FOV::compute(map, player.pos, player.effectiveFovRadius());
}

Vec2 Game::bfsNextStep() const {
    // BFS from player pos to find nearest target:
    // - Visible alive enemy (to fight)
    // - Visible ground item (to pick up)
    // - Unexplored walkable tile (to explore)
    bool visited[Map::HEIGHT][Map::WIDTH] = {};
    int parentX[Map::HEIGHT][Map::WIDTH];
    int parentY[Map::HEIGHT][Map::WIDTH];
    for (int y = 0; y < Map::HEIGHT; y++)
        for (int x = 0; x < Map::WIDTH; x++) {
            parentX[y][x] = -1;
            parentY[y][x] = -1;
        }

    struct Pos { int x, y; };
    Pos queue[Map::HEIGHT * Map::WIDTH];
    int head = 0, tail = 0;

    queue[tail++] = {player.pos.x, player.pos.y};
    visited[player.pos.y][player.pos.x] = true;

    static const int dx[] = {0, 0, -1, 1};
    static const int dy[] = {-1, 1, 0, 0};

    while (head < tail) {
        Pos cur = queue[head++];

        if (cur.x == player.pos.x && cur.y == player.pos.y) goto expand;

        // Check for targets at this tile
        {
            bool isTarget = false;

            // Visible enemy?
            if (map.isVisible(cur.x, cur.y)) {
                for (auto& e : enemies) {
                    if (e.isAlive() && e.pos.x == cur.x && e.pos.y == cur.y) {
                        isTarget = true;
                        break;
                    }
                }
            }

            // Visible ground item?
            if (!isTarget && map.isVisible(cur.x, cur.y)) {
                for (auto& item : items) {
                    if (item.onGround && item.pos.x == cur.x && item.pos.y == cur.y) {
                        isTarget = true;
                        break;
                    }
                }
            }

            // Unexplored tile?
            if (!isTarget && !map.isExplored(cur.x, cur.y)) {
                isTarget = true;
            }

            if (isTarget) {
                // Trace back to first step from player
                int tx = cur.x, ty = cur.y;
                while (parentX[ty][tx] != player.pos.x || parentY[ty][tx] != player.pos.y) {
                    int px = parentX[ty][tx];
                    int py = parentY[ty][tx];
                    tx = px; ty = py;
                }
                return {tx, ty};
            }
        }

        expand:
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (!map.inBounds(nx, ny)) continue;
            if (visited[ny][nx]) continue;
            if (!map.isWalkable(nx, ny)) continue;
            visited[ny][nx] = true;
            parentX[ny][nx] = cur.x;
            parentY[ny][nx] = cur.y;
            queue[tail++] = {nx, ny};
        }
    }

    return {-1, -1}; // nothing to do
}

bool Game::shouldStopAutoExplore() const {
    // Stop if HP < 40% (safety threshold for auto-combat)
    if (player.hp * 5 < player.maxHp * 2) return true;
    // Stop if adjacent to merchant
    if (merchantPos.x >= 0) {
        int dx = std::abs(player.pos.x - merchantPos.x);
        int dy = std::abs(player.pos.y - merchantPos.y);
        if (dx <= 1 && dy <= 1) return true;
    }
    return false;
}

void Game::handleLooking() {
    std::string desc = describeCell(lookCursor.x, lookCursor.y);
    renderer.renderWithCursor(map, player, enemies, items, traps, messageLog,
                              currentFloor, lookCursor, desc, dunGen.currentBiome, merchantPos);

    Key key = Input::getKey();
    switch (key) {
        case Key::Up:    if (lookCursor.y > 0) lookCursor.y--; break;
        case Key::Down:  if (lookCursor.y < Map::HEIGHT - 1) lookCursor.y++; break;
        case Key::Left:  if (lookCursor.x > 0) lookCursor.x--; break;
        case Key::Right: if (lookCursor.x < Map::WIDTH - 1) lookCursor.x++; break;
        case Key::Escape:
        case Key::Look:
            state = GameState::Playing;
            return;
        default: break;
    }
}

std::string Game::describeCell(int x, int y) const {
    if (!map.isVisible(x, y)) return "You can't see there.";

    // Check merchant
    if (merchantPos.x == x && merchantPos.y == y) {
        return "A traveling merchant. Press E to trade.";
    }

    // Check enemy
    for (auto& e : enemies) {
        if (e.isAlive() && e.pos.x == x && e.pos.y == y) {
            return e.name + " - HP:" + std::to_string(e.hp) + "/" + std::to_string(e.maxHp) +
                   " ATK:" + std::to_string(e.attack) + " DEF:" + std::to_string(e.defense);
        }
    }

    // Check player
    if (x == player.pos.x && y == player.pos.y) {
        return "You - HP:" + std::to_string(player.hp) + "/" + std::to_string(player.maxHp);
    }

    // Check items
    for (auto& item : items) {
        if (item.onGround && item.pos.x == x && item.pos.y == y) {
            std::string desc = item.description();
            // Equipment comparison
            if (item.type == ItemType::Weapon) {
                int diff = item.value - (player.hasEquippedWeapon() ? player.getWeaponSlot().value : 0);
                if (!player.hasEquippedWeapon()) desc += " [new!]";
                else if (diff > 0) desc += " [+" + std::to_string(diff) + " vs equipped]";
                else if (diff < 0) desc += " [" + std::to_string(diff) + " vs equipped]";
                else desc += " [same as equipped]";
            } else if (item.type == ItemType::Armor) {
                int diff = item.value - (player.hasEquippedArmor() ? player.getArmorSlot().value : 0);
                if (!player.hasEquippedArmor()) desc += " [new!]";
                else if (diff > 0) desc += " [+" + std::to_string(diff) + " vs equipped]";
                else if (diff < 0) desc += " [" + std::to_string(diff) + " vs equipped]";
                else desc += " [same as equipped]";
            }
            return desc;
        }
    }

    // Check revealed traps
    for (auto& trap : traps) {
        if (trap.revealed && trap.pos.x == x && trap.pos.y == y) {
            switch (trap.type) {
                case TrapType::Spike: return "A spike trap.";
                case TrapType::Poison: return "A poison trap.";
                case TrapType::Teleport: return "A teleport trap.";
                case TrapType::Slow: return "A slow trap.";
            }
        }
    }

    // Tile description
    Tile t = map.getTile(x, y);
    switch (t) {
        case Tile::Wall:       return "A solid wall.";
        case Tile::Floor:      return "Stone floor.";
        case Tile::StairsDown: return "Stairs leading down.";
        case Tile::StairsUp:   return "Stairs leading up.";
        case Tile::Water:      return "Shallow water. Slows movement.";
        case Tile::Lava:       return "Molten lava! Very dangerous.";
        case Tile::Fountain:   return "A healing fountain.";
    }
    return "";
}

int Game::computeScore() const {
    return currentFloor * 100 + player.level * 50 + player.killCount * 10 + player.gold;
}

void Game::loadHighScores() {
    highScores.clear();
    std::ifstream f("shadowcrypt.scores");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        ScoreEntry entry;
        int classInt;
        if (iss >> entry.score >> classInt >> entry.floor >> entry.level >> entry.kills) {
            entry.playerClass = static_cast<PlayerClass>(classInt);
            int diff = 1;
            iss >> diff;
            entry.difficulty = static_cast<Difficulty>(diff);
            highScores.push_back(entry);
        }
    }
}

void Game::saveHighScores() {
    std::ofstream f("shadowcrypt.scores");
    if (!f.is_open()) return;
    int count = std::min((int)highScores.size(), 10);
    for (int i = 0; i < count; i++) {
        auto& s = highScores[i];
        f << s.score << " " << static_cast<int>(s.playerClass) << " "
          << s.floor << " " << s.level << " " << s.kills << " "
          << static_cast<int>(s.difficulty) << "\n";
    }
}

void Game::deleteSaveFile() {
    std::remove("shadowcrypt.sav");
    saveFileExists = false;
}

void Game::saveGame() {
    std::ofstream f("shadowcrypt.sav");
    if (!f.is_open()) return;

    // PLAYER section
    f << "PLAYER\n";
    f << static_cast<int>(player.playerClass) << " "
      << player.pos.x << " " << player.pos.y << " "
      << player.hp << " " << player.maxHp << " "
      << player.baseAttack << " " << player.baseDefense << " "
      << player.level << " " << player.xp << " "
      << player.abilityCooldown << " " << (player.abilityBuffActive ? 1 : 0) << " "
      << player.lvlHp << " " << player.lvlAtk << " " << player.lvlDef << " "
      << player.poisonTurns << " " << player.poisonDmg << " "
      << player.killCount << " " << currentFloor << " "
      << player.blindTurns << " " << player.slowTurns << " "
      << player.hasteTurns << " " << player.burningTurns << " "
      << player.burningDmg << " " << player.gold << " "
      << player.shieldTurns << " " << player.shieldBonus << " "
      << player.pendingLevelUps << " " << player.turnsPlayed << " "
      << player.damageDealt << " " << player.damageTaken << " "
      << player.potionsUsed << " " << static_cast<int>(difficulty) << "\n";

    // EQUIPMENT section
    f << "EQUIPMENT\n";
    f << (player.hasEquippedWeapon() ? 1 : 0) << "\n";
    if (player.hasEquippedWeapon()) {
        auto& w = player.getWeaponSlot();
        f << w.name << "|" << w.glyph << " " << static_cast<int>(w.type) << " " << w.value << " " << static_cast<int>(w.enchantment) << "\n";
    }
    f << (player.hasEquippedArmor() ? 1 : 0) << "\n";
    if (player.hasEquippedArmor()) {
        auto& a = player.getArmorSlot();
        f << a.name << "|" << a.glyph << " " << static_cast<int>(a.type) << " " << a.value << " " << static_cast<int>(a.enchantment) << "\n";
    }

    // INVENTORY section
    f << "INVENTORY\n";
    f << player.inventory.size() << "\n";
    for (int i = 0; i < player.inventory.size(); i++) {
        auto& item = player.inventory.get(i);
        f << item.name << "|" << item.glyph << " " << static_cast<int>(item.type) << " " << item.value << " " << static_cast<int>(item.enchantment) << "\n";
    }

    // MAP section (tile ints)
    f << "MAP\n";
    for (int y = 0; y < Map::HEIGHT; y++) {
        for (int x = 0; x < Map::WIDTH; x++) {
            f << static_cast<int>(map.getTile(x, y));
            if (x < Map::WIDTH - 1) f << " ";
        }
        f << "\n";
    }

    // EXPLORED section
    f << "EXPLORED\n";
    for (int y = 0; y < Map::HEIGHT; y++) {
        for (int x = 0; x < Map::WIDTH; x++) {
            f << (map.isExplored(x, y) ? 1 : 0);
            if (x < Map::WIDTH - 1) f << " ";
        }
        f << "\n";
    }

    // ENEMIES section
    f << "ENEMIES\n";
    int aliveCount = 0;
    for (auto& e : enemies) if (e.isAlive()) aliveCount++;
    f << aliveCount << "\n";
    for (auto& e : enemies) {
        if (!e.isAlive()) continue;
        f << static_cast<int>(e.type) << " "
          << e.pos.x << " " << e.pos.y << " "
          << e.hp << " " << e.maxHp << " "
          << e.attack << " " << e.defense << " "
          << e.xpReward << " " << (e.awake ? 1 : 0) << " "
          << e.stunTurns << " " << e.summonTimer << " "
          << (e.enraged ? 1 : 0) << "\n";
    }

    // ITEMS section
    f << "ITEMS\n";
    int groundCount = 0;
    for (auto& item : items) if (item.onGround) groundCount++;
    f << groundCount << "\n";
    for (auto& item : items) {
        if (!item.onGround) continue;
        f << item.name << "|" << item.glyph << " "
          << static_cast<int>(item.type) << " " << item.value << " "
          << item.pos.x << " " << item.pos.y << " "
          << static_cast<int>(item.enchantment) << "\n";
    }

    // TRAPS section
    f << "TRAPS\n";
    f << traps.size() << "\n";
    for (auto& trap : traps) {
        f << static_cast<int>(trap.type) << " "
          << trap.pos.x << " " << trap.pos.y << " "
          << (trap.revealed ? 1 : 0) << "\n";
    }

    // ROOMS section
    f << "ROOMS\n";
    auto& rooms = dunGen.getRooms();
    f << rooms.size() << "\n";
    for (auto& r : rooms) {
        f << r.x << " " << r.y << " " << r.w << " " << r.h << " "
          << static_cast<int>(r.theme) << " " << (r.fountainUsed ? 1 : 0) << "\n";
    }

    // SHOP section
    f << "SHOP\n";
    f << (shopActive ? 1 : 0) << " " << merchantPos.x << " " << merchantPos.y << "\n";
    f << shopInventory.size() << "\n";
    for (auto& si : shopInventory) {
        f << si.item.name << "|" << si.item.glyph << " "
          << static_cast<int>(si.item.type) << " " << si.item.value << " "
          << static_cast<int>(si.item.enchantment) << " "
          << si.price << " " << (si.sold ? 1 : 0) << "\n";
    }

    // MESSAGES section (last 10)
    f << "MESSAGES\n";
    int msgCount = std::min((int)messageLog.size(), 10);
    f << msgCount << "\n";
    for (int i = (int)messageLog.size() - msgCount; i < (int)messageLog.size(); i++) {
        f << messageLog[i] << "\n";
    }

    saveFileExists = true;
}

bool Game::loadGame() {
    std::ifstream f("shadowcrypt.sav");
    if (!f.is_open()) return false;

    std::string section;

    // PLAYER
    std::getline(f, section); // "PLAYER"
    {
        int cls, px, py, hp, maxHp, bAtk, bDef, lvl, xp, cd, buff;
        int lhp, latk, ldef, pTurns, pDmg, kills, floor;
        int blind, slow, haste, burning, burnDmg, gold;
        int shieldT, shieldB, pendLvl, turns, dmgDealt, dmgTaken, potUsed, diff;
        f >> cls >> px >> py >> hp >> maxHp >> bAtk >> bDef >> lvl >> xp >> cd >> buff
          >> lhp >> latk >> ldef >> pTurns >> pDmg >> kills >> floor
          >> blind >> slow >> haste >> burning >> burnDmg >> gold
          >> shieldT >> shieldB >> pendLvl >> turns >> dmgDealt >> dmgTaken >> potUsed >> diff;
        f.ignore();

        player = Player(static_cast<PlayerClass>(cls));
        player.pos = {px, py};
        player.hp = hp;
        player.maxHp = maxHp;
        player.baseAttack = bAtk;
        player.baseDefense = bDef;
        player.level = lvl;
        player.xp = xp;
        player.abilityCooldown = cd;
        player.abilityBuffActive = (buff != 0);
        player.lvlHp = lhp;
        player.lvlAtk = latk;
        player.lvlDef = ldef;
        player.poisonTurns = pTurns;
        player.poisonDmg = pDmg;
        player.killCount = kills;
        currentFloor = floor;
        player.blindTurns = blind;
        player.slowTurns = slow;
        player.hasteTurns = haste;
        player.burningTurns = burning;
        player.burningDmg = burnDmg;
        player.gold = gold;
        player.shieldTurns = shieldT;
        player.shieldBonus = shieldB;
        player.pendingLevelUps = pendLvl;
        player.turnsPlayed = turns;
        player.damageDealt = dmgDealt;
        player.damageTaken = dmgTaken;
        player.potionsUsed = potUsed;
        difficulty = static_cast<Difficulty>(diff);
    }

    // EQUIPMENT
    std::getline(f, section); // "EQUIPMENT"
    {
        int hasW;
        f >> hasW;
        f.ignore();
        if (hasW) {
            std::string line;
            std::getline(f, line);
            size_t pipePos = line.find('|');
            std::string name = line.substr(0, pipePos);
            char glyph = line[pipePos + 1];
            int typeInt, val, enchInt = 0;
            std::istringstream iss(line.substr(pipePos + 2));
            iss >> typeInt >> val >> enchInt;
            Item item = Item::loadItem({0,0}, name, glyph, static_cast<ItemType>(typeInt), val, static_cast<Enchantment>(enchInt));
            player.setWeaponSlot(item);
        }
        int hasA;
        f >> hasA;
        f.ignore();
        if (hasA) {
            std::string line;
            std::getline(f, line);
            size_t pipePos = line.find('|');
            std::string name = line.substr(0, pipePos);
            char glyph = line[pipePos + 1];
            int typeInt, val, enchInt = 0;
            std::istringstream iss(line.substr(pipePos + 2));
            iss >> typeInt >> val >> enchInt;
            Item item = Item::loadItem({0,0}, name, glyph, static_cast<ItemType>(typeInt), val, static_cast<Enchantment>(enchInt));
            player.setArmorSlot(item);
        }
    }

    // INVENTORY
    std::getline(f, section); // "INVENTORY"
    {
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            std::string line;
            std::getline(f, line);
            size_t pipePos = line.find('|');
            std::string name = line.substr(0, pipePos);
            char glyph = line[pipePos + 1];
            int typeInt, val, enchInt = 0;
            std::istringstream iss(line.substr(pipePos + 2));
            iss >> typeInt >> val >> enchInt;
            Item item = Item::loadItem({0,0}, name, glyph, static_cast<ItemType>(typeInt), val, static_cast<Enchantment>(enchInt));
            player.inventory.add(item);
        }
    }

    // MAP
    std::getline(f, section); // "MAP"
    map.clear();
    for (int y = 0; y < Map::HEIGHT; y++) {
        for (int x = 0; x < Map::WIDTH; x++) {
            int t;
            f >> t;
            map.setTile(x, y, static_cast<Tile>(t));
        }
    }
    f.ignore();

    // EXPLORED
    std::getline(f, section); // "EXPLORED"
    for (int y = 0; y < Map::HEIGHT; y++) {
        for (int x = 0; x < Map::WIDTH; x++) {
            int e;
            f >> e;
            if (e) map.setExplored(x, y);
        }
    }
    f.ignore();

    // ENEMIES
    enemies.clear();
    std::getline(f, section); // "ENEMIES"
    {
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            int typeInt, px, py, hp, maxHp, atk, def, xpR, awake, stun, summon, enrage;
            f >> typeInt >> px >> py >> hp >> maxHp >> atk >> def >> xpR >> awake >> stun >> summon >> enrage;
            f.ignore();
            Enemy e = Enemy::create(static_cast<EnemyType>(typeInt), {px, py});
            e.hp = hp;
            e.maxHp = maxHp;
            e.attack = atk;
            e.defense = def;
            e.xpReward = xpR;
            e.awake = (awake != 0);
            e.stunTurns = stun;
            e.summonTimer = summon;
            e.enraged = (enrage != 0);
            enemies.push_back(e);
        }
    }

    // ITEMS
    items.clear();
    std::getline(f, section); // "ITEMS"
    {
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            std::string line;
            std::getline(f, line);
            size_t pipePos = line.find('|');
            std::string name = line.substr(0, pipePos);
            char glyph = line[pipePos + 1];
            int typeInt, val, ix, iy, enchInt = 0;
            std::istringstream iss(line.substr(pipePos + 2));
            iss >> typeInt >> val >> ix >> iy >> enchInt;
            items.push_back(Item::loadItem(Vec2{ix, iy}, name, glyph, static_cast<ItemType>(typeInt), val, static_cast<Enchantment>(enchInt)));
        }
    }

    // TRAPS
    traps.clear();
    std::getline(f, section); // "TRAPS"
    {
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            Trap trap;
            int typeInt, px, py, rev;
            f >> typeInt >> px >> py >> rev;
            f.ignore();
            trap.type = static_cast<TrapType>(typeInt);
            trap.pos = {px, py};
            trap.revealed = (rev != 0);
            traps.push_back(trap);
        }
    }

    // ROOMS
    std::getline(f, section); // "ROOMS"
    {
        int count;
        f >> count;
        f.ignore();
        std::vector<Room> loadedRooms;
        for (int i = 0; i < count; i++) {
            Room r;
            int themeInt, fu;
            f >> r.x >> r.y >> r.w >> r.h >> themeInt >> fu;
            f.ignore();
            r.theme = static_cast<RoomTheme>(themeInt);
            r.fountainUsed = (fu != 0);
            loadedRooms.push_back(r);
        }
        dunGen.setRooms(loadedRooms);
    }

    // SHOP
    shopInventory.clear();
    std::getline(f, section); // "SHOP"
    {
        int active, mx, my;
        f >> active >> mx >> my;
        f.ignore();
        shopActive = (active != 0);
        merchantPos = {mx, my};
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            std::string line;
            std::getline(f, line);
            size_t pipePos = line.find('|');
            std::string name = line.substr(0, pipePos);
            char glyph = line[pipePos + 1];
            int typeInt, val, enchInt, price, sold;
            std::istringstream iss(line.substr(pipePos + 2));
            iss >> typeInt >> val >> enchInt >> price >> sold;
            ShopItem si;
            si.item = Item::loadItem({0,0}, name, glyph, static_cast<ItemType>(typeInt), val, static_cast<Enchantment>(enchInt));
            si.price = price;
            si.sold = (sold != 0);
            shopInventory.push_back(si);
        }
    }

    // MESSAGES
    messageLog.clear();
    std::getline(f, section); // "MESSAGES"
    {
        int count;
        f >> count;
        f.ignore();
        for (int i = 0; i < count; i++) {
            std::string line;
            std::getline(f, line);
            messageLog.push_back(line);
        }
    }

    scoreRecorded = false;
    autoExploring = false;
    FOV::compute(map, player.pos, player.effectiveFovRadius());
    addMessage("Game loaded.");
    return true;
}

void Game::checkHighScore() {
    if (scoreRecorded) return;
    scoreRecorded = true;
    ScoreEntry entry;
    entry.score = computeScore();
    entry.playerClass = player.playerClass;
    entry.floor = currentFloor;
    entry.level = player.level;
    entry.kills = player.killCount;
    entry.difficulty = difficulty;
    highScores.push_back(entry);
    std::sort(highScores.begin(), highScores.end(),
              [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
    if ((int)highScores.size() > 10) highScores.resize(10);
    saveHighScores();
}
