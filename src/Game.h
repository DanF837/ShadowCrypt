#pragma once
#include "Map.h"
#include "DungeonGen.h"
#include "Entity.h"
#include "Item.h"
#include "Renderer.h"
#include "Input.h"
#include <vector>
#include <string>

enum class GameState { Menu, ClassSelect, Playing, Inventory, GameOver, Win, Looking };

struct ScoreEntry {
    int score;
    PlayerClass playerClass;
    int floor, level, kills;
};

class Game {
public:
    void run();

private:
    GameState state = GameState::Menu;
    Map map;
    DungeonGen dunGen;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Item> items;
    std::vector<Trap> traps;
    Renderer renderer;
    std::vector<std::string> messageLog;
    int currentFloor = 1;
    bool running = true;
    std::vector<ScoreEntry> highScores;
    bool scoreRecorded = false;

    // Look mode
    Vec2 lookCursor;

    // Shop system
    std::vector<ShopItem> shopInventory;
    Vec2 merchantPos = {-1,-1};
    bool shopActive = false;

    // Auto-explore
    bool autoExploring = false;

    void newGame(PlayerClass cls);
    void generateFloor();
    void spawnEnemies();
    void spawnItems();
    void spawnBoss();
    void spawnBoss8();
    void spawnTraps();
    void checkTraps();

    void handleMenu();
    void handleClassSelect();
    void handlePlaying();
    void handleInventory();
    void handleGameOver();
    void handleWin();
    void handleLooking();
    void handleShopInteraction();

    void movePlayer(int dx, int dy);
    void descendStairs();
    void pickUpItem();
    void updateEnemies();
    Enemy* enemyAt(int x, int y);
    void addMessage(const std::string& msg);

    std::string describeCell(int x, int y) const;
    void generateShop(const Room& room);
    void processTurn();
    Vec2 bfsNextStep() const;
    bool shouldStopAutoExplore() const;

    void loadHighScores();
    void saveHighScores();
    void checkHighScore();
    int computeScore() const;

    void saveGame();
    bool loadGame();
    void deleteSaveFile();
    bool saveFileExists = false;
};
