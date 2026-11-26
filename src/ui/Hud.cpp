#include "ui/Hud.h"

#include <algorithm>
#include <cmath>

#include "game/Archery.h"
#include "game/Gameplay.h"
#include "game/Magic.h"
#include "game/Smithing.h"

namespace journey {

namespace {
constexpr int kTabCount = 8;
const char* kTabLabels[kTabCount] = {"Bag", "Skills", "Gear", "Quests", "Map", "Combat", "Magic", "Menu"};
constexpr int kBankCols = 8, kBankRows = 5, kBankPerPage = kBankCols * kBankRows;
const Color kPanel = {16, 20, 28, 205};
const Color kPanelSolid = {16, 20, 28, 232};
const Color kBorder = {255, 255, 255, 40};
const Color kText = {236, 240, 246, 255};
const Color kDim = {160, 170, 186, 255};
const Color kAccent = {255, 205, 90, 255};
const Color kCell = {255, 255, 255, 16};

constexpr int kBagCols = 4, kBagRows = 7;

void Panel(Rectangle r, float round, Color fill = kPanel) {
    Rectangle border = {r.x - 1, r.y - 1, r.width + 2, r.height + 2};
    DrawRectangleRounded(border, round, 8, kBorder);
    DrawRectangleRounded(r, round, 8, fill);
}

void DrawRightAligned(const Font& font, const std::string& text, float rightX, float y, float size, Color c) {
    const Vector2 sz = MeasureTextEx(font, text.c_str(), size, 1.0f);
    DrawTextEx(font, text.c_str(), {rightX - sz.x, y}, size, 1.0f, c);
}

std::string WithCommas(int64_t n) {
    std::string s = std::to_string(n);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(i, ",");
    return s;
}

constexpr float kActionBarH = 66.0f;   // height of the item-action strip under the bag grid (before scaling)

// A filled triangle whichever way round the corners are given (raylib only draws counter-clockwise ones).
void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) DrawTriangle(a, c, b, col);
    else DrawTriangle(a, b, c, col);
}

namespace {
// A skull: a pale round head, two dark eyes, a nose and a jaw with teeth.
void DrawSkull(Vector2 c, float r) {
    DrawCircleV(c, r * 1.12f, {0, 0, 0, 220});
    DrawCircleV({c.x, c.y - r * 0.1f}, r, {238, 234, 220, 255});
    DrawRectangleRec({c.x - r * 0.55f, c.y + r * 0.4f, r * 1.1f, r * 0.75f}, {238, 234, 220, 255});
    DrawCircleV({c.x - r * 0.38f, c.y - r * 0.12f}, r * 0.27f, {20, 16, 20, 255});
    DrawCircleV({c.x + r * 0.38f, c.y - r * 0.12f}, r * 0.27f, {20, 16, 20, 255});
    Tri({c.x, c.y + r * 0.1f}, {c.x - r * 0.12f, c.y + r * 0.34f}, {c.x + r * 0.12f, c.y + r * 0.34f}, {20, 16, 20, 255});
    for (int i = -1; i <= 1; ++i) DrawRectangleRec({c.x + i * r * 0.32f - r * 0.04f, c.y + r * 0.62f, r * 0.08f, r * 0.5f}, {60, 54, 56, 255});
}
}  // namespace

// A little fish seen from the side: body, belly, tail, eye.
void DrawFishIcon(float cx, float cy, float u, Color body, Color belly, Color fin, bool charred) {
    Tri({cx + 0.22f * u, cy}, {cx + 0.40f * u, cy - 0.15f * u}, {cx + 0.40f * u, cy + 0.15f * u}, fin);
    DrawEllipse(static_cast<int>(cx - 0.02f * u), static_cast<int>(cy), 0.27f * u, 0.14f * u, body);
    DrawEllipse(static_cast<int>(cx - 0.02f * u), static_cast<int>(cy + 0.05f * u), 0.22f * u, 0.07f * u, belly);
    Tri({cx - 0.06f * u, cy - 0.12f * u}, {cx + 0.06f * u, cy - 0.12f * u}, {cx, cy - 0.21f * u}, fin);   // back fin
    DrawCircle(static_cast<int>(cx - 0.16f * u), static_cast<int>(cy - 0.03f * u), 0.027f * u, charred ? Color{200, 200, 200, 255} : Color{20, 20, 24, 255});
}

// A curled shrimp: a row of shrinking circles along an arch, a tail fan and two feelers.
void DrawShrimpIcon(float cx, float cy, float u, Color body, Color shell) {
    Vector2 prev{};
    for (int i = 0; i <= 8; ++i) {
        const float t = i / 8.0f;
        const float ang = PI * (1.12f + 0.80f * t);
        const Vector2 p = {cx + std::cos(ang) * 0.30f * u, cy + 0.16f * u + std::sin(ang) * 0.30f * u};
        DrawCircleV(p, (0.105f - 0.055f * t) * u, (i % 2) ? shell : body);
        prev = p;
    }
    Tri({prev.x, prev.y - 0.02f * u}, {prev.x + 0.10f * u, prev.y + 0.12f * u}, {prev.x - 0.04f * u, prev.y + 0.14f * u}, body);
    const Vector2 head = {cx + std::cos(PI * 1.12f) * 0.30f * u, cy + 0.16f * u + std::sin(PI * 1.12f) * 0.30f * u};
    DrawLineEx(head, {head.x - 0.12f * u, head.y - 0.20f * u}, 0.018f * u, shell);
    DrawLineEx(head, {head.x - 0.20f * u, head.y - 0.10f * u}, 0.018f * u, shell);
    DrawCircleV({head.x + 0.03f * u, head.y - 0.03f * u}, 0.022f * u, {20, 20, 24, 255});
}


// A drumstick: a bone with a knob, and a plump piece of meat on the end.
void DrawDrumstickIcon(float cx, float cy, float u, Color meat, Color dark) {
    const Color bone = {240, 232, 210, 255};
    DrawLineEx({cx - 0.04f * u, cy + 0.06f * u}, {cx - 0.30f * u, cy + 0.30f * u}, 0.075f * u, bone);
    DrawCircleV({cx - 0.34f * u, cy + 0.27f * u}, 0.065f * u, bone);
    DrawCircleV({cx - 0.27f * u, cy + 0.35f * u}, 0.065f * u, bone);
    DrawEllipse(static_cast<int>(cx + 0.08f * u), static_cast<int>(cy - 0.06f * u), 0.27f * u, 0.22f * u, meat);
    DrawEllipse(static_cast<int>(cx + 0.14f * u), static_cast<int>(cy - 0.02f * u), 0.17f * u, 0.14f * u, dark);
    DrawEllipse(static_cast<int>(cx + 0.02f * u), static_cast<int>(cy - 0.14f * u), 0.10f * u, 0.05f * u, Fade(WHITE, 0.25f));
}

// A steak: a rounded slab with a rim of fat and (when cooked) grill marks.
void DrawSteakIcon(float cx, float cy, float u, Color body, Color rim, bool marks) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.02f * u), 0.36f * u, 0.27f * u, rim);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.31f * u, 0.22f * u, body);
    if (marks)
        for (int i = -1; i <= 1; ++i)
            DrawLineEx({cx + i * 0.11f * u - 0.07f * u, cy - 0.15f * u}, {cx + i * 0.11f * u + 0.07f * u, cy + 0.15f * u}, 0.04f * u, {52, 34, 24, 255});
    else
        DrawEllipse(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.05f * u), 0.10f * u, 0.05f * u, Fade(WHITE, 0.28f));
}

void DrawBonesIcon(float cx, float cy, float u) {
    const Color bone = {240, 232, 210, 255}, shade = {196, 186, 160, 255};
    for (int k = 0; k < 2; ++k) {
        const float sgn = k == 0 ? 1.0f : -1.0f;
        const Vector2 a = {cx - 0.30f * u, cy + sgn * 0.30f * u}, b = {cx + 0.30f * u, cy - sgn * 0.30f * u};
        DrawLineEx(a, b, 0.085f * u, k == 0 ? bone : shade);
        for (const Vector2& e : {a, b})
            for (int side = -1; side <= 1; side += 2) {
                const float nx = sgn * 0.045f * u * side, ny = 0.045f * u * side;
                DrawCircleV({e.x + nx, e.y + ny}, 0.062f * u, k == 0 ? bone : shade);
            }
    }
}

void DrawEggIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.03f * u), 0.20f * u, 0.27f * u, {242, 232, 211, 255});
    DrawEllipse(static_cast<int>(cx - 0.06f * u), static_cast<int>(cy - 0.05f * u), 0.06f * u, 0.10f * u, Fade(WHITE, 0.55f));
    DrawEllipse(static_cast<int>(cx + 0.07f * u), static_cast<int>(cy + 0.12f * u), 0.10f * u, 0.12f * u, Fade({200, 180, 150, 255}, 0.35f));
}

void DrawHideIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.36f * u, 0.30f * u, {138, 90, 52, 255});
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) DrawCircleV({cx + sx * 0.29f * u, cy + sy * 0.22f * u}, 0.10f * u, {138, 90, 52, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.27f * u, 0.21f * u, {108, 69, 39, 255});
    DrawEllipse(static_cast<int>(cx - 0.08f * u), static_cast<int>(cy - 0.02f * u), 0.12f * u, 0.09f * u, {42, 38, 35, 255});
    DrawEllipse(static_cast<int>(cx + 0.13f * u), static_cast<int>(cy + 0.08f * u), 0.07f * u, 0.06f * u, {42, 38, 35, 255});
}


// ---- metal gear icons. `m` is the metal's colour; the light and dark shades are worked out from it.
Color Shade(Rgb c, float k, unsigned char a = 255) {
    auto f = [&](unsigned char v) { return static_cast<unsigned char>(std::clamp(v * k, 0.0f, 255.0f)); };
    return {f(c.r), f(c.g), f(c.b), a};
}

void DrawHaft(float cx, float cy, float u, float x0, float y0, float x1, float y1, float w = 0.075f) {
    DrawLineEx({cx + x0 * u, cy + y0 * u}, {cx + x1 * u, cy + y1 * u}, w * u, {122, 82, 44, 255});
}

void DrawDaggerIcon(float cx, float cy, float u, Rgb m) {
    DrawLineEx({cx - 0.34f * u, cy + 0.34f * u}, {cx - 0.14f * u, cy + 0.14f * u}, 0.085f * u, {92, 60, 32, 255});           // grip
    DrawLineEx({cx - 0.22f * u, cy + 0.02f * u}, {cx - 0.02f * u, cy + 0.22f * u}, 0.07f * u, Shade(m, 0.65f));              // guard
    Tri({cx - 0.12f * u, cy + 0.04f * u}, {cx - 0.04f * u, cy + 0.12f * u}, {cx + 0.34f * u, cy - 0.34f * u}, Shade(m, 1.0f)); // blade
    DrawLineEx({cx - 0.06f * u, cy + 0.06f * u}, {cx + 0.28f * u, cy - 0.28f * u}, 0.025f * u, Shade(m, 1.35f));
}

void DrawSwordIcon(float cx, float cy, float u, Rgb m) {
    DrawLineEx({cx - 0.38f * u, cy + 0.38f * u}, {cx - 0.22f * u, cy + 0.22f * u}, 0.085f * u, {92, 60, 32, 255});
    DrawCircleV({cx - 0.40f * u, cy + 0.40f * u}, 0.05f * u, Shade(m, 0.65f));
    DrawLineEx({cx - 0.32f * u, cy + 0.06f * u}, {cx - 0.08f * u, cy + 0.30f * u}, 0.07f * u, Shade(m, 0.65f));
    Tri({cx - 0.19f * u, cy + 0.09f * u}, {cx - 0.09f * u, cy + 0.19f * u}, {cx + 0.42f * u, cy - 0.42f * u}, Shade(m, 1.0f));
    DrawLineEx({cx - 0.12f * u, cy + 0.12f * u}, {cx + 0.34f * u, cy - 0.34f * u}, 0.028f * u, Shade(m, 1.35f));
}

void DrawAxeIcon(float cx, float cy, float u, Rgb m) {
    DrawHaft(cx, cy, u, -0.34f, 0.38f, 0.14f, -0.30f, 0.07f);
    const Vector2 c = {cx + 0.14f * u, cy - 0.17f * u};
    Tri({c.x - 0.04f * u, c.y - 0.10f * u}, {c.x + 0.06f * u, c.y + 0.08f * u}, {c.x - 0.30f * u, c.y - 0.20f * u}, Shade(m, 1.0f));      // upper blade
    Tri({c.x - 0.30f * u, c.y - 0.20f * u}, {c.x - 0.24f * u, c.y + 0.12f * u}, {c.x + 0.02f * u, c.y + 0.02f * u}, Shade(m, 0.82f));
    Tri({c.x + 0.02f * u, c.y - 0.06f * u}, {c.x + 0.10f * u, c.y + 0.10f * u}, {c.x + 0.36f * u, c.y + 0.12f * u}, Shade(m, 1.0f));      // back blade
    Tri({c.x + 0.36f * u, c.y + 0.12f * u}, {c.x + 0.30f * u, c.y - 0.12f * u}, {c.x + 0.02f * u, c.y - 0.06f * u}, Shade(m, 0.82f));
    DrawLineEx({c.x - 0.30f * u, c.y - 0.20f * u}, {c.x - 0.24f * u, c.y + 0.12f * u}, 0.025f * u, Shade(m, 1.4f));
}

void DrawMaceIcon(float cx, float cy, float u, Rgb m) {
    DrawHaft(cx, cy, u, -0.34f, 0.38f, 0.10f, -0.12f, 0.08f);
    const Vector2 c = {cx + 0.16f * u, cy - 0.20f * u};
    for (int i = 0; i < 8; ++i) {   // spikes
        const float a = i * PI / 4.0f;
        Tri({c.x + std::cos(a - 0.2f) * 0.12f * u, c.y + std::sin(a - 0.2f) * 0.12f * u},
            {c.x + std::cos(a + 0.2f) * 0.12f * u, c.y + std::sin(a + 0.2f) * 0.12f * u},
            {c.x + std::cos(a) * 0.27f * u, c.y + std::sin(a) * 0.27f * u}, Shade(m, 0.8f));
    }
    DrawCircleV(c, 0.15f * u, Shade(m, 1.0f));
    DrawCircleV({c.x - 0.04f * u, c.y - 0.04f * u}, 0.06f * u, Shade(m, 1.4f));
}

void DrawPickaxeIcon(float cx, float cy, float u, Rgb m) {
    DrawHaft(cx, cy, u, -0.32f, 0.38f, 0.14f, -0.20f, 0.075f);
    const Vector2 a = {cx - 0.20f * u, cy - 0.28f * u}, b = {cx + 0.12f * u, cy - 0.40f * u}, c = {cx + 0.38f * u, cy - 0.06f * u};
    DrawLineEx(a, b, 0.10f * u, Shade(m, 1.0f));
    DrawLineEx(b, c, 0.10f * u, Shade(m, 1.0f));
    Tri({a.x - 0.04f * u, a.y - 0.02f * u}, {a.x + 0.02f * u, a.y + 0.06f * u}, {a.x - 0.10f * u, a.y + 0.14f * u}, Shade(m, 1.3f));
    Tri({c.x + 0.06f * u, c.y - 0.04f * u}, {c.x - 0.02f * u, c.y + 0.06f * u}, {c.x + 0.10f * u, c.y + 0.16f * u}, Shade(m, 1.3f));
    DrawLineEx({a.x + 0.04f * u, a.y - 0.04f * u}, {b.x, b.y - 0.04f * u}, 0.025f * u, Shade(m, 1.4f));
}

void DrawHelmIcon(float cx, float cy, float u, Rgb m) {
    DrawCircleV({cx, cy - 0.02f * u}, 0.31f * u, Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.31f * u), static_cast<int>(cy + 0.0f * u), static_cast<int>(0.62f * u), static_cast<int>(0.14f * u), Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.31f * u), static_cast<int>(cy - 0.06f * u), static_cast<int>(0.62f * u), static_cast<int>(0.07f * u), Shade(m, 0.7f));   // brow band
    DrawRectangle(static_cast<int>(cx - 0.20f * u), static_cast<int>(cy + 0.01f * u), static_cast<int>(0.40f * u), static_cast<int>(0.20f * u), {28, 28, 34, 255});    // face opening
    DrawRectangle(static_cast<int>(cx - 0.025f * u), static_cast<int>(cy - 0.06f * u), static_cast<int>(0.05f * u), static_cast<int>(0.22f * u), Shade(m, 1.3f));   // nose guard
    DrawRectangle(static_cast<int>(cx - 0.31f * u), static_cast<int>(cy + 0.0f * u), static_cast<int>(0.11f * u), static_cast<int>(0.27f * u), Shade(m, 0.85f));     // cheeks
    DrawRectangle(static_cast<int>(cx + 0.20f * u), static_cast<int>(cy + 0.0f * u), static_cast<int>(0.11f * u), static_cast<int>(0.27f * u), Shade(m, 0.85f));
    DrawEllipse(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.20f * u), 0.10f * u, 0.05f * u, Fade(WHITE, 0.30f));
}

void DrawShieldIcon(float cx, float cy, float u, Rgb m) {
    const Vector2 tl = {cx - 0.30f * u, cy - 0.34f * u}, tr = {cx + 0.30f * u, cy - 0.34f * u}, ml = {cx - 0.32f * u, cy - 0.02f * u},
                  mr = {cx + 0.32f * u, cy - 0.02f * u}, tip = {cx, cy + 0.42f * u};
    Tri(tl, tr, mr, Shade(m, 0.65f));
    Tri(tl, mr, ml, Shade(m, 0.65f));
    Tri(ml, mr, tip, Shade(m, 0.65f));
    const Vector2 itl = {cx - 0.24f * u, cy - 0.28f * u}, itr = {cx + 0.24f * u, cy - 0.28f * u}, iml = {cx - 0.26f * u, cy - 0.02f * u},
                  imr = {cx + 0.26f * u, cy - 0.02f * u}, itip = {cx, cy + 0.33f * u};
    Tri(itl, itr, imr, Shade(m, 1.0f));
    Tri(itl, imr, iml, Shade(m, 1.0f));
    Tri(iml, imr, itip, Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.03f * u), static_cast<int>(cy - 0.26f * u), static_cast<int>(0.06f * u), static_cast<int>(0.50f * u), Shade(m, 0.55f));
    DrawRectangle(static_cast<int>(cx - 0.20f * u), static_cast<int>(cy - 0.14f * u), static_cast<int>(0.40f * u), static_cast<int>(0.06f * u), Shade(m, 0.55f));
    DrawCircleV({cx, cy - 0.11f * u}, 0.07f * u, Shade(m, 1.35f));
}

void DrawBodyIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangleRounded({cx - 0.25f * u, cy - 0.28f * u, 0.50f * u, 0.62f * u}, 0.22f, 6, Shade(m, 1.0f));
    DrawCircleV({cx - 0.30f * u, cy - 0.24f * u}, 0.14f * u, Shade(m, 0.8f));       // shoulder plates
    DrawCircleV({cx + 0.30f * u, cy - 0.24f * u}, 0.14f * u, Shade(m, 0.8f));
    DrawRectangleRounded({cx - 0.36f * u, cy - 0.20f * u, 0.14f * u, 0.38f * u}, 0.5f, 5, Shade(m, 0.75f));    // arm plates
    DrawRectangleRounded({cx + 0.22f * u, cy - 0.20f * u, 0.14f * u, 0.38f * u}, 0.5f, 5, Shade(m, 0.75f));
    DrawRectangle(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.30f * u), static_cast<int>(0.20f * u), static_cast<int>(0.07f * u), {26, 28, 34, 255});   // neck
    DrawRectangle(static_cast<int>(cx - 0.025f * u), static_cast<int>(cy - 0.20f * u), static_cast<int>(0.05f * u), static_cast<int>(0.42f * u), Shade(m, 1.35f));  // keel
    DrawRectangle(static_cast<int>(cx - 0.25f * u), static_cast<int>(cy + 0.20f * u), static_cast<int>(0.50f * u), static_cast<int>(0.06f * u), Shade(m, 0.55f));  // belt
    DrawEllipse(static_cast<int>(cx - 0.13f * u), static_cast<int>(cy - 0.14f * u), 0.07f * u, 0.04f * u, Fade(WHITE, 0.28f));
}

void DrawLegsIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangle(static_cast<int>(cx - 0.26f * u), static_cast<int>(cy - 0.34f * u), static_cast<int>(0.52f * u), static_cast<int>(0.10f * u), Shade(m, 0.6f));       // belt
    for (int side = -1; side <= 1; side += 2) {
        const float x = cx + side * 0.13f * u;
        DrawRectangleRounded({x - 0.12f * u, cy - 0.26f * u, 0.24f * u, 0.38f * u}, 0.3f, 5, Shade(m, 1.0f));           // thigh
        DrawCircleV({x, cy + 0.10f * u}, 0.09f * u, Shade(m, 0.7f));                                                      // knee
        DrawRectangleRounded({x - 0.10f * u, cy + 0.12f * u, 0.20f * u, 0.30f * u}, 0.35f, 5, Shade(m, 0.9f));           // greave
        DrawLineEx({x - 0.04f * u, cy - 0.20f * u}, {x - 0.04f * u, cy + 0.04f * u}, 0.03f * u, Shade(m, 1.35f));
    }
}


void DrawHatchetIcon(float cx, float cy, float u, Rgb m) {
    const Color metal = {m.r, m.g, m.b, 255};
    DrawLineEx({cx - 0.30f * u, cy + 0.32f * u}, {cx + 0.20f * u, cy - 0.26f * u}, 0.085f * u, {122, 82, 44, 255});
    Tri({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.26f * u, cy - 0.12f * u}, {cx - 0.06f * u, cy - 0.12f * u}, Shade(m, 0.85f));
    Tri({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.40f * u, cy - 0.30f * u}, {cx + 0.26f * u, cy - 0.12f * u}, metal);
    DrawLineEx({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.40f * u, cy - 0.30f * u}, 0.025f * u, Shade(m, 1.4f));
}


// ---- round 10 gear: scimitar, longsword, spear, warhammer, full helm, square shield, chainbody
void DrawScimitarIcon(float cx, float cy, float u, Rgb m) {
    DrawLineEx({cx - 0.38f * u, cy + 0.38f * u}, {cx - 0.24f * u, cy + 0.24f * u}, 0.085f * u, {92, 60, 32, 255});   // grip
    DrawLineEx({cx - 0.30f * u, cy + 0.08f * u}, {cx - 0.08f * u, cy + 0.30f * u}, 0.07f * u, Shade(m, 0.65f));       // guard
    const Vector2 p0 = {cx - 0.14f * u, cy + 0.14f * u}, p1 = {cx + 0.34f * u, cy - 0.02f * u}, p2 = {cx + 0.36f * u, cy - 0.42f * u};
    Vector2 prev = p0;
    for (int i = 1; i <= 10; ++i) {   // the blade: a curve that bellies out to the right
        const float t = i / 10.0f, a = (1 - t) * (1 - t), b = 2 * (1 - t) * t, c = t * t;
        const Vector2 p = {a * p0.x + b * p1.x + c * p2.x, a * p0.y + b * p1.y + c * p2.y};
        const float w = (0.15f - 0.11f * t) * u;
        DrawLineEx(prev, p, w, Shade(m, 1.0f));
        DrawCircleV(p, w / 2, Shade(m, 1.0f));
        DrawLineEx({prev.x - 0.025f * u, prev.y - 0.015f * u}, {p.x - 0.025f * u, p.y - 0.015f * u}, 0.022f * u, Shade(m, 1.4f));
        prev = p;
    }
}

void DrawLongswordIcon(float cx, float cy, float u, Rgb m) {
    DrawLineEx({cx - 0.40f * u, cy + 0.40f * u}, {cx - 0.28f * u, cy + 0.28f * u}, 0.085f * u, {92, 60, 32, 255});
    DrawCircleV({cx - 0.42f * u, cy + 0.42f * u}, 0.055f * u, Shade(m, 0.65f));
    DrawLineEx({cx - 0.40f * u, cy + 0.14f * u}, {cx - 0.14f * u, cy + 0.40f * u}, 0.075f * u, Shade(m, 0.65f));   // a broad guard
    Tri({cx - 0.27f * u, cy + 0.15f * u}, {cx - 0.15f * u, cy + 0.27f * u}, {cx + 0.46f * u, cy - 0.46f * u}, Shade(m, 1.0f));
    Tri({cx - 0.21f * u, cy + 0.21f * u}, {cx - 0.15f * u, cy + 0.27f * u}, {cx + 0.46f * u, cy - 0.46f * u}, Shade(m, 0.8f));   // the shaded half
    DrawLineEx({cx - 0.16f * u, cy + 0.16f * u}, {cx + 0.40f * u, cy - 0.40f * u}, 0.022f * u, Shade(m, 1.4f));
}

void DrawSpearIcon(float cx, float cy, float u, Rgb m) {
    DrawLineEx({cx - 0.42f * u, cy + 0.42f * u}, {cx + 0.20f * u, cy - 0.20f * u}, 0.06f * u, {122, 82, 44, 255});
    const Vector2 tip = {cx + 0.46f * u, cy - 0.46f * u}, base = {cx + 0.16f * u, cy - 0.16f * u};
    const Vector2 mid = {cx + 0.27f * u, cy - 0.27f * u}, perp = {0.075f * u, 0.075f * u};
    Tri(base, {mid.x + perp.x, mid.y + perp.y}, tip, Shade(m, 1.0f));
    Tri(base, {mid.x - perp.x, mid.y - perp.y}, tip, Shade(m, 0.78f));
    DrawLineEx(base, tip, 0.02f * u, Shade(m, 1.4f));
    DrawLineEx({cx + 0.12f * u, cy - 0.12f * u}, {cx + 0.22f * u, cy - 0.22f * u}, 0.09f * u, Shade(m, 0.6f));   // the socket
}

void DrawWarhammerIcon(float cx, float cy, float u, Rgb m) {
    DrawHaft(cx, cy, u, -0.38f, 0.40f, 0.14f, -0.22f, 0.075f);
    const Rectangle head = {cx + 0.16f * u, cy - 0.24f * u, 0.56f * u, 0.24f * u};
    DrawRectanglePro(head, {head.width / 2, head.height / 2}, 45.0f, Shade(m, 1.0f));
    DrawRectanglePro({head.x, head.y, head.width * 0.92f, head.height * 0.30f}, {head.width * 0.46f, head.height * 0.55f}, 45.0f, Shade(m, 1.3f));
    DrawRectanglePro({head.x, head.y, 0.08f * u, head.height}, {0.04f * u, head.height / 2}, 45.0f, Shade(m, 0.6f));
    DrawRectanglePro({head.x, head.y, head.width * 0.92f, 0.05f * u}, {head.width * 0.46f, -head.height * 0.20f}, 45.0f, Shade(m, 0.7f));
}

void DrawFullHelmIcon(float cx, float cy, float u, Rgb m) {
    DrawCircleV({cx, cy - 0.06f * u}, 0.31f * u, Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.31f * u), static_cast<int>(cy - 0.06f * u), static_cast<int>(0.62f * u), static_cast<int>(0.40f * u), Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.31f * u), static_cast<int>(cy + 0.24f * u), static_cast<int>(0.62f * u), static_cast<int>(0.10f * u), Shade(m, 0.7f));   // chin
    DrawRectangle(static_cast<int>(cx - 0.24f * u), static_cast<int>(cy - 0.05f * u), static_cast<int>(0.48f * u), static_cast<int>(0.07f * u), {18, 20, 26, 255});   // eye slit
    for (int i = 0; i < 4; ++i)   // breathing holes
        DrawCircleV({cx + (0.06f + i * 0.065f) * u, cy + 0.17f * u}, 0.014f * u, {20, 22, 28, 255});
    DrawRectangle(static_cast<int>(cx - 0.025f * u), static_cast<int>(cy - 0.36f * u), static_cast<int>(0.05f * u), static_cast<int>(0.58f * u), Shade(m, 1.3f));    // ridge
    DrawEllipse(static_cast<int>(cx - 0.14f * u), static_cast<int>(cy - 0.22f * u), 0.09f * u, 0.05f * u, Fade(WHITE, 0.30f));
}

void DrawSquareShieldIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangleRounded({cx - 0.33f * u, cy - 0.34f * u, 0.66f * u, 0.68f * u}, 0.18f, 6, Shade(m, 0.62f));
    DrawRectangleRounded({cx - 0.27f * u, cy - 0.28f * u, 0.54f * u, 0.56f * u}, 0.16f, 6, Shade(m, 1.0f));
    DrawRectangle(static_cast<int>(cx - 0.03f * u), static_cast<int>(cy - 0.27f * u), static_cast<int>(0.06f * u), static_cast<int>(0.54f * u), Shade(m, 0.55f));
    DrawRectangle(static_cast<int>(cx - 0.26f * u), static_cast<int>(cy - 0.03f * u), static_cast<int>(0.52f * u), static_cast<int>(0.06f * u), Shade(m, 0.55f));
    DrawCircleV({cx, cy}, 0.09f * u, Shade(m, 1.35f));
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) DrawCircleV({cx + sx * 0.21f * u, cy + sy * 0.21f * u}, 0.025f * u, Shade(m, 1.4f));
}

void DrawChainbodyIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangleRounded({cx - 0.26f * u, cy - 0.28f * u, 0.52f * u, 0.62f * u}, 0.18f, 6, Shade(m, 0.8f));
    DrawRectangleRounded({cx - 0.40f * u, cy - 0.26f * u, 0.16f * u, 0.24f * u}, 0.5f, 5, Shade(m, 0.7f));   // short sleeves
    DrawRectangleRounded({cx + 0.24f * u, cy - 0.26f * u, 0.16f * u, 0.24f * u}, 0.5f, 5, Shade(m, 0.7f));
    DrawRectangle(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.30f * u), static_cast<int>(0.20f * u), static_cast<int>(0.07f * u), {26, 28, 34, 255});   // neck
    for (int row = 0; row < 7; ++row)          // the little rings of the mail
        for (int col = 0; col < 5; ++col) {
            const float x = cx + (-0.17f + col * 0.085f + (row % 2) * 0.042f) * u, y = cy + (-0.20f + row * 0.075f) * u;
            DrawCircleLines(static_cast<int>(x), static_cast<int>(y), 0.032f * u, Shade(m, 1.35f));
        }
    DrawRectangle(static_cast<int>(cx - 0.26f * u), static_cast<int>(cy + 0.28f * u), static_cast<int>(0.52f * u), static_cast<int>(0.06f * u), Shade(m, 0.5f));
}


// ---- round 11: boots, gloves and capes
void DrawBootsIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangleRounded({cx - 0.24f * u, cy - 0.34f * u, 0.28f * u, 0.52f * u}, 0.3f, 5, Shade(m, 1.0f));                // the shaft
    DrawRectangleRounded({cx - 0.24f * u, cy + 0.02f * u, 0.56f * u, 0.26f * u}, 0.45f, 6, Shade(m, 1.0f));              // the foot, toe to the right
    DrawRectangle(static_cast<int>(cx - 0.24f * u), static_cast<int>(cy + 0.24f * u), static_cast<int>(0.56f * u), static_cast<int>(0.07f * u), Shade(m, 0.5f));   // the sole
    DrawRectangle(static_cast<int>(cx - 0.26f * u), static_cast<int>(cy - 0.36f * u), static_cast<int>(0.32f * u), static_cast<int>(0.08f * u), Shade(m, 0.72f));  // the cuff
    DrawLineEx({cx - 0.15f * u, cy - 0.24f * u}, {cx - 0.15f * u, cy + 0.06f * u}, 0.03f * u, Shade(m, 1.4f));
    DrawLineEx({cx + 0.05f * u, cy + 0.10f * u}, {cx + 0.22f * u, cy + 0.12f * u}, 0.025f * u, Shade(m, 1.35f));
}

void DrawGlovesIcon(float cx, float cy, float u, Rgb m) {
    DrawRectangleRounded({cx - 0.20f * u, cy - 0.04f * u, 0.40f * u, 0.36f * u}, 0.3f, 6, Shade(m, 1.0f));                // the palm
    for (int i = 0; i < 4; ++i)                                                                                           // four fingers
        DrawRectangleRounded({cx - 0.20f * u + i * 0.105f * u, cy - 0.34f * u + (i == 1 || i == 2 ? 0.0f : 0.05f) * u, 0.09f * u, 0.36f * u}, 0.5f, 5, Shade(m, 1.0f));
    DrawRectangleRounded({cx - 0.34f * u, cy + 0.02f * u, 0.16f * u, 0.12f * u}, 0.5f, 5, Shade(m, 0.9f));                // the thumb
    DrawRectangle(static_cast<int>(cx - 0.22f * u), static_cast<int>(cy + 0.26f * u), static_cast<int>(0.44f * u), static_cast<int>(0.10f * u), Shade(m, 0.65f));   // the cuff
    DrawLineEx({cx - 0.12f * u, cy - 0.20f * u}, {cx - 0.12f * u, cy + 0.02f * u}, 0.02f * u, Shade(m, 1.4f));
}

// A cape hanging from a clasp, edged in a second colour.
void DrawCapeIcon(float cx, float cy, float u, Rgb cloth, Rgb trim) {
    const Color c = {cloth.r, cloth.g, cloth.b, 255}, e = {trim.r, trim.g, trim.b, 255};
    const Vector2 tl = {cx - 0.20f * u, cy - 0.34f * u}, tr = {cx + 0.20f * u, cy - 0.34f * u};
    const Vector2 bl = {cx - 0.36f * u, cy + 0.36f * u}, br = {cx + 0.36f * u, cy + 0.36f * u};
    Tri(tl, tr, br, c);
    Tri(tl, br, bl, c);
    Tri({cx - 0.02f * u, cy - 0.34f * u}, tr, br, Shade(cloth, 0.8f));      // a fold in the cloth
    DrawLineEx(bl, br, 0.06f * u, e);
    DrawLineEx(tl, bl, 0.03f * u, e);
    DrawLineEx(tr, br, 0.03f * u, e);
    DrawCircleV({cx, cy - 0.34f * u}, 0.07f * u, {232, 190, 70, 255});     // the clasp
    DrawCircleV({cx - 0.02f * u, cy - 0.36f * u}, 0.025f * u, Fade(WHITE, 0.6f));
}

void DrawGearIcon(Item item, float cx, float cy, float u) {
    Tier t;
    GearType g;
    if (!GearOf(item, &t, &g)) return;
    const Rgb m = TierColor(t);
    switch (g) {
        case GearType::Dagger: DrawDaggerIcon(cx, cy, u, m); break;
        case GearType::Sword: DrawSwordIcon(cx, cy, u, m); break;
        case GearType::Axe: DrawAxeIcon(cx, cy, u, m); break;
        case GearType::Mace: DrawMaceIcon(cx, cy, u, m); break;
        case GearType::Helm: DrawHelmIcon(cx, cy, u, m); break;
        case GearType::Shield: DrawShieldIcon(cx, cy, u, m); break;
        case GearType::Legs: DrawLegsIcon(cx, cy, u, m); break;
        case GearType::Body: DrawBodyIcon(cx, cy, u, m); break;
        case GearType::Pickaxe: DrawPickaxeIcon(cx, cy, u, m); break;
        case GearType::Hatchet: DrawHatchetIcon(cx, cy, u, m); break;
        case GearType::Arrows: break;
        case GearType::Scimitar: DrawScimitarIcon(cx, cy, u, m); break;
        case GearType::Longsword: DrawLongswordIcon(cx, cy, u, m); break;
        case GearType::Spear: DrawSpearIcon(cx, cy, u, m); break;
        case GearType::Warhammer: DrawWarhammerIcon(cx, cy, u, m); break;
        case GearType::Fullhelm: DrawFullHelmIcon(cx, cy, u, m); break;
        case GearType::SquareShield: DrawSquareShieldIcon(cx, cy, u, m); break;
        case GearType::Chainbody: DrawChainbodyIcon(cx, cy, u, m); break;
        case GearType::Boots: DrawBootsIcon(cx, cy, u, m); break;
        case GearType::Gauntlets: DrawGlovesIcon(cx, cy, u, m); break;
    }
}

// A lump of ore: grey stone with flecks (or crystals) of the metal showing in it.
void DrawOreIcon(float cx, float cy, float u, Color fleck, Color light, bool crystals) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.08f * u), 0.34f * u, 0.27f * u, {112, 114, 122, 255});
    DrawEllipse(static_cast<int>(cx - 0.05f * u), static_cast<int>(cy + 0.03f * u), 0.28f * u, 0.22f * u, {148, 150, 158, 255});
    DrawEllipse(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.05f * u), 0.12f * u, 0.07f * u, Fade(WHITE, 0.25f));
    const Vector2 spots[5] = {{-0.17f, -0.02f}, {0.06f, -0.10f}, {0.17f, 0.10f}, {-0.06f, 0.14f}, {0.24f, -0.04f}};
    for (int i = 0; i < 5; ++i) {
        const Vector2 p = {cx + spots[i].x * u, cy + spots[i].y * u};
        if (crystals) {
            Tri({p.x - 0.05f * u, p.y + 0.06f * u}, {p.x + 0.05f * u, p.y + 0.06f * u}, {p.x + (i % 2 ? 0.02f : -0.02f) * u, p.y - 0.15f * u}, i % 2 ? fleck : light);
        } else {
            DrawCircleV(p, (0.058f + 0.012f * (i % 2)) * u, i % 2 ? fleck : light);
        }
    }
}

void DrawBarIcon(float cx, float cy, float u, Rgb m) {
    auto ingot = [&](float x, float y, float w) {
        const float h = 0.15f * u, tw = w * 0.8f;
        Tri({x - w / 2, y + h / 2}, {x + w / 2, y + h / 2}, {x + tw / 2, y - h / 2}, Shade(m, 0.8f));
        Tri({x - w / 2, y + h / 2}, {x + tw / 2, y - h / 2}, {x - tw / 2, y - h / 2}, Shade(m, 0.8f));
        DrawRectangle(static_cast<int>(x - tw / 2), static_cast<int>(y - h / 2), static_cast<int>(tw), static_cast<int>(h * 0.35f), Shade(m, 1.25f));   // bright top
    };
    ingot(cx - 0.17f * u, cy + 0.17f * u, 0.36f * u);
    ingot(cx + 0.17f * u, cy + 0.17f * u, 0.36f * u);
    ingot(cx, cy - 0.02f * u, 0.36f * u);
}

void DrawHammerIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.30f * u, cy + 0.34f * u}, {cx + 0.12f * u, cy - 0.12f * u}, 0.085f * u, {122, 82, 44, 255});
    DrawRectanglePro({cx + 0.18f * u, cy - 0.20f * u, 0.42f * u, 0.22f * u}, {0.21f * u, 0.11f * u}, -42.0f, {70, 72, 80, 255});
    DrawRectanglePro({cx + 0.18f * u, cy - 0.20f * u, 0.12f * u, 0.22f * u}, {0.21f * u + 0.0f, 0.11f * u}, -42.0f, {176, 182, 194, 255});
    DrawRectanglePro({cx + 0.18f * u, cy - 0.20f * u, 0.42f * u, 0.05f * u}, {0.21f * u, 0.11f * u}, -42.0f, Fade(WHITE, 0.25f));
}


// A little heap of gold coins.
void DrawCoinsIcon(float cx, float cy, float u) {
    const Color gold = {232, 184, 58, 255}, light = {255, 222, 112, 255}, dark = {170, 126, 30, 255};
    const Vector2 pos[4] = {{-0.17f, 0.14f}, {0.17f, 0.14f}, {0.0f, 0.04f}, {0.0f, -0.12f}};
    for (int i = 0; i < 4; ++i) {
        const Vector2 p = {cx + pos[i].x * u, cy + pos[i].y * u};
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y + 0.05f * u), 0.20f * u, 0.095f * u, dark);
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), 0.20f * u, 0.095f * u, i % 2 ? gold : light);
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), 0.12f * u, 0.055f * u, dark);
        DrawEllipse(static_cast<int>(p.x - 0.03f * u), static_cast<int>(p.y - 0.01f * u), 0.07f * u, 0.03f * u, Fade(WHITE, 0.35f));
    }
}

void DrawBreadIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.05f * u), 0.36f * u, 0.22f * u, {138, 90, 42, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.34f * u, 0.20f * u, {196, 134, 68, 255});
    DrawEllipse(static_cast<int>(cx - 0.05f * u), static_cast<int>(cy - 0.05f * u), 0.22f * u, 0.10f * u, {222, 166, 92, 255});
    for (int i = -1; i <= 1; ++i)
        DrawLineEx({cx + i * 0.12f * u - 0.04f * u, cy - 0.12f * u}, {cx + i * 0.12f * u + 0.05f * u, cy + 0.08f * u}, 0.03f * u, {150, 98, 46, 255});
}

// A bow seen from the side: the limb bulges one way, the string is straight, a grip wrap in the middle.
void DrawBowIcon(float cx, float cy, float u, Color wood, bool longBow) {
    const float h = (longBow ? 0.44f : 0.36f) * u;
    Vector2 prev{};
    for (int i = 0; i <= 12; ++i) {
        const float t = -1.0f + i / 6.0f;
        const Vector2 p = {cx - 0.16f * u + 0.24f * u * t * t, cy + t * h};
        if (i > 0) DrawLineEx(prev, p, 0.065f * u, wood);
        prev = p;
    }
    DrawLineEx({cx + 0.08f * u, cy - h}, {cx + 0.08f * u, cy + h}, 0.012f * u, {236, 230, 204, 255});
    DrawLineEx({cx - 0.16f * u, cy - 0.06f * u}, {cx - 0.16f * u, cy + 0.06f * u}, 0.085f * u, {92, 60, 32, 255});
    DrawCircleV({cx + 0.08f * u, cy - h}, 0.03f * u, {70, 46, 24, 255});
    DrawCircleV({cx + 0.08f * u, cy + h}, 0.03f * u, {70, 46, 24, 255});
}

// Arrows in a little bunch (the head's colour is the metal's).
void DrawArrowsIcon(float cx, float cy, float u, Rgb m) {
    const Color metal = {m.r, m.g, m.b, 255};
    for (int k = -1; k <= 1; ++k) {
        const float ox = k * 0.10f * u, oy = k * 0.05f * u;
        const Vector2 a = {cx - 0.30f * u + ox, cy + 0.30f * u + oy}, b = {cx + 0.24f * u + ox, cy - 0.26f * u + oy};
        DrawLineEx(a, b, 0.04f * u, {170, 128, 76, 255});
        Tri({b.x + 0.12f * u, b.y - 0.12f * u}, {b.x - 0.02f * u, b.y - 0.07f * u}, {b.x - 0.07f * u, b.y + 0.02f * u}, metal);
        DrawLineEx({a.x, a.y}, {a.x + 0.09f * u, a.y - 0.09f * u}, 0.075f * u, {236, 236, 228, 255});
    }
}

void DrawRuneIcon(float cx, float cy, float u, Rune r) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.03f * u), 0.33f * u, 0.31f * u, {84, 86, 96, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.31f * u, 0.30f * u, {132, 136, 148, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.26f * u, 0.25f * u, {158, 162, 174, 255});
    switch (r) {
        case Rune::Air: {   // a swirl
            const Color c = {232, 240, 246, 255};
            Vector2 prev{};
            for (int i = 0; i <= 14; ++i) {
                const float a = i * 0.55f, rad = (0.03f + i * 0.011f) * u;
                const Vector2 p = {cx + std::cos(a) * rad, cy + std::sin(a) * rad};
                if (i > 0) DrawLineEx(prev, p, 0.035f * u, c);
                prev = p;
            }
            break;
        }
        case Rune::Water:
            DrawCircleV({cx, cy + 0.05f * u}, 0.10f * u, {70, 140, 255, 255});
            Tri({cx, cy - 0.17f * u}, {cx - 0.085f * u, cy + 0.02f * u}, {cx + 0.085f * u, cy + 0.02f * u}, {70, 140, 255, 255});
            break;
        case Rune::Earth:
            Tri({cx, cy - 0.16f * u}, {cx + 0.14f * u, cy}, {cx, cy + 0.16f * u}, {150, 108, 52, 255});
            Tri({cx, cy - 0.16f * u}, {cx - 0.14f * u, cy}, {cx, cy + 0.16f * u}, {118, 82, 38, 255});
            break;
        case Rune::Fire:
            Tri({cx, cy - 0.18f * u}, {cx - 0.12f * u, cy + 0.13f * u}, {cx + 0.12f * u, cy + 0.13f * u}, {255, 104, 28, 255});
            Tri({cx, cy - 0.04f * u}, {cx - 0.055f * u, cy + 0.13f * u}, {cx + 0.055f * u, cy + 0.13f * u}, {255, 214, 96, 255});
            break;
        case Rune::Mind:
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 0.13f * u, {232, 112, 208, 255});
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 0.075f * u, {232, 112, 208, 255});
            DrawCircleV({cx, cy}, 0.03f * u, {232, 112, 208, 255});
            break;
        case Rune::Chaos:
            DrawLineEx({cx - 0.12f * u, cy - 0.12f * u}, {cx + 0.12f * u, cy + 0.12f * u}, 0.05f * u, {200, 34, 44, 255});
            DrawLineEx({cx + 0.12f * u, cy - 0.12f * u}, {cx - 0.12f * u, cy + 0.12f * u}, 0.05f * u, {200, 34, 44, 255});
            break;
    }
}

void DrawStaffIcon(float cx, float cy, float u, int kind) {   // 0 plain, 1 air, 2 water, 3 earth, 4 fire
    DrawLineEx({cx - 0.32f * u, cy + 0.36f * u}, {cx + 0.20f * u, cy - 0.22f * u}, 0.07f * u, {138, 94, 52, 255});
    DrawLineEx({cx - 0.10f * u, cy + 0.12f * u}, {cx + 0.02f * u, cy - 0.01f * u}, 0.085f * u, {82, 54, 30, 255});
    const Vector2 top = {cx + 0.26f * u, cy - 0.28f * u};
    static const Color kOrb[5] = {{110, 76, 44, 255}, {232, 242, 246, 255}, {70, 140, 255, 255}, {124, 90, 44, 255}, {255, 106, 26, 255}};
    if (kind > 0) DrawCircleV(top, 0.19f * u, Fade(kOrb[kind], 0.25f));
    DrawCircleV(top, 0.12f * u, kOrb[kind]);
    if (kind > 0) DrawCircleV({top.x - 0.03f * u, top.y - 0.03f * u}, 0.04f * u, Fade(WHITE, 0.7f));
}

void DrawWizardIcon(float cx, float cy, float u, int part) {   // 0 hat, 1 robe, 2 skirt
    const Color cloth = {54, 82, 184, 255}, dark = {38, 57, 127, 255}, gold = {227, 195, 107, 255};
    if (part == 0) {
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.20f * u), 0.34f * u, 0.09f * u, dark);
        Tri({cx + 0.10f * u, cy - 0.40f * u}, {cx - 0.20f * u, cy + 0.20f * u}, {cx + 0.20f * u, cy + 0.20f * u}, cloth);
        DrawLineEx({cx - 0.19f * u, cy + 0.14f * u}, {cx + 0.19f * u, cy + 0.14f * u}, 0.05f * u, gold);
    } else if (part == 1) {
        DrawRectangleRounded({cx - 0.18f * u, cy - 0.28f * u, 0.36f * u, 0.56f * u}, 0.2f, 6, cloth);
        DrawRectangleRounded({cx - 0.34f * u, cy - 0.24f * u, 0.18f * u, 0.46f * u}, 0.3f, 6, dark);
        DrawRectangleRounded({cx + 0.16f * u, cy - 0.24f * u, 0.18f * u, 0.46f * u}, 0.3f, 6, dark);
        DrawLineEx({cx - 0.18f * u, cy + 0.02f * u}, {cx + 0.18f * u, cy + 0.02f * u}, 0.05f * u, gold);
        DrawLineEx({cx, cy - 0.26f * u}, {cx, cy + 0.26f * u}, 0.025f * u, gold);
    } else {
        Tri({cx - 0.12f * u, cy - 0.26f * u}, {cx + 0.12f * u, cy - 0.26f * u}, {cx + 0.30f * u, cy + 0.28f * u}, cloth);
        Tri({cx - 0.12f * u, cy - 0.26f * u}, {cx - 0.30f * u, cy + 0.28f * u}, {cx + 0.30f * u, cy + 0.28f * u}, cloth);
        DrawLineEx({cx - 0.29f * u, cy + 0.26f * u}, {cx + 0.29f * u, cy + 0.26f * u}, 0.045f * u, gold);
        DrawLineEx({cx - 0.13f * u, cy - 0.22f * u}, {cx + 0.13f * u, cy - 0.22f * u}, 0.05f * u, gold);
    }
}


// ---- icons for the gathering and crafting items
void DrawLogIcon(float cx, float cy, float u, Color bark, Color cut, Color ring) {
    const Rectangle body = {cx, cy, u * 0.62f, u * 0.26f};
    DrawRectanglePro(body, {body.width / 2, body.height / 2}, -35.0f, bark);
    DrawRectanglePro({cx, cy - u * 0.02f, u * 0.62f, u * 0.08f}, {u * 0.31f, u * 0.13f}, -35.0f, Color{static_cast<unsigned char>(std::min(255, bark.r + 28)), static_cast<unsigned char>(std::min(255, bark.g + 22)), static_cast<unsigned char>(std::min(255, bark.b + 14)), 255});
    const float ex = cx + std::cos(-35.0f * DEG2RAD) * u * 0.31f, ey = cy + std::sin(-35.0f * DEG2RAD) * u * 0.31f;
    DrawCircle(static_cast<int>(ex), static_cast<int>(ey), u * 0.14f, cut);
    DrawCircleLines(static_cast<int>(ex), static_cast<int>(ey), u * 0.09f, ring);
    DrawCircleLines(static_cast<int>(ex), static_cast<int>(ey), u * 0.04f, ring);
}

void DrawRodIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.34f * u, cy + 0.36f * u}, {cx + 0.30f * u, cy - 0.34f * u}, 0.05f * u, {150, 104, 58, 255});
    DrawCircleV({cx - 0.14f * u, cy + 0.14f * u}, 0.07f * u, {110, 112, 120, 255});
    DrawLineEx({cx + 0.30f * u, cy - 0.34f * u}, {cx + 0.38f * u, cy + 0.20f * u}, 0.012f * u, {236, 232, 210, 255});
    DrawCircleV({cx + 0.38f * u, cy + 0.22f * u}, 0.03f * u, {220, 70, 60, 255});
}

void DrawHarpoonIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.34f * u, cy + 0.36f * u}, {cx + 0.22f * u, cy - 0.22f * u}, 0.05f * u, {150, 104, 58, 255});
    const Color iron = {170, 176, 188, 255};
    Tri({cx + 0.38f * u, cy - 0.38f * u}, {cx + 0.16f * u, cy - 0.28f * u}, {cx + 0.28f * u, cy - 0.16f * u}, iron);
    Tri({cx + 0.12f * u, cy - 0.14f * u}, {cx + 0.24f * u, cy - 0.30f * u}, {cx + 0.28f * u, cy - 0.10f * u}, iron);
}

void DrawPotIcon(float cx, float cy, float u, Color body) {   // a lobster pot (wicker) or a clay pot
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.20f * u), 0.28f * u, 0.09f * u, Shade(Rgb{body.r, body.g, body.b}, 0.7f));
    DrawRectangleRounded({cx - 0.27f * u, cy - 0.15f * u, 0.54f * u, 0.36f * u}, 0.45f, 8, body);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.15f * u), 0.27f * u, 0.08f * u, Shade(Rgb{body.r, body.g, body.b}, 1.2f));
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.15f * u), 0.20f * u, 0.05f * u, Shade(Rgb{body.r, body.g, body.b}, 0.5f));
}

void DrawCageIcon(float cx, float cy, float u) {
    const Color wicker = {176, 138, 78, 255};
    DrawRectangleRounded({cx - 0.30f * u, cy - 0.22f * u, 0.60f * u, 0.46f * u}, 0.4f, 8, wicker);
    for (int i = -2; i <= 2; ++i) DrawLineEx({cx + i * 0.11f * u, cy - 0.20f * u}, {cx + i * 0.11f * u, cy + 0.22f * u}, 0.02f * u, {110, 80, 40, 255});
    for (int i = -1; i <= 1; ++i) DrawLineEx({cx - 0.28f * u, cy + i * 0.14f * u}, {cx + 0.28f * u, cy + i * 0.14f * u}, 0.02f * u, {110, 80, 40, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.02f * u), 0.07f * u, 0.11f * u, {40, 30, 20, 255});
}

void DrawFeathersIcon(float cx, float cy, float u) {
    for (int k = -1; k <= 1; ++k) {
        const float a = k * 0.45f;
        const Vector2 b = {cx + std::sin(a) * 0.04f * u, cy + 0.30f * u};
        const Vector2 t = {cx + std::sin(a) * 0.40f * u, cy - 0.30f * u + static_cast<float>(std::abs(k)) * 0.07f * u};
        DrawLineEx(b, t, 0.03f * u, {200, 196, 180, 255});
        Tri({t.x, t.y}, {(b.x + t.x) / 2 - 0.10f * u, (b.y + t.y) / 2}, {(b.x + t.x) / 2 + 0.10f * u, (b.y + t.y) / 2}, k == 0 ? Color{240, 240, 236, 255} : Color{214, 120, 80, 255});
    }
}

void DrawBigFishIcon(float cx, float cy, float u, Color body, Color belly, Color fin, bool sword, bool lobster, bool cooked) {
    if (lobster) {
        const Color c = cooked ? Color{232, 84, 56, 255} : Color{60, 80, 120, 255};
        DrawEllipse(static_cast<int>(cx - 0.05f * u), static_cast<int>(cy + 0.02f * u), 0.28f * u, 0.12f * u, c);
        DrawEllipse(static_cast<int>(cx + 0.18f * u), static_cast<int>(cy - 0.02f * u), 0.10f * u, 0.09f * u, Shade(Rgb{c.r, c.g, c.b}, 0.85f));
        for (int s = -1; s <= 1; s += 2) {
            DrawLineEx({cx + 0.22f * u, cy - 0.03f * u}, {cx + 0.40f * u, cy - 0.17f * u * s * -1.0f}, 0.04f * u, c);
            DrawCircleV({cx + 0.40f * u, cy + s * 0.15f * u}, 0.06f * u, c);
        }
        Tri({cx - 0.30f * u, cy}, {cx - 0.44f * u, cy - 0.10f * u}, {cx - 0.44f * u, cy + 0.10f * u}, Shade(Rgb{c.r, c.g, c.b}, 0.8f));
        return;
    }
    DrawFishIcon(cx, cy, u * 1.15f, body, belly, fin, false);
    if (sword) DrawLineEx({cx + 0.28f * u, cy - 0.02f * u}, {cx + 0.46f * u, cy - 0.06f * u}, 0.035f * u, {200, 206, 216, 255});
}

void DrawGemIcon(float cx, float cy, float u, Color c, bool cut) {
    if (cut) {
        const Vector2 p[6] = {{cx - 0.14f * u, cy - 0.20f * u}, {cx + 0.14f * u, cy - 0.20f * u}, {cx + 0.28f * u, cy - 0.02f * u},
                              {cx, cy + 0.28f * u}, {cx - 0.28f * u, cy - 0.02f * u}, {cx - 0.14f * u, cy - 0.20f * u}};
        for (int i = 0; i < 4; ++i) Tri(p[i < 4 ? i : 0], p[i + 1], {cx, cy - 0.02f * u}, i % 2 ? Color{static_cast<unsigned char>(c.r * 0.7f), static_cast<unsigned char>(c.g * 0.7f), static_cast<unsigned char>(c.b * 0.7f), 255} : c);
        Tri({cx - 0.14f * u, cy - 0.20f * u}, {cx + 0.14f * u, cy - 0.20f * u}, {cx, cy - 0.02f * u}, Fade(WHITE, 0.45f));
    } else {   // rough: a dull lump with a glint
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.04f * u), 0.26f * u, 0.22f * u, {104, 104, 110, 255});
        DrawEllipse(static_cast<int>(cx - 0.02f * u), static_cast<int>(cy), 0.20f * u, 0.16f * u, {136, 136, 144, 255});
        Tri({cx - 0.04f * u, cy - 0.10f * u}, {cx + 0.12f * u, cy + 0.02f * u}, {cx - 0.06f * u, cy + 0.10f * u}, c);
    }
}

void DrawClayIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.08f * u), 0.32f * u, 0.22f * u, {138, 102, 78, 255});
    DrawEllipse(static_cast<int>(cx - 0.04f * u), static_cast<int>(cy), 0.26f * u, 0.17f * u, {170, 128, 98, 255});
    DrawEllipse(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy - 0.05f * u), 0.10f * u, 0.05f * u, Fade(WHITE, 0.25f));
}

void DrawChiselIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.30f * u, cy + 0.32f * u}, {cx + 0.06f * u, cy - 0.04f * u}, 0.09f * u, {150, 104, 58, 255});
    Tri({cx + 0.04f * u, cy - 0.02f * u}, {cx + 0.34f * u, cy - 0.36f * u}, {cx + 0.12f * u, cy + 0.04f * u}, {176, 182, 194, 255});
}

void DrawNeedleIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.30f * u, cy + 0.32f * u}, {cx + 0.30f * u, cy - 0.32f * u}, 0.035f * u, {206, 210, 220, 255});
    DrawCircleLines(static_cast<int>(cx + 0.27f * u), static_cast<int>(cy - 0.28f * u), 0.04f * u, {60, 62, 70, 255});
}

void DrawThreadIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.16f * u, cy - 0.22f * u, 0.32f * u, 0.44f * u}, 0.3f, 6, {206, 70, 70, 255});
    for (int i = -2; i <= 2; ++i) DrawLineEx({cx - 0.16f * u, cy + i * 0.08f * u}, {cx + 0.16f * u, cy + i * 0.08f * u + 0.03f * u}, 0.012f * u, {240, 150, 150, 255});
    DrawRectangle(static_cast<int>(cx - 0.20f * u), static_cast<int>(cy - 0.25f * u), static_cast<int>(0.40f * u), static_cast<int>(0.05f * u), {138, 96, 54, 255});
    DrawRectangle(static_cast<int>(cx - 0.20f * u), static_cast<int>(cy + 0.20f * u), static_cast<int>(0.40f * u), static_cast<int>(0.05f * u), {138, 96, 54, 255});
}

void DrawLeatherIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.30f * u, cy - 0.20f * u, 0.60f * u, 0.42f * u}, 0.3f, 6, {150, 100, 60, 255});
    DrawRectangleRounded({cx - 0.26f * u, cy - 0.16f * u, 0.52f * u, 0.34f * u}, 0.3f, 6, {182, 128, 80, 255});
    DrawLineEx({cx - 0.20f * u, cy}, {cx + 0.20f * u, cy}, 0.018f * u, {130, 86, 50, 255});
}

void DrawMouldIcon(float cx, float cy, float u, bool amulet) {
    DrawRectangleRounded({cx - 0.30f * u, cy - 0.22f * u, 0.60f * u, 0.44f * u}, 0.18f, 6, {110, 114, 124, 255});
    DrawRectangleRounded({cx - 0.26f * u, cy - 0.18f * u, 0.52f * u, 0.36f * u}, 0.18f, 6, {146, 150, 160, 255});
    if (amulet) {
        DrawCircleV({cx, cy + 0.04f * u}, 0.10f * u, {60, 62, 70, 255});
        DrawLineEx({cx - 0.12f * u, cy - 0.10f * u}, {cx, cy - 0.06f * u}, 0.025f * u, {60, 62, 70, 255});
        DrawLineEx({cx + 0.12f * u, cy - 0.10f * u}, {cx, cy - 0.06f * u}, 0.025f * u, {60, 62, 70, 255});
    } else {
        DrawRing({cx, cy}, 0.07f * u, 0.12f * u, 0, 360, 18, {60, 62, 70, 255});
    }
}

void DrawRingIcon(float cx, float cy, float u, Color gem, bool hasGem) {
    DrawRing({cx, cy + 0.06f * u}, 0.14f * u, 0.22f * u, 0, 360, 24, {232, 184, 58, 255});
    DrawRing({cx - 0.02f * u, cy + 0.04f * u}, 0.15f * u, 0.17f * u, 200, 320, 12, {255, 232, 140, 255});
    if (hasGem) {
        Tri({cx - 0.10f * u, cy - 0.16f * u}, {cx + 0.10f * u, cy - 0.16f * u}, {cx, cy - 0.30f * u}, gem);
        Tri({cx - 0.10f * u, cy - 0.16f * u}, {cx + 0.10f * u, cy - 0.16f * u}, {cx, cy - 0.04f * u}, Fade(WHITE, 0.4f));
    }
}

void DrawAmuletIcon(float cx, float cy, float u, Color gem, bool hasGem) {
    const Color gold = {232, 184, 58, 255};
    DrawLineEx({cx - 0.30f * u, cy - 0.34f * u}, {cx - 0.02f * u, cy + 0.08f * u}, 0.03f * u, gold);
    DrawLineEx({cx + 0.30f * u, cy - 0.34f * u}, {cx + 0.02f * u, cy + 0.08f * u}, 0.03f * u, gold);
    DrawCircleV({cx, cy + 0.16f * u}, 0.15f * u, gold);
    DrawCircleV({cx, cy + 0.16f * u}, 0.10f * u, hasGem ? gem : Color{255, 222, 112, 255});
    if (hasGem) DrawCircleV({cx - 0.03f * u, cy + 0.12f * u}, 0.03f * u, Fade(WHITE, 0.6f));
}

void DrawPieDishIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.06f * u), 0.34f * u, 0.14f * u, {170, 94, 66, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.32f * u, 0.13f * u, {206, 122, 88, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.24f * u, 0.08f * u, {150, 80, 56, 255});
}

void DrawBowlIcon(float cx, float cy, float u) {
    Tri({cx - 0.30f * u, cy - 0.10f * u}, {cx + 0.30f * u, cy - 0.10f * u}, {cx + 0.14f * u, cy + 0.20f * u}, {196, 116, 82, 255});
    Tri({cx - 0.30f * u, cy - 0.10f * u}, {cx + 0.14f * u, cy + 0.20f * u}, {cx - 0.14f * u, cy + 0.20f * u}, {196, 116, 82, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.10f * u), 0.30f * u, 0.08f * u, {226, 146, 108, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.10f * u), 0.24f * u, 0.05f * u, {140, 76, 52, 255});
}

// A rolled-up scroll tied with a ribbon (the coloured band says which place it goes to).
void DrawScrollIcon(float cx, float cy, float u, Color band) {
    const Color paper{236, 222, 182, 255}, edge{200, 178, 126, 255};
    DrawLineEx({cx - 0.28f * u, cy + 0.20f * u}, {cx + 0.28f * u, cy - 0.20f * u}, 0.22f * u, paper);
    DrawCircleV({cx - 0.28f * u, cy + 0.20f * u}, 0.11f * u, edge);
    DrawCircleV({cx + 0.28f * u, cy - 0.20f * u}, 0.11f * u, edge);
    DrawLineEx({cx - 0.04f * u, cy + 0.05f * u}, {cx + 0.04f * u, cy - 0.05f * u}, 0.24f * u, band);
    DrawCircleV({cx - 0.26f * u, cy + 0.20f * u}, 0.05f * u, {150, 120, 70, 255});
    DrawCircleV({cx + 0.26f * u, cy - 0.20f * u}, 0.05f * u, {150, 120, 70, 255});
}

// Leather armour reuses the armour shapes in a leather brown.
void DrawLeatherArmourIcon(Item item, float cx, float cy, float u) {
    const Rgb leather = {168, 114, 68};
    if (item == Item::LeatherCap) DrawHelmIcon(cx, cy, u, leather);
    else if (item == Item::LeatherBody) DrawBodyIcon(cx, cy, u, leather);
    else DrawLegsIcon(cx, cy, u, leather);
}


// ---- round 10: the kitchen, tools, and a few more things (all little hand-drawn pictures)
Color Mix(Color a, Color b, float t) {
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255};
}

void DrawPieIcon(float cx, float cy, float u, bool raw, Color filling) {
    const Color crust = raw ? Color{226, 206, 160, 255} : Color{204, 134, 72, 255};
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.09f * u), 0.38f * u, 0.17f * u, Mix(crust, BLACK, 0.35f));
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.36f * u, 0.19f * u, crust);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.01f * u), 0.27f * u, 0.12f * u, raw ? Mix(crust, WHITE, 0.25f) : Mix(crust, filling, 0.35f));
    for (int i = -2; i <= 2; ++i) {   // a lattice of pastry strips
        DrawLineEx({cx + i * 0.09f * u - 0.07f * u, cy - 0.10f * u}, {cx + i * 0.09f * u + 0.07f * u, cy + 0.09f * u}, 0.026f * u, raw ? Mix(crust, BLACK, 0.12f) : Mix(crust, BLACK, 0.2f));
        DrawLineEx({cx + i * 0.09f * u + 0.07f * u, cy - 0.10f * u}, {cx + i * 0.09f * u - 0.07f * u, cy + 0.09f * u}, 0.026f * u, raw ? Mix(crust, BLACK, 0.12f) : Mix(crust, BLACK, 0.2f));
    }
    if (!raw) DrawEllipse(static_cast<int>(cx - 0.12f * u), static_cast<int>(cy - 0.08f * u), 0.09f * u, 0.035f * u, Fade(WHITE, 0.30f));
    DrawCircleV({cx, cy - 0.01f * u}, 0.035f * u, filling);   // a bit of filling peeping through the middle
}

void DrawStewIcon(float cx, float cy, float u, bool raw) {
    DrawBowlIcon(cx, cy + 0.04f * u, u);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.06f * u), 0.25f * u, 0.065f * u, raw ? Color{198, 192, 160, 255} : Color{146, 84, 44, 255});
    for (int i = 0; i < 4; ++i)
        DrawCircleV({cx + (-0.14f + i * 0.09f) * u, cy + (-0.07f + (i % 2) * 0.02f) * u}, 0.03f * u, raw ? Color{214, 120, 112, 255} : Color{196, 130, 72, 255});
    if (!raw) {   // steam
        DrawLineEx({cx - 0.06f * u, cy - 0.16f * u}, {cx - 0.02f * u, cy - 0.30f * u}, 0.02f * u, Fade(WHITE, 0.45f));
        DrawLineEx({cx + 0.08f * u, cy - 0.16f * u}, {cx + 0.12f * u, cy - 0.32f * u}, 0.02f * u, Fade(WHITE, 0.45f));
    }
}

void DrawCakeIcon(float cx, float cy, float u, bool raw) {
    const Color sponge = raw ? Color{232, 214, 168, 255} : Color{214, 166, 96, 255};
    DrawRectangleRounded({cx - 0.32f * u, cy - 0.04f * u, 0.64f * u, 0.32f * u}, 0.2f, 6, raw ? Color{120, 124, 134, 255} : sponge);   // (a tin when raw)
    DrawRectangleRounded({cx - 0.28f * u, cy - 0.10f * u, 0.56f * u, 0.30f * u}, 0.2f, 6, sponge);
    if (!raw) {
        DrawRectangleRounded({cx - 0.30f * u, cy - 0.14f * u, 0.60f * u, 0.14f * u}, 0.5f, 6, {250, 238, 232, 255});   // icing
        for (int i = -1; i <= 1; ++i) DrawCircleV({cx + i * 0.17f * u, cy - 0.06f * u}, 0.04f * u, {250, 238, 232, 255});
        DrawCircleV({cx, cy - 0.20f * u}, 0.06f * u, {206, 40, 56, 255});   // a cherry
        DrawLineEx({cx, cy - 0.24f * u}, {cx + 0.05f * u, cy - 0.34f * u}, 0.015f * u, {70, 110, 50, 255});
    }
}

void DrawPizzaIcon(float cx, float cy, float u, bool raw) {
    const Color crust = raw ? Color{226, 208, 164, 255} : Color{214, 158, 84, 255};
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.04f * u), 0.40f * u, 0.30f * u, Mix(crust, BLACK, 0.3f));
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.40f * u, 0.30f * u, crust);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.33f * u, 0.24f * u, raw ? Color{214, 110, 96, 255} : Color{200, 70, 52, 255});
    const Color cheese = raw ? Color{240, 226, 160, 255} : Color{246, 204, 80, 255};
    for (int i = 0; i < 6; ++i) {
        const float a = i * 1.05f;
        DrawCircleV({cx + std::cos(a) * 0.15f * u, cy + std::sin(a) * 0.11f * u}, 0.065f * u, cheese);
    }
    DrawCircleV({cx, cy}, 0.07f * u, cheese);
}

void DrawDoughIcon(float cx, float cy, float u, Color c) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.08f * u), 0.34f * u, 0.18f * u, Mix(c, BLACK, 0.2f));
    DrawEllipse(static_cast<int>(cx - 0.10f * u), static_cast<int>(cy), 0.22f * u, 0.17f * u, c);
    DrawEllipse(static_cast<int>(cx + 0.12f * u), static_cast<int>(cy + 0.02f * u), 0.20f * u, 0.15f * u, Mix(c, WHITE, 0.15f));
    DrawEllipse(static_cast<int>(cx - 0.12f * u), static_cast<int>(cy - 0.06f * u), 0.08f * u, 0.04f * u, Fade(WHITE, 0.3f));
}

void DrawCheeseIcon(float cx, float cy, float u) {
    Tri({cx - 0.34f * u, cy + 0.20f * u}, {cx + 0.34f * u, cy + 0.20f * u}, {cx + 0.34f * u, cy - 0.06f * u}, {246, 206, 84, 255});
    Tri({cx - 0.34f * u, cy + 0.20f * u}, {cx + 0.34f * u, cy - 0.06f * u}, {cx - 0.34f * u, cy - 0.06f * u}, {250, 218, 112, 255});
    DrawRectangle(static_cast<int>(cx - 0.34f * u), static_cast<int>(cy + 0.08f * u), static_cast<int>(0.68f * u), static_cast<int>(0.12f * u), {222, 176, 60, 255});
    DrawCircleV({cx - 0.14f * u, cy + 0.02f * u}, 0.04f * u, {206, 160, 48, 255});
    DrawCircleV({cx + 0.12f * u, cy + 0.09f * u}, 0.03f * u, {206, 160, 48, 255});
    DrawCircleV({cx + 0.20f * u, cy + 0.0f * u}, 0.025f * u, {206, 160, 48, 255});
}

void DrawRoundVeg(float cx, float cy, float u, Color body, Color top, float rx, float ry) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.04f * u), rx * u, ry * u, Mix(body, BLACK, 0.25f));
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rx * u, ry * u, body);
    DrawEllipse(static_cast<int>(cx - 0.08f * u), static_cast<int>(cy - 0.08f * u), 0.07f * u, 0.045f * u, Fade(WHITE, 0.35f));
    DrawCircleV({cx, cy - ry * u * 0.95f}, 0.04f * u, top);
}

void DrawTomatoIcon(float cx, float cy, float u) {
    DrawRoundVeg(cx, cy + 0.02f * u, u, {214, 44, 40, 255}, {70, 140, 56, 255}, 0.30f, 0.26f);
    for (int i = 0; i < 5; ++i) {
        const float a = i * 1.257f - 1.57f;
        Tri({cx, cy - 0.22f * u}, {cx + std::cos(a - 0.3f) * 0.14f * u, cy - 0.22f * u + std::sin(a - 0.3f) * 0.07f * u},
            {cx + std::cos(a + 0.3f) * 0.14f * u, cy - 0.22f * u + std::sin(a + 0.3f) * 0.07f * u}, {70, 140, 56, 255});
    }
}

void DrawPotatoIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.04f * u), 0.34f * u, 0.22f * u, {132, 96, 56, 255});
    DrawEllipse(static_cast<int>(cx - 0.02f * u), static_cast<int>(cy), 0.32f * u, 0.20f * u, {176, 134, 84, 255});
    for (int i = 0; i < 4; ++i) DrawCircleV({cx + (-0.16f + i * 0.11f) * u, cy + (-0.04f + (i % 2) * 0.08f) * u}, 0.018f * u, {110, 80, 46, 255});
}

void DrawAppleIcon(float cx, float cy, float u) {
    DrawRoundVeg(cx, cy + 0.04f * u, u, {120, 180, 60, 255}, {96, 64, 36, 255}, 0.28f, 0.27f);
    DrawLineEx({cx, cy - 0.22f * u}, {cx + 0.03f * u, cy - 0.36f * u}, 0.025f * u, {96, 64, 36, 255});
    Tri({cx + 0.03f * u, cy - 0.30f * u}, {cx + 0.18f * u, cy - 0.38f * u}, {cx + 0.12f * u, cy - 0.26f * u}, {76, 150, 56, 255});
}

void DrawOnionIcon(float cx, float cy, float u) {
    DrawRoundVeg(cx, cy + 0.06f * u, u, {226, 190, 120, 255}, {206, 170, 100, 255}, 0.27f, 0.24f);
    Tri({cx - 0.10f * u, cy - 0.12f * u}, {cx + 0.10f * u, cy - 0.12f * u}, {cx, cy - 0.34f * u}, {226, 190, 120, 255});
    DrawLineEx({cx - 0.10f * u, cy - 0.16f * u}, {cx - 0.14f * u, cy + 0.20f * u}, 0.012f * u, {190, 150, 90, 255});
    DrawLineEx({cx + 0.10f * u, cy - 0.16f * u}, {cx + 0.14f * u, cy + 0.20f * u}, 0.012f * u, {190, 150, 90, 255});
}

void DrawCabbageIcon(float cx, float cy, float u) {
    DrawCircleV({cx, cy + 0.02f * u}, 0.32f * u, {96, 160, 70, 255});
    DrawCircleV({cx - 0.03f * u, cy - 0.01f * u}, 0.26f * u, {132, 196, 94, 255});
    DrawCircleV({cx - 0.06f * u, cy - 0.04f * u}, 0.14f * u, {170, 218, 130, 255});
    DrawLineEx({cx, cy - 0.26f * u}, {cx - 0.04f * u, cy + 0.26f * u}, 0.014f * u, {80, 130, 60, 255});
    DrawLineEx({cx - 0.22f * u, cy - 0.08f * u}, {cx + 0.12f * u, cy + 0.12f * u}, 0.014f * u, {80, 130, 60, 255});
}

void DrawBerriesIcon(float cx, float cy, float u) {
    Tri({cx - 0.04f * u, cy - 0.20f * u}, {cx + 0.20f * u, cy - 0.24f * u}, {cx + 0.08f * u, cy - 0.08f * u}, {76, 150, 56, 255});
    const Vector2 b[4] = {{-0.15f, 0.04f}, {0.08f, 0.10f}, {-0.02f, -0.08f}, {0.17f, -0.04f}};
    for (const Vector2& p : b) {
        DrawCircleV({cx + p.x * u, cy + p.y * u}, 0.12f * u, {196, 36, 56, 255});
        DrawCircleV({cx + (p.x - 0.04f) * u, cy + (p.y - 0.04f) * u}, 0.03f * u, Fade(WHITE, 0.55f));
    }
}

void DrawFlourIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.26f * u, cy - 0.14f * u, 0.52f * u, 0.50f * u}, 0.3f, 6, {222, 210, 180, 255});
    DrawRectangleRounded({cx - 0.20f * u, cy - 0.06f * u, 0.40f * u, 0.38f * u}, 0.3f, 6, {240, 232, 208, 255});
    Tri({cx - 0.20f * u, cy - 0.14f * u}, {cx + 0.20f * u, cy - 0.14f * u}, {cx, cy - 0.32f * u}, {222, 210, 180, 255});
    DrawLineEx({cx - 0.16f * u, cy - 0.14f * u}, {cx + 0.16f * u, cy - 0.14f * u}, 0.04f * u, {160, 110, 60, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.10f * u), 0.12f * u, 0.07f * u, {250, 246, 232, 255});
}

void DrawJugIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.20f * u, cy - 0.16f * u, 0.40f * u, 0.44f * u}, 0.45f, 8, {196, 124, 86, 255});
    DrawRectangleRounded({cx - 0.12f * u, cy - 0.28f * u, 0.24f * u, 0.16f * u}, 0.3f, 6, {196, 124, 86, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.27f * u), 0.12f * u, 0.035f * u, {70, 130, 206, 255});
    DrawRing({cx + 0.24f * u, cy - 0.02f * u}, 0.07f * u, 0.11f * u, -90, 90, 10, {196, 124, 86, 255});
    DrawEllipse(static_cast<int>(cx - 0.07f * u), static_cast<int>(cy - 0.04f * u), 0.04f * u, 0.10f * u, Fade(WHITE, 0.25f));
}

void DrawMilkIcon(float cx, float cy, float u) {
    Tri({cx - 0.28f * u, cy - 0.12f * u}, {cx + 0.28f * u, cy - 0.12f * u}, {cx + 0.20f * u, cy + 0.30f * u}, {138, 142, 154, 255});
    Tri({cx - 0.28f * u, cy - 0.12f * u}, {cx + 0.20f * u, cy + 0.30f * u}, {cx - 0.20f * u, cy + 0.30f * u}, {160, 164, 176, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.12f * u), 0.28f * u, 0.07f * u, {250, 250, 246, 255});
    DrawRing({cx, cy - 0.12f * u}, 0.28f * u, 0.30f * u, 180, 360, 14, {110, 114, 126, 255});
    DrawLineEx({cx - 0.28f * u, cy - 0.12f * u}, {cx + 0.28f * u, cy - 0.12f * u}, 0.02f * u, {110, 114, 126, 255});
}

void DrawKnifeIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.34f * u, cy + 0.34f * u}, {cx - 0.10f * u, cy + 0.10f * u}, 0.09f * u, {110, 72, 38, 255});
    DrawLineEx({cx - 0.14f * u, cy + 0.14f * u}, {cx - 0.04f * u, cy + 0.04f * u}, 0.11f * u, {120, 124, 134, 255});
    Tri({cx - 0.08f * u, cy + 0.08f * u}, {cx + 0.02f * u, cy - 0.02f * u}, {cx + 0.36f * u, cy - 0.36f * u}, {196, 202, 212, 255});
    Tri({cx - 0.08f * u, cy + 0.08f * u}, {cx - 0.02f * u, cy + 0.14f * u}, {cx + 0.36f * u, cy - 0.36f * u}, {160, 166, 178, 255});
}

void DrawSpadeIcon(float cx, float cy, float u) {
    DrawLineEx({cx - 0.30f * u, cy + 0.36f * u}, {cx + 0.10f * u, cy - 0.10f * u}, 0.06f * u, {136, 92, 50, 255});
    DrawLineEx({cx + 0.02f * u, cy - 0.20f * u}, {cx + 0.24f * u, cy - 0.04f * u}, 0.06f * u, {136, 92, 50, 255});   // the T handle
    const Rectangle blade = {cx + 0.22f * u, cy - 0.20f * u, 0.30f * u, 0.34f * u};
    DrawRectanglePro(blade, {0.0f, 0.0f}, 45.0f, {160, 166, 178, 255});
    DrawRectanglePro({blade.x, blade.y, 0.04f * u, 0.34f * u}, {0.0f, 0.0f}, 45.0f, {206, 212, 222, 255});
}

void DrawCandleIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.10f * u, cy - 0.10f * u, 0.20f * u, 0.46f * u}, 0.3f, 6, {240, 230, 196, 255});
    DrawRectangle(static_cast<int>(cx + 0.03f * u), static_cast<int>(cy - 0.10f * u), static_cast<int>(0.07f * u), static_cast<int>(0.46f * u), {214, 200, 160, 255});
    DrawLineEx({cx, cy - 0.10f * u}, {cx, cy - 0.16f * u}, 0.015f * u, {40, 34, 30, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.26f * u), 0.065f * u, 0.11f * u, {255, 170, 40, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 0.24f * u), 0.035f * u, 0.07f * u, {255, 240, 170, 255});
    DrawRectangle(static_cast<int>(cx - 0.17f * u), static_cast<int>(cy + 0.32f * u), static_cast<int>(0.34f * u), static_cast<int>(0.06f * u), {150, 120, 70, 255});
}

void DrawKeyIcon(float cx, float cy, float u) {
    const Color rust = {132, 138, 96, 255};
    DrawRing({cx - 0.18f * u, cy - 0.18f * u}, 0.08f * u, 0.15f * u, 0, 360, 18, rust);
    DrawLineEx({cx - 0.10f * u, cy - 0.10f * u}, {cx + 0.28f * u, cy + 0.28f * u}, 0.07f * u, rust);
    DrawLineEx({cx + 0.18f * u, cy + 0.18f * u}, {cx + 0.28f * u, cy + 0.08f * u}, 0.06f * u, rust);
    DrawLineEx({cx + 0.26f * u, cy + 0.26f * u}, {cx + 0.34f * u, cy + 0.18f * u}, 0.06f * u, rust);
    DrawCircleV({cx - 0.10f * u, cy - 0.12f * u}, 0.02f * u, {180, 120, 60, 255});
}

void DrawBookIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.28f * u, cy - 0.30f * u, 0.56f * u, 0.62f * u}, 0.1f, 6, {96, 58, 40, 255});
    DrawRectangle(static_cast<int>(cx - 0.24f * u), static_cast<int>(cy - 0.26f * u), static_cast<int>(0.46f * u), static_cast<int>(0.54f * u), {222, 206, 164, 255});
    DrawRectangle(static_cast<int>(cx - 0.28f * u), static_cast<int>(cy - 0.30f * u), static_cast<int>(0.07f * u), static_cast<int>(0.62f * u), {70, 40, 28, 255});
    for (int i = 0; i < 5; ++i)
        DrawLineEx({cx - 0.14f * u, cy + (-0.16f + i * 0.09f) * u}, {cx + 0.18f * u, cy + (-0.16f + i * 0.09f) * u}, 0.012f * u, {120, 96, 70, 255});
    DrawCircleV({cx + 0.08f * u, cy + 0.30f * u}, 0.03f * u, {196, 58, 58, 255});   // a ribbon marker
}

void DrawStudsIcon(float cx, float cy, float u) {
    const Vector2 p[7] = {{-0.20f, 0.08f}, {0.0f, 0.14f}, {0.20f, 0.06f}, {-0.10f, -0.08f}, {0.12f, -0.10f}, {-0.02f, 0.28f}, {0.0f, -0.24f}};
    for (const Vector2& q : p) {
        DrawCircleV({cx + q.x * u, cy + q.y * u}, 0.065f * u, {132, 138, 150, 255});
        DrawCircleV({cx + (q.x - 0.015f) * u, cy + (q.y - 0.02f) * u}, 0.028f * u, {206, 212, 222, 255});
    }
}

void DrawBowstringIcon(float cx, float cy, float u) {
    for (int i = 0; i < 5; ++i)
        DrawRing({cx, cy}, (0.10f + i * 0.045f) * u, (0.12f + i * 0.045f) * u, 0, 360, 24, i % 2 ? Color{232, 224, 190, 255} : Color{206, 196, 160, 255});
    DrawLineEx({cx + 0.20f * u, cy + 0.04f * u}, {cx + 0.40f * u, cy + 0.30f * u}, 0.025f * u, {232, 224, 190, 255});
}

void DrawUnstrungBowIcon(float cx, float cy, float u, Color wood, bool longBow) {
    const float h = (longBow ? 0.44f : 0.36f) * u;
    Vector2 prev{};
    for (int i = 0; i <= 12; ++i) {
        const float t = -1.0f + i / 6.0f;
        const Vector2 p = {cx - 0.16f * u + 0.24f * u * t * t, cy + t * h};
        if (i > 0) DrawLineEx(prev, p, 0.065f * u, wood);
        prev = p;
    }
    DrawLineEx({cx - 0.16f * u, cy - 0.06f * u}, {cx - 0.16f * u, cy + 0.06f * u}, 0.085f * u, {92, 60, 32, 255});
    DrawCircleV({cx + 0.08f * u, cy - h}, 0.03f * u, {70, 46, 24, 255});     // the empty nocks
    DrawCircleV({cx + 0.08f * u, cy + h}, 0.03f * u, {70, 46, 24, 255});
}

void DrawHardLeatherIcon(float cx, float cy, float u) {
    DrawRectangleRounded({cx - 0.30f * u, cy - 0.20f * u, 0.60f * u, 0.42f * u}, 0.3f, 6, {112, 72, 42, 255});
    DrawRectangleRounded({cx - 0.26f * u, cy - 0.16f * u, 0.52f * u, 0.34f * u}, 0.3f, 6, {146, 98, 60, 255});
    DrawLineEx({cx - 0.20f * u, cy}, {cx + 0.20f * u, cy}, 0.018f * u, {96, 62, 36, 255});
    DrawLineEx({cx - 0.14f * u, cy - 0.10f * u}, {cx + 0.14f * u, cy + 0.10f * u}, 0.012f * u, {96, 62, 36, 255});
}

// Hard and studded leather armour: the armour shapes in a darker brown (studded ones with little steel studs).
void DrawHardLeatherArmourIcon(Item item, float cx, float cy, float u) {
    const bool studded = item == Item::StuddedCap || item == Item::StuddedBody || item == Item::StuddedChaps;
    const Rgb leather = studded ? Rgb{110, 74, 46} : Rgb{128, 84, 50};
    if (item == Item::HardLeatherCap || item == Item::StuddedCap) DrawHelmIcon(cx, cy, u, leather);
    else if (item == Item::HardLeatherBody || item == Item::StuddedBody) DrawBodyIcon(cx, cy, u, leather);
    else DrawLegsIcon(cx, cy, u, leather);
    if (studded) {
        for (int i = 0; i < 6; ++i)
            DrawCircleV({cx + (-0.18f + (i % 3) * 0.18f) * u, cy + (-0.10f + (i / 3) * 0.20f) * u}, 0.028f * u, {196, 202, 214, 255});
    }
}

void DrawMantaIcon(float cx, float cy, float u, Color body, Color belly) {
    Tri({cx - 0.40f * u, cy}, {cx, cy - 0.28f * u}, {cx + 0.28f * u, cy}, body);
    Tri({cx - 0.40f * u, cy}, {cx, cy + 0.28f * u}, {cx + 0.28f * u, cy}, belly);
    DrawLineEx({cx + 0.28f * u, cy}, {cx + 0.46f * u, cy + 0.10f * u}, 0.025f * u, body);
    DrawCircleV({cx + 0.08f * u, cy - 0.05f * u}, 0.022f * u, {20, 20, 24, 255});
}

void DrawTurtleIcon(float cx, float cy, float u, Color shell, Color skin) {
    DrawEllipse(static_cast<int>(cx - 0.02f * u), static_cast<int>(cy), 0.30f * u, 0.22f * u, shell);
    DrawEllipse(static_cast<int>(cx - 0.04f * u), static_cast<int>(cy - 0.02f * u), 0.22f * u, 0.15f * u, Mix(shell, WHITE, 0.2f));
    DrawCircleV({cx + 0.32f * u, cy - 0.02f * u}, 0.09f * u, skin);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) DrawEllipse(static_cast<int>(cx + sx * 0.20f * u), static_cast<int>(cy + sy * 0.20f * u), 0.09f * u, 0.05f * u, skin);
    DrawLineEx({cx - 0.12f * u, cy - 0.08f * u}, {cx + 0.08f * u, cy + 0.08f * u}, 0.012f * u, Mix(shell, BLACK, 0.4f));
    DrawLineEx({cx + 0.08f * u, cy - 0.08f * u}, {cx - 0.12f * u, cy + 0.08f * u}, 0.012f * u, Mix(shell, BLACK, 0.4f));
}

void DrawBurntPieIcon(float cx, float cy, float u) {
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 0.08f * u), 0.36f * u, 0.16f * u, {28, 24, 22, 255});
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 0.34f * u, 0.18f * u, {52, 44, 40, 255});
    DrawEllipse(static_cast<int>(cx - 0.06f * u), static_cast<int>(cy - 0.03f * u), 0.16f * u, 0.07f * u, {72, 62, 56, 255});
}

// Returns true if the item was one of the round-10 ones (and was drawn).
bool DrawRound10Icon(Item item, float cx, float cy, float u) {
    const Color wood[3] = {{196, 150, 96, 255}, {150, 98, 52, 255}, {170, 70, 46, 255}};
    switch (item) {
        case Item::Knife: DrawKnifeIcon(cx, cy, u); return true;
        case Item::Bowstring: DrawBowstringIcon(cx, cy, u); return true;
        case Item::UnstrungShortbow: DrawUnstrungBowIcon(cx, cy, u, wood[0], false); return true;
        case Item::UnstrungLongbow: DrawUnstrungBowIcon(cx, cy, u, wood[0], true); return true;
        case Item::UnstrungOakShortbow: DrawUnstrungBowIcon(cx, cy, u, wood[1], false); return true;
        case Item::UnstrungOakLongbow: DrawUnstrungBowIcon(cx, cy, u, wood[1], true); return true;
        case Item::UnstrungYewShortbow: DrawUnstrungBowIcon(cx, cy, u, wood[2], false); return true;
        case Item::UnstrungYewLongbow: DrawUnstrungBowIcon(cx, cy, u, wood[2], true); return true;
        case Item::MapleLogs: DrawLogIcon(cx, cy, u, {150, 74, 44, 255}, {232, 176, 120, 255}, {190, 110, 70, 255}); return true;
        case Item::TeakLogs: DrawLogIcon(cx, cy, u, {150, 118, 70, 255}, {238, 214, 160, 255}, {190, 156, 100, 255}); return true;
        case Item::MahoganyLogs: DrawLogIcon(cx, cy, u, {96, 40, 34, 255}, {178, 98, 84, 255}, {130, 64, 52, 255}); return true;
        case Item::MagicLogs: DrawLogIcon(cx, cy, u, {96, 70, 150, 255}, {200, 176, 240, 255}, {140, 110, 200, 255}); return true;
        case Item::Flour: DrawFlourIcon(cx, cy, u); return true;
        case Item::JugOfWater: DrawJugIcon(cx, cy, u); return true;
        case Item::Cheese: DrawCheeseIcon(cx, cy, u); return true;
        case Item::Tomato: DrawTomatoIcon(cx, cy, u); return true;
        case Item::Potato: DrawPotatoIcon(cx, cy, u); return true;
        case Item::CookingApple: DrawAppleIcon(cx, cy, u); return true;
        case Item::BucketOfMilk: DrawMilkIcon(cx, cy, u); return true;
        case Item::Onion: DrawOnionIcon(cx, cy, u); return true;
        case Item::Cabbage: DrawCabbageIcon(cx, cy, u); return true;
        case Item::Redberries: DrawBerriesIcon(cx, cy, u); return true;
        case Item::BreadDough: DrawDoughIcon(cx, cy, u, {232, 212, 170, 255}); return true;
        case Item::PastryDough: DrawDoughIcon(cx, cy, u, {240, 226, 188, 255}); return true;
        case Item::PizzaBase: DrawDoughIcon(cx, cy, u, {236, 220, 176, 255}); return true;
        case Item::RawMeatPie: DrawPieIcon(cx, cy, u, true, {170, 70, 60, 255}); return true;
        case Item::MeatPie: DrawPieIcon(cx, cy, u, false, {170, 70, 60, 255}); return true;
        case Item::RawApplePie: DrawPieIcon(cx, cy, u, true, {150, 190, 70, 255}); return true;
        case Item::ApplePie: DrawPieIcon(cx, cy, u, false, {150, 190, 70, 255}); return true;
        case Item::RawRedberryPie: DrawPieIcon(cx, cy, u, true, {196, 36, 56, 255}); return true;
        case Item::RedberryPie: DrawPieIcon(cx, cy, u, false, {196, 36, 56, 255}); return true;
        case Item::RawGardenPie: DrawPieIcon(cx, cy, u, true, {90, 160, 70, 255}); return true;
        case Item::GardenPie: DrawPieIcon(cx, cy, u, false, {90, 160, 70, 255}); return true;
        case Item::RawFishPie: DrawPieIcon(cx, cy, u, true, {96, 130, 176, 255}); return true;
        case Item::FishPie: DrawPieIcon(cx, cy, u, false, {96, 130, 176, 255}); return true;
        case Item::UncookedStew: DrawStewIcon(cx, cy, u, true); return true;
        case Item::Stew: DrawStewIcon(cx, cy, u, false); return true;
        case Item::UncookedCake: DrawCakeIcon(cx, cy, u, true); return true;
        case Item::Cake: DrawCakeIcon(cx, cy, u, false); return true;
        case Item::RawPizza: DrawPizzaIcon(cx, cy, u, true); return true;
        case Item::PlainPizza: DrawPizzaIcon(cx, cy, u, false); return true;
        case Item::BurntPie: DrawBurntPieIcon(cx, cy, u); return true;
        case Item::RawRatMeat: DrawDrumstickIcon(cx, cy, u, {206, 150, 156, 255}, {176, 120, 128, 255}); return true;
        case Item::RatMeat: DrawDrumstickIcon(cx, cy, u, {166, 112, 68, 255}, {128, 84, 48, 255}); return true;
        case Item::RawWolfMeat: DrawSteakIcon(cx, cy, u, {180, 66, 84, 255}, {232, 206, 196, 255}, false); return true;
        case Item::WolfMeat: DrawSteakIcon(cx, cy, u, {132, 78, 44, 255}, {104, 60, 34, 255}, true); return true;
        case Item::RawMackerel: DrawFishIcon(cx, cy, u, {80, 126, 140, 255}, {206, 222, 226, 255}, {60, 98, 112, 255}, false); return true;
        case Item::Mackerel: DrawFishIcon(cx, cy, u, {190, 136, 80, 255}, {230, 196, 140, 255}, {156, 106, 56, 255}, false); return true;
        case Item::RawCod: DrawBigFishIcon(cx, cy, u, {136, 150, 118, 255}, {226, 226, 204, 255}, {104, 118, 90, 255}, false, false, false); return true;
        case Item::Cod: DrawBigFishIcon(cx, cy, u, {206, 164, 108, 255}, {240, 214, 170, 255}, {170, 128, 76, 255}, false, false, true); return true;
        case Item::RawEel: DrawFishIcon(cx, cy, u * 1.1f, {70, 84, 70, 255}, {140, 150, 120, 255}, {54, 66, 54, 255}, false); return true;
        case Item::Eel: DrawFishIcon(cx, cy, u * 1.1f, {140, 100, 64, 255}, {196, 160, 110, 255}, {110, 76, 46, 255}, false); return true;
        case Item::RawMantaRay: DrawMantaIcon(cx, cy, u, {60, 82, 120, 255}, {212, 220, 232, 255}); return true;
        case Item::MantaRay: DrawMantaIcon(cx, cy, u, {150, 100, 70, 255}, {226, 190, 150, 255}); return true;
        case Item::RawSeaTurtle: DrawTurtleIcon(cx, cy, u, {78, 120, 82, 255}, {120, 150, 104, 255}); return true;
        case Item::SeaTurtle: DrawTurtleIcon(cx, cy, u, {170, 116, 70, 255}, {210, 160, 106, 255}); return true;
        case Item::HardLeather: DrawHardLeatherIcon(cx, cy, u); return true;
        case Item::HardLeatherCap:
        case Item::HardLeatherBody:
        case Item::HardLeatherChaps:
        case Item::StuddedCap:
        case Item::StuddedBody:
        case Item::StuddedChaps: DrawHardLeatherArmourIcon(item, cx, cy, u); return true;
        case Item::Studs: DrawStudsIcon(cx, cy, u); return true;
        case Item::UncutOpal: DrawGemIcon(cx, cy, u, {214, 226, 238, 255}, false); return true;
        case Item::Opal: DrawGemIcon(cx, cy, u, {214, 226, 238, 255}, true); return true;
        case Item::UncutJade: DrawGemIcon(cx, cy, u, {70, 170, 120, 255}, false); return true;
        case Item::Jade: DrawGemIcon(cx, cy, u, {70, 170, 120, 255}, true); return true;
        case Item::UncutRedTopaz: DrawGemIcon(cx, cy, u, {232, 110, 44, 255}, false); return true;
        case Item::RedTopaz: DrawGemIcon(cx, cy, u, {232, 110, 44, 255}, true); return true;
        case Item::Spade: DrawSpadeIcon(cx, cy, u); return true;
        case Item::Candle: DrawCandleIcon(cx, cy, u); return true;
        case Item::RustyKey: DrawKeyIcon(cx, cy, u); return true;
        case Item::CaptainsLog: DrawBookIcon(cx, cy, u); return true;
        case Item::LeatherBoots: DrawBootsIcon(cx, cy, u, {168, 114, 68}); return true;
        case Item::HardLeatherBoots: DrawBootsIcon(cx, cy, u, {128, 84, 50}); return true;
        case Item::StuddedBoots: DrawBootsIcon(cx, cy, u, {110, 74, 46}); DrawCircleV({cx - 0.1f * u, cy - 0.15f * u}, 0.025f * u, {196, 202, 214, 255}); return true;
        case Item::LeatherGloves: DrawGlovesIcon(cx, cy, u, {168, 114, 68}); return true;
        case Item::HardLeatherGloves: DrawGlovesIcon(cx, cy, u, {128, 84, 50}); return true;
        case Item::OpalRing: DrawRingIcon(cx, cy, u, {214, 226, 238, 255}, true); return true;
        case Item::OpalAmulet: DrawAmuletIcon(cx, cy, u, {214, 226, 238, 255}, true); return true;
        case Item::JadeRing: DrawRingIcon(cx, cy, u, {70, 170, 120, 255}, true); return true;
        case Item::JadeAmulet: DrawAmuletIcon(cx, cy, u, {70, 170, 120, 255}, true); return true;
        case Item::RedTopazRing: DrawRingIcon(cx, cy, u, {232, 110, 44, 255}, true); return true;
        case Item::RedTopazAmulet: DrawAmuletIcon(cx, cy, u, {232, 110, 44, 255}, true); return true;
        default:
            if (const CapeInfo* cape = FindCape(item)) {
                DrawCapeIcon(cx, cy, u, cape->main, cape->trim);
                return true;
            }
            return false;
    }
}

// Returns true if the item was one of the new ones (and was drawn).
bool DrawExtraIcon(Item item, float cx, float cy, float u) {
    switch (item) {
        case Item::OakLogs: DrawLogIcon(cx, cy, u, {140, 96, 52, 255}, {226, 190, 130, 255}, {170, 124, 72, 255}); return true;
        case Item::WillowLogs: DrawLogIcon(cx, cy, u, {104, 112, 62, 255}, {206, 204, 128, 255}, {150, 156, 86, 255}); return true;
        case Item::YewLogs: DrawLogIcon(cx, cy, u, {116, 54, 44, 255}, {196, 126, 104, 255}, {150, 82, 66, 255}); return true;
        case Item::IronAxe: DrawHatchetIcon(cx, cy, u, TierColor(Tier::Iron)); return true;
        case Item::SteelAxe: DrawHatchetIcon(cx, cy, u, TierColor(Tier::Steel)); return true;
        case Item::MithrilAxe: DrawHatchetIcon(cx, cy, u, TierColor(Tier::Mithril)); return true;
        case Item::FlyFishingRod: DrawRodIcon(cx, cy, u); return true;
        case Item::Harpoon: DrawHarpoonIcon(cx, cy, u); return true;
        case Item::LobsterPot: DrawCageIcon(cx, cy, u); return true;
        case Item::Feathers: DrawFeathersIcon(cx, cy, u); return true;
        case Item::RawTrout: DrawBigFishIcon(cx, cy, u, {150, 176, 118, 255}, {226, 226, 196, 255}, {112, 138, 88, 255}, false, false, false); return true;
        case Item::Trout: DrawBigFishIcon(cx, cy, u, {196, 146, 82, 255}, {232, 196, 130, 255}, {170, 112, 54, 255}, false, false, true); return true;
        case Item::RawSalmon: DrawBigFishIcon(cx, cy, u, {222, 130, 106, 255}, {246, 214, 196, 255}, {186, 96, 84, 255}, false, false, false); return true;
        case Item::Salmon: DrawBigFishIcon(cx, cy, u, {212, 124, 78, 255}, {240, 190, 140, 255}, {170, 90, 52, 255}, false, false, true); return true;
        case Item::RawTuna: DrawBigFishIcon(cx, cy, u, {74, 104, 156, 255}, {196, 206, 220, 255}, {52, 72, 122, 255}, false, false, false); return true;
        case Item::Tuna: DrawBigFishIcon(cx, cy, u, {160, 106, 72, 255}, {222, 176, 130, 255}, {126, 82, 52, 255}, false, false, true); return true;
        case Item::RawLobster: DrawBigFishIcon(cx, cy, u, {}, {}, {}, false, true, false); return true;
        case Item::Lobster: DrawBigFishIcon(cx, cy, u, {}, {}, {}, false, true, true); return true;
        case Item::RawSwordfish: DrawBigFishIcon(cx, cy, u, {116, 138, 170, 255}, {212, 220, 232, 255}, {86, 106, 140, 255}, true, false, false); return true;
        case Item::Swordfish: DrawBigFishIcon(cx, cy, u, {182, 138, 96, 255}, {232, 200, 156, 255}, {146, 104, 68, 255}, true, false, true); return true;
        case Item::Clay: DrawClayIcon(cx, cy, u); return true;
        case Item::SilverOre: DrawOreIcon(cx, cy, u, {206, 212, 224, 255}, {240, 244, 252, 255}, true); return true;
        case Item::GoldOre: DrawOreIcon(cx, cy, u, {232, 184, 58, 255}, {255, 226, 120, 255}, true); return true;
        case Item::UncutSapphire: DrawGemIcon(cx, cy, u, {60, 104, 232, 255}, false); return true;
        case Item::UncutEmerald: DrawGemIcon(cx, cy, u, {50, 190, 100, 255}, false); return true;
        case Item::UncutRuby: DrawGemIcon(cx, cy, u, {224, 44, 62, 255}, false); return true;
        case Item::Sapphire: DrawGemIcon(cx, cy, u, {60, 104, 232, 255}, true); return true;
        case Item::Emerald: DrawGemIcon(cx, cy, u, {50, 190, 100, 255}, true); return true;
        case Item::Ruby: DrawGemIcon(cx, cy, u, {224, 44, 62, 255}, true); return true;
        case Item::SilverBar: DrawBarIcon(cx, cy, u, {206, 212, 224}); return true;
        case Item::GoldBar: DrawBarIcon(cx, cy, u, {232, 184, 58}); return true;
        case Item::Chisel: DrawChiselIcon(cx, cy, u); return true;
        case Item::Needle: DrawNeedleIcon(cx, cy, u); return true;
        case Item::Thread: DrawThreadIcon(cx, cy, u); return true;
        case Item::Leather: DrawLeatherIcon(cx, cy, u); return true;
        case Item::LeatherCap:
        case Item::LeatherBody:
        case Item::LeatherChaps: DrawLeatherArmourIcon(item, cx, cy, u); return true;
        case Item::RingMould: DrawMouldIcon(cx, cy, u, false); return true;
        case Item::AmuletMould: DrawMouldIcon(cx, cy, u, true); return true;
        case Item::GoldRing: DrawRingIcon(cx, cy, u, {}, false); return true;
        case Item::SapphireRing: DrawRingIcon(cx, cy, u, {60, 104, 232, 255}, true); return true;
        case Item::EmeraldRing: DrawRingIcon(cx, cy, u, {50, 190, 100, 255}, true); return true;
        case Item::RubyRing: DrawRingIcon(cx, cy, u, {224, 44, 62, 255}, true); return true;
        case Item::GoldAmulet: DrawAmuletIcon(cx, cy, u, {}, false); return true;
        case Item::SapphireAmulet: DrawAmuletIcon(cx, cy, u, {60, 104, 232, 255}, true); return true;
        case Item::EmeraldAmulet: DrawAmuletIcon(cx, cy, u, {50, 190, 100, 255}, true); return true;
        case Item::RubyAmulet: DrawAmuletIcon(cx, cy, u, {224, 44, 62, 255}, true); return true;
        case Item::Pot: DrawPotIcon(cx, cy, u, {190, 112, 78, 255}); return true;
        case Item::PieDish: DrawPieDishIcon(cx, cy, u); return true;
        case Item::Bowl: DrawBowlIcon(cx, cy, u); return true;
        case Item::CastleScroll: DrawScrollIcon(cx, cy, u, {196, 58, 58, 255}); return true;
        case Item::MillbrookScroll: DrawScrollIcon(cx, cy, u, {58, 140, 70, 255}); return true;
        case Item::LakesideScroll: DrawScrollIcon(cx, cy, u, {56, 110, 200, 255}); return true;
        case Item::ElmswickScroll: DrawScrollIcon(cx, cy, u, {176, 112, 48, 255}); return true;
        case Item::SaltmereScroll: DrawScrollIcon(cx, cy, u, {36, 164, 172, 255}); return true;
        // ---- adamant
        case Item::AdamantAxe: DrawHatchetIcon(cx, cy, u, TierColor(Tier::Adamant)); return true;
        case Item::AdamantOre: DrawOreIcon(cx, cy, u, {46, 138, 85, 255}, {147, 239, 181, 255}, true); return true;
        case Item::AdamantBar: DrawBarIcon(cx, cy, u, TierColor(Tier::Adamant)); return true;
        // ---- more fish
        case Item::RawSardine: DrawFishIcon(cx, cy, u, {150, 170, 190, 255}, {220, 230, 240, 255}, {110, 130, 152, 255}, false); return true;
        case Item::Sardine: DrawFishIcon(cx, cy, u, {200, 150, 90, 255}, {234, 200, 140, 255}, {172, 116, 60, 255}, false); return true;
        case Item::RawHerring: DrawFishIcon(cx, cy, u, {112, 150, 142, 255}, {214, 226, 222, 255}, {86, 120, 114, 255}, false); return true;
        case Item::Herring: DrawFishIcon(cx, cy, u, {190, 146, 84, 255}, {230, 198, 140, 255}, {160, 110, 56, 255}, false); return true;
        case Item::RawPike: DrawBigFishIcon(cx, cy, u, {96, 138, 96, 255}, {206, 220, 176, 255}, {70, 108, 70, 255}, false, false, false); return true;
        case Item::Pike: DrawBigFishIcon(cx, cy, u, {188, 140, 80, 255}, {228, 196, 132, 255}, {150, 104, 56, 255}, false, false, true); return true;
        case Item::RawBass: DrawBigFishIcon(cx, cy, u, {104, 128, 150, 255}, {206, 216, 226, 255}, {78, 100, 124, 255}, false, false, false); return true;
        case Item::Bass: DrawBigFishIcon(cx, cy, u, {178, 128, 84, 255}, {226, 190, 140, 255}, {142, 98, 60, 255}, false, false, true); return true;
        case Item::RawShark: DrawBigFishIcon(cx, cy, u, {122, 132, 146, 255}, {228, 232, 238, 255}, {88, 98, 112, 255}, false, false, false); return true;
        case Item::Shark: DrawBigFishIcon(cx, cy, u, {176, 126, 90, 255}, {228, 194, 150, 255}, {140, 98, 66, 255}, false, false, true); return true;
        // ---- gems and the jewellery made from them
        case Item::UncutDiamond: DrawGemIcon(cx, cy, u, {196, 230, 250, 255}, false); return true;
        case Item::Diamond: DrawGemIcon(cx, cy, u, {196, 230, 250, 255}, true); return true;
        case Item::UncutDragonstone: DrawGemIcon(cx, cy, u, {150, 70, 214, 255}, false); return true;
        case Item::Dragonstone: DrawGemIcon(cx, cy, u, {150, 70, 214, 255}, true); return true;
        case Item::DiamondRing: DrawRingIcon(cx, cy, u, {196, 230, 250, 255}, true); return true;
        case Item::DiamondAmulet: DrawAmuletIcon(cx, cy, u, {196, 230, 250, 255}, true); return true;
        case Item::DragonstoneAmulet: DrawAmuletIcon(cx, cy, u, {150, 70, 214, 255}, true); return true;
        case Item::MummysAmulet: DrawAmuletIcon(cx, cy, u, {226, 196, 120, 255}, true); return true;
        case Item::TrollToothAmulet: DrawAmuletIcon(cx, cy, u, {236, 232, 214, 255}, true); return true;
        case Item::AbyssalAmulet: DrawAmuletIcon(cx, cy, u, {118, 58, 176, 255}, true); return true;
        case Item::AbyssalRing: DrawRingIcon(cx, cy, u, {96, 52, 150, 255}, true); return true;
        default: return DrawRound10Icon(item, cx, cy, u);
    }
}

// Little hand-drawn icons for items (no image files needed).
void DrawItemIcon(Item item, Rectangle r) {
    const float cx = r.x + r.width / 2, cy = r.y + r.height / 2, u = r.width;
    if (DrawExtraIcon(item, cx, cy, u)) return;
    switch (item) {
        case Item::BronzeAxe: {
            // wooden haft from bottom-left to top-right, bronze head with a bright cutting edge
            DrawLineEx({cx - 0.30f * u, cy + 0.32f * u}, {cx + 0.20f * u, cy - 0.26f * u}, 0.085f * u, {122, 82, 44, 255});
            Tri({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.26f * u, cy - 0.12f * u}, {cx - 0.06f * u, cy - 0.12f * u}, {196, 128, 62, 255});
            Tri({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.40f * u, cy - 0.30f * u}, {cx + 0.26f * u, cy - 0.12f * u}, {222, 156, 84, 255});
            DrawLineEx({cx + 0.04f * u, cy - 0.38f * u}, {cx + 0.40f * u, cy - 0.30f * u}, 0.025f * u, {250, 214, 160, 255});
            break;
        }
        case Item::SmallNet: {
            const Vector2 hoop = {cx + 0.10f * u, cy - 0.12f * u};
            DrawLineEx({cx - 0.32f * u, cy + 0.34f * u}, {hoop.x - 0.06f * u, hoop.y + 0.10f * u}, 0.07f * u, {122, 82, 44, 255});
            DrawCircleV(hoop, 0.25f * u, {226, 220, 190, 120});
            for (int i = -2; i <= 2; ++i) {   // net mesh
                DrawLineEx({hoop.x + i * 0.09f * u, hoop.y - 0.20f * u}, {hoop.x + i * 0.09f * u, hoop.y + 0.20f * u}, 0.012f * u, {236, 230, 204, 200});
                DrawLineEx({hoop.x - 0.22f * u, hoop.y + i * 0.09f * u}, {hoop.x + 0.22f * u, hoop.y + i * 0.09f * u}, 0.012f * u, {236, 230, 204, 200});
            }
            DrawRing(hoop, 0.22f * u, 0.27f * u, 0, 360, 24, {96, 62, 30, 255});
            break;
        }
        case Item::Tinderbox: {
            DrawRectangleRounded({cx - 0.30f * u, cy - 0.12f * u, 0.60f * u, 0.36f * u}, 0.18f, 6, {92, 60, 32, 255});
            DrawRectangleRounded({cx - 0.32f * u, cy - 0.20f * u, 0.64f * u, 0.14f * u}, 0.4f, 6, {138, 142, 150, 255});
            DrawRectangle(static_cast<int>(cx - 0.30f * u), static_cast<int>(cy + 0.02f * u), static_cast<int>(0.60f * u), static_cast<int>(0.04f * u), {138, 142, 150, 255});
            DrawCircleV({cx + 0.16f * u, cy - 0.27f * u}, 0.05f * u, {255, 200, 70, 255});   // a spark
            DrawCircleV({cx + 0.16f * u, cy - 0.27f * u}, 0.025f * u, {255, 245, 190, 255});
            break;
        }
        case Item::RawShrimps: DrawShrimpIcon(cx, cy, u, {206, 168, 170, 255}, {176, 136, 140, 255}); break;
        case Item::Shrimps: DrawShrimpIcon(cx, cy, u, {240, 112, 70, 255}, {214, 84, 52, 255}); break;
        case Item::RawAnchovies: DrawFishIcon(cx, cy, u, {118, 148, 176, 255}, {206, 220, 232, 255}, {96, 124, 152, 255}, false); break;
        case Item::Anchovies: DrawFishIcon(cx, cy, u, {196, 140, 72, 255}, {232, 196, 130, 255}, {170, 112, 54, 255}, false); break;
        case Item::BurntFish: DrawFishIcon(cx, cy, u, {44, 40, 38, 255}, {70, 62, 58, 255}, {32, 30, 30, 255}, true); break;
        case Item::Bones: DrawBonesIcon(cx, cy, u); break;
        case Item::RawChicken: DrawDrumstickIcon(cx, cy, u, {232, 150, 150, 255}, {208, 118, 124, 255}); break;
        case Item::Chicken: DrawDrumstickIcon(cx, cy, u, {190, 118, 58, 255}, {152, 88, 40, 255}); break;
        case Item::RawBeef: DrawSteakIcon(cx, cy, u, {212, 84, 98, 255}, {244, 222, 210, 255}, false); break;
        case Item::Beef: DrawSteakIcon(cx, cy, u, {160, 90, 46, 255}, {126, 72, 34, 255}, true); break;
        case Item::BurntMeat: DrawSteakIcon(cx, cy, u, {52, 44, 40, 255}, {36, 30, 28, 255}, false); break;
        case Item::Egg: DrawEggIcon(cx, cy, u); break;
        case Item::Cowhide: DrawHideIcon(cx, cy, u); break;
        case Item::Coins: DrawCoinsIcon(cx, cy, u); break;
        case Item::Bread: DrawBreadIcon(cx, cy, u); break;
        case Item::CopperOre: DrawOreIcon(cx, cy, u, {198, 104, 47, 255}, {236, 157, 96, 255}, false); break;
        case Item::TinOre: DrawOreIcon(cx, cy, u, {207, 213, 220, 255}, {244, 247, 250, 255}, false); break;
        case Item::IronOre: DrawOreIcon(cx, cy, u, {140, 75, 43, 255}, {193, 117, 65, 255}, false); break;
        case Item::Coal: DrawOreIcon(cx, cy, u, {38, 35, 32, 255}, {75, 69, 64, 255}, false); break;
        case Item::MithrilOre: DrawOreIcon(cx, cy, u, {74, 111, 216, 255}, {157, 191, 255, 255}, true); break;
        case Item::BronzeBar: DrawBarIcon(cx, cy, u, TierColor(Tier::Bronze)); break;
        case Item::IronBar: DrawBarIcon(cx, cy, u, TierColor(Tier::Iron)); break;
        case Item::SteelBar: DrawBarIcon(cx, cy, u, TierColor(Tier::Steel)); break;
        case Item::MithrilBar: DrawBarIcon(cx, cy, u, TierColor(Tier::Mithril)); break;
        case Item::Hammer: DrawHammerIcon(cx, cy, u); break;
        case Item::Logs: {
            // a log lying diagonally: bark body + pale cut end with growth rings
            const Rectangle body = {cx, cy, u * 0.62f, u * 0.26f};
            DrawRectanglePro(body, {body.width / 2, body.height / 2}, -35.0f, {112, 72, 38, 255});
            DrawRectanglePro({cx, cy - u * 0.02f, u * 0.62f, u * 0.08f}, {u * 0.31f, u * 0.13f}, -35.0f, {140, 94, 52, 255});
            const float ex = cx + std::cos(-35.0f * DEG2RAD) * u * 0.31f, ey = cy + std::sin(-35.0f * DEG2RAD) * u * 0.31f;
            DrawCircle(static_cast<int>(ex), static_cast<int>(ey), u * 0.14f, {214, 170, 108, 255});
            DrawCircleLines(static_cast<int>(ex), static_cast<int>(ey), u * 0.09f, {160, 114, 62, 255});
            DrawCircleLines(static_cast<int>(ex), static_cast<int>(ey), u * 0.04f, {160, 114, 62, 255});
            break;
        }
        default:
            if (GearOf(item, nullptr, nullptr)) {
                DrawGearIcon(item, cx, cy, u);
                break;
            }
            if (const BowInfo* bow = FindBow(item)) {
                static const Color kWood[3] = {{196, 150, 96, 255}, {150, 98, 52, 255}, {170, 70, 46, 255}};
                DrawBowIcon(cx, cy, u, kWood[(static_cast<int>(item) - static_cast<int>(Item::Shortbow)) / 2], (bow->range >= 9));
                break;
            }
            if (const ArrowInfo* arrow = FindArrow(item)) {
                DrawArrowsIcon(cx, cy, u, TierColor(static_cast<Tier>(arrow->metal)));
                break;
            }
            {
                Rune rn;
                if (IsRune(item, &rn)) {
                    DrawRuneIcon(cx, cy, u, rn);
                    break;
                }
            }
            if (IsStaff(item)) {
                DrawStaffIcon(cx, cy, u, static_cast<int>(item) - static_cast<int>(Item::Staff));
                break;
            }
            if (FindRobe(item)) {
                DrawWizardIcon(cx, cy, u, static_cast<int>(item) - static_cast<int>(Item::WizardHat));
                break;
            }
            DrawRectangleRounded({r.x + u * 0.2f, r.y + u * 0.2f, u * 0.6f, u * 0.6f}, 0.3f, 6, {150, 150, 160, 255});
    }
}
}  // namespace

float Hud::Scale() const {
    return std::clamp(GetScreenHeight() / 720.0f, 0.8f, 3.0f);
}

Rectangle Hud::ChatRect() const {
    const float s = Scale();
    const float w = std::min(560.0f * s, GetScreenWidth() * 0.62f);
    const float h = 112.0f * s;
    return {12.0f * s, GetScreenHeight() - h - 12.0f * s, w, h};
}

Rectangle Hud::TabRect(int i) const {
    const float s = Scale();
    const float size = 46.0f * s;
    const float gap = 6.0f * s;
    const float total = kTabCount * size + (kTabCount - 1) * gap;
    return {GetScreenWidth() - total - 12.0f * s + i * (size + gap), 12.0f * s, size, size};
}

Rectangle Hud::PanelRect() const {
    const float s = Scale();
    float w = 230.0f * s, h = 66.0f * s;   // "coming soon" size
    if (openTab_ == kTabBag) {
        w = (24.0f + kBagCols * 54.0f + (kBagCols - 1) * 4.0f) * s;
        h = (30.0f + 12.0f + kBagRows * 54.0f + (kBagRows - 1) * 4.0f + 12.0f + kActionBarH) * s;
    } else if (openTab_ == kTabSkills) {
        w = 270.0f * s;
        h = (34.0f + kSkillCount * 30.0f + 34.0f) * s;
    } else if (openTab_ == kTabCombat) {
        w = 270.0f * s;
        h = (34.0f + 52.0f + 22.0f + kCombatStyleCount * 46.0f + 34.0f) * s;
    } else if (openTab_ == kTabGear) {
        w = 270.0f * s;
        h = (34.0f + 4 * 68.0f + 14.0f + 138.0f + 10.0f) * s;
    } else if (openTab_ == kTabMagic) {
        w = 300.0f * s;
        h = (34.0f + 24.0f + std::min(kSpellsPerPage, SpellCount()) * 52.0f + (SpellPageCount() > 1 ? 36.0f : 0.0f) + 12.0f) * s;
    } else if (openTab_ == kTabMenu) {
        w = 260.0f * s;
        h = 220.0f * s;
        if (helpPage_) {
            w = std::min(340.0f * s, GetScreenWidth() * 0.94f);
            h = std::min((34.0f + 16.0f * static_cast<float>(HelpLines(GetFontDefault(), 12.0f * s, w - 24.0f * s).size()) + 54.0f) * s,
                         GetScreenHeight() - (TabRect(0).y + TabRect(0).height + 20.0f * s));
        }
    } else if (openTab_ == kTabMap) {
        w = std::min(390.0f * s, GetScreenWidth() * 0.94f);
        h = std::min(460.0f * s, GetScreenHeight() - (TabRect(0).y + TabRect(0).height + 20.0f * s));
    } else if (openTab_ == kTabQuests) {
        w = 320.0f * s;
        float rows = 34.0f + 12.0f;
        bool any = false;
        if (quests_ && bag_) {
            const Font font = GetFontDefault();
            std::string finished;
            for (int i = 0; i < quests_->Count(); ++i) {
                const QuestState& st = quests_->State(i);
                if (st.done) {
                    finished += (finished.empty() ? "" : ", ") + AllQuests()[static_cast<size_t>(i)].name;
                    continue;
                }
                if (!st.started) continue;
                any = true;
                rows += 26.0f + 16.0f * static_cast<float>(WrapText(font, quests_->ObjectiveText(i, *bag_), 12.0f * s, w - 40.0f * s).size());
            }
            if (!finished.empty()) {
                any = true;
                rows += 28.0f + 14.0f * static_cast<float>(WrapText(font, finished, 11.0f * s, w - 28.0f * s).size());
            }
        }
        h = (any ? rows + 14.0f : 34.0f + 56.0f) * s;
        h = std::min(h, GetScreenHeight() - (TabRect(0).y + TabRect(0).height + 20.0f * s));
    }
    const Rectangle tabs = TabRect(0);
    return {GetScreenWidth() - w - 12.0f * s, tabs.y + tabs.height + 8.0f * s, w, h};
}

Rectangle Hud::BagCellRect(int slot) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    const int col = slot % kBagCols, row = slot / kBagCols;
    const float cell = 54.0f * s, gap = 4.0f * s;
    return {p.x + 12.0f * s + col * (cell + gap), p.y + 30.0f * s + 12.0f * s + row * (cell + gap), cell, cell};
}

Rectangle Hud::ActionButtonRect(int index) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    const float w = 54.0f * s, h = 30.0f * s, gap = 4.0f * s;
    return {p.x + 12.0f * s + index * (w + gap), p.y + p.height - 12.0f * s - h, w, h};
}

bool Hud::ActionAt(Vector2 p, ItemAction* action) const {
    if (openTab_ != kTabBag || !bag_ || selected_ < 0 || selected_ >= Inventory::kSlots) return false;
    const std::vector<ItemAction> acts = ActionsFor(bag_->Slot(selected_).item, *bag_);
    for (size_t i = 0; i < acts.size(); ++i)
        if (CheckCollisionPointRec(p, ActionButtonRect(static_cast<int>(i)))) {
            *action = acts[i];
            return true;
        }
    return false;
}

Rectangle Hud::StyleRect(int i) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    return {p.x + 12.0f * s, p.y + (34.0f + 52.0f + 22.0f + i * 46.0f) * s, p.width - 24.0f * s, 40.0f * s};
}

int Hud::StyleAt(Vector2 p) const {
    if (openTab_ != kTabCombat || attackKind_ == AttackKind::Magic) return -1;
    const int n = attackKind_ == AttackKind::Ranged ? 3 : kCombatStyleCount;
    for (int i = 0; i < n; ++i)
        if (CheckCollisionPointRec(p, StyleRect(i))) return i;
    return -1;
}

Rectangle Hud::GearSlotRect(int slot) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    const float cell = 58.0f * s;
    const float cx = p.x + p.width / 2 - cell / 2;
    const float y0 = p.y + 34.0f * s;
    switch (static_cast<EquipSlot>(slot)) {
        case EquipSlot::Head: return {cx, y0, cell, cell};
        case EquipSlot::Weapon: return {cx - 74.0f * s, y0 + 68.0f * s, cell, cell};
        case EquipSlot::Body: return {cx, y0 + 68.0f * s, cell, cell};
        case EquipSlot::Shield: return {cx + 74.0f * s, y0 + 68.0f * s, cell, cell};
        case EquipSlot::Legs: return {cx, y0 + 136.0f * s, cell, cell};
        case EquipSlot::Ammo: return {cx + 74.0f * s, y0, cell, cell};
        case EquipSlot::Amulet: return {cx - 74.0f * s, y0, cell, cell};
        case EquipSlot::Ring: return {cx + 74.0f * s, y0 + 136.0f * s, cell, cell};
        case EquipSlot::Hands: return {cx - 74.0f * s, y0 + 136.0f * s, cell, cell};
        case EquipSlot::Cape: return {cx - 74.0f * s, y0 + 204.0f * s, cell, cell};
        case EquipSlot::Feet: return {cx, y0 + 204.0f * s, cell, cell};
    }
    return {};
}

int Hud::EquipSlotAt(Vector2 p) const {
    if (openTab_ != kTabGear) return -1;
    for (int i = 0; i < kEquipSlotCount; ++i)
        if (CheckCollisionPointRec(p, GearSlotRect(i))) return i;
    return -1;
}

// ---- the make panel
// A long list (the smith's eighteen pieces) does not fit a phone screen: it is shown a page of rows at a time.
int Hud::MakeRowsPerPage() const {
    const float s = Scale();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    const float rowH = (smith ? 40.0f : 52.0f) * s;
    const float fixed = 42.0f * s + (smith ? 40.0f * s : 0.0f) + 6.0f * s + 52.0f * s;
    const int n = static_cast<int>(std::max<size_t>(1, make_.options.size()));
    const int fits = static_cast<int>((GetScreenHeight() - 16.0f * s - fixed) / rowH);
    if (n <= fits) return n;
    return std::max(3, static_cast<int>((GetScreenHeight() - 16.0f * s - fixed - 36.0f * s) / rowH));
}

int Hud::MakePages() const {
    const int per = MakeRowsPerPage();
    return std::max(1, (static_cast<int>(make_.options.size()) + per - 1) / per);
}

Rectangle Hud::MakeRectOf() const {
    const float s = Scale();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    const float w = std::min(400.0f * s, GetScreenWidth() * 0.94f);
    const float rows = static_cast<float>(std::min<int>(MakeRowsPerPage(), static_cast<int>(std::max<size_t>(1, make_.options.size()))));
    const float rowH = (smith ? 40.0f : 52.0f) * s;
    const float h = 42.0f * s + (smith ? 40.0f * s : 0.0f) + rows * rowH + (MakePages() > 1 ? 36.0f * s : 0.0f) + 6.0f * s + 52.0f * s;
    return {(GetScreenWidth() - w) / 2, std::max(8.0f * s, (GetScreenHeight() - h) / 2 - 20.0f * s), w, std::min(h, GetScreenHeight() - 16.0f * s)};
}

Rectangle Hud::MakeRowRect(int i) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    const float rowH = (smith ? 40.0f : 52.0f) * s;
    const int per = MakeRowsPerPage();
    return {r.x + 10.0f * s, r.y + 42.0f * s + (smith ? 40.0f * s : 0.0f) + (i - makePage_ * per) * rowH, r.width - 20.0f * s, rowH - 4.0f * s};
}

Rectangle Hud::MakePageRect(bool next) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    return {next ? r.x + r.width - 10.0f * s - 80.0f * s : r.x + 10.0f * s, r.y + r.height - 52.0f * s - 6.0f * s - 32.0f * s, 80.0f * s, 30.0f * s};
}

Rectangle Hud::MakeTierRect(int t) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    const float w = (r.width - 20.0f * s - (kTierCount - 1) * 6.0f * s) / kTierCount;
    return {r.x + 10.0f * s + t * (w + 6.0f * s), r.y + 42.0f * s, w, 32.0f * s};
}

Rectangle Hud::MakeQtyRect(int i) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    const float w = (r.width - 20.0f * s - 2 * 8.0f * s) / 3.0f;
    return {r.x + 10.0f * s + i * (w + 8.0f * s), r.y + r.height - 44.0f * s, w, 34.0f * s};
}

Rectangle Hud::MakeCloseRect() const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    return {r.x + r.width - 38.0f * s, r.y + 5.0f * s, 30.0f * s, 30.0f * s};
}

void Hud::StepMakePage(int step) {
    makePage_ = std::clamp(makePage_ + step, 0, std::max(0, MakePages() - 1));
}

bool Hud::MakeHits(Vector2 p) const {
    return makeOpen_ && CheckCollisionPointRec(p, MakeRectOf());
}

Hud::MakeHit Hud::MakeAt(Vector2 p) const {
    MakeHit h;
    if (!makeOpen_) return h;
    if (CheckCollisionPointRec(p, MakeCloseRect())) return {MakeHitKind::Close, 0};
    if (make_.kind == MakeMenu::Kind::Smith)
        for (int t = 0; t < kTierCount; ++t)
            if (CheckCollisionPointRec(p, MakeTierRect(t))) return {MakeHitKind::Tier, t};
    {
        const int per = MakeRowsPerPage();
        const int first = makePage_ * per, last = std::min(static_cast<int>(make_.options.size()), first + per);
        for (int i = first; i < last; ++i)
            if (CheckCollisionPointRec(p, MakeRowRect(i))) return {MakeHitKind::Option, i};
        if (MakePages() > 1) {
            if (makePage_ > 0 && CheckCollisionPointRec(p, MakePageRect(false))) return {MakeHitKind::Page, -1};
            if (makePage_ < MakePages() - 1 && CheckCollisionPointRec(p, MakePageRect(true))) return {MakeHitKind::Page, 1};
        }
    }
    static const int kQty[3] = {1, 5, 99};
    for (int i = 0; i < 3; ++i)
        if (CheckCollisionPointRec(p, MakeQtyRect(i))) return {MakeHitKind::Qty, kQty[i]};
    return h;
}

// ---- the shop window
int Hud::ShopPages() const {
    constexpr int kRows = 7;
    return std::max(1, (static_cast<int>(ShopList().size()) + kRows - 1) / kRows);
}

Rectangle Hud::ShopRectOf() const {
    const float s = Scale();
    const float w = std::min(440.0f * s, GetScreenWidth() * 0.95f);
    const float h = std::min((46.0f + 36.0f + 7 * 44.0f + 62.0f) * s, GetScreenHeight() - 16.0f * s);
    return {(GetScreenWidth() - w) / 2, std::max(8.0f * s, (GetScreenHeight() - h) / 2 - 10.0f * s), w, h};
}

Rectangle Hud::ShopRowRect(int i) const {
    const float s = Scale();
    const Rectangle r = ShopRectOf();
    return {r.x + 10.0f * s, r.y + 82.0f * s + i * 44.0f * s, r.width - 20.0f * s, 40.0f * s};
}

Rectangle Hud::ShopTabRect(int t) const {
    const float s = Scale();
    const Rectangle r = ShopRectOf();
    const float w = (r.width - 20.0f * s) / 2;
    return {r.x + 10.0f * s + t * w, r.y + 46.0f * s, w - 4.0f * s, 30.0f * s};
}

Rectangle Hud::ShopQtyRect(int i) const {
    const float s = Scale();
    const Rectangle r = ShopRectOf();
    return {r.x + 10.0f * s + i * 72.0f * s, r.y + r.height - 50.0f * s, 66.0f * s, 38.0f * s};
}

Rectangle Hud::ShopPageRect(int i) const {
    const float s = Scale();
    const Rectangle r = ShopRectOf();
    return {r.x + r.width - 10.0f * s - (2 - i) * 46.0f * s, r.y + r.height - 50.0f * s, 40.0f * s, 38.0f * s};
}

Rectangle Hud::ShopCloseRect() const {
    const float s = Scale();
    const Rectangle r = ShopRectOf();
    return {r.x + r.width - 38.0f * s, r.y + 6.0f * s, 30.0f * s, 30.0f * s};
}

bool Hud::ShopHits(Vector2 p) const {
    return shopOpen_ && CheckCollisionPointRec(p, ShopRectOf());
}

Hud::ShopHit Hud::ShopAt(Vector2 p) const {
    ShopHit h;
    if (!shopOpen_) return h;
    if (CheckCollisionPointRec(p, ShopCloseRect())) return {ShopHitKind::Close, 0};
    if (CheckCollisionPointRec(p, ShopTabRect(0))) return {ShopHitKind::TabBuy, 0};
    if (CheckCollisionPointRec(p, ShopTabRect(1))) return {ShopHitKind::TabSell, 0};
    static const int kQty[3] = {1, 5, 99};
    for (int i = 0; i < 3; ++i)
        if (CheckCollisionPointRec(p, ShopQtyRect(i))) return {ShopHitKind::Qty, kQty[i]};
    if (CheckCollisionPointRec(p, ShopPageRect(0))) return {ShopHitKind::PagePrev, 0};
    if (CheckCollisionPointRec(p, ShopPageRect(1))) return {ShopHitKind::PageNext, 0};
    const std::vector<ShopLine>& list = ShopList();
    for (int i = 0; i < 7; ++i) {
        const int idx = shopPage_ * 7 + i;
        if (idx >= static_cast<int>(list.size())) break;
        if (CheckCollisionPointRec(p, ShopRowRect(i))) return {shopSell_ ? ShopHitKind::Sell : ShopHitKind::Buy, idx};
    }
    return h;
}

void Hud::DrawCoins(float s, const Font& font, float x, float y) const {
    if (!bag_) return;
    const int coins = bag_->Count(Item::Coins);
    const std::string txt = WithCommas(coins);
    const float ts = 14.0f * s;
    const float w = 40.0f * s + MeasureTextEx(font, txt.c_str(), ts, 1.0f).x;
    const Rectangle r = {x, y, std::max(w, 86.0f * s), 28.0f * s};
    Panel(r, 0.4f);
    DrawCoinsIcon(r.x + 16.0f * s, r.y + 15.0f * s, 22.0f * s);
    DrawTextEx(font, txt.c_str(), {r.x + 32.0f * s, r.y + (r.height - ts) / 2}, ts, 1.0f, {255, 224, 120, 255});
}

void Hud::DrawShop(float s, const Font& font) const {
    if (!shopOpen_) return;
    const Rectangle r = ShopRectOf();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 90});
    Panel(r, 0.04f, {16, 20, 28, 246});
    DrawTextEx(font, shop_.name.c_str(), {r.x + 14.0f * s, r.y + 8.0f * s}, 18.0f * s, 1.0f, kAccent);
    DrawTextEx(font, TextFormat("%s   Your coins: %s", shop_.keeper.c_str(), WithCommas(shop_.coins).c_str()), {r.x + 14.0f * s, r.y + 28.0f * s}, 12.0f * s, 1.0f, kDim);
    const Rectangle cl = ShopCloseRect();
    DrawRectangleRounded(cl, 0.3f, 6, {120, 50, 44, 235});
    DrawLineEx({cl.x + 9.0f * s, cl.y + 9.0f * s}, {cl.x + cl.width - 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    DrawLineEx({cl.x + cl.width - 9.0f * s, cl.y + 9.0f * s}, {cl.x + 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    for (int t = 0; t < 2; ++t) {
        const Rectangle tr = ShopTabRect(t);
        const bool on = (t == 1) == shopSell_;
        DrawRectangleRounded(tr, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
        if (on) DrawRectangleRoundedLinesEx(tr, 0.25f, 6, 2.0f * s, kAccent);
        const char* label = t == 0 ? "Buy" : "Sell";
        const float ts = 14.0f * s;
        const Vector2 sz = MeasureTextEx(font, label, ts, 1.0f);
        DrawTextEx(font, label, {tr.x + (tr.width - sz.x) / 2, tr.y + (tr.height - sz.y) / 2}, ts, 1.0f, on ? kAccent : kText);
    }
    const std::vector<ShopLine>& list = ShopList();
    if (list.empty()) {
        DrawTextEx(font, shopSell_ ? "You have nothing this shop wants." : "Nothing for sale right now.", {r.x + 16.0f * s, r.y + 96.0f * s}, 13.0f * s, 1.0f, kDim);
    }
    const Color coin = {255, 224, 120, 255};
    for (int i = 0; i < 7; ++i) {
        const int idx = shopPage_ * 7 + i;
        if (idx >= static_cast<int>(list.size())) break;
        const ShopLine& l = list[idx];
        const Rectangle row = ShopRowRect(i);
        const bool can = shopSell_ || l.ok;
        const bool hot = CheckCollisionPointRec(pointer_, row);
        DrawRectangleRounded(row, 0.2f, 6, can ? (hot ? Color{70, 120, 76, 200} : Color{44, 90, 56, 150}) : Color{255, 255, 255, static_cast<unsigned char>(hot ? 26 : 14)});
        DrawRectangleRoundedLinesEx(row, 0.2f, 6, 1.0f * s, can ? Color{140, 220, 150, 100} : kBorder);
        const Rectangle ic = {row.x + 3.0f * s, row.y + 1.0f * s, row.height - 2.0f * s, row.height - 2.0f * s};
        DrawItemIcon(l.item, ic);
        const float nx = ic.x + ic.width + 8.0f * s;
        DrawTextEx(font, GetItemInfo(l.item).name, {nx, row.y + 4.0f * s}, 15.0f * s, 1.0f, can ? kText : kDim);
        DrawTextEx(font, shopSell_ ? TextFormat("you have %d", l.stock) : TextFormat("%d in stock", l.stock), {nx, row.y + row.height - 15.0f * s}, 11.0f * s, 1.0f, kDim);
        const std::string price = WithCommas(l.price);
        const float pts = 15.0f * s;
        const float pw = MeasureTextEx(font, price.c_str(), pts, 1.0f).x;
        DrawCoinsIcon(row.x + row.width - pw - 24.0f * s, row.y + row.height / 2, 20.0f * s);
        DrawTextEx(font, price.c_str(), {row.x + row.width - pw - 8.0f * s, row.y + (row.height - pts) / 2}, pts, 1.0f,
                   (shopSell_ || l.ok) ? coin : Color{255, 120, 110, 255});
    }
    static const int kQty[3] = {1, 5, 99};
    const char* labels[3] = {"1", "5", shopSell_ ? "All" : "Max"};
    for (int i = 0; i < 3; ++i) {
        const Rectangle q = ShopQtyRect(i);
        const bool on = shopQty_ == kQty[i];
        DrawRectangleRounded(q, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
        if (on) DrawRectangleRoundedLinesEx(q, 0.25f, 6, 2.0f * s, kAccent);
        const float ts = 14.0f * s;
        const Vector2 sz = MeasureTextEx(font, labels[i], ts, 1.0f);
        DrawTextEx(font, labels[i], {q.x + (q.width - sz.x) / 2, q.y + (q.height - sz.y) / 2}, ts, 1.0f, on ? kAccent : kText);
    }
    for (int i = 0; i < 2; ++i) {
        const Rectangle pr = ShopPageRect(i);
        const bool enabled = i == 0 ? shopPage_ > 0 : shopPage_ < ShopPages() - 1;
        DrawRectangleRounded(pr, 0.25f, 6, Color{255, 255, 255, static_cast<unsigned char>(enabled ? 28 : 10)});
        const Vector2 c = {pr.x + pr.width / 2, pr.y + pr.height / 2};
        const float a = 8.0f * s, dir = i == 0 ? -1.0f : 1.0f;
        Tri({c.x - dir * a * 0.5f, c.y - a}, {c.x - dir * a * 0.5f, c.y + a}, {c.x + dir * a * 0.9f, c.y}, enabled ? kText : kDim);
    }
    if (ShopPages() > 1)
        DrawTextEx(font, TextFormat("%d / %d", shopPage_ + 1, ShopPages()), {r.x + r.width - 160.0f * s, r.y + r.height - 38.0f * s}, 12.0f * s, 1.0f, kDim);
}


// ---- the bank window
int Hud::BankPages() const {
    const int n = bank_ ? static_cast<int>(bank_->Slots().size()) : 0;
    return std::max(1, (n + kBankPerPage - 1) / kBankPerPage);
}

Rectangle Hud::BankRectOf() const {
    const float s = Scale();
    const float w = std::min(604.0f * s, GetScreenWidth() * 0.97f);
    const float h = std::min(464.0f * s, GetScreenHeight() - 16.0f * s);
    return {(GetScreenWidth() - w) / 2, std::max(8.0f * s, (GetScreenHeight() - h) / 2 - 6.0f * s), w, h};
}

Rectangle Hud::BankCellRect(int i) const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    const float cell = 44.0f * s, gap = 3.0f * s;
    return {r.x + 12.0f * s + (i % kBankCols) * (cell + gap), r.y + 70.0f * s + (i / kBankCols) * (cell + gap), cell, cell};
}

Rectangle Hud::BankBagCellRect(int slot) const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    const float cell = 44.0f * s, gap = 3.0f * s;
    const float x0 = r.x + 12.0f * s + kBankCols * (cell + gap) + 14.0f * s;
    return {x0 + (slot % 4) * (cell + gap), r.y + 70.0f * s + (slot / 4) * (cell + gap), cell, cell};
}

Rectangle Hud::BankQtyRect(int i) const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    return {r.x + 12.0f * s + i * 54.0f * s, r.y + 70.0f * s + kBankRows * 47.0f * s + 8.0f * s, 50.0f * s, 34.0f * s};
}

Rectangle Hud::BankDepositRect() const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    return {r.x + 12.0f * s + 4 * 54.0f * s + 6.0f * s, r.y + 70.0f * s + kBankRows * 47.0f * s + 8.0f * s, 150.0f * s, 34.0f * s};
}

Rectangle Hud::BankPageRect(int i) const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    const float x1 = r.x + 12.0f * s + kBankCols * 47.0f * s - 3.0f * s;
    return {x1 - (2 - i) * 38.0f * s, r.y + 40.0f * s, 34.0f * s, 24.0f * s};
}

Rectangle Hud::BankCloseRect() const {
    const float s = Scale();
    const Rectangle r = BankRectOf();
    return {r.x + r.width - 38.0f * s, r.y + 6.0f * s, 30.0f * s, 30.0f * s};
}

bool Hud::BankHits(Vector2 p) const {
    return bankOpen_ && CheckCollisionPointRec(p, BankRectOf());
}

Hud::BankHit Hud::BankAt(Vector2 p) const {
    BankHit h;
    if (!bankOpen_) return h;
    if (CheckCollisionPointRec(p, BankCloseRect())) return {BankHitKind::Close, 0};
    static const int kQty[4] = {1, 5, 10, Bank::kMaxStack};
    for (int i = 0; i < 4; ++i)
        if (CheckCollisionPointRec(p, BankQtyRect(i))) return {BankHitKind::Qty, kQty[i]};
    if (CheckCollisionPointRec(p, BankDepositRect())) return {BankHitKind::DepositAll, 0};
    if (CheckCollisionPointRec(p, BankPageRect(0))) return {BankHitKind::PagePrev, 0};
    if (CheckCollisionPointRec(p, BankPageRect(1))) return {BankHitKind::PageNext, 0};
    const int n = bank_ ? static_cast<int>(bank_->Slots().size()) : 0;
    for (int i = 0; i < kBankPerPage; ++i) {
        const int idx = bankPage_ * kBankPerPage + i;
        if (idx >= n) break;
        if (CheckCollisionPointRec(p, BankCellRect(i))) return {BankHitKind::BankCell, idx};
    }
    for (int i = 0; i < Inventory::kSlots; ++i)
        if (CheckCollisionPointRec(p, BankBagCellRect(i))) return {BankHitKind::BagCell, i};
    return h;
}

namespace {
// 12, 5,300, 150K, 12M: the count under an item
std::string ShortCount(int n) {
    if (n < 100000) return std::to_string(n);
    if (n < 10000000) return std::to_string(n / 1000) + "K";
    return std::to_string(n / 1000000) + "M";
}
Color CountColor(int n) {
    if (n >= 10000000) return {120, 235, 140, 255};
    if (n >= 100000) return {255, 255, 255, 255};
    return {255, 240, 120, 255};
}
}  // namespace

void Hud::DrawBank(float s, const Font& font) const {
    if (!bankOpen_) return;
    const Rectangle r = BankRectOf();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 90});
    Panel(r, 0.03f, {16, 20, 28, 246});
    DrawTextEx(font, "Bank", {r.x + 14.0f * s, r.y + 8.0f * s}, 20.0f * s, 1.0f, kAccent);
    const int used = bank_ ? bank_->Used() : 0;
    DrawTextEx(font, TextFormat("%d / %d slots used", used, Bank::kSlots), {r.x + 78.0f * s, r.y + 13.0f * s}, 12.0f * s, 1.0f, kDim);
    const Rectangle cl = BankCloseRect();
    DrawRectangleRounded(cl, 0.3f, 6, {120, 50, 44, 235});
    DrawLineEx({cl.x + 9.0f * s, cl.y + 9.0f * s}, {cl.x + cl.width - 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    DrawLineEx({cl.x + cl.width - 9.0f * s, cl.y + 9.0f * s}, {cl.x + 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);

    const Rectangle bagCell0 = BankBagCellRect(0);
    DrawTextEx(font, "In the bank", {r.x + 14.0f * s, r.y + 44.0f * s}, 13.0f * s, 1.0f, kText);
    DrawTextEx(font, "Your backpack", {bagCell0.x, r.y + 44.0f * s}, 13.0f * s, 1.0f, kText);

    // what is under the pointer (desktop): its name and exact count
    Item hoverItem = Item::None;
    int hoverCount = 0;
    const int n = bank_ ? static_cast<int>(bank_->Slots().size()) : 0;
    for (int i = 0; i < kBankPerPage; ++i) {
        const int idx = bankPage_ * kBankPerPage + i;
        const Rectangle c = BankCellRect(i);
        const bool filled = idx < n;
        const bool hot = filled && CheckCollisionPointRec(pointer_, c);
        DrawRectangleRounded(c, 0.14f, 6, hot ? Color{255, 255, 255, 40} : kCell);
        if (!filled) continue;
        const ItemStack& st = bank_->Slots()[static_cast<size_t>(idx)];
        DrawItemIcon(st.item, c);
        DrawTextEx(font, ShortCount(st.count).c_str(), {c.x + 3.0f * s, c.y + 2.0f * s}, 11.0f * s, 1.0f, CountColor(st.count));
        if (hot) {
            hoverItem = st.item;
            hoverCount = st.count;
        }
    }
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const Rectangle c = BankBagCellRect(i);
        const ItemStack st = bag_ ? bag_->Slot(i) : ItemStack{};
        const bool hot = st.item != Item::None && CheckCollisionPointRec(pointer_, c);
        DrawRectangleRounded(c, 0.14f, 6, hot ? Color{255, 255, 255, 40} : kCell);
        if (st.item == Item::None) continue;
        DrawItemIcon(st.item, c);
        if (GetItemInfo(st.item).stackable && st.count > 1)
            DrawTextEx(font, ShortCount(st.count).c_str(), {c.x + 3.0f * s, c.y + 2.0f * s}, 11.0f * s, 1.0f, CountColor(st.count));
        if (hot) {
            hoverItem = st.item;
            hoverCount = st.count;
        }
    }
    if (hoverItem != Item::None)
        DrawTextEx(font, TextFormat("%s  x%s", GetItemInfo(hoverItem).name, WithCommas(hoverCount).c_str()),
                   {r.x + 14.0f * s, r.y + 70.0f * s + kBankRows * 47.0f * s + 52.0f * s}, 13.0f * s, 1.0f, kAccent);
    else
        DrawTextEx(font, "Tap an item in the bank to take it out,\nor an item in your backpack to put it in.",
                   {r.x + 14.0f * s, r.y + 70.0f * s + kBankRows * 47.0f * s + 52.0f * s}, 12.0f * s, 1.0f, kDim);

    static const int kQty[4] = {1, 5, 10, Bank::kMaxStack};
    static const char* kQtyLabel[4] = {"1", "5", "10", "All"};
    for (int i = 0; i < 4; ++i) {
        const Rectangle q = BankQtyRect(i);
        const bool on = bankQty_ == kQty[i];
        DrawRectangleRounded(q, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
        if (on) DrawRectangleRoundedLinesEx(q, 0.25f, 6, 2.0f * s, kAccent);
        const float ts = 14.0f * s;
        const Vector2 sz = MeasureTextEx(font, kQtyLabel[i], ts, 1.0f);
        DrawTextEx(font, kQtyLabel[i], {q.x + (q.width - sz.x) / 2, q.y + (q.height - sz.y) / 2}, ts, 1.0f, on ? kAccent : kText);
    }
    const Rectangle dp = BankDepositRect();
    DrawRectangleRounded(dp, 0.25f, 6, CheckCollisionPointRec(pointer_, dp) ? Color{70, 120, 76, 220} : Color{44, 90, 56, 200});
    DrawRectangleRoundedLinesEx(dp, 0.25f, 6, 1.0f * s, Color{140, 220, 150, 120});
    {
        const float ts = 13.0f * s;
        const Vector2 sz = MeasureTextEx(font, "Deposit everything", ts, 1.0f);
        DrawTextEx(font, "Deposit everything", {dp.x + (dp.width - sz.x) / 2, dp.y + (dp.height - sz.y) / 2}, ts, 1.0f, kText);
    }
    for (int i = 0; i < 2; ++i) {
        const Rectangle pr = BankPageRect(i);
        const bool enabled = i == 0 ? bankPage_ > 0 : bankPage_ < BankPages() - 1;
        DrawRectangleRounded(pr, 0.25f, 6, Color{255, 255, 255, static_cast<unsigned char>(enabled ? 28 : 10)});
        const Vector2 c = {pr.x + pr.width / 2, pr.y + pr.height / 2};
        const float a = 7.0f * s, dir = i == 0 ? -1.0f : 1.0f;
        Tri({c.x - dir * a * 0.5f, c.y - a}, {c.x - dir * a * 0.5f, c.y + a}, {c.x + dir * a * 0.9f, c.y}, enabled ? kText : kDim);
    }
    DrawTextEx(font, TextFormat("%d / %d", bankPage_ + 1, BankPages()), {BankPageRect(0).x - 44.0f * s, r.y + 44.0f * s}, 12.0f * s, 1.0f, kDim);
}

// ---- text wrapping, the dialogue box and the quest log
std::vector<std::string> Hud::WrapText(const Font& font, const std::string& text, float size, float maxWidth) const {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {   // paragraphs split at '\n'
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line;
        size_t pos = start;
        while (pos < end) {
            size_t sp = text.find(' ', pos);
            if (sp == std::string::npos || sp > end) sp = end;
            const std::string word = text.substr(pos, sp - pos);
            const std::string trial = line.empty() ? word : line + " " + word;
            if (!line.empty() && MeasureTextEx(font, trial.c_str(), size, 1.0f).x > maxWidth) {
                lines.push_back(line);
                line = word;
            } else {
                line = trial;
            }
            pos = sp + 1;
        }
        lines.push_back(line);
        if (end >= text.size()) break;
        start = end + 1;
    }
    return lines;
}

Rectangle Hud::DialogueRectOf() const {
    const float s = Scale();
    const Font font = GetFontDefault();
    const float w = std::min(700.0f * s, GetScreenWidth() * 0.96f);
    const std::vector<std::string> lines = WrapText(font, dialogue_.text, 16.0f * s, w - 32.0f * s);
    const float optionsH = dialogue_.options.empty() ? 24.0f * s : dialogue_.options.size() * 38.0f * s + 4.0f * s;
    const float h = 40.0f * s + lines.size() * 21.0f * s + optionsH + 14.0f * s;
    return {(GetScreenWidth() - w) / 2, GetScreenHeight() - h - 14.0f * s, w, h};
}

Rectangle Hud::DialogueOptionRect(int i) const {
    const float s = Scale();
    const Font font = GetFontDefault();
    const Rectangle r = DialogueRectOf();
    const std::vector<std::string> lines = WrapText(font, dialogue_.text, 16.0f * s, r.width - 32.0f * s);
    return {r.x + 14.0f * s, r.y + 40.0f * s + lines.size() * 21.0f * s + 6.0f * s + i * 38.0f * s, r.width - 28.0f * s, 34.0f * s};
}

bool Hud::DialogueHits(Vector2 p) const {
    return dialogue_.open && CheckCollisionPointRec(p, DialogueRectOf());
}

int Hud::DialogueAt(Vector2 p) const {
    if (!dialogue_.open) return -1;
    if (!CheckCollisionPointRec(p, DialogueRectOf())) return -1;
    for (size_t i = 0; i < dialogue_.options.size(); ++i)
        if (CheckCollisionPointRec(p, DialogueOptionRect(static_cast<int>(i)))) return static_cast<int>(i);
    return dialogue_.options.empty() ? kDialogueContinue : -3;   // (on the box but not on an option: nothing)
}

void Hud::DrawDialogue(float s, const Font& font) const {
    if (!dialogue_.open) return;
    const Rectangle r = DialogueRectOf();
    Panel(r, 0.05f, {16, 20, 28, 244});
    if (!dialogue_.speaker.empty()) DrawTextEx(font, dialogue_.speaker.c_str(), {r.x + 16.0f * s, r.y + 10.0f * s}, 18.0f * s, 1.0f, kAccent);
    else DrawTextEx(font, "Quest", {r.x + 16.0f * s, r.y + 10.0f * s}, 18.0f * s, 1.0f, {120, 220, 130, 255});
    const std::vector<std::string> lines = WrapText(font, dialogue_.text, 16.0f * s, r.width - 32.0f * s);
    float y = r.y + 38.0f * s;
    for (const std::string& l : lines) {
        DrawTextEx(font, l.c_str(), {r.x + 16.0f * s, y}, 16.0f * s, 1.0f, kText);
        y += 21.0f * s;
    }
    if (dialogue_.options.empty()) {
        DrawTextEx(font, "Tap to continue", {r.x + r.width - 140.0f * s, r.y + r.height - 24.0f * s}, 12.0f * s, 1.0f, kDim);
    } else {
        for (size_t i = 0; i < dialogue_.options.size(); ++i) {
            const Rectangle o = DialogueOptionRect(static_cast<int>(i));
            const bool hot = CheckCollisionPointRec(pointer_, o);
            DrawRectangleRounded(o, 0.25f, 6, hot ? Color{70, 120, 76, 220} : Color{44, 90, 56, 190});
            DrawRectangleRoundedLinesEx(o, 0.25f, 6, 1.0f * s, Color{140, 220, 150, 120});
            DrawTextEx(font, dialogue_.options[i].c_str(), {o.x + 12.0f * s, o.y + (o.height - 15.0f * s) / 2}, 15.0f * s, 1.0f, kText);
        }
    }
}

namespace {
// A thin scroll bar down the right edge of a panel.
void DrawScrollBar(float x, float top, float bottom, float scroll, float scrollMax, float view, float s) {
    if (scrollMax <= 0.0f) return;
    const float h = bottom - top;
    DrawRectangleRounded({x, top, 4.0f * s, h}, 0.5f, 4, {255, 255, 255, 40});
    const float thumb = std::max(18.0f * s, h * view / (view + scrollMax));
    DrawRectangleRounded({x, top + (h - thumb) * (scroll / scrollMax), 4.0f * s, thumb}, 0.5f, 4, {255, 214, 120, 190});
}

// The gold quest diamond of the map: filled for a step he is on, hollow for a quest someone has for him.
void DrawQuestDiamond(Vector2 c, float r, bool hollow) {
    const Color gold = {255, 205, 90, 255};
    auto diamond = [&](float k, Color col) {
        Tri({c.x, c.y - r * k}, {c.x - r * k, c.y}, {c.x + r * k, c.y}, col);
        Tri({c.x, c.y + r * k}, {c.x - r * k, c.y}, {c.x + r * k, c.y}, col);
    };
    diamond(1.3f, {0, 0, 0, 230});
    diamond(1.0f, gold);
    if (hollow) diamond(0.5f, {20, 22, 30, 255});
}
}  // namespace

bool Hud::ScrollableAt(Vector2 p) const {
    if (openTab_ != kTabQuests && !(openTab_ == kTabMenu && helpPage_)) return false;
    return CheckCollisionPointRec(p, PanelRect());
}

void Hud::DrawQuests(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Quests", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    if (!quests_ || !bag_) return;
    DrawRightAligned(font, TextFormat("%d quest points", quests_->Points()), panel.x + panel.width - 12.0f * s, panel.y + 10.0f * s, 12.0f * s, kDim);
    const float top = panel.y + 30.0f * s, start = panel.y + 34.0f * s, bottom = panel.y + panel.height - 6.0f * s;
    scroll_ = std::clamp(scroll_, 0.0f, scrollMax_);   // (the limit comes from the last frame)
    float y = start - scroll_;
    bool any = false;
    std::string finished;
    int finishedCount = 0;
    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(top), static_cast<int>(panel.width), static_cast<int>(bottom - top));
    for (int i = 0; i < quests_->Count(); ++i) {   // the ones in progress, in full (the page scrolls when there are many)...
        const QuestState& st = quests_->State(i);
        if (st.done) {
            finished += (finished.empty() ? "" : ", ") + AllQuests()[static_cast<size_t>(i)].name;
            ++finishedCount;
            continue;
        }
        if (!st.started) continue;
        any = true;
        const std::vector<std::string> lines = WrapText(font, quests_->ObjectiveText(i, *bag_), 12.0f * s, panel.width - 40.0f * s);
        DrawCircleV({panel.x + 18.0f * s, y + 9.0f * s}, 5.0f * s, Color{255, 205, 90, 255});
        DrawTextEx(font, AllQuests()[static_cast<size_t>(i)].name.c_str(), {panel.x + 30.0f * s, y + 1.0f * s}, 15.0f * s, 1.0f, kText);
        y += 26.0f * s;
        for (const std::string& l : lines) {
            DrawTextEx(font, l.c_str(), {panel.x + 30.0f * s, y - 6.0f * s}, 12.0f * s, 1.0f, {236, 214, 140, 255});
            y += 16.0f * s;
        }
    }
    if (finishedCount > 0) {   // ...and the finished ones as one short paragraph (there are a lot of them in the end)
        any = true;
        y += 6.0f * s;
        DrawCircleV({panel.x + 18.0f * s, y + 8.0f * s}, 5.0f * s, Color{110, 215, 130, 255});
        DrawTextEx(font, TextFormat("Completed (%d of %d)", finishedCount, quests_->Count()), {panel.x + 30.0f * s, y}, 14.0f * s, 1.0f, kText);
        y += 22.0f * s;
        for (const std::string& l : WrapText(font, finished, 11.0f * s, panel.width - 28.0f * s)) {
            DrawTextEx(font, l.c_str(), {panel.x + 14.0f * s, y}, 11.0f * s, 1.0f, kDim);
            y += 14.0f * s;
        }
    }
    if (!any) DrawTextEx(font, "Talk to people to find quests.\nLook for anyone with something to ask.", {panel.x + 12.0f * s, panel.y + 40.0f * s}, 13.0f * s, 1.0f, kDim);
    EndScissorMode();
    const float view = bottom - start;
    scrollMax_ = std::max(0.0f, (y + scroll_) - start + 8.0f * s - view);
    DrawScrollBar(panel.x + panel.width - 7.0f * s, top + 2.0f * s, bottom, scroll_, scrollMax_, view, s);
}

// ---- the Menu tab
Rectangle Hud::SettingsRow(int row) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    return {p.x + 10.0f * s, p.y + (36.0f + static_cast<float>(row) * 46.0f) * s, p.width - 20.0f * s, 38.0f * s};
}

Rectangle Hud::SettingsButton(int row, bool plus) const {
    const float s = Scale();
    const Rectangle r = SettingsRow(row);
    return plus ? Rectangle{r.x + r.width - 34.0f * s, r.y + 4.0f * s, 30.0f * s, 30.0f * s} : Rectangle{r.x + 86.0f * s, r.y + 4.0f * s, 30.0f * s, 30.0f * s};
}

Rectangle Hud::HelpBackRect() const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    return {p.x + p.width - 78.0f * s, p.y + p.height - 40.0f * s, 68.0f * s, 30.0f * s};
}

std::vector<std::string> Hud::HelpLines(const Font& font, float size, float maxWidth) const {
    static const char* kHelp[] = {
        "Tap or click the ground to walk. Tap a tree, rock, fishing spot, person or creature to use it. Long-press (or right-click) for more choices, such as Examine.",
        "Bag: tap an item to use it. Use a tinderbox on logs to light a fire, raw food on a fire or cooking range to cook it, a chisel on a gem, a needle on leather.",
        "Fighting: pick a style in the Combat tab. Wield a bow (arrows in the quiver) to shoot, or a staff and choose a spell to cast. Eat food to heal.",
        "Making things: a knife on logs carves a bow, a bowstring strings it. Flour and water make dough; bake dough and pies on a fire or range. The smith's menu has pages.",
        "Quests: talk to people who have something to ask. The Quests tab shows what to do next. Some steps say to use an item somewhere: pick the item (Use), then tap the ground there.",
        "Map: the red area is the Abyss, home of the best rocks and trees and of the players who hunt. Die there and you lose all but your three most valuable items; attack a player and a skull marks you for ten minutes, and a skulled death loses everything.",
        "Banks hold 200 items, shared between towns. Shopkeepers buy what they sell. A teleport scroll takes you to a town you have already visited.",
        "Camera: arrow keys or a two-finger drag turn it, the mouse wheel or a pinch zooms. F6 changes your look.",
    };
    std::vector<std::string> out;
    for (const char* t : kHelp) {
        const std::vector<std::string> lines = WrapText(font, t, size, maxWidth);
        out.insert(out.end(), lines.begin(), lines.end());
        if (playerLines_.empty()) out.push_back("");   // (with the player list below, the paragraphs run together: the page has to fit the screen)
    }
    if (!out.empty() && playerLines_.empty()) out.pop_back();
    if (!playerLines_.empty()) {   // who else is playing (the other adventurers): how many, and the top of the hiscores
        out.push_back("");
        for (const std::string& l : playerLines_) out.push_back(l);
    }
    return out;
}

void Hud::DrawHelp(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "How to play", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    const std::vector<std::string> lines = HelpLines(font, 12.0f * s, panel.width - 24.0f * s);
    const float top = panel.y + 30.0f * s, start = panel.y + 34.0f * s, bottom = panel.y + panel.height - 44.0f * s;   // (the Back button is under the text)
    scroll_ = std::clamp(scroll_, 0.0f, scrollMax_);
    float y = start - scroll_;
    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(top), static_cast<int>(panel.width), static_cast<int>(bottom - top));
    for (const std::string& l : lines) {
        if (!l.empty()) DrawTextEx(font, l.c_str(), {panel.x + 12.0f * s, y}, 12.0f * s, 1.0f, kText);
        y += 16.0f * s;
    }
    EndScissorMode();
    scrollMax_ = std::max(0.0f, (y + scroll_) - start - (bottom - start));
    DrawScrollBar(panel.x + panel.width - 7.0f * s, top + 2.0f * s, bottom, scroll_, scrollMax_, bottom - start, s);
    const Rectangle b = HelpBackRect();
    DrawRectangleRounded(b, 0.25f, 6, CheckCollisionPointRec(pointer_, b) ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235});
    const Vector2 sz = MeasureTextEx(font, "Back", 14.0f * s, 1.0f);
    DrawTextEx(font, "Back", {b.x + (b.width - sz.x) / 2, b.y + (b.height - sz.y) / 2}, 14.0f * s, 1.0f, kText);
}

Hud::SettingsHit Hud::SettingsAt(Vector2 p) const {
    if (openTab_ != kTabMenu) return SettingsHit::None;
    if (helpPage_) return CheckCollisionPointRec(p, HelpBackRect()) ? SettingsHit::Back : SettingsHit::None;
    if (CheckCollisionPointRec(p, SettingsRow(3))) return SettingsHit::Help;
    if (CheckCollisionPointRec(p, SettingsRow(0))) return SettingsHit::Appearance;
    if (CheckCollisionPointRec(p, SettingsButton(1, false))) return SettingsHit::MusicDown;
    if (CheckCollisionPointRec(p, SettingsButton(1, true))) return SettingsHit::MusicUp;
    if (CheckCollisionPointRec(p, SettingsButton(2, false))) return SettingsHit::SfxDown;
    if (CheckCollisionPointRec(p, SettingsButton(2, true))) return SettingsHit::SfxUp;
    return SettingsHit::None;
}

void Hud::DrawSettings(Rectangle panel, float s, const Font& font) const {
    if (helpPage_) {
        DrawHelp(panel, s, font);
        return;
    }
    DrawTextEx(font, "Menu", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    {
        const Rectangle hb = SettingsRow(3);
        DrawRectangleRounded(hb, 0.2f, 6, CheckCollisionPointRec(pointer_, hb) ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235});
        const Vector2 hs = MeasureTextEx(font, "How to play", 14.0f * s, 1.0f);
        DrawTextEx(font, "How to play", {hb.x + (hb.width - hs.x) / 2, hb.y + (hb.height - hs.y) / 2}, 14.0f * s, 1.0f, kText);
    }
    const Rectangle a = SettingsRow(0);
    const bool hot = CheckCollisionPointRec(pointer_, a);
    DrawRectangleRounded(a, 0.2f, 6, hot ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235});
    const Vector2 sz = MeasureTextEx(font, "Change appearance", 14.0f * s, 1.0f);
    DrawTextEx(font, "Change appearance", {a.x + (a.width - sz.x) / 2, a.y + (a.height - sz.y) / 2}, 14.0f * s, 1.0f, kText);
    for (int row = 1; row <= 2; ++row) {
        const Rectangle r = SettingsRow(row);
        const float v = row == 1 ? musicVol_ : sfxVol_;
        DrawTextEx(font, row == 1 ? "Music" : "Sound", {r.x + 4.0f * s, r.y + 10.0f * s}, 14.0f * s, 1.0f, kText);
        for (int plus = 0; plus < 2; ++plus) {
            const Rectangle b = SettingsButton(row, plus != 0);
            DrawRectangleRounded(b, 0.25f, 6, CheckCollisionPointRec(pointer_, b) ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235});
            DrawTextEx(font, plus ? "+" : "-", {b.x + 10.0f * s, b.y + 5.0f * s}, 20.0f * s, 1.0f, kText);
        }
        const Rectangle lo = SettingsButton(row, false), hi = SettingsButton(row, true);
        const Rectangle bar = {lo.x + lo.width + 6.0f * s, r.y + 13.0f * s, hi.x - (lo.x + lo.width) - 12.0f * s, 12.0f * s};
        DrawRectangleRounded(bar, 0.5f, 6, {20, 24, 34, 255});
        if (v > 0.0f) DrawRectangleRounded({bar.x, bar.y, bar.width * std::clamp(v, 0.0f, 1.0f), bar.height}, 0.5f, 6, kAccent);
    }
}

// ---- the world map
namespace {
// What open ground looks like on the map, by the place it is in (the tile map only knows "walkable").
Color MapGround(const std::string& area) {
    if (area == "Troll Mountains") return {136, 128, 112, 255};
    if (area == "Mining Caves") return {76, 70, 66, 255};
    if (area == "Desert Ruins") return {222, 204, 150, 255};
    if (area == "The Dragon's Temple" || area == "Castle Crypt") return {188, 182, 170, 255};
    if (area == "The Abyss") return {70, 52, 92, 255};
    if (area == "Quarry" || area == "Goblin Camp" || area == "Millbrook Mine" || area == "Chicken Coop" || area == "Cow Pen") return {156, 122, 82, 255};
    if (area == "Market Square" || area == "Millbrook Square" || area == "Castle Courtyard" || area == "Throne Hall" || area == "Smithy" ||
        area == "Millbrook Smithy" || area == "Bank" || area == "Millbrook Bank" || area == "The Journey's Rest Inn" || area == "Millbrook Crafters' Yard" ||
        area == "Millbrook Chapel")
        return {182, 176, 164, 255};
    if (area == "Frostpeak Range") return {150, 146, 140, 255};
    if (area == "The Northern Reach" || area == "Northwatch Moor") return {116, 148, 112, 255};
    if (area == "Whispering Pines") return {68, 118, 84, 255};
    if (area == "Elmwood") return {60, 136, 70, 255};
    if (area == "Greenmarch" || area == "The Sunfields") return {198, 182, 104, 255};
    if (area == "Mirewood Swamp") return {92, 104, 74, 255};
    if (area == "Barrow Downs") return {120, 114, 104, 255};
    if (area == "Eastwatch Downs") return {166, 166, 94, 255};
    if (area == "Darkwood") return {50, 100, 66, 255};
    if (area == "Giant's Rise") return {148, 138, 110, 255};
    if (area == "The Iron Range") return {122, 116, 108, 255};
    if (area == "The Dune Sea") return {214, 164, 104, 255};
    if (area == "Elmswick" || area == "Saltmere") return {188, 176, 150, 255};
    if (area == "The Eastern Sea") return {226, 212, 160, 255};
    return {112, 170, 90, 255};
}
}  // namespace

void Hud::SetMap(const TileMap* map, const People* people) {
    map_ = map;
    people_ = people;
    if (mapTex_.id != 0) UnloadTexture(mapTex_);
    mapTex_ = {};
    if (!map) return;
    Image img = GenImageColor(map->Width(), map->Height(), BLACK);
    for (int z = 0; z < map->Height(); ++z)
        for (int x = 0; x < map->Width(); ++x) {
            Color c;
            switch (map->At(x, z)) {
                case '~': c = {64, 126, 206, 255}; break;
                case '#': c = {96, 88, 80, 255}; break;
                case 'D': c = {160, 106, 52, 255}; break;
                case 'T': c = {40, 104, 48, 255}; break;
                case 'o': c = {128, 118, 104, 255}; break;
                case 'f': c = {176, 146, 92, 255}; break;
                default: c = MapGround(map->AreaAt(x, z)); break;
            }
            ImageDrawPixel(&img, x, z, c);
        }
    mapTex_ = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(mapTex_, TEXTURE_FILTER_POINT);
}

void Hud::DrawMap(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Map", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    if (!map_ || mapTex_.id == 0) return;
    const std::string& here = map_->AreaAt(playerTile_.x, playerTile_.z);
    if (!here.empty()) DrawRightAligned(font, here.c_str(), panel.x + panel.width - 12.0f * s, panel.y + 10.0f * s, 13.0f * s, kText);
    const float top = panel.y + 32.0f * s, bottom = panel.y + panel.height - 26.0f * s;
    const float gap = 8.0f * s;
    const float mw = static_cast<float>(map_->Width()), mh = static_cast<float>(map_->Height());
    // on the right the whole world, to scale; on the left a close-up that follows him in both directions
    const float stripH = bottom - top;
    const float stripW = std::min(stripH * mw / mh, panel.width * 0.36f);
    const Rectangle zoom = {panel.x + 10.0f * s, top, panel.width - 20.0f * s - stripW - gap, bottom - top};
    const float tilePx = zoom.width / std::min(mw, 76.0f);
    const float cols = std::min(zoom.width / tilePx, mw);   // how many tile columns and rows the close-up shows
    const float rows = std::min(zoom.height / tilePx, mh);
    const float x0 = std::floor(std::clamp(static_cast<float>(playerTile_.x) + 0.5f - cols / 2.0f, 0.0f, std::max(0.0f, mw - cols)));
    const float z0 = std::floor(std::clamp(static_cast<float>(playerTile_.z) + 0.5f - rows / 2.0f, 0.0f, std::max(0.0f, mh - rows)));
    DrawRectangleRec({zoom.x - 2.0f * s, zoom.y - 2.0f * s, zoom.width + 4.0f * s, zoom.height + 4.0f * s}, {10, 12, 18, 255});
    DrawTexturePro(mapTex_, {x0, z0, cols, rows}, {zoom.x, zoom.y, cols * tilePx, rows * tilePx}, {0.0f, 0.0f}, 0.0f, WHITE);
    BeginScissorMode(static_cast<int>(zoom.x), static_cast<int>(zoom.y), static_cast<int>(zoom.width), static_cast<int>(zoom.height));
    // danger zones (the Abyss) get a red tint and a red border, as the design document says
    for (const AreaDef& a : map_->Areas()) {
        if (!IsDangerousArea(a.name)) continue;
        const Rectangle ar = {zoom.x + (static_cast<float>(a.rect.x0) - x0) * tilePx, zoom.y + (static_cast<float>(a.rect.z0) - z0) * tilePx,
                              static_cast<float>(a.rect.x1 - a.rect.x0) * tilePx, static_cast<float>(a.rect.z1 - a.rect.z0) * tilePx};
        DrawRectangleRec(ar, {200, 24, 24, 46});
        DrawRectangleLinesEx(ar, std::max(2.0f, 2.0f * s), {236, 52, 44, 235});
    }
    // place names: the smallest places first, and a name that would sit on top of another one is left out
    std::vector<const AreaDef*> areas;
    for (const AreaDef& a : map_->Areas()) areas.push_back(&a);
    std::sort(areas.begin(), areas.end(), [](const AreaDef* a, const AreaDef* b) {
        return (a->rect.x1 - a->rect.x0) * (a->rect.z1 - a->rect.z0) < (b->rect.x1 - b->rect.x0) * (b->rect.z1 - b->rect.z0);
    });
    std::vector<Rectangle> taken;
    const float ts = 10.0f * s;
    for (const AreaDef* a : areas) {
        // the centre of the part of the place that is in view
        const float vx0 = std::max(static_cast<float>(a->rect.x0), x0), vx1 = std::min(static_cast<float>(a->rect.x1), x0 + cols);
        const float vz0 = std::max(static_cast<float>(a->rect.z0), z0), vz1 = std::min(static_cast<float>(a->rect.z1), z0 + rows);
        if (vx1 - vx0 < 3.0f || vz1 - vz0 < 3.0f) continue;
        const float cz = (vz0 + vz1) * 0.5f;
        const Vector2 sz = MeasureTextEx(font, a->name.c_str(), ts, 1.0f);
        float cx = zoom.x + ((vx0 + vx1) * 0.5f - x0) * tilePx - sz.x / 2.0f;
        cx = std::clamp(cx, zoom.x + 2.0f * s, zoom.x + zoom.width - sz.x - 2.0f * s);
        const Rectangle box = {cx - 2.0f * s, zoom.y + (cz - z0) * tilePx - sz.y / 2.0f, sz.x + 4.0f * s, sz.y};
        bool clash = false;
        for (const Rectangle& t : taken) clash = clash || CheckCollisionRecs(box, t);
        if (clash) continue;
        taken.push_back(box);
        DrawTextEx(font, a->name.c_str(), {box.x + 2.0f * s + 1.0f, box.y + 1.0f}, ts, 1.0f, {0, 0, 0, 220});
        DrawTextEx(font, a->name.c_str(), {box.x + 2.0f * s, box.y}, ts, 1.0f, {255, 246, 214, 255});
    }
    if (people_) {
        for (const Person& p : people_->All()) {
            const float py = zoom.y + (p.z - z0) * tilePx, px = zoom.x + (p.x - x0) * tilePx;
            if (py < zoom.y || py > zoom.y + zoom.height || px < zoom.x || px > zoom.x + zoom.width) continue;
            const std::string shop = p.info->shop;
            const std::string key = p.info->key;
            Color c = {240, 240, 240, 255};
            if (shop == "bank") c = {255, 214, 70, 255};
            else if (!shop.empty()) c = {96, 176, 255, 255};
            else if (key == "silas" || key == "barnaby") c = {110, 240, 240, 255};
            DrawCircleV({px, py}, 2.6f * s, {0, 0, 0, 200});
            DrawCircleV({px, py}, 1.8f * s, c);
        }
    }
    // quests: a gold diamond at the next step of each quest in progress (hollow: someone with a new quest for him); one off the edge of the
    // close-up is an arrow on the edge, pointing the way
    for (const QuestMarker& m : markers_) {
        const float px = zoom.x + (m.x - x0) * tilePx, py = zoom.y + (m.z - z0) * tilePx;
        if (px >= zoom.x && px <= zoom.x + zoom.width && py >= zoom.y && py <= zoom.y + zoom.height) {
            DrawQuestDiamond({px, py}, 4.4f * s, m.offer);
            continue;
        }
        if (m.offer) continue;
        const Vector2 mid = {zoom.x + zoom.width * 0.5f, zoom.y + zoom.height * 0.5f};
        const float dx = px - mid.x, dy = py - mid.y, len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) continue;
        const Vector2 d = {dx / len, dy / len};
        const float hx = zoom.width * 0.5f - 10.0f * s, hy = zoom.height * 0.5f - 10.0f * s;
        const float t = std::min(std::fabs(d.x) > 1e-4f ? hx / std::fabs(d.x) : 1e9f, std::fabs(d.y) > 1e-4f ? hy / std::fabs(d.y) : 1e9f);
        const Vector2 at = {mid.x + d.x * t, mid.y + d.y * t}, n = {-d.y, d.x};
        const float r = 6.5f * s;
        Tri({at.x + d.x * r * 1.5f, at.y + d.y * r * 1.5f}, {at.x - d.x * r * 0.4f + n.x * r, at.y - d.y * r * 0.4f + n.y * r},
            {at.x - d.x * r * 0.4f - n.x * r, at.y - d.y * r * 0.4f - n.y * r}, {0, 0, 0, 230});
        Tri({at.x + d.x * r * 1.2f, at.y + d.y * r * 1.2f}, {at.x - d.x * r * 0.2f + n.x * r * 0.7f, at.y - d.y * r * 0.2f + n.y * r * 0.7f},
            {at.x - d.x * r * 0.2f - n.x * r * 0.7f, at.y - d.y * r * 0.2f - n.y * r * 0.7f}, {255, 205, 90, 255});
    }
    const Vector2 me = {zoom.x + (static_cast<float>(playerTile_.x) + 0.5f - x0) * tilePx, zoom.y + (static_cast<float>(playerTile_.z) + 0.5f - z0) * tilePx};
    DrawCircleV(me, 5.0f * s, {0, 0, 0, 230});
    DrawCircleV(me, 3.6f * s, {255, 70, 60, 255});
    EndScissorMode();
    // the whole world: where the close-up is, and where he stands
    const Rectangle strip = {zoom.x + zoom.width + gap, top, stripW, stripH};
    DrawRectangleRec({strip.x - 2.0f * s, strip.y - 2.0f * s, strip.width + 4.0f * s, strip.height + 4.0f * s}, {10, 12, 18, 255});
    DrawTexturePro(mapTex_, {0.0f, 0.0f, mw, mh}, strip, {0.0f, 0.0f}, 0.0f, WHITE);
    for (const AreaDef& a : map_->Areas()) {
        if (!IsDangerousArea(a.name)) continue;
        const Rectangle ar = {strip.x + static_cast<float>(a.rect.x0) / mw * strip.width, strip.y + static_cast<float>(a.rect.z0) / mh * strip.height,
                              static_cast<float>(a.rect.x1 - a.rect.x0) / mw * strip.width, static_cast<float>(a.rect.z1 - a.rect.z0) / mh * strip.height};
        DrawRectangleRec(ar, {200, 24, 24, 70});
        DrawRectangleLinesEx(ar, std::max(1.0f, s), {236, 52, 44, 235});
    }
    DrawRectangleLinesEx({strip.x + x0 / mw * strip.width, strip.y + z0 / mh * strip.height, cols / mw * strip.width, rows / mh * strip.height}, std::max(1.0f, s),
                         {255, 240, 150, 255});
    for (const QuestMarker& m : markers_) {   // (the quests in progress; the offers would crowd it)
        if (m.offer) continue;
        const Vector2 c = {strip.x + m.x / mw * strip.width, strip.y + m.z / mh * strip.height};
        DrawCircleV(c, 3.0f * s, {0, 0, 0, 230});
        DrawCircleV(c, 2.1f * s, {255, 205, 90, 255});
    }
    DrawCircleV({strip.x + (static_cast<float>(playerTile_.x) + 0.5f) / mw * strip.width, strip.y + (static_cast<float>(playerTile_.z) + 0.5f) / mh * strip.height},
                2.4f * s, {255, 70, 60, 255});
    // legend
    float lx = panel.x + 12.0f * s;
    const float ly = panel.y + panel.height - 17.0f * s;
    const std::pair<Color, const char*> legend[] = {{{255, 70, 60, 255}, "you"}, {{255, 214, 70, 255}, "bank"}, {{96, 176, 255, 255}, "shop"},
                                                    {{110, 240, 240, 255}, "ferry"}, {{240, 240, 240, 255}, "person"},
                                                    {{255, 205, 90, 255}, "quest"}};
    for (const auto& e : legend) {
        DrawCircleV({lx + 4.0f * s, ly + 6.0f * s}, 3.5f * s, e.first);
        DrawTextEx(font, e.second, {lx + 11.0f * s, ly}, 11.0f * s, 1.0f, kDim);
        lx += 11.0f * s + MeasureTextEx(font, e.second, 11.0f * s, 1.0f).x + 10.0f * s;
    }
}

// ---- the spellbook
Rectangle Hud::SpellRect(int i) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    return {p.x + 10.0f * s, p.y + (34.0f + 24.0f + static_cast<float>(i % kSpellsPerPage) * 52.0f) * s, p.width - 20.0f * s, 48.0f * s};
}

int Hud::SpellPageCount() const { return (SpellCount() + kSpellsPerPage - 1) / kSpellsPerPage; }

Rectangle Hud::SpellPageRect(bool next) const {
    const float s = Scale();
    const Rectangle p = PanelRect();
    return {next ? p.x + p.width - 10.0f * s - 70.0f * s : p.x + 10.0f * s, p.y + p.height - 44.0f * s, 70.0f * s, 30.0f * s};
}

int Hud::SpellPageStep(Vector2 p) const {
    if (openTab_ != kTabMagic || SpellPageCount() <= 1) return 0;
    if (CheckCollisionPointRec(p, SpellPageRect(false))) return -1;
    if (CheckCollisionPointRec(p, SpellPageRect(true))) return 1;
    return 0;
}

void Hud::StepSpellPage(int step) {
    spellPage_ = std::clamp(spellPage_ + step, 0, std::max(0, SpellPageCount() - 1));
}

int Hud::SpellAt(Vector2 p) const {
    if (openTab_ != kTabMagic) return -1;
    for (int i = spellPage_ * kSpellsPerPage; i < std::min(SpellCount(), (spellPage_ + 1) * kSpellsPerPage); ++i)
        if (CheckCollisionPointRec(p, SpellRect(i))) return i;
    return -1;
}

void Hud::DrawMagic(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Spellbook", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    const int level = skills_ ? skills_->Level(Skill::Magic) : 1;
    DrawRightAligned(font, TextFormat("Magic level %d", level), panel.x + panel.width - 12.0f * s, panel.y + 10.0f * s, 13.0f * s, kText);
    const Item weapon = equip_ ? equip_->Get(EquipSlot::Weapon) : Item::None;
    DrawTextEx(font, IsStaff(weapon) ? "Tap a spell to choose it, then attack." : "Wield a staff to cast spells.",
               {panel.x + 12.0f * s, panel.y + 32.0f * s}, 12.0f * s, 1.0f, IsStaff(weapon) ? kDim : Color{255, 160, 120, 255});
    for (int i = spellPage_ * kSpellsPerPage; i < std::min(SpellCount(), (spellPage_ + 1) * kSpellsPerPage); ++i) {
        const SpellInfo& sp = GetSpell(i);
        const Rectangle r = SpellRect(i);
        const bool on = i == spell_;
        const bool levelOk = level >= sp.level;
        DrawRectangleRounded(r, 0.18f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, static_cast<unsigned char>(CheckCollisionPointRec(pointer_, r) ? 26 : 14)});
        if (on) DrawRectangleRoundedLinesEx(r, 0.18f, 6, 2.0f * s, kAccent);
        const Rgb sc = SpellColor(sp);
        DrawCircleV({r.x + 14.0f * s, r.y + 14.0f * s}, 6.0f * s, {sc.r, sc.g, sc.b, static_cast<unsigned char>(levelOk ? 255 : 110)});
        DrawTextEx(font, sp.name, {r.x + 28.0f * s, r.y + 5.0f * s}, 15.0f * s, 1.0f, levelOk ? (on ? kAccent : kText) : kDim);
        DrawRightAligned(font, TextFormat("Lvl %d", sp.level), r.x + r.width - 8.0f * s, r.y + 6.0f * s, 12.0f * s, levelOk ? kDim : Color{255, 120, 110, 255});
        float x = r.x + 10.0f * s;
        for (int k = 0; k < sp.costCount; ++k) {
            const Rune rune = sp.cost[k].rune;
            const int need = RunesNeeded(sp, rune, weapon);
            const int have = bag_ ? bag_->Count(RuneItem(rune)) : 0;
            const bool free = need == 0;
            const Rectangle ic = {x, r.y + 22.0f * s, 22.0f * s, 22.0f * s};
            DrawItemIcon(RuneItem(rune), ic);
            const std::string txt = free ? "free" : "x" + std::to_string(sp.cost[k].count);
            DrawTextEx(font, txt.c_str(), {x + 22.0f * s, r.y + 27.0f * s}, 12.0f * s, 1.0f,
                       free ? Color{140, 220, 150, 255} : (have >= need ? Color{150, 230, 160, 255} : Color{255, 120, 110, 255}));
            x += 22.0f * s + MeasureTextEx(font, txt.c_str(), 12.0f * s, 1.0f).x + 10.0f * s;
        }
        DrawRightAligned(font, TextFormat("max %d", sp.maxHit), r.x + r.width - 8.0f * s, r.y + 28.0f * s, 12.0f * s, kDim);
    }
    if (SpellPageCount() > 1) {   // < Page 1/2 >
        for (int next = 0; next < 2; ++next) {
            const Rectangle b = SpellPageRect(next != 0);
            const bool ok = next ? spellPage_ < SpellPageCount() - 1 : spellPage_ > 0;
            DrawRectangleRounded(b, 0.25f, 6, ok ? (CheckCollisionPointRec(pointer_, b) ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235}) : Color{40, 44, 56, 150});
            const char* label = next ? "Next >" : "< Prev";
            const Vector2 sz = MeasureTextEx(font, label, 13.0f * s, 1.0f);
            DrawTextEx(font, label, {b.x + (b.width - sz.x) / 2, b.y + (b.height - sz.y) / 2}, 13.0f * s, 1.0f, ok ? kText : kDim);
        }
        const char* pg = TextFormat("Page %d/%d", spellPage_ + 1, SpellPageCount());
        const Vector2 sz = MeasureTextEx(font, pg, 12.0f * s, 1.0f);
        DrawTextEx(font, pg, {panel.x + (panel.width - sz.x) / 2, panel.y + panel.height - 36.0f * s}, 12.0f * s, 1.0f, kDim);
    }
}

// A small pill under the hit points: the arrows left in the quiver, or the spell a staff will cast and how many casts the runes allow.
void Hud::DrawAmmoPill(float s, const Font& font, float x, float y) const {
    if (!equip_) return;
    const Item weapon = equip_->Get(EquipSlot::Weapon);
    std::string txt;
    Item icon = Item::None;
    Color col = kText;
    if (IsBow(weapon)) {
        const int n = equip_->AmmoCount();
        txt = std::to_string(n) + " arrows";
        icon = n > 0 ? equip_->Get(EquipSlot::Ammo) : Item::BronzeArrows;
        if (n <= 0) col = {255, 120, 110, 255};
    } else if (IsStaff(weapon)) {
        const SpellInfo& sp = GetSpell(spell_);
        int casts = 999999;
        for (int k = 0; k < sp.costCount; ++k) {
            const int need = RunesNeeded(sp, sp.cost[k].rune, weapon);
            if (need > 0) casts = std::min(casts, (bag_ ? bag_->Count(RuneItem(sp.cost[k].rune)) : 0) / need);
        }
        txt = casts >= 999999 ? std::string(sp.name) + "  (endless)" : std::string(sp.name) + "  x" + std::to_string(casts);
        icon = RuneItem(sp.element);
        if (casts <= 0) col = {255, 120, 110, 255};
    } else {
        return;
    }
    const float ts = 13.0f * s;
    const float w = 34.0f * s + MeasureTextEx(font, txt.c_str(), ts, 1.0f).x;
    const Rectangle r = {x, y, std::max(w, 110.0f * s), 28.0f * s};
    Panel(r, 0.4f);
    DrawItemIcon(icon, {r.x + 2.0f * s, r.y + 2.0f * s, 24.0f * s, 24.0f * s});
    DrawTextEx(font, txt.c_str(), {r.x + 30.0f * s, r.y + (r.height - ts) / 2}, ts, 1.0f, col);
}

bool Hud::Hits(Vector2 p) const {
    if (BankHits(p) || DialogueHits(p)) return true;
    if (ShopHits(p)) return true;
    if (MakeHits(p)) return true;
    if (MenuOpen() && MenuHits(p)) return true;
    if (CheckCollisionPointRec(p, ChatRect())) return true;
    for (int i = 0; i < kTabCount; ++i)
        if (CheckCollisionPointRec(p, TabRect(i))) return true;
    if (openTab_ >= 0 && CheckCollisionPointRec(p, PanelRect())) return true;
    return false;
}

int Hud::TabAt(Vector2 p) const {
    for (int i = 0; i < kTabCount; ++i)
        if (CheckCollisionPointRec(p, TabRect(i))) return i;
    return -1;
}

void Hud::ToggleTab(int tab) {
    if (tab < 0 || tab >= kTabCount) return;
    helpPage_ = false;
    openTab_ = (openTab_ == tab) ? -1 : tab;
}

int Hud::BagSlotAt(Vector2 p) const {
    if (openTab_ != kTabBag) return -1;
    for (int i = 0; i < Inventory::kSlots; ++i)
        if (CheckCollisionPointRec(p, BagCellRect(i))) return i;
    return -1;
}

void Hud::Say(const std::string& text, Color color) {
    lines_.push_back({text, color});
    while (lines_.size() > 6) lines_.pop_front();
}

void Hud::ShowArea(const std::string& name) {
    area_ = name;
    areaTime_ = 0.0f;
}

void Hud::XpDrop(Skill skill, int amount) {
    int lane = 0;   // drops that arrive together (Attack + Hitpoints XP) stack instead of printing on top of each other
    for (const Drop& d : drops_)
        if (d.age < 0.7f) lane = std::max(lane, d.lane + 1);
    drops_.push_back({std::string("+") + std::to_string(amount) + " " + SkillName(skill) + " xp", 0.0f, lane});
    while (drops_.size() > 4) drops_.pop_front();
}

void Hud::Update(float dt) {
    const int scrollKey = openTab_ * 2 + (helpPage_ ? 1 : 0);
    if (scrollKey != scrollKey_) {   // another page: start at the top
        scrollKey_ = scrollKey;
        scroll_ = scrollMax_ = 0.0f;
    }
    if (selected_ >= 0 && (!bag_ || bag_->Slot(selected_).item == Item::None)) selected_ = -1;   // the item is gone
    areaTime_ += dt;
    for (Splat& sp : splats_) sp.age += dt;
    splats_.erase(std::remove_if(splats_.begin(), splats_.end(), [](const Splat& sp) { return sp.age > 1.3f; }), splats_.end());
    for (Drop& d : drops_) d.age += dt;
    while (!drops_.empty() && drops_.front().age > 2.4f) drops_.pop_front();
}

void Hud::DrawBag(Rectangle panel, float s, const Font& font) const {
    const int used = bag_ ? Inventory::kSlots - bag_->FreeSlots() : 0;
    DrawTextEx(font, "Backpack", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    DrawRightAligned(font, std::to_string(used) + "/" + std::to_string(Inventory::kSlots), panel.x + panel.width - 12.0f * s,
                     panel.y + 9.0f * s, 14.0f * s, kDim);
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const Rectangle c = BagCellRect(i);
        const bool sel = (i == selected_);
        DrawRectangleRounded(c, 0.14f, 6, sel ? Color{255, 205, 90, 60} : kCell);
        if (sel) DrawRectangleRoundedLinesEx(c, 0.14f, 6, 2.0f * s, kAccent);
        if (i == useSlot_) {   // the item waiting to be used with something else
            DrawRectangleRounded(c, 0.14f, 6, Color{90, 220, 120, 50});
            DrawRectangleRoundedLinesEx(c, 0.14f, 6, 2.5f * s, {110, 235, 140, 255});
        }
        if (!bag_) continue;
        const ItemStack& st = bag_->Slot(i);
        if (st.item == Item::None) continue;
        DrawItemIcon(st.item, c);
        if (GetItemInfo(st.item).stackable && st.count > 1)
            DrawTextEx(font, std::to_string(st.count).c_str(), {c.x + 4.0f * s, c.y + 3.0f * s}, 13.0f * s, 1.0f, {255, 240, 120, 255});
    }

    // Under the grid: the selected item's name and what you can do with it.
    const float lineY = panel.y + panel.height - (kActionBarH - 4.0f) * s;
    if (bag_ && selected_ >= 0 && selected_ < Inventory::kSlots && bag_->Slot(selected_).item != Item::None) {
        const Item item = bag_->Slot(selected_).item;
        DrawTextEx(font, GetItemInfo(item).name, {panel.x + 12.0f * s, lineY}, 14.0f * s, 1.0f, kAccent);
        const std::vector<ItemAction> acts = ActionsFor(item, *bag_);
        for (size_t i = 0; i < acts.size(); ++i) {
            const Rectangle b = ActionButtonRect(static_cast<int>(i));
            const bool active = acts[i] == ItemAction::Use && useSlot_ == selected_;
            Color fill = {44, 90, 56, 235};
            if (acts[i] == ItemAction::Drop) fill = {120, 50, 44, 235};
            else if (acts[i] == ItemAction::Use) fill = active ? Color{50, 140, 70, 240} : Color{48, 64, 96, 235};
            else if (acts[i] == ItemAction::Examine) fill = {58, 62, 74, 235};
            DrawRectangleRounded(b, 0.3f, 6, fill);
            DrawRectangleRoundedLinesEx(b, 0.3f, 6, 1.0f * s, kBorder);
            const float ts = 12.0f * s;
            const Vector2 sz = MeasureTextEx(font, ActionName(acts[i]), ts, 1.0f);
            DrawTextEx(font, ActionName(acts[i]), {b.x + (b.width - sz.x) / 2, b.y + (b.height - sz.y) / 2}, ts, 1.0f, kText);
        }
    } else {
        DrawTextEx(font, "Tap an item to select it.", {panel.x + 12.0f * s, lineY}, 12.0f * s, 1.0f, kDim);
    }
}

void Hud::DrawSkills(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Skills", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    DrawRightAligned(font, "level / 99", panel.x + panel.width - 12.0f * s, panel.y + 10.0f * s, 12.0f * s, kDim);
    if (!skills_) return;
    float y = panel.y + 34.0f * s;
    for (int i = 0; i < kSkillCount; ++i) {
        const Skill sk = static_cast<Skill>(i);
        const float x0 = panel.x + 12.0f * s, x1 = panel.x + panel.width - 12.0f * s;
        DrawTextEx(font, SkillName(sk), {x0, y + 2.0f * s}, 15.0f * s, 1.0f, kText);
        DrawRightAligned(font, std::to_string(skills_->Level(sk)), x1, y + 2.0f * s, 15.0f * s, kAccent);
        int64_t into = 0, need = 1;
        skills_->Progress(sk, &into, &need);
        const Rectangle bar = {x0, y + 21.0f * s, x1 - x0, 4.0f * s};
        DrawRectangleRec(bar, {255, 255, 255, 26});
        DrawRectangleRec({bar.x, bar.y, bar.width * static_cast<float>(into) / static_cast<float>(std::max<int64_t>(1, need)), bar.height},
                         {110, 200, 120, 255});
        y += 30.0f * s;
    }
    const float fy = panel.y + panel.height - 26.0f * s;
    DrawTextEx(font, TextFormat("Total level %d", skills_->TotalLevel()), {panel.x + 12.0f * s, fy}, 14.0f * s, 1.0f, kText);
    DrawRightAligned(font, WithCommas(skills_->TotalXp()) + " xp", panel.x + panel.width - 12.0f * s, fy + 1.0f * s, 12.0f * s, kDim);
}

void Hud::DrawGear(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Worn gear", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    static const char* kHint[kEquipSlotCount] = {"Head", "Body", "Legs", "Weapon", "Shield", "Quiver", "Ring", "Amulet", "Feet", "Hands", "Cape"};
    for (int i = 0; i < kEquipSlotCount; ++i) {
        const Rectangle r = GearSlotRect(i);
        DrawRectangleRounded(r, 0.14f, 6, kCell);
        DrawRectangleRoundedLinesEx(r, 0.14f, 6, 1.0f * s, kBorder);
        const Item item = equip_ ? equip_->Get(static_cast<EquipSlot>(i)) : Item::None;
        if (item != Item::None) {
            DrawItemIcon(item, r);
            if (i == static_cast<int>(EquipSlot::Ammo) && equip_)
                DrawTextEx(font, std::to_string(equip_->AmmoCount()).c_str(), {r.x + 4.0f * s, r.y + 3.0f * s}, 13.0f * s, 1.0f, {255, 240, 120, 255});
        } else {
            const float ts = 11.0f * s;
            const Vector2 sz = MeasureTextEx(font, kHint[i], ts, 1.0f);
            DrawTextEx(font, kHint[i], {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, ts, 1.0f, {255, 255, 255, 60});
        }
    }
    // totals
    const EquipBonuses b = equip_ ? equip_->Total() : EquipBonuses{};
    float y = panel.y + panel.height - 142.0f * s;
    DrawLineEx({panel.x + 10.0f * s, y - 6.0f * s}, {panel.x + panel.width - 10.0f * s, y - 6.0f * s}, 1.0f, kBorder);
    auto line = [&](const char* label, int v, Color c) {
        DrawTextEx(font, label, {panel.x + 14.0f * s, y}, 14.0f * s, 1.0f, kText);
        DrawRightAligned(font, (v >= 0 ? "+" : "") + std::to_string(v), panel.x + panel.width - 14.0f * s, y, 14.0f * s, c);
        y += 20.0f * s;
    };
    line("Attack (accuracy)", b.attack, b.attack > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Strength (damage)", b.strength, b.strength > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Defence (armour)", b.defence, b.defence > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Ranged (bow)", b.ranged, b.ranged > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Ranged (arrows)", b.rangedStrength, b.rangedStrength > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Magic (accuracy)", b.magic, b.magic > 0 ? Color{120, 220, 130, 255} : kDim);
    static const char* kAnimName[7] = {"Fists", "Stab", "Slash", "Chop", "Crush", "Shoot", "Cast"};
    const WeaponAnim anim = equip_ ? equip_->Anim() : WeaponAnim::Unarmed;
    const float swingSeconds = (anim == WeaponAnim::Shoot || anim == WeaponAnim::Cast || !equip_) ? kDefaultSwingTicks * kTickSeconds
                                                                                                  : WeaponTicks(equip_->Get(EquipSlot::Weapon)) * kTickSeconds;
    DrawTextEx(font, TextFormat("%s, %.1f s a swing (tap a slot to remove)", kAnimName[static_cast<int>(anim)], swingSeconds),
               {panel.x + 14.0f * s, y + 1.0f * s}, 10.0f * s, 1.0f, kDim);
}

void Hud::DrawMake(float s, const Font& font) const {
    if (!makeOpen_) return;
    const Rectangle r = MakeRectOf();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 90});
    Panel(r, 0.04f, {16, 20, 28, 246});
    DrawTextEx(font, make_.kind == MakeMenu::Kind::Craft ? make_.title.c_str() : (smith ? "What will you smith?" : "What will you smelt?"),
               {r.x + 14.0f * s, r.y + 11.0f * s}, 18.0f * s, 1.0f, kAccent);
    const Rectangle cl = MakeCloseRect();
    DrawRectangleRounded(cl, 0.3f, 6, {120, 50, 44, 235});
    DrawLineEx({cl.x + 9.0f * s, cl.y + 9.0f * s}, {cl.x + cl.width - 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    DrawLineEx({cl.x + cl.width - 9.0f * s, cl.y + 9.0f * s}, {cl.x + 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    if (smith) {
        for (int t = 0; t < kTierCount; ++t) {
            const Rectangle tr = MakeTierRect(t);
            const bool on = t == make_.tier;
            const Tier tier = static_cast<Tier>(t);
            const bool haveBars = bag_ && bag_->Count(BarFor(tier)) > 0;
            DrawRectangleRounded(tr, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
            if (on) DrawRectangleRoundedLinesEx(tr, 0.25f, 6, 2.0f * s, kAccent);
            const Rgb mc = TierColor(tier);
            DrawCircleV({tr.x + 13.0f * s, tr.y + tr.height / 2}, 6.0f * s, {mc.r, mc.g, mc.b, 255});
            const float ts = 11.0f * s;
            const Vector2 sz = MeasureTextEx(font, TierName(tier), ts, 1.0f);
            DrawTextEx(font, TierName(tier), {tr.x + 24.0f * s + (tr.width - 24.0f * s - sz.x) / 2, tr.y + (tr.height - sz.y) / 2}, ts, 1.0f,
                       on ? kAccent : (haveBars ? kText : kDim));
        }
    }
    const int perPage = MakeRowsPerPage();
    const int firstRow = makePage_ * perPage, lastRow = std::min(static_cast<int>(make_.options.size()), firstRow + perPage);
    for (int i = firstRow; i < lastRow; ++i) {
        const MakeOption& o = make_.options[static_cast<size_t>(i)];
        const Rectangle row = MakeRowRect(i);
        const bool can = o.haveAll && o.levelOk;
        const bool hot = CheckCollisionPointRec(pointer_, row);
        DrawRectangleRounded(row, 0.2f, 6, can ? (hot ? Color{70, 120, 76, 200} : Color{44, 90, 56, 170}) : Color{255, 255, 255, static_cast<unsigned char>(hot ? 26 : 14)});
        DrawRectangleRoundedLinesEx(row, 0.2f, 6, 1.0f * s, can ? Color{140, 220, 150, 120} : kBorder);
        const Rectangle ic = {row.x + 4.0f * s, row.y + 2.0f * s, row.height - 4.0f * s, row.height - 4.0f * s};
        DrawItemIcon(o.product, ic);
        const float nx = ic.x + ic.width + 8.0f * s;
        DrawTextEx(font, o.name.c_str(), {nx, row.y + 3.0f * s}, 15.0f * s, 1.0f, can ? kText : kDim);
        const Color need = o.levelOk ? kDim : Color{255, 120, 110, 255};
        DrawTextEx(font, o.detail.c_str(), {nx, row.y + row.height - 16.0f * s}, 11.0f * s, 1.0f, o.haveAll ? kDim : Color{232, 170, 110, 255});
        DrawRightAligned(font, TextFormat("Level %d", o.level), row.x + row.width - 8.0f * s, row.y + 4.0f * s, 12.0f * s, need);
        if (o.canMake > 0)
            DrawRightAligned(font, TextFormat("can make %d", o.canMake), row.x + row.width - 8.0f * s, row.y + row.height - 16.0f * s, 11.0f * s, {140, 220, 150, 255});
    }
    if (MakePages() > 1) {   // < Prev   Page 1/2   Next >
        for (int next = 0; next < 2; ++next) {
            const Rectangle b = MakePageRect(next != 0);
            const bool ok = next ? makePage_ < MakePages() - 1 : makePage_ > 0;
            DrawRectangleRounded(b, 0.25f, 6, ok ? (CheckCollisionPointRec(pointer_, b) ? Color{86, 96, 120, 240} : Color{58, 66, 86, 235}) : Color{40, 44, 56, 150});
            const char* label = next ? "Next >" : "< Prev";
            const Vector2 sz = MeasureTextEx(font, label, 13.0f * s, 1.0f);
            DrawTextEx(font, label, {b.x + (b.width - sz.x) / 2, b.y + (b.height - sz.y) / 2}, 13.0f * s, 1.0f, ok ? kText : kDim);
        }
        const char* pg = TextFormat("Page %d/%d", makePage_ + 1, MakePages());
        const Vector2 sz = MeasureTextEx(font, pg, 12.0f * s, 1.0f);
        DrawTextEx(font, pg, {r.x + (r.width - sz.x) / 2, MakePageRect(false).y + (30.0f * s - sz.y) / 2}, 12.0f * s, 1.0f, kDim);
    }
    static const char* kQtyLabel[3] = {"Make 1", "Make 5", "Make all"};
    static const int kQty[3] = {1, 5, 99};
    for (int i = 0; i < 3; ++i) {
        const Rectangle q = MakeQtyRect(i);
        const bool on = makeQty_ == kQty[i];
        DrawRectangleRounded(q, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
        if (on) DrawRectangleRoundedLinesEx(q, 0.25f, 6, 2.0f * s, kAccent);
        const float ts = 13.0f * s;
        const Vector2 sz = MeasureTextEx(font, kQtyLabel[i], ts, 1.0f);
        DrawTextEx(font, kQtyLabel[i], {q.x + (q.width - sz.x) / 2, q.y + (q.height - sz.y) / 2}, ts, 1.0f, on ? kAccent : kText);
    }
}

void Hud::DrawCombat(Rectangle panel, float s, const Font& font) const {
    DrawTextEx(font, "Combat", {panel.x + 12.0f * s, panel.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
    if (!skills_) return;
    DrawRightAligned(font, TextFormat("Combat level %d", skills_->CombatLevel()), panel.x + panel.width - 12.0f * s, panel.y + 10.0f * s,
                     13.0f * s, kText);
    float y = panel.y + 34.0f * s;
    DrawTextEx(font, TextFormat("Hitpoints  %d / %d", skills_->Hp(), skills_->MaxHp()), {panel.x + 12.0f * s, y}, 14.0f * s, 1.0f, kText);
    const Rectangle bar = {panel.x + 12.0f * s, y + 20.0f * s, panel.width - 24.0f * s, 8.0f * s};
    DrawRectangleRec(bar, {120, 28, 28, 255});
    DrawRectangleRec({bar.x, bar.y, bar.width * static_cast<float>(skills_->Hp()) / static_cast<float>(std::max(1, skills_->MaxHp())), bar.height},
                     {64, 190, 84, 255});
    if (attackKind_ == AttackKind::Magic) {
        const SpellInfo& sp = GetSpell(spell_);
        DrawTextEx(font, "Casting spells", {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f) * s}, 12.0f * s, 1.0f, kDim);
        DrawTextEx(font, sp.name, {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f + 22.0f) * s}, 18.0f * s, 1.0f, kAccent);
        DrawTextEx(font, TextFormat("hits up to %d, level %d\nMagic XP for every cast", sp.maxHit, sp.level),
                   {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f + 50.0f) * s}, 13.0f * s, 1.0f, kText);
        DrawTextEx(font, "Choose another spell in the Magic tab.", {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f + 100.0f) * s}, 12.0f * s, 1.0f, kDim);
    } else {
        const bool bow = attackKind_ == AttackKind::Ranged;
        DrawTextEx(font, bow ? "Archery style" : "Attack style", {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f) * s}, 12.0f * s, 1.0f, kDim);
        static const char* kBowName[3] = {"Accurate", "Rapid", "Longrange"};
        static const char* kBowTrains[3] = {"Ranged XP, better aim", "Ranged XP, harder hits", "Ranged + Defence XP, +2 range"};
        for (int i = 0; i < (bow ? 3 : kCombatStyleCount); ++i) {
            const CombatStyle st = StyleFromIndex(i);
            const Rectangle r = StyleRect(i);
            const bool on = (st == combatStyle_) || (bow && i == 0 && combatStyle_ == CombatStyle::Controlled);
            DrawRectangleRounded(r, 0.2f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 18});
            if (on) DrawRectangleRoundedLinesEx(r, 0.2f, 6, 2.0f * s, kAccent);
            DrawTextEx(font, bow ? kBowName[i] : StyleName(st), {r.x + 10.0f * s, r.y + 5.0f * s}, 15.0f * s, 1.0f, on ? kAccent : kText);
            DrawTextEx(font, bow ? kBowTrains[i] : StyleTrains(st), {r.x + 10.0f * s, r.y + 23.0f * s}, 12.0f * s, 1.0f, kDim);
        }
        if (bow && equip_)
            DrawTextEx(font, TextFormat("%d arrows in the quiver", equip_->AmmoCount()), {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f + 22.0f + 3 * 46.0f + 2.0f) * s}, 12.0f * s, 1.0f, kText);
    }
    DrawTextEx(font, "You and your enemy take turns.", {panel.x + 12.0f * s, panel.y + panel.height - 24.0f * s}, 11.0f * s, 1.0f, kDim);
}

// Hit points, top-left: a heart, a bar and the numbers.
void Hud::DrawVitals(float s, const Font& font, float y) const {
    if (!skills_) return;
    const Rectangle r = {12.0f * s, y, 150.0f * s, 28.0f * s};
    Panel(r, 0.4f);
    const float hx = r.x + 17.0f * s, hy = r.y + 14.0f * s, hr = 5.5f * s;   // a little heart
    const Color heart = {224, 52, 60, 255};
    DrawCircleV({hx - hr * 0.8f, hy - hr * 0.5f}, hr, heart);
    DrawCircleV({hx + hr * 0.8f, hy - hr * 0.5f}, hr, heart);
    DrawTriangle({hx - hr * 1.75f, hy - hr * 0.2f}, {hx, hy + hr * 1.9f}, {hx + hr * 1.75f, hy - hr * 0.2f}, heart);
    const Rectangle bar = {r.x + 34.0f * s, r.y + 7.0f * s, r.width - 44.0f * s, 14.0f * s};
    const float frac = static_cast<float>(skills_->Hp()) / static_cast<float>(std::max(1, skills_->MaxHp()));
    DrawRectangleRounded(bar, 0.5f, 6, {110, 26, 26, 255});
    if (frac > 0.0f) DrawRectangleRounded({bar.x, bar.y, std::max(bar.height, bar.width * frac), bar.height}, 0.5f, 6,
                                          poisoned_ ? Color{150, 200, 40, 255} : Color{64, 190, 84, 255});
    if (poisoned_) DrawCircleV({r.x + r.width - 8.0f * s, r.y + 7.0f * s}, 4.0f * s, {170, 60, 200, 255});
    const std::string txt = std::to_string(skills_->Hp()) + " / " + std::to_string(skills_->MaxHp());
    const float ts = 12.0f * s;
    const Vector2 sz = MeasureTextEx(font, txt.c_str(), ts, 1.0f);
    DrawTextEx(font, txt.c_str(), {bar.x + (bar.width - sz.x) / 2 + 1.0f, bar.y + (bar.height - sz.y) / 2 + 1.0f}, ts, 1.0f, {0, 0, 0, 180});
    DrawTextEx(font, txt.c_str(), {bar.x + (bar.width - sz.x) / 2, bar.y + (bar.height - sz.y) / 2}, ts, 1.0f, WHITE);
    float py = r.y + r.height + 4.0f * s;
    if (abyssLevel_ > 0) {   // how deep he is: players can fight him (and he them) within this many combat levels
        const Rectangle p = {r.x, py, 150.0f * s, 26.0f * s};
        Panel(p, 0.4f);
        const Color c = abyssLevel_ < 20 ? Color{240, 210, 90, 255} : (abyssLevel_ < 40 ? Color{240, 150, 60, 255} : Color{240, 70, 60, 255});
        DrawTextEx(font, TextFormat("Abyss level %d", abyssLevel_), {p.x + 8.0f * s, p.y + 5.0f * s}, 14.0f * s, 1.0f, c);
        DrawTextEx(font, TextFormat("+-%d", abyssLevel_), {p.x + p.width - 38.0f * s, p.y + 6.0f * s}, 12.0f * s, 1.0f, kDim);
        py += 30.0f * s;
    }
    if (skullTimer_ > 0.0f) {   // the skull: how long it stays over his head
        const Rectangle p = {r.x, py, 96.0f * s, 26.0f * s};
        Panel(p, 0.4f);
        DrawSkull({p.x + 15.0f * s, p.y + 12.0f * s}, 8.0f * s);
        const int secs = static_cast<int>(skullTimer_ + 0.99f);
        const std::string t = TextFormat("%d:%02d", secs / 60, secs % 60);
        DrawTextEx(font, t.c_str(), {p.x + 32.0f * s, p.y + 5.0f * s}, 15.0f * s, 1.0f, {236, 120, 110, 255});
    }
}

void Hud::AddHitsplat(Vector3 world, int damage) {
    splats_.push_back({world, damage, 0.0f});
    while (splats_.size() > 12) splats_.erase(splats_.begin());
}

void Hud::QueueBar(Vector3 world, float fraction, bool hero) {
    bars_.push_back({world, std::clamp(fraction, 0.0f, 1.0f), hero});
}

void Hud::QueueLabel(Vector3 world, const std::string& text, Color color, float size) { labels_.push_back({world, text, color, size}); }

void Hud::QueueSkull(Vector3 world) { skulls_.push_back(world); }


void Hud::DrawWorld(const Camera3D& camera) {
    const float s = Scale();
    const Font font = GetFontDefault();
    for (const Label& l : labels_) {   // names and speech: bold text with a dark edge, centred over the head
        const Vector2 p = GetWorldToScreen(l.pos, camera);
        const float ts = l.size * s;
        const Vector2 sz = MeasureTextEx(font, l.text.c_str(), ts, 1.0f);
        const float x = p.x - sz.x / 2.0f, y = p.y - sz.y / 2.0f;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                if (dx || dy) DrawTextEx(font, l.text.c_str(), {x + dx, y + dy}, ts, 1.0f, {0, 0, 0, 210});
        DrawTextEx(font, l.text.c_str(), {x, y}, ts, 1.0f, l.color);
    }
    labels_.clear();
    for (const Vector3& sk : skulls_) DrawSkull(GetWorldToScreen(sk, camera), 9.0f * s);
    skulls_.clear();
    for (const Bar& b : bars_) {
        const Vector2 p = GetWorldToScreen(b.pos, camera);
        const float w = 46.0f * s, h = 7.0f * s;
        const Rectangle r = {p.x - w / 2, p.y - h / 2, w, h};
        DrawRectangleRec({r.x - 1.5f, r.y - 1.5f, r.width + 3.0f, r.height + 3.0f}, {0, 0, 0, 200});
        DrawRectangleRec(r, {170, 36, 36, 255});
        DrawRectangleRec({r.x, r.y, r.width * b.fraction, r.height}, b.hero ? Color{70, 200, 90, 255} : Color{70, 200, 90, 255});
    }
    bars_.clear();
    for (size_t i = 0; i < splats_.size(); ++i) {
        const Splat& sp = splats_[i];
        const Vector2 p = GetWorldToScreen(sp.pos, camera);
        const float rise = std::min(sp.age, 0.5f) * 30.0f * s;
        const float fade = std::clamp((1.3f - sp.age) / 0.35f, 0.0f, 1.0f);
        const float pop = sp.age < 0.12f ? 0.6f + 0.4f * (sp.age / 0.12f) : 1.0f;
        const float jitter = (static_cast<int>(i % 3) - 1) * 13.0f * s;
        const Vector2 c = {p.x + jitter, p.y - rise};
        const float r = 15.0f * s * pop;
        const bool miss = sp.damage <= 0;
        const Color fill = miss ? Color{54, 104, 216, 255} : Color{204, 34, 34, 255};
        const Color edge = miss ? Color{18, 40, 112, 255} : Color{108, 8, 8, 255};
        const unsigned char a = static_cast<unsigned char>(255 * fade);
        DrawCircleV(c, r + 2.0f * s, Fade(edge, fade));
        DrawCircleV(c, r, {fill.r, fill.g, fill.b, a});
        DrawCircleV({c.x - r * 0.3f, c.y - r * 0.35f}, r * 0.35f, {255, 255, 255, static_cast<unsigned char>(46 * fade)});
        const std::string txt = std::to_string(std::max(0, sp.damage));
        const float ts = 17.0f * s * pop;
        const Vector2 sz = MeasureTextEx(font, txt.c_str(), ts, 1.0f);
        DrawTextEx(font, txt.c_str(), {c.x - sz.x / 2 + 1.0f, c.y - sz.y / 2 + 1.0f}, ts, 1.0f, {0, 0, 0, static_cast<unsigned char>(170 * fade)});
        DrawTextEx(font, txt.c_str(), {c.x - sz.x / 2, c.y - sz.y / 2}, ts, 1.0f, {255, 255, 255, a});
    }
}

// ---- context menu

float Hud::RunsWidth(const std::vector<TextRun>& runs, float size, const Font& font) const {
    float w = 0;
    for (const TextRun& r : runs) w += MeasureTextEx(font, r.text.c_str(), size, 1.0f).x + 1.0f;
    return w;
}

void Hud::DrawRuns(const std::vector<TextRun>& runs, float x, float y, float size, const Font& font, float alpha) const {
    for (const TextRun& r : runs) {
        const unsigned char a = static_cast<unsigned char>(255 * alpha);
        DrawTextEx(font, r.text.c_str(), {x + 1.0f, y + 1.0f}, size, 1.0f, {0, 0, 0, static_cast<unsigned char>(150 * alpha)});
        DrawTextEx(font, r.text.c_str(), {x, y}, size, 1.0f, {r.color.r, r.color.g, r.color.b, a});
        x += MeasureTextEx(font, r.text.c_str(), size, 1.0f).x + 1.0f;
    }
}

void Hud::OpenMenu(Vector2 at, std::vector<MenuEntry> entries) {
    menu_ = std::move(entries);
    menuAt_ = at;
}

Rectangle Hud::MenuRect() const {
    const float s = Scale();
    const Font font = GetFontDefault();
    const float ts = 15.0f * s;
    float w = std::max(150.0f * s, MeasureTextEx(font, "Choose Option", 13.0f * s, 1.0f).x + 24.0f * s);
    for (const MenuEntry& e : menu_) w = std::max(w, RunsWidth(e.label, ts, font) + 28.0f * s);
    const float h = 24.0f * s + static_cast<float>(menu_.size()) * 28.0f * s + 8.0f * s;
    const float x = std::clamp(menuAt_.x - 10.0f * s, 4.0f, std::max(4.0f, GetScreenWidth() - w - 4.0f));
    const float y = std::clamp(menuAt_.y - 14.0f * s, 4.0f, std::max(4.0f, GetScreenHeight() - h - 4.0f));
    return {x, y, w, h};
}

Rectangle Hud::MenuRowRect(int i) const {
    const float s = Scale();
    const Rectangle r = MenuRect();
    return {r.x + 4.0f * s, r.y + 24.0f * s + i * 28.0f * s, r.width - 8.0f * s, 28.0f * s};
}

bool Hud::MenuHits(Vector2 p) const {
    return MenuOpen() && CheckCollisionPointRec(p, MenuRect());
}

int Hud::MenuEntryAt(Vector2 p) const {
    for (size_t i = 0; i < menu_.size(); ++i)
        if (CheckCollisionPointRec(p, MenuRowRect(static_cast<int>(i)))) return static_cast<int>(i);
    return -1;
}

void Hud::DrawMenu(float s, const Font& font) const {
    if (!MenuOpen()) return;
    const Rectangle r = MenuRect();
    Panel(r, 0.05f, {16, 20, 28, 244});
    DrawTextEx(font, "Choose Option", {r.x + 12.0f * s, r.y + 5.0f * s}, 13.0f * s, 1.0f, kAccent);
    DrawLineEx({r.x + 6.0f * s, r.y + 22.0f * s}, {r.x + r.width - 6.0f * s, r.y + 22.0f * s}, 1.0f, kBorder);
    for (size_t i = 0; i < menu_.size(); ++i) {
        const Rectangle row = MenuRowRect(static_cast<int>(i));
        const bool hot = CheckCollisionPointRec(pointer_, row);
        if (hot) DrawRectangleRounded(row, 0.25f, 6, {255, 255, 255, 34});
        const float ts = 15.0f * s;
        DrawRuns(menu_[i].label, row.x + 10.0f * s, row.y + (row.height - ts) / 2 - 1.0f, ts, font);
    }
}

void Hud::Draw(bool debug, const std::string& debugText) const {
    const float s = Scale();
    const Font font = GetFontDefault();

    // Chat box
    const Rectangle chat = ChatRect();
    Panel(chat, 0.08f);
    const float fs = 15.0f * s;
    float y = chat.y + chat.height - fs - 10.0f * s;
    for (auto it = lines_.rbegin(); it != lines_.rend() && y > chat.y; ++it) {
        DrawTextEx(font, it->text.c_str(), {chat.x + 12.0f * s, y}, fs, 1.0f, it->color);
        y -= fs + 3.0f * s;
    }

    // Tab bar (the open tab is highlighted)
    for (int i = 0; i < kTabCount; ++i) {
        const Rectangle r = TabRect(i);
        Panel(r, 0.22f, i == openTab_ ? Color{70, 56, 24, 235} : kPanel);
        const float ts = 11.0f * s;
        const Vector2 sz = MeasureTextEx(font, kTabLabels[i], ts, 1.0f);
        DrawTextEx(font, kTabLabels[i], {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, ts, 1.0f,
                   i == openTab_ ? kAccent : kText);
    }

    // Open tab panel
    if (openTab_ >= 0) {
        const Rectangle p = PanelRect();
        Panel(p, 0.05f, kPanelSolid);
        if (openTab_ == kTabBag) DrawBag(p, s, font);
        else if (openTab_ == kTabSkills) DrawSkills(p, s, font);
        else if (openTab_ == kTabCombat) DrawCombat(p, s, font);
        else if (openTab_ == kTabGear) DrawGear(p, s, font);
        else if (openTab_ == kTabMagic) DrawMagic(p, s, font);
        else if (openTab_ == kTabQuests) DrawQuests(p, s, font);
        else if (openTab_ == kTabMap) DrawMap(p, s, font);
        else if (openTab_ == kTabMenu) DrawSettings(p, s, font);
        else {
            DrawTextEx(font, kTabLabels[openTab_], {p.x + 12.0f * s, p.y + 8.0f * s}, 16.0f * s, 1.0f, kAccent);
            DrawTextEx(font, "Coming soon.", {p.x + 12.0f * s, p.y + 34.0f * s}, 14.0f * s, 1.0f, kDim);
        }
    }

    // Experience drops: rise and fade under the area banner
    for (const Drop& d : drops_) {
        const float a = std::clamp(1.0f - (d.age - 1.4f) / 1.0f, 0.0f, 1.0f);
        const float ts = 22.0f * s;
        const Vector2 sz = MeasureTextEx(font, d.text.c_str(), ts, 1.0f);
        const float yy = 90.0f * s - d.age * 22.0f * s + d.lane * 26.0f * s;
        DrawTextEx(font, d.text.c_str(), {(GetScreenWidth() - sz.x) / 2 + 1.0f, yy + 112.0f * s + 1.0f}, ts, 1.0f, {0, 0, 0, static_cast<unsigned char>(160 * a)});
        DrawTextEx(font, d.text.c_str(), {(GetScreenWidth() - sz.x) / 2, yy + 112.0f * s}, ts, 1.0f, {255, 235, 140, static_cast<unsigned char>(255 * a)});
    }

    // Area banner: fades in, holds, fades out
    if (areaTime_ < 3.6f && !area_.empty()) {
        float a = 1.0f;
        if (areaTime_ < 0.4f) a = areaTime_ / 0.4f;
        if (areaTime_ > 2.8f) a = std::max(0.0f, (3.6f - areaTime_) / 0.8f);
        const float ts = 30.0f * s;
        const Vector2 sz = MeasureTextEx(font, area_.c_str(), ts, 2.0f);
        const Rectangle r = {(GetScreenWidth() - sz.x) / 2 - 24.0f * s, 14.0f * s, sz.x + 48.0f * s, sz.y + 18.0f * s};
        Panel(r, 0.35f);
        DrawTextEx(font, area_.c_str(), {r.x + 24.0f * s, r.y + 9.0f * s}, ts, 2.0f,
                   {kAccent.r, kAccent.g, kAccent.b, static_cast<unsigned char>(255 * a)});
    }

    // Hit points (top-left, below the debug line when that is shown) and what a tap would do (desktop hover)
    const float vy = debug ? 34.0f * s : 12.0f * s;
    DrawVitals(s, font, vy);
    DrawCoins(s, font, 12.0f * s + 150.0f * s + 8.0f * s, vy);
    const bool pill = equip_ && (IsBow(equip_->Get(EquipSlot::Weapon)) || IsStaff(equip_->Get(EquipSlot::Weapon)));
    if (pill) DrawAmmoPill(s, font, 12.0f * s, vy + 34.0f * s);
    if (!hover_.empty() && !MenuOpen()) {
        std::vector<TextRun> runs = hover_;
        if (hoverMore_ > 0) runs.push_back({" / " + std::to_string(hoverMore_) + " more option" + (hoverMore_ == 1 ? "" : "s"), {255, 255, 255}});
        const float ts = 14.0f * s;
        const float w = RunsWidth(runs, ts, font) + 20.0f * s;
        const Rectangle r = {12.0f * s, vy + (pill ? 68.0f : 34.0f) * s, w, 26.0f * s};
        Panel(r, 0.3f);
        DrawRuns(runs, r.x + 10.0f * s, r.y + (r.height - ts) / 2 - 1.0f, ts, font);
    }

    DrawMake(s, font);
    DrawShop(s, font);
    DrawBank(s, font);
    DrawDialogue(s, font);
    DrawMenu(s, font);

    if (debug) {
        DrawTextEx(font, debugText.c_str(), {12.0f * s, 12.0f * s}, 14.0f * s, 1.0f, {255, 255, 255, 230});
    }
}

}  // namespace journey
