#pragma once
#include "Map.h"
#include "DungeonGen.h"
#include "Entity.h"
#include <vector>
#include <string>

class Renderer {
public:
    void clearScreen();
    void render(const Map& map, const Player& player,
                const std::vector<Enemy>& enemies,
                const std::vector<Item>& items,
                const std::vector<Trap>& traps,
                const std::vector<std::string>& log,
                int floor, Biome biome = Biome::Stone,
                Vec2 merchantPos = {-1,-1});
    void renderWithCursor(const Map& map, const Player& player,
                const std::vector<Enemy>& enemies,
                const std::vector<Item>& items,
                const std::vector<Trap>& traps,
                const std::vector<std::string>& log,
                int floor, Vec2 cursor, const std::string& desc,
                Biome biome = Biome::Stone,
                Vec2 merchantPos = {-1,-1});
    void renderTitle(const std::vector<std::string>& topScores = {}, bool hasSaveFile = false);
    void renderGameOver(int floor, int level);
    void renderWin(int level);
    void renderInventory(const Player& player);
    void renderHelp();
    void renderClassSelect();
    void renderShop(const std::vector<struct ShopItem>& shopItems, int playerGold);

private:
    std::string buffer;

    void moveCursor(int x, int y);
    void setColor(int fg, int bg = -1);
    void resetColor();
    char tileChar(Tile t, Biome biome = Biome::Stone) const;
    void getTileColor(Tile t, bool visible, int& fg, Biome biome = Biome::Stone) const;
};
