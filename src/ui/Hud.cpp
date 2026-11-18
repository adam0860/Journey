#include "ui/Hud.h"

#include <algorithm>
#include <cmath>

namespace journey {

namespace {
constexpr int kTabCount = 6;
const char* kTabLabels[kTabCount] = {"Bag", "Skills", "Quests", "Map", "Gear", "Menu"};
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

// Little hand-drawn icons for items (no image files needed).
void DrawItemIcon(Item item, Rectangle r) {
    const float cx = r.x + r.width / 2, cy = r.y + r.height / 2, u = r.width;
    switch (item) {
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
        h = (30.0f + 12.0f + kBagRows * 54.0f + (kBagRows - 1) * 4.0f + 12.0f) * s;
    } else if (openTab_ == kTabSkills) {
        w = 270.0f * s;
        h = (34.0f + kSkillCount * 30.0f + 34.0f) * s;
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

bool Hud::Hits(Vector2 p) const {
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
    drops_.push_back({std::string("+") + std::to_string(amount) + " " + SkillName(skill) + " xp", 0.0f});
    while (drops_.size() > 4) drops_.pop_front();
}

void Hud::Update(float dt) {
    areaTime_ += dt;
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
        DrawRectangleRounded(c, 0.14f, 6, kCell);
        if (!bag_) continue;
        const ItemStack& st = bag_->Slot(i);
        if (st.item == Item::None) continue;
        DrawItemIcon(st.item, c);
        if (GetItemInfo(st.item).stackable && st.count > 1)
            DrawTextEx(font, std::to_string(st.count).c_str(), {c.x + 4.0f * s, c.y + 3.0f * s}, 13.0f * s, 1.0f, {255, 240, 120, 255});
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
        const float yy = 90.0f * s - d.age * 22.0f * s;
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

    if (debug) {
        DrawTextEx(font, debugText.c_str(), {12.0f * s, 12.0f * s}, 14.0f * s, 1.0f, {255, 255, 255, 230});
    }
}

}  // namespace journey
