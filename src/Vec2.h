#pragma once

struct Vec2 {
    int x = 0;
    int y = 0;

    Vec2() = default;
    Vec2(int x, int y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }

    int distanceSq(const Vec2& o) const {
        int dx = x - o.x, dy = y - o.y;
        return dx * dx + dy * dy;
    }
};
