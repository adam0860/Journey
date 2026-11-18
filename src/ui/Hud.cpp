#include "ui/Hud.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {
constexpr int kTabCount = 7;
const char* kTabLabels[kTabCount] = {"Bag", "Skills", "Gear", "Quests", "Map", "Combat", "Menu"};
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

// Little hand-drawn icons for items (no image files needed).
void DrawItemIcon(Item item, Rectangle r) {
    const float cx = r.x + r.width / 2, cy = r.y + r.height / 2, u = r.width;
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
        h = (34.0f + 3 * 68.0f + 14.0f + 96.0f + 10.0f) * s;
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
    if (openTab_ != kTabCombat) return -1;
    for (int i = 0; i < kCombatStyleCount; ++i)
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
Rectangle Hud::MakeRectOf() const {
    const float s = Scale();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    const float w = std::min(400.0f * s, GetScreenWidth() * 0.94f);
    const float rows = static_cast<float>(std::max<size_t>(1, make_.options.size()));
    const float rowH = (smith ? 40.0f : 52.0f) * s;
    const float h = 42.0f * s + (smith ? 40.0f * s : 0.0f) + rows * rowH + 6.0f * s + 52.0f * s;
    return {(GetScreenWidth() - w) / 2, std::max(8.0f * s, (GetScreenHeight() - h) / 2 - 20.0f * s), w, std::min(h, GetScreenHeight() - 16.0f * s)};
}

Rectangle Hud::MakeRowRect(int i) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    const float rowH = (smith ? 40.0f : 52.0f) * s;
    return {r.x + 10.0f * s, r.y + 42.0f * s + (smith ? 40.0f * s : 0.0f) + i * rowH, r.width - 20.0f * s, rowH - 4.0f * s};
}

Rectangle Hud::MakeTierRect(int t) const {
    const float s = Scale();
    const Rectangle r = MakeRectOf();
    const float w = (r.width - 20.0f * s - 3 * 6.0f * s) / 4.0f;
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
    for (size_t i = 0; i < make_.options.size(); ++i)
        if (CheckCollisionPointRec(p, MakeRowRect(static_cast<int>(i)))) return {MakeHitKind::Option, static_cast<int>(i)};
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

bool Hud::Hits(Vector2 p) const {
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
    if (tab < 0 || tab >= kTabCount || tab == kTabMenu) return;
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
    static const char* kHint[kEquipSlotCount] = {"Head", "Body", "Legs", "Weapon", "Shield"};
    for (int i = 0; i < kEquipSlotCount; ++i) {
        const Rectangle r = GearSlotRect(i);
        DrawRectangleRounded(r, 0.14f, 6, kCell);
        DrawRectangleRoundedLinesEx(r, 0.14f, 6, 1.0f * s, kBorder);
        const Item item = equip_ ? equip_->Get(static_cast<EquipSlot>(i)) : Item::None;
        if (item != Item::None) {
            DrawItemIcon(item, r);
        } else {
            const float ts = 11.0f * s;
            const Vector2 sz = MeasureTextEx(font, kHint[i], ts, 1.0f);
            DrawTextEx(font, kHint[i], {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, ts, 1.0f, {255, 255, 255, 60});
        }
    }
    // totals
    const EquipBonuses b = equip_ ? equip_->Total() : EquipBonuses{};
    float y = panel.y + panel.height - 100.0f * s;
    DrawLineEx({panel.x + 10.0f * s, y - 6.0f * s}, {panel.x + panel.width - 10.0f * s, y - 6.0f * s}, 1.0f, kBorder);
    auto line = [&](const char* label, int v, Color c) {
        DrawTextEx(font, label, {panel.x + 14.0f * s, y}, 14.0f * s, 1.0f, kText);
        DrawRightAligned(font, (v >= 0 ? "+" : "") + std::to_string(v), panel.x + panel.width - 14.0f * s, y, 14.0f * s, c);
        y += 20.0f * s;
    };
    line("Attack (accuracy)", b.attack, b.attack > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Strength (damage)", b.strength, b.strength > 0 ? Color{120, 220, 130, 255} : kDim);
    line("Defence (armour)", b.defence, b.defence > 0 ? Color{120, 220, 130, 255} : kDim);
    static const char* kAnimName[5] = {"Fists", "Stab", "Slash", "Chop", "Crush"};
    DrawTextEx(font, TextFormat("Fighting style: %s   (tap a slot to remove)", kAnimName[static_cast<int>(equip_ ? equip_->Anim() : WeaponAnim::Unarmed)]),
               {panel.x + 14.0f * s, y + 1.0f * s}, 10.0f * s, 1.0f, kDim);
}

void Hud::DrawMake(float s, const Font& font) const {
    if (!makeOpen_) return;
    const Rectangle r = MakeRectOf();
    const bool smith = make_.kind == MakeMenu::Kind::Smith;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 90});
    Panel(r, 0.04f, {16, 20, 28, 246});
    DrawTextEx(font, smith ? "What will you smith?" : "What will you smelt?", {r.x + 14.0f * s, r.y + 11.0f * s}, 18.0f * s, 1.0f, kAccent);
    const Rectangle cl = MakeCloseRect();
    DrawRectangleRounded(cl, 0.3f, 6, {120, 50, 44, 235});
    DrawLineEx({cl.x + 9.0f * s, cl.y + 9.0f * s}, {cl.x + cl.width - 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    DrawLineEx({cl.x + cl.width - 9.0f * s, cl.y + 9.0f * s}, {cl.x + 9.0f * s, cl.y + cl.height - 9.0f * s}, 2.0f * s, kText);
    if (smith) {
        for (int t = 0; t < kTierCount; ++t) {
            const Rectangle tr = MakeTierRect(t);
            const bool on = t == make_.tier;
            const Tier tier = static_cast<Tier>(t);
            const bool haveBars = bag_ && bag_->Count(static_cast<Item>(static_cast<int>(Item::BronzeBar) + t)) > 0;
            DrawRectangleRounded(tr, 0.25f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 20});
            if (on) DrawRectangleRoundedLinesEx(tr, 0.25f, 6, 2.0f * s, kAccent);
            const Rgb mc = TierColor(tier);
            DrawCircleV({tr.x + 13.0f * s, tr.y + tr.height / 2}, 6.0f * s, {mc.r, mc.g, mc.b, 255});
            const float ts = 12.0f * s;
            const Vector2 sz = MeasureTextEx(font, TierName(tier), ts, 1.0f);
            DrawTextEx(font, TierName(tier), {tr.x + 24.0f * s + (tr.width - 24.0f * s - sz.x) / 2, tr.y + (tr.height - sz.y) / 2}, ts, 1.0f,
                       on ? kAccent : (haveBars ? kText : kDim));
        }
    }
    for (size_t i = 0; i < make_.options.size(); ++i) {
        const MakeOption& o = make_.options[i];
        const Rectangle row = MakeRowRect(static_cast<int>(i));
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
    DrawTextEx(font, "Attack style", {panel.x + 12.0f * s, panel.y + (34.0f + 52.0f) * s}, 12.0f * s, 1.0f, kDim);
    for (int i = 0; i < kCombatStyleCount; ++i) {
        const CombatStyle st = StyleFromIndex(i);
        const Rectangle r = StyleRect(i);
        const bool on = (st == combatStyle_);
        DrawRectangleRounded(r, 0.2f, 6, on ? Color{70, 56, 24, 235} : Color{255, 255, 255, 18});
        if (on) DrawRectangleRoundedLinesEx(r, 0.2f, 6, 2.0f * s, kAccent);
        DrawTextEx(font, StyleName(st), {r.x + 10.0f * s, r.y + 5.0f * s}, 15.0f * s, 1.0f, on ? kAccent : kText);
        DrawTextEx(font, StyleTrains(st), {r.x + 10.0f * s, r.y + 23.0f * s}, 12.0f * s, 1.0f, kDim);
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
    if (frac > 0.0f) DrawRectangleRounded({bar.x, bar.y, std::max(bar.height, bar.width * frac), bar.height}, 0.5f, 6, {64, 190, 84, 255});
    const std::string txt = std::to_string(skills_->Hp()) + " / " + std::to_string(skills_->MaxHp());
    const float ts = 12.0f * s;
    const Vector2 sz = MeasureTextEx(font, txt.c_str(), ts, 1.0f);
    DrawTextEx(font, txt.c_str(), {bar.x + (bar.width - sz.x) / 2 + 1.0f, bar.y + (bar.height - sz.y) / 2 + 1.0f}, ts, 1.0f, {0, 0, 0, 180});
    DrawTextEx(font, txt.c_str(), {bar.x + (bar.width - sz.x) / 2, bar.y + (bar.height - sz.y) / 2}, ts, 1.0f, WHITE);
}

void Hud::AddHitsplat(Vector3 world, int damage) {
    splats_.push_back({world, damage, 0.0f});
    while (splats_.size() > 12) splats_.erase(splats_.begin());
}

void Hud::QueueBar(Vector3 world, float fraction, bool hero) {
    bars_.push_back({world, std::clamp(fraction, 0.0f, 1.0f), hero});
}

void Hud::DrawWorld(const Camera3D& camera) {
    const float s = Scale();
    const Font font = GetFontDefault();
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
    if (!hover_.empty() && !MenuOpen()) {
        std::vector<TextRun> runs = hover_;
        if (hoverMore_ > 0) runs.push_back({" / " + std::to_string(hoverMore_) + " more option" + (hoverMore_ == 1 ? "" : "s"), {255, 255, 255}});
        const float ts = 14.0f * s;
        const float w = RunsWidth(runs, ts, font) + 20.0f * s;
        const Rectangle r = {12.0f * s, vy + 34.0f * s, w, 26.0f * s};
        Panel(r, 0.3f);
        DrawRuns(runs, r.x + 10.0f * s, r.y + (r.height - ts) / 2 - 1.0f, ts, font);
    }

    DrawMake(s, font);
    DrawShop(s, font);
    DrawMenu(s, font);

    if (debug) {
        DrawTextEx(font, debugText.c_str(), {12.0f * s, 12.0f * s}, 14.0f * s, 1.0f, {255, 255, 255, 230});
    }
}

}  // namespace journey
