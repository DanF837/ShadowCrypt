#include "Input.h"

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
static char getchUnix() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}
#endif

bool Input::keyPending() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
#endif
}

Key Input::getKey() {
#ifdef _WIN32
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        ch = _getch();
        switch (ch) {
            case 72: return Key::Up;
            case 80: return Key::Down;
            case 75: return Key::Left;
            case 77: return Key::Right;
        }
        return Key::None;
    }
#else
    char ch = getchUnix();
    if (ch == 27) {
        char seq1 = getchUnix();
        if (seq1 == '[') {
            char seq2 = getchUnix();
            switch (seq2) {
                case 'A': return Key::Up;
                case 'B': return Key::Down;
                case 'D': return Key::Left;
                case 'C': return Key::Right;
            }
        }
        return Key::Escape;
    }
#endif

    switch (ch) {
        case 'w': case 'W': return Key::Up;
        case 's': case 'S': return Key::Down;
        case 'a': case 'A': return Key::Left;
        case 'd': case 'D': return Key::Right;
        case 'e': case 'E': return Key::Use;
        case 'i': case 'I': return Key::Inventory;
        case '>': case '.': return Key::Stairs;
        case 'q': case 'Q': return Key::Quit;
        case 'h': case 'H': return Key::Help;
        case 'r': case 'R': return Key::Ability;
        case 'f': case 'F': return Key::Save;
        case 'l': case 'L': return Key::Load;
        case 'x': case 'X': return Key::Look;
        case 'z': case 'Z': return Key::AutoExplore;
        case 27:            return Key::Escape;
        case '1':           return Key::Num1;
        case '2':           return Key::Num2;
        case '3':           return Key::Num3;
        case '4':           return Key::Num4;
        case '5':           return Key::Num5;
        case '6':           return Key::Num6;
        case '7':           return Key::Num7;
        case '8':           return Key::Num8;
        case '9':           return Key::Num9;
        case '0':           return Key::Num0;
    }
    return Key::None;
}
