#include "render/CharacterRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "raymath.h"

namespace journey {

bool CharacterRenderer::Load(const LitShader& shader, const char* dir) {
    dir_ = dir;
    shader_ = &shader;
    char* text = LoadFileText(TextFormat("%s/catalog.txt", dir));
    if (!text) {
        TraceLog(LOG_ERROR, "CHARACTER: missing %s/catalog.txt", dir);
        return false;
    }
    std::istringstream in(text);
    UnloadFileText(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kind;
        if (!(ls >> kind) || kind.rfind("//", 0) == 0) continue;
        if (kind == "joint") {
            std::string name;
            Joint j;
            ls >> name >> j.x >> j.y >> j.z;
            joints_[name] = j;
            continue;
        }
        StyleInfo s;
        std::string name;
        ls >> s.id >> name;
        for (char& c : name) if (c == '_') c = ' ';
        s.name = name;
        std::string flag;
        while (ls >> flag) {
            if (flag == "sleeves=0") s.hasSleeves = false;
            if (flag == "legs=0") s.hasLegs = false;
            if (flag.rfind("for=", 0) == 0 && flag.size() > 4) s.forWho = flag[4];
        }
        if (kind == "hair") catalog_.hair.push_back(s);
        else if (kind == "shirt") catalog_.shirt.push_back(s);
        else if (kind == "pants") catalog_.pants.push_back(s);
        else if (kind == "shoes") catalog_.shoes.push_back(s);
    }
    TraceLog(LOG_INFO, "CHARACTER: %d hair, %d shirts, %d pants, %d shoes", (int)catalog_.hair.size(),
             (int)catalog_.shirt.size(), (int)catalog_.pants.size(), (int)catalog_.shoes.size());
    return !catalog_.hair.empty() && joints_.count("torso");
}

void CharacterRenderer::Unload() {
    for (auto& kv : models_) {
        if (kv.second) {
            textures_.Detach(*kv.second);
            UnloadModel(*kv.second);
        }
    }
    models_.clear();
    textures_.UnloadAll();
}

Model* CharacterRenderer::Get(const std::string& file) {
    auto it = models_.find(file);
    if (it != models_.end()) return it->second.get();
    const char* path = TextFormat("%s/%s.glb", dir_.c_str(), file.c_str());
    std::unique_ptr<Model> m;
    {   // (FileExists() cannot see files packed inside an Android APK, so just try to load)
        Model loaded = LoadModel(path);
        if (loaded.meshCount > 0) {
            textures_.Share(loaded);
            shader_->ApplyTo(loaded);
            m = std::make_unique<Model>(loaded);
        }
    }
    Model* raw = m.get();
    models_[file] = std::move(m);
    return raw;
}

void CharacterRenderer::DrawPart(const std::string& file, const char* joint, float swing, float bob, const CharacterPose& p,
                                 Color tint, bool upper, const Matrix* local) {
    Model* m = Get(file);
    if (!m) return;
    const Joint& j = joints_[joint];
    Matrix t = local ? *local : MatrixIdentity();
    t = MatrixMultiply(t, MatrixRotateX(swing));
    if (upper && (lean_ != 0.0f || twist_ != 0.0f)) {
        // Leaning / twisting: the whole upper body turns about the hips (the torso joint), arms riding along with it.
        const Joint& hip = joints_["torso"];
        t = MatrixMultiply(t, MatrixTranslate(j.x - hip.x, j.y - hip.y, j.z - hip.z));
        t = MatrixMultiply(t, MatrixRotateY(twist_));
        t = MatrixMultiply(t, MatrixRotateX(lean_));
        t = MatrixMultiply(t, MatrixTranslate(hip.x, hip.y + bob, hip.z));
    } else {
        t = MatrixMultiply(t, MatrixTranslate(j.x, j.y + bob, j.z));
    }
    if (p.fall > 0.0f) {   // toppling over backwards about the feet
        t = MatrixMultiply(t, MatrixRotateX(-1.5708f * p.fall));
        t = MatrixMultiply(t, MatrixTranslate(0.0f, 0.18f * p.fall, 0.0f));
    }
    if (p.scale != 1.0f) t = MatrixMultiply(t, MatrixScale(p.scale, p.scale, p.scale));
    t = MatrixMultiply(t, MatrixRotateY(p.facing));
    t = MatrixMultiply(t, MatrixTranslate(p.x, 0.0f, p.z));
    m->transform = t;
    DrawModel(*m, {0, 0, 0}, 1.0f, tint);
}

void CharacterRenderer::SetShadowSampling(const LitShader& lit, bool sample) {
    for (auto& kv : models_)
        if (kv.second) lit.SetShadowSampling(*kv.second, sample);
}

void CharacterRenderer::Draw(const Appearance& a, const CharacterPose& p) {
    const char g = a.female ? 'f' : 'm';
    const float swing = p.moving ? std::sin(p.phase) * 0.72f : 0.0f;
    const float bob = p.moving ? std::fabs(std::sin(p.phase)) * 0.035f : 0.006f * std::sin(p.time * 2.2f);
    const float sway = p.moving ? 0.0f : 0.03f * std::sin(p.time * 1.7f);   // idle arm drift

    const Color skin = p.tintSkin ? ToLinear(p.skinTint) : ToLinear(SkinTones()[a.skin]);
    const Color hair = ToLinear(HairColors()[a.hairColor]);
    const Color shirt = p.tintShirt ? ToLinear(p.shirtTint) : ToLinear(ClothColors()[a.shirtColor]);
    const Color pants = p.tintPants ? ToLinear(p.pantsTint) : ToLinear(ClothColors()[a.pantsColor]);
    const Color shoes = ToLinear(ShoeColors()[a.shoesColor]);

    const StyleInfo* hs = (a.hair < (int)catalog_.hair.size() && !p.hideHair) ? &catalog_.hair[a.hair] : nullptr;
    const StyleInfo* ss = a.shirt < (int)catalog_.shirt.size() ? &catalog_.shirt[a.shirt] : nullptr;
    const StyleInfo* ps = a.pants < (int)catalog_.pants.size() ? &catalog_.pants[a.pants] : nullptr;
    const StyleInfo* fs = a.shoes < (int)catalog_.shoes.size() ? &catalog_.shoes[a.shoes] : nullptr;
    const GearLook& gear = p.gear;

    // forward swing is a negative rotation about +x; legs and arms alternate
    float legL = -swing, legR = swing, armL = swing + sway, armR = -swing - sway;
    float spreadL = 0.0f, spreadR = 0.0f;   // arms out to the side (rotation about z; + moves a hand towards his left)
    float bobY = bob;
    lean_ = 0.0f;
    twist_ = 0.0f;
    std::string tool;             // glb held in the right fist
    float toolTilt = 0.0f;        // extra rotation of the tool about the fist (radians)
    float shieldFace = -0.55f;    // how the shield is turned about the arm (0 = facing out to his left, -pi/2 = facing forward)
    auto smooth = [](float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); };
    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    const bool bowed = !gear.bow.empty();
    const bool armed = !gear.weapon.empty() || bowed;
    const bool shielded = !gear.shield.empty() && !bowed;   // the bow takes the shield hand

    // A swing is "wind-up, strike, follow-through, recover" laid over the half second after a strike begins. The hit lands
    // at 0.30 s (kImpactSeconds), exactly where the strike amount reaches 1.
    auto windAmount = [&](float t) { return t < 0.20f ? smooth(t / 0.20f) : (t < 0.30f ? 1.0f - smooth((t - 0.20f) / 0.10f) : 0.0f); };
    auto strikeAmount = [&](float t) {
        if (t < 0.20f) return 0.0f;
        if (t < 0.30f) return smooth((t - 0.20f) / 0.10f);
        if (t < 0.36f) return 1.0f;
        return t < 0.60f ? 1.0f - smooth((t - 0.36f) / 0.24f) : 0.0f;
    };

    switch (p.anim) {
        case Anim::Chop: {   // one axe swing per 0.6 s game tick: wind up overhead, a fast chop down, then recover
            tool = !gear.axe.empty() ? gear.axe : "held_axe";
            const float u = std::fmod(p.time / 0.6f, 1.0f);
            if (u < 0.50f) {                       // raise (arm back over the head, body leans back a touch)
                const float k = smooth(u / 0.50f);
                armR = mix(-0.85f, -2.65f, k);
                lean_ = mix(0.0f, -0.12f, k);
            } else if (u < 0.66f) {                // strike: accelerating down, body throws itself forward
                float b = (u - 0.50f) / 0.16f;
                b *= b;
                armR = mix(-2.65f, -0.85f, b);
                lean_ = mix(-0.12f, 0.32f, b);
            } else {                               // recover
                armR = -0.85f;
                lean_ = mix(0.32f, 0.0f, smooth((u - 0.66f) / 0.34f));
            }
            armL = armR * 0.22f - 0.30f;           // the off-hand comes along for balance
            legL = 0.16f;                          // braced stance, right foot back
            legR = -0.22f;
            bobY = -0.03f * std::max(0.0f, lean_);
            break;
        }
        case Anim::Fish: {   // reach out and lower the net, sweep it through the water, lift it back (one cast = 1.2 s)
            tool = "held_net";
            const float u = std::fmod(p.time / 1.2f, 1.0f);
            if (u < 0.30f) {
                const float k = smooth(u / 0.30f);
                armR = mix(-0.55f, -1.25f, k);
                lean_ = mix(0.05f, 0.50f, k);
            } else if (u < 0.65f) {
                const float b = (u - 0.30f) / 0.35f;
                armR = -1.25f + 0.22f * std::sin(b * 3.1416f);
                lean_ = 0.50f + 0.04f * std::sin(b * 3.1416f);
            } else {
                const float c = smooth((u - 0.65f) / 0.35f);
                armR = mix(-1.25f, -0.55f, c);
                lean_ = mix(0.50f, 0.05f, c);
            }
            armL = -0.30f - 0.10f * lean_;
            legL = 0.12f;
            legR = -0.12f;
            bobY = -0.02f * lean_;
            break;
        }
        case Anim::Light: {  // bend over the logs and strike the tinderbox, again and again
            tool = "held_tinderbox";
            toolTilt = 0.35f;
            lean_ = 0.62f;
            armR = -1.05f + 0.20f * std::sin(p.time * 17.0f);
            armL = -0.95f + 0.05f * std::sin(p.time * 9.0f);
            legL = 0.14f;
            legR = -0.14f;
            bobY = -0.02f;
            break;
        }
        case Anim::Cook: {   // hold the food out over the fire, turning it
            lean_ = 0.14f;
            armR = -1.20f + 0.10f * std::sin(p.time * 5.0f);
            armL = -1.00f + 0.10f * std::sin(p.time * 5.0f + 1.3f);
            legL = legR = 0.0f;
            break;
        }
        case Anim::Smelt: {  // feed ore into the furnace: both arms out in front, a rhythmic push and a step back from the heat
            const float k = 0.5f + 0.5f * std::sin(p.workPhase * 6.2832f);
            lean_ = 0.10f + 0.10f * k;
            armR = -1.22f + 0.14f * k;
            armL = -1.12f + 0.14f * k;
            spreadR = -0.10f;
            spreadL = 0.10f;
            legL = 0.10f;
            legR = -0.10f;
            bobY = -0.012f * k;
            break;
        }
        case Anim::Mine: {   // pickaxe: swing back over the shoulder, then drive it down and in; the pick lands where the phase wraps
            tool = !gear.pick.empty() ? gear.pick : "held_bronze_pickaxe";
            const float q = p.workPhase;
            float h;                                // 0 = pick down at the rock, 1 = raised
            if (q < 0.12f) h = 0.0f;
            else if (q < 0.80f) h = smooth((q - 0.12f) / 0.68f);
            else {
                const float b = (q - 0.80f) / 0.20f;
                h = 1.0f - b * b;
            }
            armR = mix(-0.70f, -2.55f, h);
            lean_ = mix(0.36f, -0.10f, h);
            spreadR = mix(0.05f, -0.10f, h);
            armL = armR * 0.45f - 0.35f;            // the off-hand rides along on the haft
            spreadL = -0.10f;
            legL = 0.20f;
            legR = -0.26f;
            bobY = -0.035f * (1.0f - h);
            break;
        }
        case Anim::Smith: { // hammer: raise, then bring it down on the anvil; the other hand holds the bar
            tool = "held_hammer";
            const float q = p.workPhase;
            float h;
            if (q < 0.15f) h = 0.0f;
            else if (q < 0.82f) h = smooth((q - 0.15f) / 0.67f);
            else {
                const float b = (q - 0.82f) / 0.18f;
                h = 1.0f - b * b;
            }
            armR = mix(-0.75f, -1.75f, h);
            lean_ = mix(0.34f, 0.12f, h);
            armL = -1.0f + 0.03f * std::sin(p.time * 3.0f);
            spreadL = -0.18f;
            legL = 0.14f;
            legR = -0.18f;
            bobY = -0.02f * (1.0f - h);
            break;
        }
        case Anim::Attack: {
            const float t = p.strikeAge;
            const float guard = -0.60f + 0.05f * std::sin(p.time * 4.0f);
            const float w = windAmount(t), st = strikeAmount(t);
            legL = 0.22f;
            legR = -0.26f;
            if (!armed || p.weaponAnim == WeaponAnim::Unarmed) {   // fists up in a fighting stance; each strike is a quick jab
                float jab = 0.0f, wind = 0.0f;
                if (t < 0.50f) {
                    const float u = t / 0.50f;
                    wind = smooth(u / 0.40f) * (1.0f - smooth((u - 0.40f) / 0.08f));
                    jab = smooth((u - 0.40f) / 0.20f) * (1.0f - smooth((u - 0.66f) / 0.34f));
                }
                const float fist = guard + 0.60f * wind - 1.00f * jab;           // the punching arm: back, then straight out
                armR = p.strikeLeft ? guard : fist;
                armL = p.strikeLeft ? fist : guard - 0.15f;
                lean_ = 0.10f + 0.20f * jab - 0.06f * wind;
                bobY = -0.012f + 0.008f * std::sin(p.time * 5.0f) - 0.01f * jab;
                if (shielded) {                                                  // a shield stays up while he punches with the other hand
                    armL = -0.80f;
                    spreadL = -0.35f;
                    shieldFace = -1.25f;
                }
                break;
            }
            tool = gear.weapon;
            const float rest = -0.70f + 0.04f * std::sin(p.time * 4.0f);       // weapon held ready, pointing forward and down
            if (p.weaponAnim == WeaponAnim::Shoot) {   // archery: the bow arm points at the target, the other hand draws the string and lets go at 0.18 s
                const float pull = t < 0.18f ? smooth(t / 0.18f) : (t < 0.30f ? 1.0f - smooth((t - 0.18f) / 0.12f) : 0.0f);
                armL = -1.50f + 0.02f * std::sin(p.time * 9.0f);
                spreadL = 0.04f;
                armR = mix(-1.28f, -0.62f, pull);
                spreadR = 0.22f * pull;
                lean_ = 0.07f - 0.10f * pull;
                twist_ = -0.10f * pull;
                legL = 0.14f;
                legR = -0.18f;
                bobY = -0.012f;
                break;
            }
            switch (p.weaponAnim) {
                case WeaponAnim::Stab: {   // dagger: pull the hand back, then thrust straight out with a lunge
                    armR = rest + 0.62f * w - 0.88f * st;
                    lean_ = 0.10f - 0.06f * w + 0.30f * st;
                    twist_ = 0.20f * st - 0.10f * w;
                    legL = 0.22f + 0.14f * st;
                    legR = -0.26f - 0.24f * st;
                    bobY = -0.012f - 0.02f * st;
                    break;
                }
                case WeaponAnim::Slash: {  // sword: up and out to the right, then a diagonal cut across the body
                    armR = mix(mix(rest, -2.15f, w), -0.95f, st);
                    spreadR = mix(mix(0.0f, -0.75f, w), 0.55f, st);
                    twist_ = mix(mix(0.0f, -0.45f, w), 0.50f, st);
                    lean_ = mix(mix(0.10f, -0.05f, w), 0.26f, st);
                    legR = -0.26f - 0.10f * st;
                    bobY = -0.012f - 0.012f * st;
                    break;
                }
                case WeaponAnim::Chop: {   // battleaxe: both hands up overhead, then a heavy two-handed chop down
                    armR = mix(mix(rest, -2.75f, w), -0.80f, st);
                    lean_ = mix(mix(0.10f, -0.16f, w), 0.40f, st);
                    twist_ = mix(mix(0.0f, -0.10f, w), 0.14f, st);
                    legL = 0.20f + 0.08f * st;
                    legR = -0.28f - 0.10f * st;
                    bobY = -0.012f - 0.04f * st;
                    break;
                }
                case WeaponAnim::Cast: {   // staff: raise it overhead, the other hand sweeping up, then thrust it at the target (contact 0.30 s)
                    armR = mix(mix(rest, -2.25f, w), -1.30f, st);
                    spreadR = mix(mix(0.0f, -0.25f, w), 0.10f, st);
                    armL = mix(mix(-0.35f, -1.30f, w), -1.55f, st);
                    spreadL = mix(0.0f, -0.35f, w * (1.0f - st) + st);
                    lean_ = mix(mix(0.10f, -0.10f, w), 0.22f, st);
                    twist_ = mix(mix(0.0f, -0.18f, w), 0.16f, st);
                    legL = 0.18f;
                    legR = -0.22f + 0.04f * st;
                    bobY = -0.012f - 0.02f * st;
                    break;
                }
                case WeaponAnim::Crush: {  // mace: lift to the shoulder, smash down in a heavy arc and hold the blow
                    armR = mix(mix(rest, -2.30f, w), -0.55f, st);
                    spreadR = mix(mix(0.0f, -0.35f, w), 0.22f, st);
                    lean_ = mix(mix(0.10f, -0.12f, w), 0.46f, st);
                    twist_ = mix(mix(0.0f, -0.22f, w), 0.22f, st);
                    bobY = -0.012f - 0.05f * st;
                    break;
                }
                default: break;
            }
            // the other hand: raises the shield in front of him, or swings for balance
            if (p.weaponAnim == WeaponAnim::Cast) {
                // (set above)
            } else if (shielded) {
                armL = -0.85f + 0.05f * std::sin(p.time * 3.0f);
                spreadL = -0.38f;
                shieldFace = -1.25f;
                if (p.weaponAnim == WeaponAnim::Chop) armL = armR * 0.5f - 0.2f;
            } else {
                armL = -0.30f - 0.20f * w + 0.22f * st;
                if (p.weaponAnim == WeaponAnim::Chop || p.weaponAnim == WeaponAnim::Crush) armL = armR * 0.35f - 0.25f;
            }
            break;
        }
        case Anim::Craft: {  // working with the hands at waist height: turning, pressing, stitching
            lean_ = 0.22f;
            armR = -1.05f + 0.16f * std::sin(p.time * 7.0f);
            armL = -0.95f + 0.16f * std::sin(p.time * 7.0f + 2.4f);
            spreadR = -0.10f;
            spreadL = 0.10f;
            legL = 0.06f;
            legR = -0.06f;
            bobY = -0.015f;
            break;
        }
        case Anim::Bury: {   // crouch over the ground and dig with both hands
            lean_ = 0.78f;
            armR = -1.25f + 0.40f * std::sin(p.time * 9.0f);
            armL = -1.25f + 0.40f * std::sin(p.time * 9.0f + 3.14f);
            legL = 0.18f;
            legR = -0.18f;
            bobY = -0.03f;
            break;
        }
        case Anim::None: {
            if (bowed && !p.moving) {   // a bow carried ready in front of him
                armL = -0.95f - sway;
            } else if (armed && !p.moving) {   // standing around with a weapon: held ready in front of him rather than hanging dead
                armR = -0.30f - sway;
            }
            break;
        }
    }

    // ---- being struck at: raise the shield (or guard), and flinch back if the blow lands
    if ((p.anim == Anim::None || p.anim == Anim::Attack) && p.blockAge < 0.75f) {
        const float t = p.blockAge;
        const float up = smooth(t / 0.14f) * (1.0f - smooth((t - 0.46f) / 0.26f));      // guard raised, held over the impact (0.30 s)
        if (shielded) {
            armL = mix(armL, -0.98f, up);
            spreadL = mix(spreadL, -0.70f, up);
            shieldFace = mix(shieldFace, -1.55f, up);
        } else {
            armL = mix(armL, -1.05f, up * 0.8f);
            spreadL = mix(spreadL, -0.20f, up * 0.8f);
            if (p.anim == Anim::None) armR = mix(armR, -1.05f, up * 0.7f);              // the weapon (or fist) comes up too
        }
        lean_ += -0.05f * up;
        if (p.blockLanded && t > 0.28f && t < 0.56f) {                                // the hit: knocked back a little
            const float k = std::sin((t - 0.28f) / 0.28f * 3.1416f);
            lean_ += -0.30f * k;
            bobY -= 0.035f * k;
            twist_ += 0.14f * k;
        }
    }

    const std::string gs(1, g);
    const bool helm = !gear.helm.empty(), body = !gear.body.empty(), legs = !gear.legs.empty();
    const Matrix spreadMatL = MatrixRotateZ(spreadL), spreadMatR = MatrixRotateZ(spreadR);

    // ---- body
    DrawPart("body_" + gs + "_torso", "torso", 0, bobY, p, skin, true);
    DrawPart("body_" + gs + "_head", "head", 0, bobY, p, skin, true);
    DrawPart("body_arm_l", "arm_l", armL, bobY, p, skin, true, &spreadMatL);
    DrawPart("body_arm_r", "arm_r", armR, bobY, p, skin, true, &spreadMatR);
    DrawPart("body_leg_l", "leg_l", legL, bobY, p, skin);
    DrawPart("body_leg_r", "leg_r", legR, bobY, p, skin);
    // ---- hair (a helmet covers it)
    if (hs && !helm) DrawPart("hair_" + hs->id, "head", 0, bobY, p, hair, true);
    // ---- shirt (plate armour goes straight over the skin)
    if (ss && !body) {
        DrawPart("shirt_" + ss->id + "_torso_" + gs, "torso", 0, bobY, p, shirt, true);
        if (ss->hasSleeves) {
            DrawPart("shirt_" + ss->id + "_sleeve_l", "arm_l", armL, bobY, p, shirt, true, &spreadMatL);
            DrawPart("shirt_" + ss->id + "_sleeve_r", "arm_r", armR, bobY, p, shirt, true, &spreadMatR);
        }
    }
    // ---- pants (the hips stay with the legs; the shirt hem covers the join when the body leans)
    if (ps && !legs) {
        DrawPart("pants_" + ps->id + "_hips_" + gs, "torso", 0, bobY, p, pants);
        if (ps->hasLegs) {
            DrawPart("pants_" + ps->id + "_leg_l", "leg_l", legL, bobY, p, pants);
            DrawPart("pants_" + ps->id + "_leg_r", "leg_r", legR, bobY, p, pants);
        }
    }
    // ---- shoes
    if (fs && fs->id != "none") {
        DrawPart("shoes_" + fs->id + "_l", "leg_l", legL, bobY, p, shoes);
        DrawPart("shoes_" + fs->id + "_r", "leg_r", legR, bobY, p, shoes);
    }
    // ---- worn armour (the metal's own colours: no tint)
    if (helm) DrawPart(gear.helm, "head", 0, bobY, p, WHITE, true);
    if (body) {
        DrawPart(gear.body + "_torso_" + gs, "torso", 0, bobY, p, WHITE, true);
        DrawPart(gear.body + "_arm_l", "arm_l", armL, bobY, p, WHITE, true, &spreadMatL);
        DrawPart(gear.body + "_arm_r", "arm_r", armR, bobY, p, WHITE, true, &spreadMatR);
    }
    if (legs) {
        DrawPart(gear.legs + "_hips_" + gs, "torso", 0, bobY, p, WHITE);
        DrawPart(gear.legs + "_leg_l", "leg_l", legL, bobY, p, WHITE);
        DrawPart(gear.legs + "_leg_r", "leg_r", legR, bobY, p, WHITE);
    }
    if (!gear.boots.empty()) {
        DrawPart(gear.boots + "_leg_l", "leg_l", legL, bobY, p, WHITE);
        DrawPart(gear.boots + "_leg_r", "leg_r", legR, bobY, p, WHITE);
    }
    if (!gear.gloves.empty()) {
        DrawPart(gear.gloves + "_arm_l", "arm_l", armL, bobY, p, WHITE, true, &spreadMatL);
        DrawPart(gear.gloves + "_arm_r", "arm_r", armR, bobY, p, WHITE, true, &spreadMatR);
    }
    if (!gear.cape.empty()) {   // it streams out behind him when he walks and hangs still when he stands
        const float flutter = 0.05f + (p.moving ? 0.07f + 0.10f * std::fabs(std::sin(p.phase)) : 0.012f * std::sin(p.time * 1.9f));
        DrawPart(gear.cape, "torso", flutter, bobY, p, WHITE, true);
    }
    if (!gear.amulet.empty()) DrawPart(gear.amulet, "torso", 0, bobY, p, WHITE, true);   // (over the armour)
    // ---- the tool in the right fist. Item models have their origin at the grip; the hand sits at (0,-0.575,0.016)
    // from the shoulder joint, so the tool turns with the arm. While nothing is being worked, he carries his weapon.
    if (tool.empty() && armed && (p.anim == Anim::None)) tool = gear.weapon;
    if (!tool.empty()) {
        Matrix local = MatrixMultiply(MatrixRotateX(toolTilt), MatrixTranslate(0.0f, -0.575f, 0.016f));
        local = MatrixMultiply(local, spreadMatR);
        DrawPart(tool, "arm_r", armR, bobY, p, WHITE, true, &local);
    }
    // ---- the bow in the left hand: counter-rotated against the arm's swing so it stays upright (its belly points forward)
    if (bowed) {
        Matrix local = MatrixRotateX(-armL);
        local = MatrixMultiply(local, MatrixRotateZ(-spreadL));
        local = MatrixMultiply(local, MatrixTranslate(0.0f, -0.575f, 0.016f));
        local = MatrixMultiply(local, spreadMatL);
        DrawPart(gear.bow, "arm_l", armL, bobY, p, WHITE, true, &local);
    }
    // ---- the shield on the left forearm. It is counter-rotated against the arm's own swing, so it stays upright and
    // faces where shieldFace says (out to the side while he walks, towards the enemy when he fights or blocks).
    if (shielded) {
        Matrix local = MatrixRotateY(shieldFace);
        local = MatrixMultiply(local, MatrixRotateX(-armL));
        local = MatrixMultiply(local, MatrixRotateZ(-spreadL));
        local = MatrixMultiply(local, MatrixTranslate(0.0f, -0.52f, 0.016f));
        local = MatrixMultiply(local, spreadMatL);
        DrawPart(gear.shield, "arm_l", armL, bobY, p, WHITE, true, &local);
    }
}

}  // namespace journey
