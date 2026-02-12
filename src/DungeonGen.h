#pragma once
#include "Map.h"
#include "Vec2.h"
#include <vector>

enum class RoomTheme { Normal, Armory, Library, Crypt, Shrine, Shop };
enum class Biome { Stone, Cave, Inferno };

struct Room {
    int x, y, w, h;
    RoomTheme theme = RoomTheme::Normal;
    bool fountainUsed = false;
    Vec2 center() const { return {x + w / 2, y + h / 2}; }
};

class DungeonGen {
public:
    void generate(Map& map, int floor);
    const std::vector<Room>& getRooms() const { return rooms; }
    std::vector<Room>& getRooms() { return rooms; }
    void setRooms(const std::vector<Room>& r) { rooms = r; }
    Biome currentBiome = Biome::Stone;
    static Biome biomeForFloor(int floor);

private:
    struct BSPNode {
        int x, y, w, h;
        BSPNode* left = nullptr;
        BSPNode* right = nullptr;
        Room room{};
        bool hasRoom = false;
    };

    std::vector<Room> rooms;

    void splitBSP(BSPNode* node, int depth);
    void createRooms(BSPNode* node, Map& map);
    void connectRooms(BSPNode* node, Map& map);
    void carveRoom(Map& map, const Room& r);
    void carveCorridor(Map& map, Vec2 a, Vec2 b);
    void placeStairs(Map& map, int floor);
    void freeBSP(BSPNode* node);
    static Room findRoom(BSPNode* node);
    void assignThemes(int floor);
    void applyThemeTiles(Map& map);
};
