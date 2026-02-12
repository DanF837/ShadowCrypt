// Standalone screenshot: generates one dungeon frame and prints it
#include "Map.h"
#include "DungeonGen.h"
#include "Entity.h"
#include "Renderer.h"
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>

int main() {
    std::srand((unsigned)std::time(nullptr));

    // --- Title screen ---
    Renderer renderer;
    renderer.renderTitle();
    printf("\n\n--- PRESS ANY KEY (simulated) ---\n\n");

    // --- Generate floor 1 ---
    Map map;
    DungeonGen gen;
    gen.generate(map, 1);

    Player player;
    auto& rooms = gen.getRooms();
    if (!rooms.empty()) player.init(rooms.front().center());

    // Spawn some enemies
    std::vector<Enemy> enemies;
    for (int i = 0; i < 5 && rooms.size() > 1; i++) {
        int ri = 1 + std::rand() % ((int)rooms.size() - 1);
        const Room& r = rooms[ri];
        int ex = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int ey = r.y + 1 + std::rand() % std::max(1, r.h - 2);
        if (map.isWalkable(ex, ey)) {
            enemies.push_back(Enemy::create(Enemy::randomForFloor(1), {ex, ey}));
        }
    }

    // Spawn some items
    std::vector<Item> items;
    for (int i = 0; i < 3 && rooms.size() > 1; i++) {
        int ri = 1 + std::rand() % ((int)rooms.size() - 1);
        const Room& r = rooms[ri];
        int ix = r.x + 1 + std::rand() % std::max(1, r.w - 2);
        int iy = r.y + 1 + std::rand() % std::max(1, r.h - 2);
        if (map.isWalkable(ix, iy))
            items.emplace_back(Vec2{ix, iy}, "Health Potion", '!', ItemType::HealthPotion, 12);
    }

    // Compute FOV
    FOV::compute(map, player.pos);

    // Render the game screen
    std::vector<std::string> log = {
        "You enter the dungeon...",
        "Floor 1.",
        "A rat scurries in the darkness.",
        "You see a potion on the ground.",
        "Press WASD to move, E to pick up."
    };
    renderer.render(map, player, enemies, items, log, 1);

    printf("\n\n--- INVENTORY SCREEN ---\n\n");

    // Show inventory screen with a sample item
    player.inventory.add(Item({0,0}, "Health Potion", '!', ItemType::HealthPotion, 12));
    player.inventory.add(Item({0,0}, "Short Sword", '/', ItemType::Weapon, 5));
    renderer.renderInventory(player);

    printf("\n\n--- GAME OVER SCREEN ---\n\n");
    renderer.renderGameOver(3, 4);

    printf("\n\n--- VICTORY SCREEN ---\n\n");
    renderer.renderWin(7);

    printf("\n");
    return 0;
}
