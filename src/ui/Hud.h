// Prototype HUD in a modern dark-glass style, laid out like the classic client:
// tab bar top-right, chat box bottom-left, area banner top-centre.
#pragma once
#include <deque>
#include <string>

#include "raylib.h"

namespace journey {

class Hud {
public:
    void Say(const std::string& text, Color color = {255, 224, 120, 255});
    void ShowArea(const std::string& name);
    void Update(float dt);
    void Draw(bool debug, const std::string& debugText) const;

    // True if a screen point is over a HUD panel (so taps there do not walk the hero).
    bool Hits(Vector2 p) const;
    // Which tab (0..5) is at this point, or -1. Tab 5 is "Menu".
    int TabAt(Vector2 p) const;

private:
    struct Line {
        std::string text;
        Color color;
    };
    float Scale() const;
    Rectangle ChatRect() const;
    Rectangle TabRect(int i) const;

    std::deque<Line> lines_;
    std::string area_;
    float areaTime_ = 99.0f;
};

}  // namespace journey
