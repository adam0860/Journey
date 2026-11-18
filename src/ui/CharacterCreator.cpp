#include "ui/CharacterCreator.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace journey {

namespace {
const Color kPanel = {16, 20, 28, 225};
const Color kBorder = {255, 255, 255, 46};
const Color kText = {236, 240, 246, 255};
const Color kDim = {150, 162, 180, 255};
const Color kAccent = {255, 205, 90, 255};
const Color kBtn = {40, 48, 64, 255};
const Color kBtnHot = {62, 74, 98, 255};

struct Ui {
    float s = 1.0f;
    Vector2 mouse{};
    bool click = false;
    Font font{};
};

bool Hot(const Ui& ui, Rectangle r) { return CheckCollisionPointRec(ui.mouse, r); }

void Text(const Ui& ui, const char* t, float x, float y, float size, Color c) {
    DrawTextEx(ui.font, t, {x, y}, size, 1.0f, c);
}
float TextW(const Ui& ui, const char* t, float size) { return MeasureTextEx(ui.font, t, size, 1.0f).x; }
void TextC(const Ui& ui, const char* t, Rectangle r, float size, Color c) {
    const Vector2 m = MeasureTextEx(ui.font, t, size, 1.0f);
    DrawTextEx(ui.font, t, {r.x + (r.width - m.x) / 2, r.y + (r.height - m.y) / 2}, size, 1.0f, c);
}

void Panel(Rectangle r, float round) {
    DrawRectangleRounded({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, round, 8, kBorder);
    DrawRectangleRounded(r, round, 8, kPanel);
}

bool Button(const Ui& ui, Rectangle r, const char* label, bool selected, bool enabled = true, Color accent = kAccent) {
    const bool hot = enabled && Hot(ui, r);
    Color bg = selected ? Color{accent.r, accent.g, accent.b, 255} : (hot ? kBtnHot : kBtn);
    if (!enabled) bg = {34, 38, 46, 255};
    DrawRectangleRounded(r, 0.28f, 8, bg);
    if (selected) DrawRectangleRoundedLinesEx(r, 0.28f, 8, 1.5f, {255, 255, 255, 120});
    const Color tc = !enabled ? Color{110, 118, 130, 255} : (selected ? Color{24, 20, 10, 255} : kText);
    TextC(ui, label, r, 15.0f * ui.s, tc);
    return enabled && hot && ui.click;
}

// small triangle arrow button; dir = -1 left, +1 right
bool Arrow(const Ui& ui, Rectangle r, int dir) {
    const bool hot = Hot(ui, r);
    DrawRectangleRounded(r, 0.3f, 8, hot ? kBtnHot : kBtn);
    const float cx = r.x + r.width / 2, cy = r.y + r.height / 2, a = r.height * 0.22f;
    if (dir < 0) DrawTriangle({cx + a * 0.7f, cy - a}, {cx - a * 0.9f, cy}, {cx + a * 0.7f, cy + a}, kText);
    else DrawTriangle({cx - a * 0.7f, cy - a}, {cx - a * 0.7f, cy + a}, {cx + a * 0.9f, cy}, kText);
    return hot && ui.click;
}

// one row of colour swatches; returns true if `index` changed
bool Swatches(const Ui& ui, float x, float y, float size, float gap, const std::vector<Color>& colors, int* index) {
    bool changed = false;
    for (int i = 0; i < (int)colors.size(); ++i) {
        const Rectangle r = {x + i * (size + gap), y, size, size};
        const bool hot = Hot(ui, r);
        DrawRectangleRounded(r, 0.3f, 6, colors[i]);
        if (i == *index) {
            DrawRectangleRoundedLinesEx({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 0.3f, 6, 2.5f, kAccent);
        } else if (hot) {
            DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2.0f, {255, 255, 255, 190});
        } else {
            DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.0f, {0, 0, 0, 90});
        }
        if (hot && ui.click) {
            *index = i;
            changed = true;
        }
    }
    return changed;
}

int Wrap(int v, int n) { return n <= 0 ? 0 : ((v % n) + n) % n; }

// "label  [<] value [>]"
bool Selector(const Ui& ui, float x, float y, float w, const char* label, const std::string& value, int* index, int count) {
    const float h = 30.0f * ui.s, a = 32.0f * ui.s, labelW = 62.0f * ui.s;
    Text(ui, label, x, y + (h - 15.0f * ui.s) / 2, 15.0f * ui.s, kDim);
    const float bx = x + labelW;
    bool changed = false;
    if (Arrow(ui, {bx, y, a, h}, -1)) { *index = Wrap(*index - 1, count); changed = true; }
    const Rectangle mid = {bx + a + 4 * ui.s, y, w - labelW - 2 * a - 8 * ui.s, h};
    DrawRectangleRounded(mid, 0.28f, 8, {26, 31, 42, 255});
    TextC(ui, value.c_str(), mid, 15.0f * ui.s, kText);
    if (Arrow(ui, {mid.x + mid.width + 4 * ui.s, y, a, h}, +1)) { *index = Wrap(*index + 1, count); changed = true; }
    return changed;
}
}  // namespace

void CharacterCreator::Begin(const Appearance& initial) {
    look_ = initial;
    yaw_ = 20.0f;
    autoSpin_ = true;
    walking_ = false;
}

bool CharacterCreator::Frame(float dt, LitShader& lit, CharacterRenderer& chars) {
    time_ += dt;
    if (walking_) phase_ += dt * 8.5f;

    Ui ui;
    ui.s = std::clamp(GetScreenHeight() / 720.0f, 0.75f, 3.0f);
    ui.mouse = GetMousePosition();
    ui.click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (injected_) {
        ui.mouse = injectedPos_;
        ui.click = true;
        injected_ = false;
    }
    const Ui uiKeys = ui;               // the on-screen keyboard keeps working while the rest of the screen is locked
    if (oskOpen_) ui.click = false;
    ui.font = GetFontDefault();
    const float s = ui.s;
    const float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    const CharacterCatalog& cat = chars.Catalog();
    bool start = false;

    // ---------- layout ----------
    const float pad = 14.0f * s;
    const Rectangle panel = {16.0f * s, 16.0f * s, std::min(420.0f * s, W * 0.5f), H - 32.0f * s};
    const float previewCx = panel.x + panel.width + (W - panel.x - panel.width) / 2.0f;

    // ---------- preview input (drag to spin) ----------
    const Rectangle previewArea = {panel.x + panel.width, 0, W - panel.x - panel.width, H};
    if (ui.click && Hot(ui, previewArea)) dragging_ = true;
    if (oskOpen_) dragging_ = false;
    if (dragging_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            yaw_ += GetMouseDelta().x * 0.6f;
            autoSpin_ = false;
        } else {
            dragging_ = false;
        }
    }
    if (autoSpin_) yaw_ += 18.0f * dt;

    // ---------- background + 3D preview ----------
    DrawRectangleGradientV(0, 0, (int)W, (int)H, {40, 52, 76, 255}, {10, 12, 18, 255});

    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 30.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    const float dist = 4.5f;
    const float halfV = dist * std::tan(cam.fovy * 0.5f * DEG2RAD);
    const float shift = (previewCx - W / 2.0f) / (H / 2.0f) * halfV;   // push the hero toward the right half
    const Vector3 focus = {0, 0.86f, 0};
    const Vector3 fwd = Vector3Normalize({0, -0.12f, -1});
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.up));
    cam.target = Vector3Subtract(focus, Vector3Scale(right, shift));
    cam.position = Vector3Subtract(cam.target, Vector3Scale(fwd, dist));

    lit.SetFrame(cam.position, time_, true);
    BeginMode3D(cam);
    DrawCylinder({0, -0.045f, 0}, 0.8f, 0.8f, 0.04f, 64, {255, 205, 90, 255});
    DrawCylinder({0, -0.03f, 0}, 0.76f, 0.76f, 0.05f, 64, {30, 36, 48, 255});
    lit.BindShadow(false);
    CharacterPose pose;
    pose.facing = yaw_ * DEG2RAD;
    pose.moving = walking_;
    pose.phase = phase_;
    pose.time = time_;
    chars.Draw(look_, pose);
    EndMode3D();

    // ---------- panel ----------
    Panel(panel, 0.03f);
    const float x = panel.x + pad, iw = panel.width - 2 * pad;
    float y = panel.y + pad;
    Text(ui, "Create your adventurer", x, y, 24.0f * s, kAccent);
    y += 38.0f * s;

    // name
    Text(ui, "Name", x, y, 14.0f * s, kDim);
    y += 19.0f * s;
    const Rectangle field = {x, y, iw, 38.0f * s};
    DrawRectangleRounded(field, 0.2f, 8, {26, 31, 42, 255});
    DrawRectangleRoundedLinesEx(field, 0.2f, 8, 1.5f, ValidName(look_.name) ? Color{120, 200, 140, 200} : Color{255, 205, 90, 160});
    if (useOsk_ && ui.click && Hot(ui, field)) oskOpen_ = true;
    for (int c = GetCharPressed(); c != 0; c = GetCharPressed()) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
            if (look_.name.size() < 12) look_.name.push_back((char)c);
    }
    if (IsKeyPressed(KEY_BACKSPACE)) { if (!look_.name.empty()) look_.name.pop_back(); backHeld_ = 0; }
    else if (IsKeyDown(KEY_BACKSPACE)) {
        backHeld_ += dt;
        if (backHeld_ > 0.45f && (int)(backHeld_ * 20) != (int)((backHeld_ - dt) * 20) && !look_.name.empty()) look_.name.pop_back();
    }
    const std::string shown = look_.name + ((int)(time_ * 2.0f) % 2 == 0 ? "|" : " ");
    Text(ui, shown.c_str(), field.x + 12 * s, field.y + (field.height - 20 * s) / 2, 20.0f * s, kText);
    Text(ui, TextFormat("%d/12", (int)look_.name.size()), field.x + field.width - 44 * s, field.y + (field.height - 13 * s) / 2, 13.0f * s, kDim);
    y += field.height + 12.0f * s;

    // body type
    const float half = (iw - 8 * s) / 2;
    if (Button(ui, {x, y, half, 34.0f * s}, "Male", !look_.female)) look_.female = false;
    if (Button(ui, {x + half + 8 * s, y, half, 34.0f * s}, "Female", look_.female)) look_.female = true;
    y += 34.0f * s + 14.0f * s;

    // skin
    Text(ui, "Skin tone", x, y, 14.0f * s, kDim);
    y += 20.0f * s;
    {
        const float sz = 30.0f * s;
        const float gap = (iw - 8 * sz) / 7.0f;
        Swatches(ui, x, y, sz, gap, SkinTones(), &look_.skin);
        y += sz + 16.0f * s;
    }

    auto section = [&](const char* label, const std::vector<StyleInfo>& styles, int* style, const std::vector<Color>& colors, int* colorIdx) {
        const std::string& nm = styles.empty() ? std::string("-") : styles[Wrap(*style, (int)styles.size())].name;
        Selector(ui, x, y, iw, label, nm, style, (int)styles.size());
        y += 30.0f * s + 7.0f * s;
        const float n = (float)colors.size();
        const float gap = 3.0f * s;
        const float sz = std::min(26.0f * s, (iw - (n - 1) * gap) / n);
        Swatches(ui, x, y, sz, gap, colors, colorIdx);
        y += sz + 14.0f * s;
    };
    {   // hair: only the styles this body type may choose
        const std::vector<int> ok = cat.HairFor(look_.female);
        std::vector<StyleInfo> shownStyles;
        int pos = 0;
        for (int i = 0; i < (int)ok.size(); ++i) {
            shownStyles.push_back(cat.hair[ok[i]]);
            if (ok[i] == look_.hair) pos = i;
        }
        section("Hair", shownStyles, &pos, HairColors(), &look_.hairColor);
        if (!ok.empty()) look_.hair = ok[Wrap(pos, (int)ok.size())];
    }
    section("Shirt", cat.shirt, &look_.shirt, ClothColors(), &look_.shirtColor);
    section("Pants", cat.pants, &look_.pants, ClothColors(), &look_.pantsColor);
    section("Shoes", cat.shoes, &look_.shoes, ShoeColors(), &look_.shoesColor);

    // buttons pinned to the bottom of the panel
    const float bh = 44.0f * s;
    const float by = panel.y + panel.height - pad - bh;
    const float rw = (iw - 8 * s) / 2;
    if (Button(ui, {x, by - bh - 8 * s, rw, 34.0f * s}, "Randomize", false)) {
        Randomize(look_, 0, (int)cat.shirt.size(), (int)cat.pants.size(), (int)cat.shoes.size());
        const std::vector<int> ok = cat.HairFor(look_.female);
        if (!ok.empty()) look_.hair = ok[GetRandomValue(0, (int)ok.size() - 1)];
    }
    if (Button(ui, {x + rw + 8 * s, by - bh - 8 * s, rw, 34.0f * s}, walking_ ? "Stop walking" : "Preview walk", walking_)) walking_ = !walking_;
    const bool ok = ValidName(look_.name);
    if (Button(ui, {x, by, iw, bh}, "Start Journey", ok, ok, {120, 210, 140, 255})) start = true;
    if (!ok) {
        const char* msg = "Name needs 2-12 letters or numbers";
        Text(ui, msg, x + (iw - TextW(ui, msg, 13.0f * s)) / 2, by - bh - 8 * s - 20.0f * s, 13.0f * s, kAccent);
    }
    if (ok && !oskOpen_ && IsKeyPressed(KEY_ENTER)) start = true;

    if (oskOpen_) {
        const Ui& k = uiKeys;
        const char* rows[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
        const float gap = 8.0f * s, kh = 52.0f * s;
        const float kw = std::min(86.0f * s, (W - 32.0f * s - 9 * gap) / 10.0f);
        const float boardH = 5 * (kh + gap) + gap + 26.0f * s;
        const Rectangle board = {W / 2 - (10 * kw + 9 * gap) / 2 - 14 * s, H - boardH - 12.0f * s, 10 * kw + 9 * gap + 28 * s, boardH};
        DrawRectangle(0, 0, (int)W, (int)H, {0, 0, 0, 90});
        Panel(board, 0.04f);
        Text(k, "Type your name", board.x + 14 * s, board.y + 6 * s, 14.0f * s, kDim);
        const std::string preview = look_.name + "|";
        TextC(k, preview.c_str(), {board.x, board.y + 2 * s, board.width, 26.0f * s}, 20.0f * s, kText);
        float ky = board.y + 30.0f * s;
        for (int r = 0; r < 4; ++r) {
            const int n = (int)std::string(rows[r]).size();
            const float rowW = n * kw + (n - 1) * gap;
            float kx = board.x + (board.width - rowW) / 2;
            for (int i = 0; i < n; ++i) {
                const char ch = rows[r][i];
                const char label[2] = {ch, 0};
                if (Button(k, {kx, ky, kw, kh}, label, false) && look_.name.size() < 12) {
                    const bool upper = !(ch >= 'A' && ch <= 'Z') || look_.name.empty() || look_.name.back() == ' ';
                    look_.name.push_back((ch >= 'A' && ch <= 'Z' && !upper) ? (char)(ch - 'A' + 'a') : ch);
                }
                kx += kw + gap;
            }
            ky += kh + gap;
        }
        const float bx = board.x + 14 * s, bw = board.width - 28 * s;
        const float wBack = bw * 0.26f, wDone = bw * 0.26f, wSpace = bw - wBack - wDone - 2 * gap;
        if (Button(k, {bx, ky, wBack, kh}, "Backspace", false) && !look_.name.empty()) look_.name.pop_back();
        if (Button(k, {bx + wBack + gap, ky, wSpace, kh}, "Space", false) && look_.name.size() < 12 && !look_.name.empty() && look_.name.back() != ' ')
            look_.name.push_back(' ');
        if (Button(k, {bx + wBack + wSpace + 2 * gap, ky, wDone, kh}, "Done", true, true, {120, 210, 140, 255})) oskOpen_ = false;
    }

    const char* hint = "Drag to turn your character";
    Text(ui, hint, previewCx - TextW(ui, hint, 14.0f * s) / 2, H - 34.0f * s, 14.0f * s, kDim);
    return start;
}

}  // namespace journey
