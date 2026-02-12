#pragma once

enum class Key {
    Up, Down, Left, Right,
    Use,        // 'e' — pick up / use
    Inventory,  // 'i' — open inventory
    Stairs,     // '>' — descend stairs
    Quit,       // 'q'
    Help,       // 'h' — help screen
    Ability,    // 'r' — use class ability
    Save,       // 'f' — save game
    Load,       // 'l' — load game
    Look,       // 'x' — examine
    AutoExplore,// 'z' — auto-explore
    None,
    Num1, Num2, Num3, Num4, Num5,
    Num6, Num7, Num8, Num9, Num0,
    Escape
};

namespace Input {
    Key getKey();
    bool keyPending();
}
