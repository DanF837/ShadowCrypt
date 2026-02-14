# Shadowcrypt

A terminal-based roguelike dungeon crawler built with C++17 using ANSI escape codes for rendering.

## Gameplay

Descend through 8 floors of a dungeon across 3 biomes, fighting enemies, collecting enchanted loot, trading with merchants, and leveling up. Defeat the Dragon mid-boss on floor 5 and slay the Lich on floor 8 to win.

### Controls

| Key               | Action                              |
| ----------------- | ----------------------------------- |
| WASD / Arrow Keys | Move                                |
| E                 | Pick up items / Trade with merchant |
| S                 | Sell items (at merchant)            |
| M                 | Message log                         |
| I                 | Open inventory                      |
| R                 | Use class ability                   |
| X                 | Examine (look mode)                 |
| Z                 | Auto-explore                        |
| T                 | Wait/rest in place                  |
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
- **Gold Economy** - Gold auto-pickup on walk, buy and sell items at merchants
- **Difficulty Modes** - Easy, Normal, and Hard settings
- **Level-Up Choices** - Pick Vitality, Power, or Fortitude on each level up
- **Critical Hits** - 10% chance for double damage (both sides)
- **Consumables** - Health/Shield Potions, Teleport Scrolls, Bombs, stat scrolls
- **Look Mode** - Examine any tile for detailed information
- **Auto-explore** - Automatically walk toward enemies, items, and unexplored areas
- **Boss Fights** - Dragon mid-boss (floor 5), Lich final boss (floor 8) with phase changes
- **Enemy Abilities** - Necromancers summon skeletons, Liches raise undead and teleport, Dragons enrage
- **Death Recap** - Detailed stats screen showing kills, damage, equipment on death
- **Message Log** - Scroll through your full message history
- **Coloured Messages** - Combat, healing, gold, and status messages colour-coded for quick scanning
- **Enemy HP Display** - Visible enemies shown with HP in the HUD
- **Equipment Comparison** - See stat differences when picking up or examining gear
- **Wait/Rest** - Press T to skip your turn and let enemies come to you

## Building

Requires a C++17 compiler.

### Windows (MinGW)

```
mingw32-make
.\shadowcrypt.exe
```

### Linux

```
make
./shadowcrypt
```

### macOS

```
make
./shadowcrypt
```

On macOS you may need to install Xcode Command Line Tools (`xcode-select --install`) if `g++` is not available.
