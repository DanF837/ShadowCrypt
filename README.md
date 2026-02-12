# Shadowcrypt

A terminal-based roguelike dungeon crawler built with C++17 using ANSI escape codes for rendering.

## Gameplay

Descend through 8 floors of a dungeon across 3 biomes, fighting enemies, collecting enchanted loot, trading with merchants, and leveling up. Defeat the Dragon mid-boss on floor 5 and slay the Lich on floor 8 to win.

### Controls

| Key               | Action                              |
| ----------------- | ----------------------------------- |
| WASD / Arrow Keys | Move                                |
| E                 | Pick up items / Trade with merchant |
| I                 | Open inventory                      |
| R                 | Use class ability                   |
| X                 | Examine (look mode)                 |
| Z                 | Auto-explore                        |
| > or .            | Descend stairs                      |
| F                 | Save game                           |
| Q                 | Quit                                |

### Classes

| Class   | HP  | ATK | DEF | Ability                           |
| ------- | --- | --- | --- | --------------------------------- |
| Warrior | 40  | 4   | 4   | Shield Bash (stun + bonus damage) |
| Rogue   | 25  | 7   | 1   | Backstab (3x damage)              |
| Mage    | 20  | 8   | 1   | Fireball (AoE damage)             |
| Cleric  | 35  | 3   | 3   | Divine Heal (50% HP restore)      |

### Biomes

| Floors | Biome   | Visual                |
| ------ | ------- | --------------------- |
| 1-3    | Stone   | Classic dungeon       |
| 4-5    | Cave    | `%` walls, `,` floors |
| 6-8    | Inferno | Red walls, heavy lava |

### Symbols

| Symbol | Meaning        |
| ------ | -------------- |
| `@`    | Player         |
| `r`    | Rat            |
| `s`    | Skeleton       |
| `g`    | Ghost          |
| `D`    | Demon / Dragon |
| `a`    | Archer         |
| `n`    | Necromancer    |
| `L`    | Lich           |
| `M`    | Merchant       |
| `$`    | Gold           |
| `!`    | Potion         |
| `?`    | Scroll         |
| `/`    | Weapon         |
| `[`    | Armor          |
| `>`    | Stairs down    |
| `#`    | Wall           |
| `.`    | Floor          |

### Features

- **Ai\* Pathfinding** - Enemies navigate around obstacles intelligently
- **Status Effects** - Poison, burning, blind, slow, and haste
- **Enchantment System** - Rusty, Sharp, Flaming, Frozen, Vampiric, Blessed, and Legendary gear
- **Gold Economy** - Collect gold from enemies and buy items from merchants
- **Look Mode** - Examine any tile for detailed information
- **Auto-explore** - Automatically walk to unexplored areas
- **Boss Fights** - Dragon mid-boss (floor 5), Lich final boss (floor 8)
- **Enemy Abilities** - Necromancers summon skeletons, Liches raise undead, Dragons enrage

## Building

Requires a C++17 compiler (e.g. MinGW g++).

```
mingw32-make
```

## Running

```
.\shadowcrypt.exe
```
