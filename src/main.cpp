#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <array>
#include <print>
#include <iostream>

const int SCREEN_WIDTH{800};
const int SCREEN_HEIGHT{600};
const int HEX_SIZE{16};
const float HEX_RADIUS{std::sqrt(3.0f) * HEX_SIZE};

class Hex
{
public:
    int q;
    int r;
    int s;

    constexpr Hex(int q_in, int r_in, int s_in)
        : q(q_in), r(r_in), s(s_in)
    {
        // Ensure the constraint is checked at compile time if possible
        assert(q + r + s == 0 && "Invalid Hex: q + r + s must equal 0");
    }

    bool operator==(const Hex &other) const
    {
        return q == other.q && r == other.r;
    }

    Vector2 toPixel() const
    {
        return {
            HEX_SIZE * ((std::sqrt(3.0f) * static_cast<float>(q)) + ((std::sqrt(3.0f) / 2) * static_cast<float>(r))),
            HEX_SIZE * ((3.0f / 2) * static_cast<float>(r))};
    }
};

constexpr Hex hexAdd(const Hex &a, const Hex &b)
{
    return {a.q + b.q, a.r + b.r, a.s + b.s};
}

constexpr Hex hexSubtract(const Hex &a, const Hex &b)
{
    return {a.q - b.q, a.r - b.r, a.s - b.s};
}

constexpr Hex hexMultiply(const Hex &a, int scalar)
{
    return {a.q * scalar, a.r * scalar, a.s * scalar};
}

constexpr int hexLength(const Hex &hex)
{
    return int((abs(hex.q) + abs(hex.r) + abs(hex.s)) / 2);
}

constexpr int hexDistance(const Hex &a, const Hex &b)
{
    return hexLength(hexSubtract(a, b));
}

enum class HexDirection
{
    East,      // right  (1, 0, -1)
    SouthEast, // up-right (1, -1, 0)
    SouthWest, // up-left (0, -1, 1)
    West,      // left (-1, 0, 1)
    NorthWest, // down-left (-1, 1, 0)
    NorthEast  // down-right (0, 1, -1)
};

constexpr std::array<Hex, 6> hex_directions = {
    Hex(1, 0, -1), Hex(1, -1, 0), Hex(0, -1, 1),
    Hex(-1, 0, 1), Hex(-1, 1, 0), Hex(0, 1, -1)};

constexpr const Hex &hexDirection(HexDirection direction)
{
    return hex_directions[(size_t)direction];
}

constexpr Hex hexNeighbour(const Hex &hex, HexDirection direction)
{
    return hexAdd(hex, hexDirection(direction));
}

// This code is implementing a hash function for a custom Hex class to allow it to be used in hash-based containers like std::unordered_map or std::unordered_set.
namespace std
{
    template <>
    struct hash<Hex>
    {
        size_t operator()(const Hex &h) const
        {
            hash<int> int_hash;
            size_t hq = int_hash(h.q);
            size_t hr = int_hash(h.r);
            // magic constant from golden ratio for better distribution.
            return hq ^ (hr + 0x9e3779b9 + (hq << 6) + (hq >> 2));
        }
    };
}

const std::array<Color, 3> availableColors{
    ORANGE,
    MAROON,
    LIME};

class Cell
{
public:
    std::optional<Hex> rotatingTo;
    float rotationProgress;
    Color color;
    Cell() : color(availableColors[static_cast<size_t>(GetRandomValue(0, availableColors.size() - 1))]) {}

    void startRotation(const Hex &hex)
    {
        rotatingTo = hex;
        rotationProgress = 0.0f;
    }

    void stepRotation(float dt)
    {
        if (rotatingTo)
        {
            float newRotation = rotationProgress + dt * 4.0f;
            if (newRotation > 1.0f)
            {
                newRotation = 1.0f;
            }
            rotationProgress = newRotation;
        }
    }

    bool rotationDone() const
    {
        return !rotatingTo.has_value() || rotationProgress >= 1.0f;
    }

    void resetRotation()
    {
        rotationProgress = -1.0f;
        rotatingTo.reset();
    }
};

/**
 * Rotates a point around a pivot by a certain progress towards a 120-degree clockwise rotation.
 *
 * @param start The starting point to be rotated.
 * @param pivot The pivot point around which the rotation occurs.
 * @param progress The progress of the rotation, where 0.0f is the start and 1.0f is the end of the rotation.
 * @return The new position of the point after rotation.
 */
Vector2 rotatePoint(const Vector2 &start, const Vector2 &end, const Vector2 &pivot, const float progress)
{
    // Calculate vector from pivot to start and end
    Vector2 startRelative = Vector2Subtract(start, pivot);
    Vector2 endRelative = Vector2Subtract(end, pivot);

    // Get the angle using atan2
    float startAngle = atan2f(startRelative.y, startRelative.x);
    float endAngle = atan2f(endRelative.y, endRelative.x);

    // Ensure shortest rotation path
    float angleDiff = endAngle - startAngle;
    if (angleDiff > PI)
        angleDiff -= 2 * PI;
    else if (angleDiff < -PI)
        angleDiff += 2 * PI;

    // Interpolate angle
    float currentAngle = startAngle + angleDiff * progress;

    // Calculate radius (distance from pivot to start)
    float radius = Vector2Length(startRelative);

    // Compute new position
    return {
        pivot.x + cosf(currentAngle) * radius,
        pivot.y + sinf(currentAngle) * radius};
}

Vector2 hexesPixelPivot(const std::array<Hex, 3> &hexes)
{
    Vector2 sum = {0, 0};
    for (const Hex &hex : hexes)
    {
        sum = Vector2Add(sum, hex.toPixel());
    }
    return Vector2Scale(sum, 1.0f / 3.0f);
}

class HexMap
{
    std::unordered_map<Hex, Cell> cells;
    std::optional<std::array<Hex, 3>> rotation;

public:
    using iterator = typename std::unordered_map<Hex, Cell>::iterator;
    using const_iterator = typename std::unordered_map<Hex, Cell>::const_iterator;

    iterator begin() { return cells.begin(); }
    iterator end() { return cells.end(); }

    const_iterator begin() const { return cells.begin(); }
    const_iterator end() const { return cells.end(); }

    const auto &getRotation() const { return rotation; }
    bool hasRotation() const { return rotation.has_value(); }

    void insert(const Hex &hex, const Cell &cell)
    {
        cells.emplace(hex, cell);
    }

    void startRotation(const std::array<Hex, 3> &hexes)
    {
        rotation = hexes;
        auto &rot = *rotation;

        Cell &cell0 = cells.at(rot[0]);
        Cell &cell1 = cells.at(rot[1]);
        Cell &cell2 = cells.at(rot[2]);

        cell1.startRotation(rot[0]);
        cell2.startRotation(rot[1]);
        cell0.startRotation(rot[2]);
    }

    void stepRotation(float dt)
    {
        if (rotation)
        {
            auto &rot = *rotation;
            Cell &cell0 = cells.at(rot[0]);
            Cell &cell1 = cells.at(rot[1]);
            Cell &cell2 = cells.at(rot[2]);

            cell0.stepRotation(dt);
            cell1.stepRotation(dt);
            cell2.stepRotation(dt);

            if (cell0.rotationDone() && cell1.rotationDone() && cell2.rotationDone())
            {
                std::swap(cell0, cells[*cell0.rotatingTo]);
                std::swap(cell1, cells[*cell1.rotatingTo]);
                std::swap(cell2, cells[*cell2.rotatingTo]);

                cell0.resetRotation();
                cell1.resetRotation();
                cell2.resetRotation();

                rotation.reset();
            }
        }
    }

    const Cell &at(const Hex &h) const
    {
        return cells.at(h);
    }
};

class Cursor
{
    std::array<Hex, 3> hexes;

public:
    Cursor(Hex topHex) : hexes{
                             topHex,
                             hexNeighbour(topHex, HexDirection::NorthWest),
                             hexNeighbour(topHex, HexDirection::NorthEast)} {}

    const auto &getHexes() const { return hexes; }

    void moveUp()
    {
        if (hexes[0].r % 2 == 0)
        {
            move(HexDirection::SouthEast);
        }
        else
        {
            move(HexDirection::SouthWest);
        }
    }

    void moveDown()
    {
        if (hexes[0].r % 2 == 0)
        {
            move(HexDirection::NorthEast);
        }
        else
        {
            move(HexDirection::NorthWest);
        }
    }

    void moveLeft()
    {
        move(HexDirection::West);
    }

    void moveRight()
    {
        move(HexDirection::East);
    }

    void move(HexDirection hexDir)
    {
        hexes[0] = hexNeighbour(hexes[0], hexDir);
        hexes[1] = hexNeighbour(hexes[1], hexDir);
        hexes[2] = hexNeighbour(hexes[2], hexDir);
    }
};

HexMap generateHexMap(int size)
{
    HexMap hexMap = HexMap();
    for (int q = -size; q <= size; q++)
    {
        int r1 = std::max(-size, -q - size);
        int r2 = std::min(size, -q + size);
        for (int r = r1; r <= r2; r++)
        {
            hexMap.insert(Hex(q, r, -q - r), Cell());
        }
    }
    return hexMap;
}

void drawGrid(HexMap &hexMap, const Cursor &cursor)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    const auto &rotation = hexMap.getRotation();

    for (auto &pair : hexMap)
    {
        const Hex &hex = pair.first;
        const Cell &cell = pair.second;

        Vector2 pos;
        if (cell.rotatingTo.has_value() && rotation)
        {

            const Vector2 startPos = hex.toPixel();
            const Vector2 endPos = cell.rotatingTo->toPixel();
            pos = rotatePoint(startPos, endPos, hexesPixelPivot(*rotation), cell.rotationProgress);
        }
        else
        {
            pos = hex.toPixel();
        }

        pos.x += SCREEN_WIDTH / 2.f;
        pos.y += SCREEN_HEIGHT / 2.f;

        DrawPoly(pos, 6, HEX_SIZE, 30, cell.color);
        DrawPolyLinesEx(pos, 6, HEX_SIZE, 30, 1, WHITE);
    }

    for (const auto &hex : cursor.getHexes())
    {
        Vector2 pos = hex.toPixel();
        pos.x += SCREEN_WIDTH / 2.f;
        pos.y += SCREEN_HEIGHT / 2.f;

        DrawPolyLinesEx(pos, 6, HEX_SIZE, 30, 3, BLACK);
    }

    /*
    // DEBUG CODE FOR VISUALIZING ROTAION OF HEXES
    Vector2 circlePivot = hexesPixelPivot(cursor.getHexes());
    circlePivot.x += SCREEN_WIDTH / 2.f;
    circlePivot.y += SCREEN_HEIGHT / 2.f;

    DrawCircleV(circlePivot, 10, PINK);
    auto hexes = cursor.getHexes();
    auto hex1 = hexes[0];
    auto hex2 = hexes[1];
    auto hex3 = hexes[2];
    auto hexPos1 = rotatePoint(hex1.toPixel(), hex2.toPixel(), hexesPixelPivot(cursor.getHexes()), 0);
    // auto hexPos1 = hex2.toPixel();
    hexPos1.x += (SCREEN_WIDTH / 2.f);
    hexPos1.y += (SCREEN_HEIGHT / 2.f);
    auto hexPos2 = rotatePoint(hex1.toPixel(), hex2.toPixel(), hexesPixelPivot(cursor.getHexes()), 0.5);
    hexPos2.x += (SCREEN_WIDTH / 2.f);
    hexPos2.y += (SCREEN_HEIGHT / 2.f);
    auto hexPos3 = rotatePoint(hex1.toPixel(), hex2.toPixel(), hexesPixelPivot(cursor.getHexes()), 1);
    hexPos3.x += (SCREEN_WIDTH / 2.f);
    hexPos3.y += (SCREEN_HEIGHT / 2.f);

    DrawCircleV(hexPos1, 10, PURPLE);
    DrawCircleV(hexPos2, 10, BROWN);
    DrawCircleV(hexPos3, 10, BLACK);
    */

    EndDrawing();
}

int main()
{
    // Initialize the Window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "My Game");

    // Setting the Frames Per Second
    SetTargetFPS(60);

    HexMap hexMap = generateHexMap(10);
    Cursor cursor = Cursor(Hex(2, 2, -4));

    // The Game Loop
    while (!WindowShouldClose() /*WindowShouldClose returns true if esc is clicked and closes the window*/)
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_UP))
        {
            cursor.moveUp();
        }
        else if (IsKeyPressed(KEY_DOWN))
        {
            cursor.moveDown();
        }
        else if (IsKeyPressed(KEY_LEFT))
        {
            cursor.moveLeft();
        }
        else if (IsKeyPressed(KEY_RIGHT))
        {
            cursor.moveRight();
        }
        else if (IsKeyPressed(KEY_SPACE) && !hexMap.hasRotation())
        {
            hexMap.startRotation(cursor.getHexes());
        }

        hexMap.stepRotation(dt);

        drawGrid(hexMap, cursor);
    }
    CloseWindow();
    return 0;
}
