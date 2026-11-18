// Prototype HUD in a modern dark-glass style, laid out like the classic client:
// tab bar top-right, chat box bottom-left, area banner top-centre. Tapping the Bag or Skills tab
// opens a panel under the tab bar (tap the tab again to close it).
#pragma once
#include <deque>
#include <string>

#include "game/Inventory.h"
#include "game/Skills.h"
#include "raylib.h"

namespace journey {

class Hud {
public:
    static constexpr int kTabBag = 0, kTabSkills = 1, kTabMenu = 5;

    // The HUD reads (never changes) the player's skills and bag. Call once at start-up.
    void Bind(const Skills* skills, const Inventory* bag) {
        skills_ = skills;
        bag_ = bag;
    }

    void Say(const std::string& text, Color color = {255, 224, 120, 255});
    void ShowArea(const std::string& name);
    // Floating "+25 xp" text, like the classic client's experience drops.
    void XpDrop(Skill skill, int amount);
    void Update(float dt);
    void Draw(bool debug, const std::string& debugText) const;

    // True if a screen point is over a HUD panel (so taps there do not walk the hero).
    bool Hits(Vector2 p) const;
    // Which tab (0..5) is at this point, or -1. Tab 5 is "Menu".
    int TabAt(Vector2 p) const;
    // Open the tab's panel, or close it if it is already open. (The Menu tab has no panel.)
    void ToggleTab(int tab);
    void CloseTabs() { openTab_ = -1; }
    int OpenTab() const { return openTab_; }
    // Which bag slot (0..27) is under this point, or -1 (also -1 while the bag is closed).
    int BagSlotAt(Vector2 p) const;

private:
    struct Line {
        std::string text;
        Color color;
    };
    struct Drop {
        std::string text;
        float age;
    };
    float Scale() const;
    Rectangle ChatRect() const;
    Rectangle TabRect(int i) const;
    Rectangle PanelRect() const;          // the open tab's panel (only valid while openTab_ >= 0)
    Rectangle BagCellRect(int slot) const;
    void DrawBag(Rectangle panel, float s, const Font& font) const;
    void DrawSkills(Rectangle panel, float s, const Font& font) const;

    std::deque<Line> lines_;
    std::deque<Drop> drops_;
    std::string area_;
    float areaTime_ = 99.0f;
    int openTab_ = -1;
    const Skills* skills_ = nullptr;
    const Inventory* bag_ = nullptr;
};

}  // namespace journey
