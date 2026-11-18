#include "ui/Hud.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {
constexpr int kTabCount = 6;
const char* kTabLabels[kTabCount] = {"Bag", "Skills", "Quests", "Map", "Gear", "Menu"};
const Color kPanel = {16, 20, 28, 205};
const Color kBorder = {255, 255, 255, 40};
const Color kText = {236, 240, 246, 255};
const Color kAccent = {255, 205, 90, 255};

void Panel(Rectangle r, float round) {
    Rectangle border = {r.x - 1, r.y - 1, r.width + 2, r.height + 2};
    DrawRectangleRounded(border, round, 8, kBorder);
    DrawRectangleRounded(r, round, 8, kPanel);
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

bool Hud::Hits(Vector2 p) const {
    if (CheckCollisionPointRec(p, ChatRect())) return true;
    for (int i = 0; i < kTabCount; ++i)
        if (CheckCollisionPointRec(p, TabRect(i))) return true;
    return false;
}

int Hud::TabAt(Vector2 p) const {
    for (int i = 0; i < kTabCount; ++i)
        if (CheckCollisionPointRec(p, TabRect(i))) return i;
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

void Hud::Update(float dt) {
    areaTime_ += dt;
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

    // Tab bar
    for (int i = 0; i < kTabCount; ++i) {
        const Rectangle r = TabRect(i);
        Panel(r, 0.22f);
        const float ts = 11.0f * s;
        const Vector2 sz = MeasureTextEx(font, kTabLabels[i], ts, 1.0f);
        DrawTextEx(font, kTabLabels[i], {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, ts, 1.0f, kText);
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

    if (debug) {
        DrawTextEx(font, debugText.c_str(), {12.0f * s, 12.0f * s}, 14.0f * s, 1.0f, {255, 255, 255, 230});
    }
}

}  // namespace journey
