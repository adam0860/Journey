// Character creation screen: name, gender, skin tone, hair / shirt / pants / shoes with colours,
// and a live 3D preview you can spin by dragging. Immediate-mode UI: mouse and touch both work
// (raylib maps a touch to the left mouse button).
#pragma once
#include <string>

#include "game/Appearance.h"
#include "raylib.h"
#include "render/CharacterRenderer.h"
#include "render/LitShader.h"

namespace journey {

class CharacterCreator {
public:
    void Begin(const Appearance& initial);
    const Appearance& Look() const { return look_; }
    Appearance& Look() { return look_; }
    void SetPreviewYaw(float degrees) { yaw_ = degrees; autoSpin_ = false; }
    void SetWalking(bool w) { walking_ = w; }
    // Phones have no physical keyboard: draw our own keyboard when the name field is tapped.
    void SetOnScreenKeyboard(bool enabled, bool openNow = false) { useOsk_ = enabled; oskOpen_ = enabled && openNow; }
    void InjectClick(Vector2 p) { injected_ = true; injectedPos_ = p; }    // dev/testing: pretend the user tapped here

    // Call between BeginDrawing/EndDrawing. Draws everything and handles input.
    // Returns true on the frame the player presses "Start Journey".
    bool Frame(float dt, LitShader& lit, CharacterRenderer& chars);

private:
    Appearance look_;
    float yaw_ = 20.0f;
    bool autoSpin_ = true;
    bool walking_ = false;
    float time_ = 0.0f;
    float phase_ = 0.0f;
    float backHeld_ = 0.0f;
    bool dragging_ = false;
    bool useOsk_ = false, oskOpen_ = false;
    bool injected_ = false;
    Vector2 injectedPos_{0, 0};
};

}  // namespace journey
