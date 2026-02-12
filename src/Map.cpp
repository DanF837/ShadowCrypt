#include "Map.h"
#include <cstring>

Map::Map() {
    clear();
}

void Map::clear() {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) {
            tiles[y][x] = Tile::Wall;
            visible[y][x] = false;
            explored[y][x] = false;
        }
}

Tile Map::getTile(int x, int y) const {
    if (!inBounds(x, y)) return Tile::Wall;
    return tiles[y][x];
}

void Map::setTile(int x, int y, Tile t) {
    if (inBounds(x, y)) tiles[y][x] = t;
}

bool Map::isWalkable(int x, int y) const {
    Tile t = getTile(x, y);
    return t == Tile::Floor || t == Tile::StairsDown || t == Tile::StairsUp
        || t == Tile::Water || t == Tile::Lava || t == Tile::Fountain;
}

bool Map::inBounds(int x, int y) const {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

bool Map::isOpaque(int x, int y) const {
    return getTile(x, y) == Tile::Wall;
}

bool Map::isDangerous(int x, int y) const {
    return getTile(x, y) == Tile::Lava;
}

void Map::setVisible(int x, int y, bool v) {
    if (inBounds(x, y)) {
        visible[y][x] = v;
        if (v) explored[y][x] = true;
    }
}

bool Map::isVisible(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return visible[y][x];
}

void Map::setExplored(int x, int y) {
    if (inBounds(x, y)) explored[y][x] = true;
}

bool Map::isExplored(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return explored[y][x];
}

void Map::clearVisible() {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            visible[y][x] = false;
}

Vec2 Map::stairsDownPos() const {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (tiles[y][x] == Tile::StairsDown) return {x, y};
    return {-1, -1};
}

Vec2 Map::stairsUpPos() const {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (tiles[y][x] == Tile::StairsUp) return {x, y};
    return {-1, -1};
}

// --- FOV: Recursive shadowcasting ---
// Based on the algorithm by Bjorn Bergstrom
// 8 octants, each processed with a recursive scan

#include <cmath>
#include <algorithm>

static int multipliers[4][8] = {
    {1, 0, 0,-1,-1, 0, 0, 1},
    {0, 1,-1, 0, 0,-1, 1, 0},
    {0, 1, 1, 0, 0,-1,-1, 0},
    {1, 0, 0, 1,-1, 0, 0,-1}
};

static void castLight(Map& map, Vec2 origin, int radius,
                      int row, float startSlope, float endSlope,
                      int xx, int xy, int yx, int yy) {
    if (startSlope < endSlope) return;

    float nextStartSlope = startSlope;

    for (int i = row; i <= radius; i++) {
        bool blocked = false;
        for (int dx = -i, dy = -i; dx <= 0; dx++) {
            float lSlope = ((float)dx - 0.5f) / ((float)dy + 0.5f);
            float rSlope = ((float)dx + 0.5f) / ((float)dy - 0.5f);

            if (startSlope < rSlope) continue;
            if (endSlope > lSlope) break;

            int mx = origin.x + dx * xx + dy * xy;
            int my = origin.y + dx * yx + dy * yy;

            if ((int)(dx * dx + dy * dy) <= radius * radius) {
                map.setVisible(mx, my, true);
            }

            if (blocked) {
                if (map.isOpaque(mx, my)) {
                    nextStartSlope = rSlope;
                    continue;
                } else {
                    blocked = false;
                    startSlope = nextStartSlope;
                }
            } else {
                if (map.isOpaque(mx, my) && i < radius) {
                    blocked = true;
                    castLight(map, origin, radius, i + 1,
                              startSlope, lSlope, xx, xy, yx, yy);
                    nextStartSlope = rSlope;
                }
            }
        }
        if (blocked) break;
    }
}

void FOV::compute(Map& map, Vec2 origin, int radius) {
    map.clearVisible();
    map.setVisible(origin.x, origin.y, true);

    for (int oct = 0; oct < 8; oct++) {
        castLight(map, origin, radius, 1, 1.0f, 0.0f,
                  multipliers[0][oct], multipliers[1][oct],
                  multipliers[2][oct], multipliers[3][oct]);
    }
}
