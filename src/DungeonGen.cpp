#include "DungeonGen.h"
#include <cstdlib>
#include <algorithm>

static int randRange(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + std::rand() % (hi - lo + 1);
}

Biome DungeonGen::biomeForFloor(int floor) {
    if (floor <= 3) return Biome::Stone;
    if (floor <= 5) return Biome::Cave;
    return Biome::Inferno;
}

void DungeonGen::generate(Map& map, int floor) {
    map.clear();
    rooms.clear();
    currentBiome = biomeForFloor(floor);

    BSPNode root;
    root.x = 1;
    root.y = 1;
    root.w = Map::WIDTH - 2;
    root.h = Map::HEIGHT - 2;

    int depth = 4;
    splitBSP(&root, depth);
    createRooms(&root, map);
    connectRooms(&root, map);
    placeStairs(map, floor);
    assignThemes(floor);
    applyThemeTiles(map);

    // Free child nodes (root is stack-allocated)
    freeBSP(root.left);
    freeBSP(root.right);
    root.left = root.right = nullptr;
}

void DungeonGen::splitBSP(BSPNode* node, int depth) {
    if (depth <= 0 || node->w < 10 || node->h < 8) return;

    bool splitH = (node->h > node->w) ? true
                : (node->w > node->h) ? false
                : (std::rand() % 2 == 0);

    if (splitH) {
        int split = randRange(node->y + 3, node->y + node->h - 4);
        node->left = new BSPNode{node->x, node->y, node->w, split - node->y};
        node->right = new BSPNode{node->x, split, node->w, node->y + node->h - split};
    } else {
        int split = randRange(node->x + 4, node->x + node->w - 5);
        node->left = new BSPNode{node->x, node->y, split - node->x, node->h};
        node->right = new BSPNode{split, node->y, node->x + node->w - split, node->h};
    }

    splitBSP(node->left, depth - 1);
    splitBSP(node->right, depth - 1);
}

void DungeonGen::createRooms(BSPNode* node, Map& map) {
    if (node->left || node->right) {
        if (node->left) createRooms(node->left, map);
        if (node->right) createRooms(node->right, map);
        return;
    }

    int rw = randRange(4, std::min(node->w - 2, 12));
    int rh = randRange(3, std::min(node->h - 2, 8));
    int rx = randRange(node->x + 1, node->x + node->w - rw - 1);
    int ry = randRange(node->y + 1, node->y + node->h - rh - 1);

    Room r{rx, ry, rw, rh};
    node->room = r;
    node->hasRoom = true;
    rooms.push_back(r);
    carveRoom(map, r);
}

Room DungeonGen::findRoom(BSPNode* node) {
    if (node->hasRoom) return node->room;
    if (node->left) return findRoom(node->left);
    if (node->right) return findRoom(node->right);
    return {};
}

void DungeonGen::connectRooms(BSPNode* node, Map& map) {
    if (!node->left || !node->right) return;

    connectRooms(node->left, map);
    connectRooms(node->right, map);

    Room a = findRoom(node->left);
    Room b = findRoom(node->right);
    carveCorridor(map, a.center(), b.center());
}

void DungeonGen::carveRoom(Map& map, const Room& r) {
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            map.setTile(x, y, Tile::Floor);
}

void DungeonGen::carveCorridor(Map& map, Vec2 a, Vec2 b) {
    int x = a.x, y = a.y;

    while (x != b.x) {
        map.setTile(x, y, Tile::Floor);
        x += (b.x > x) ? 1 : -1;
    }
    while (y != b.y) {
        map.setTile(x, y, Tile::Floor);
        y += (b.y > y) ? 1 : -1;
    }
    map.setTile(b.x, b.y, Tile::Floor);
}

void DungeonGen::placeStairs(Map& map, int floor) {
    if (rooms.size() < 2) return;

    // Stairs up in first room (except floor 1)
    if (floor > 1) {
        Vec2 c = rooms.front().center();
        map.setTile(c.x, c.y, Tile::StairsUp);
    }

    // Stairs down in last room (except floor 8)
    if (floor < 8) {
        Vec2 c = rooms.back().center();
        map.setTile(c.x, c.y, Tile::StairsDown);
    }
}

void DungeonGen::freeBSP(BSPNode* node) {
    if (!node) return;
    freeBSP(node->left);
    freeBSP(node->right);
    delete node;
}

void DungeonGen::assignThemes(int floor) {
    bool shopPlaced = false;
    for (size_t i = 0; i < rooms.size(); i++) {
        if (i == 0) { rooms[i].theme = RoomTheme::Normal; continue; } // spawn room
        if (floor <= 1) {
            rooms[i].theme = (std::rand() % 100 < 15) ? RoomTheme::Armory : RoomTheme::Normal;
        } else {
            int roll = std::rand() % 100;
            if (roll < 30) rooms[i].theme = RoomTheme::Normal;
            else if (roll < 50) rooms[i].theme = RoomTheme::Armory;
            else if (roll < 70) rooms[i].theme = RoomTheme::Library;
            else if (roll < 85) rooms[i].theme = RoomTheme::Crypt;
            else rooms[i].theme = RoomTheme::Shrine;
        }
    }
    // Place one Shop room on floors 2+
    if (floor >= 2 && rooms.size() > 2) {
        for (size_t i = 1; i < rooms.size(); i++) {
            if (!shopPlaced && rooms[i].theme == RoomTheme::Normal) {
                rooms[i].theme = RoomTheme::Shop;
                shopPlaced = true;
                break;
            }
        }
        if (!shopPlaced) {
            // Force a shop on a random non-spawn room
            int idx = 1 + std::rand() % ((int)rooms.size() - 1);
            rooms[idx].theme = RoomTheme::Shop;
        }
    }
}

void DungeonGen::applyThemeTiles(Map& map) {
    // Place fountain at Shrine room centers
    for (auto& room : rooms) {
        if (room.theme == RoomTheme::Shrine) {
            Vec2 c = room.center();
            if (map.getTile(c.x, c.y) == Tile::Floor) {
                map.setTile(c.x, c.y, Tile::Fountain);
                room.fountainUsed = false;
            }
        }
    }

    // Sprinkle Water/Lava in corridor-like tiles (floor tiles with 2+ adjacent walls)
    for (int y = 1; y < Map::HEIGHT - 1; y++) {
        for (int x = 1; x < Map::WIDTH - 1; x++) {
            if (map.getTile(x, y) != Tile::Floor) continue;
            int wallCount = 0;
            if (map.getTile(x-1, y) == Tile::Wall) wallCount++;
            if (map.getTile(x+1, y) == Tile::Wall) wallCount++;
            if (map.getTile(x, y-1) == Tile::Wall) wallCount++;
            if (map.getTile(x, y+1) == Tile::Wall) wallCount++;

            // Count walkable neighbors — skip if only 2 (corridor bottleneck)
            // so lava never blocks the only path
            int walkCount = 4 - wallCount;

            if (currentBiome == Biome::Inferno) {
                if (wallCount >= 2 && walkCount >= 3 && std::rand() % 100 < 20) {
                    map.setTile(x, y, Tile::Lava);
                }
            } else {
                if (wallCount >= 2 && std::rand() % 100 < 8) {
                    // Lava only where there's room to walk around; water is fine anywhere
                    Tile env = (std::rand() % 3 == 0 && walkCount >= 3) ? Tile::Lava : Tile::Water;
                    map.setTile(x, y, env);
                }
            }
        }
    }
}
