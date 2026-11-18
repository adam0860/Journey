// Draws the modular character: body + hair + shirt + pants + shoes, each a small glTF in pivot space,
// tinted with the colours the player picked and animated as rigid parts (walk swing, bob, breathing).
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/Appearance.h"
#include "game/Gear.h"
#include "raylib.h"
#include "render/LitShader.h"
#include "render/TextureCache.h"

namespace journey {

struct StyleInfo {
    std::string id;
    std::string name;     // display name
    bool hasSleeves = true;
    bool hasLegs = true;
    char forWho = 'u';     // hair only: 'm' men, 'f' women, 'u' both
};

struct CharacterCatalog {
    std::vector<StyleInfo> hair, shirt, pants, shoes;
    // indices into `hair` that this body type may choose
    std::vector<int> HairFor(bool female) const {
        std::vector<int> out;
        for (int i = 0; i < (int)hair.size(); ++i)
            if (hair[i].forWho == 'u' || hair[i].forWho == (female ? 'f' : 'm')) out.push_back(i);
        return out;
    }
};

// What the hero is doing with his hands (each plays its own animation, and some put a tool in his right hand).
enum class Anim { None, Chop, Fish, Light, Cook, Attack, Bury, Mine, Smelt, Smith };

// The model files for what he wears and wields (empty = nothing). The game fills these in from his Equipment.
struct GearLook {
    std::string weapon;   // "held_bronze_sword": in his right fist
    std::string shield;   // "held_iron_shield": on his left forearm
    std::string helm;     // "gear_steel_helm"
    std::string body;     // "gear_bronze_body"   (+ _torso_m / _arm_l ... parts)
    std::string legs;     // "gear_mithril_legs"  (+ _hips_m / _leg_l ... parts)
    std::string pick;     // "held_iron_pickaxe": the pickaxe he mines with
};

struct CharacterPose {
    float x = 0, z = 0;      // world position (tile units)
    float facing = 0;        // radians, 0 = facing +z
    float phase = 0;         // walk phase
    bool moving = false;
    Anim anim = Anim::None;  // Chop: axe overhead swing; Fish: net cast and sweep; Light: tinderbox; Cook: hold food over the fire;
                             // Attack: fists up, a punch per strike; Bury: dig with both hands
    float time = 0;          // drives the animations and idle breathing
    float strikeAge = 99;    // Attack: seconds since the last punch began (a guard stance when it is old)
    bool strikeLeft = false; // Attack: which fist threw it
    WeaponAnim weaponAnim = WeaponAnim::Unarmed;   // Attack: how the wielded weapon swings (punch / stab / slash / chop / crush)
    GearLook gear;
    float workPhase = 0;     // Mine / Smith: 0..1 through a swing; the tool lands where it wraps round from 1 to 0
    float blockAge = 99;     // seconds since a creature began a strike at him (he raises his shield or guard, and flinches if it lands)
    bool blockLanded = false;
};

class CharacterRenderer {
public:
    bool Load(const LitShader& shader, const char* dir = "assets/models/character");
    void Unload();
    const CharacterCatalog& Catalog() const { return catalog_; }
    void Draw(const Appearance& look, const CharacterPose& pose);
    void SetShadowSampling(const LitShader& lit, bool sample);

private:
    struct Joint {
        float x = 0, y = 0, z = 0;
    };
    Model* Get(const std::string& file);
    // swing: rotation about the joint (arms/legs). upper: part belongs to the upper body, which can lean forward from the
    // hips (lean_). local: extra transform applied first (used to put a held tool in the fist).
    void DrawPart(const std::string& file, const char* joint, float swing, float bob, const CharacterPose& p, Color tint,
                  bool upper = false, const Matrix* local = nullptr);

    float lean_ = 0.0f;   // forward lean of the upper body this frame (radians, + = forward)
    float twist_ = 0.0f;  // the chest turning about the spine (radians, + = towards his left)
    std::string dir_;
    const LitShader* shader_ = nullptr;
    CharacterCatalog catalog_;
    std::map<std::string, Joint> joints_;
    std::map<std::string, std::unique_ptr<Model>> models_;
    TextureCache textures_;
};

}  // namespace journey
