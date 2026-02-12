#pragma once
#include "Vec2.h"
#include <vector>

enum class Tile { Wall, Floor, StairsDown, StairsUp, Water, Lava, Fountain };

enum class TrapType { Spike, Poison, Teleport, Slow };

struct Trap {
    Vec2 pos;
    TrapType type;
    bool revealed = false;
};

class Map {
public:
    static const int WIDTH = 80;
    static const int HEIGHT = 18;

    Map();
    void clear();
    Tile getTile(int x, int y) const;
    void setTile(int x, int y, Tile t);
    bool isWalkable(int x, int y) const;
    bool inBounds(int x, int y) const;
    bool isOpaque(int x, int y) const;
    bool isDangerous(int x, int y) const;

    void setVisible(int x, int y, bool v);
    bool isVisible(int x, int y) const;
    void setExplored(int x, int y);
    bool isExplored(int x, int y) const;
    void clearVisible();

    Vec2 stairsDownPos() const;
    Vec2 stairsUpPos() const;

private:
    Tile tiles[HEIGHT][WIDTH];
    bool visible[HEIGHT][WIDTH];
    bool explored[HEIGHT][WIDTH];
};

namespace FOV {
    void compute(Map& map, Vec2 origin, int radius = 8);
}
