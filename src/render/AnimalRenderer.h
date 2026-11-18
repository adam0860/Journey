// Draws the creatures (chickens, cows, calves, goblins). Each is a few small rigid parts (body, head, legs, tail...) in pivot space,
// built by tools/build_animals.py; this class swings and tilts them to animate: walking, idling (pecking / grazing /
// looking about), striking, flinching, falling over when dead, and growing in when it (re)appears.
#pragma once
#include <map>
#include <memory>
#include <string>

#include "game/Npc.h"
#include "raylib.h"
#include "render/LitShader.h"
#include "render/TextureCache.h"

namespace journey {

class AnimalRenderer {
public:
    bool Load(const LitShader& shader, const char* dir = "assets/models/animals");
    void Unload();
    void SetShadowSampling(const LitShader& lit, bool sample);

    // Draws one animal (alive, or its corpse while it is still lying there). `time` is the global clock.
    void Draw(const Npc& npc, float time);
    // Skips the animal if it is not worth drawing (too far, or its corpse has gone). `focus` = hero position.
    bool Visible(const Npc& npc, float focusX, float focusZ) const;

    // How tall the animal is (for hit splats and health bars above its head), in metres.
    static float Height(Species s);

private:
    struct Joint {
        float x = 0, y = 0, z = 0;
    };
    struct Pose {   // what the animation decided this frame
        float bodyPitch = 0, bodyRoll = 0, offY = 0, offZ = 0;     // whole body
        float headPitch = 0, headYaw = 0, headRoll = 0;
        float tailRoll = 0;
        float legFL = 0, legFR = 0, legBL = 0, legBR = 0;          // cows: swing about the hip
        float wingRoll = 0;                                        // chickens: spread (+) / tuck (-)
        float legL = 0, legR = 0;                                  // chickens
        float spreadLegs = 0;                                      // lying down: legs splay out sideways
        float armL = 0, armR = 0;                                  // goblins: swing about the shoulder (negative = forward / up)
    };
    Model* Get(const std::string& file);
    void DrawPart(const std::string& file, const Joint& joint, float rx, float ry, float rz, const Matrix& body);
    Pose AnimateChicken(const Npc& n, float time) const;
    Pose AnimateBovine(const Npc& n, float time, bool calf) const;
    Pose AnimateGoblin(const Npc& n, float time) const;

    std::string dir_;
    const LitShader* shader_ = nullptr;
    std::map<std::string, Joint> joints_;      // "<species>.<part>"
    std::map<std::string, int> colorways_;     // species -> count
    std::map<std::string, std::unique_ptr<Model>> models_;
    TextureCache textures_;
};

}  // namespace journey
