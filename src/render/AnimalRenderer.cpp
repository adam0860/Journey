#include "render/AnimalRenderer.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "raymath.h"

namespace journey {

namespace {

float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float Mix(float a, float b, float t) { return a + (b - a) * t; }

constexpr float kPi = 3.14159265f;

}  // namespace

float AnimalRenderer::Height(Species s) {
    return GetSpecies(s).height;
}

bool AnimalRenderer::Load(const LitShader& shader, const char* dir) {
    dir_ = dir;
    shader_ = &shader;
    // two catalogs: the farm animals and goblins (build_animals.py), and the monsters (build_monsters.py)
    for (const char* file : {"catalog.txt", "catalog_monsters.txt"}) {
        char* text = LoadFileText(TextFormat("%s/%s", dir, file));
        if (!text) {
            TraceLog(LOG_WARNING, "ANIMALS: missing %s/%s", dir, file);
            continue;
        }
        std::istringstream in(text);
        UnloadFileText(text);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::string kind;
            if (!(ls >> kind) || kind.rfind("//", 0) == 0) continue;
            if (kind == "joint") {
                std::string species, part;
                Joint j;
                ls >> species >> part >> j.x >> j.y >> j.z;
                joints_[species + "." + part] = j;
            } else if (kind == "colorways") {
                std::string species;
                int n = 1;
                ls >> species >> n;
                colorways_[species] = std::max(1, n);
            }
        }
    }
    TraceLog(LOG_INFO, "ANIMALS: %d joints, %d species", (int)joints_.size(), (int)colorways_.size());
    return !joints_.empty();
}

void AnimalRenderer::Unload() {
    for (auto& kv : models_) {
        if (kv.second) {
            textures_.Detach(*kv.second);
            UnloadModel(*kv.second);
        }
    }
    models_.clear();
    textures_.UnloadAll();
}

Model* AnimalRenderer::Get(const std::string& file) {
    auto it = models_.find(file);
    if (it != models_.end()) return it->second.get();
    std::unique_ptr<Model> m;
    Model loaded = LoadModel(TextFormat("%s/%s.glb", dir_.c_str(), file.c_str()));
    if (loaded.meshCount > 0) {
        textures_.Share(loaded);
        shader_->ApplyTo(loaded);
        m = std::make_unique<Model>(loaded);
    } else {
        TraceLog(LOG_WARNING, "ANIMALS: could not load %s.glb", file.c_str());
    }
    Model* raw = m.get();
    models_[file] = std::move(m);
    return raw;
}

void AnimalRenderer::SetShadowSampling(const LitShader& lit, bool sample) {
    for (auto& kv : models_)
        if (kv.second) lit.SetShadowSampling(*kv.second, sample);
}

void AnimalRenderer::DrawPart(const std::string& file, const Joint& j, float rx, float ry, float rz, const Matrix& body) {
    Model* m = Get(file);
    if (!m) return;
    Matrix t = MatrixRotateX(rx);
    t = MatrixMultiply(t, MatrixRotateZ(rz));
    t = MatrixMultiply(t, MatrixRotateY(ry));
    t = MatrixMultiply(t, MatrixTranslate(j.x, j.y, j.z));
    t = MatrixMultiply(t, body);
    m->transform = t;
    DrawModel(*m, {0, 0, 0}, 1.0f, WHITE);
}

bool AnimalRenderer::Visible(const Npc& n, float fx, float fz) const {
    if (n.dead && n.deathT > Npcs::kCorpseSeconds) return false;
    const float dx = n.x - fx, dz = n.z - fz;
    return dx * dx + dz * dz < 45.0f * 45.0f;
}

// ---------------------------------------------------------------------------------------------- chicken

AnimalRenderer::Pose AnimalRenderer::AnimateChicken(const Npc& n, float time) const {
    Pose p;
    const float t = time + n.id * 1.7f;
    const float ph = n.walkPhase;
    p.wingRoll = -0.10f;   // wings tucked in
    if (n.Moving()) {
        const float s = std::sin(ph);
        p.legL = s * 0.95f;
        p.legR = -s * 0.95f;
        p.offY = std::fabs(s) * 0.02f;
        p.headPitch = -0.10f + 0.22f * std::sin(ph + 1.2f);   // the head bobs forward and back as it walks
        p.bodyPitch = 0.05f;
    } else {
        // Idle: peck at the ground twice, then look around, over and over (each chicken out of step with the others).
        const float cyc = std::fmod(t / 3.6f, 1.0f);
        if (cyc < 0.5f) {
            const float pk = std::pow(std::max(0.0f, std::sin(cyc / 0.5f * 4.0f * kPi)), 2.0f);
            p.headPitch = 0.15f + 1.15f * pk;
            p.bodyPitch = 0.30f * pk;
            p.legL = p.legR = -0.12f * pk;
        } else {
            p.headYaw = 0.55f * std::sin((cyc - 0.5f) * 2.0f * kPi * 1.5f);
            p.headPitch = -0.08f;
        }
        p.offY = 0.004f * std::sin(t * 3.0f);
    }
    // striking: lunge forward and stab down with the beak, wings flared
    if (n.attackT < 0.55f) {
        const float u = std::sin(n.attackT / 0.55f * kPi);
        p.offZ += 0.15f * u;
        p.bodyPitch += 0.45f * u;
        p.headPitch = 1.15f * u;
        p.wingRoll = 0.9f * u;
    }
    // flinching
    if (n.hurtT < 0.45f) {
        const float d = 1.0f - n.hurtT / 0.45f;
        p.bodyRoll += 0.30f * std::sin(n.hurtT * 38.0f) * d;
        p.wingRoll = std::max(p.wingRoll, 0.75f * d);
        p.headPitch -= 0.35f * d;
        p.offZ -= 0.05f * d;
    }
    // falling over
    if (n.dead) {
        const float e = Smooth(n.deathT / 0.9f);
        p.bodyRoll = (kPi / 2.0f) * e + 0.05f * std::sin(n.deathT * 20.0f) * (1.0f - e);
        p.offY = -0.105f * e;
        p.headPitch = Mix(p.headPitch, 0.4f, e);
        p.headRoll = 0.5f * e;
        p.wingRoll = 0.6f * e;
        p.spreadLegs = e;
        p.legL = 0.5f * e;
        p.legR = -0.3f * e;
    }
    return p;
}

// ---------------------------------------------------------------------------------------------- cow and calf

AnimalRenderer::Pose AnimalRenderer::AnimateBovine(const Npc& n, float time, bool calf) const {
    Pose p;
    const float t = time + n.id * 2.3f;
    const float ph = n.walkPhase;
    const float amp = calf ? 0.60f : 0.45f;
    p.tailRoll = 0.30f * std::sin(t * 2.2f);
    if (n.Moving()) {
        const float s = std::sin(ph);
        p.legFL = p.legBR = s * amp;       // diagonal pairs move together
        p.legFR = p.legBL = -s * amp;
        p.offY = std::fabs(s) * (calf ? 0.03f : 0.016f);
        p.headPitch = 0.10f * std::sin(ph * 2.0f + 0.5f) + 0.05f;
        p.bodyRoll = 0.025f * s;
        p.tailRoll += 0.25f * s;
    } else {
        // Idle: graze (head down, chewing), then lift the head and look about; calves fidget more.
        const float cyc = std::fmod(t / (calf ? 6.5f : 9.0f), 1.0f);
        const float down = Smooth((cyc < 0.55f ? cyc / 0.08f : (0.62f - cyc) / 0.07f));
        p.headPitch = 0.98f * down + (1.0f - down) * (0.06f + 0.05f * std::sin(t * 0.9f));
        p.headPitch += 0.07f * down * std::sin(t * 8.0f);              // chewing
        p.bodyPitch = 0.05f * down;
        p.headYaw = (1.0f - down) * 0.55f * std::sin(t * 0.8f);
        if (calf) p.offY = 0.012f * std::max(0.0f, std::sin(t * 2.6f));
        p.legFL = -0.12f * down;                                       // a front leg steps forward a little while grazing
    }
    // striking: rear back, then drive the head and shoulders forward in a butt
    if (n.attackT < 0.75f) {
        const float u = n.attackT / 0.75f;
        const float wind = Smooth(u / 0.35f) * (1.0f - Smooth((u - 0.35f) / 0.12f));
        const float strike = Smooth((u - 0.35f) / 0.12f) * (1.0f - Smooth((u - 0.5f) / 0.5f));
        p.headPitch = Mix(p.headPitch, -0.40f * wind + 0.75f * strike, std::min(1.0f, wind + strike));
        p.offZ += -0.10f * wind + 0.26f * strike;
        p.bodyPitch += -0.05f * wind + 0.10f * strike;
        p.legFL += 0.5f * strike;
    }
    if (n.hurtT < 0.5f) {
        const float d = 1.0f - n.hurtT / 0.5f;
        p.bodyPitch -= 0.10f * d;
        p.headPitch -= 0.35f * d;
        p.bodyRoll += 0.08f * std::sin(n.hurtT * 30.0f) * d;
        p.tailRoll += 0.6f * d;
    }
    if (n.dead) {
        const float e = Smooth(n.deathT / 1.1f);
        p.bodyRoll = (kPi / 2.0f) * e;
        p.offY = -(calf ? 0.22f : 0.48f) * e;
        p.headPitch = Mix(p.headPitch, 0.35f, e);
        p.headRoll = 0.5f * e;
        p.spreadLegs = e;
        p.legFL = 0.25f * e;
        p.legFR = -0.1f * e;
        p.legBL = -0.2f * e;
        p.legBR = 0.15f * e;
    }
    return p;
}

// ---------------------------------------------------------------------------------------------- goblin

AnimalRenderer::Pose AnimalRenderer::AnimateGoblin(const Npc& n, float time) const {
    Pose p;
    const float t = time + n.id * 1.9f;
    const float ph = n.walkPhase;
    p.bodyPitch = 0.10f;   // always a little hunched
    if (n.Moving()) {
        const float s = std::sin(ph);
        p.legL = s * 0.85f;
        p.legR = -s * 0.85f;
        p.armL = -0.15f - s * 0.65f;
        p.armR = -0.35f + s * 0.45f;
        p.offY = std::fabs(s) * 0.035f;
        p.bodyRoll = 0.05f * s;
        p.bodyPitch = n.hunting ? 0.22f : 0.14f;   // hunting: leaning into the chase
        p.headPitch = 0.08f;
    } else if (n.hunting) {
        // squared up for a fight: knees bent, club arm cocked, head thrust forward and bobbing
        p.armR = -0.75f + 0.08f * std::sin(t * 6.0f);
        p.armL = -0.55f + 0.08f * std::sin(t * 5.0f + 1.0f);
        p.legL = 0.18f;
        p.legR = -0.22f;
        p.bodyPitch = 0.20f;
        p.headPitch = 0.15f + 0.05f * std::sin(t * 7.0f);
        p.offY = -0.02f + 0.01f * std::sin(t * 8.0f);
    } else {
        // idle: shifts its weight, scratches, looks about with a sneer
        p.armL = -0.12f + 0.06f * std::sin(t * 1.3f);
        p.armR = -0.30f + 0.05f * std::sin(t * 1.1f + 1.0f);
        p.headYaw = 0.55f * std::sin(t * 0.55f);
        p.headPitch = 0.05f * std::sin(t * 0.9f);
        p.bodyRoll = 0.025f * std::sin(t * 0.8f);
        p.offY = 0.006f * std::sin(t * 2.0f);
        const float scratch = std::fmod(t / 7.0f, 1.0f);   // now and then it scratches its head
        if (scratch > 0.80f) p.armL = -2.2f + 0.25f * std::sin(t * 14.0f);
    }
    // striking: raise the weapon overhead, then smash it down; the hit lands 0.30 s after the strike begins
    if (n.attackT < 0.70f) {
        const float a = n.attackT;
        const float wind = a < 0.22f ? Smooth(a / 0.22f) : (a < 0.30f ? 1.0f - Smooth((a - 0.22f) / 0.08f) : 0.0f);
        const float strike = a < 0.22f ? 0.0f : (a < 0.30f ? Smooth((a - 0.22f) / 0.08f) : (a < 0.40f ? 1.0f : 1.0f - Smooth((a - 0.40f) / 0.30f)));
        p.armR = Mix(Mix(p.armR, -2.65f, wind), -0.55f, strike);
        p.armL = Mix(p.armL, -1.0f, std::min(1.0f, wind * 0.6f + strike * 0.3f));
        p.bodyPitch += -0.18f * wind + 0.38f * strike;
        p.offZ += 0.20f * strike;
        p.offY -= 0.04f * strike;
        p.legL += 0.25f * strike;
        p.headPitch += 0.25f * strike;
    }
    if (n.hurtT < 0.5f) {
        const float d = 1.0f - n.hurtT / 0.5f;
        p.bodyPitch -= 0.30f * d;
        p.headPitch -= 0.45f * d;
        p.armL = Mix(p.armL, -0.9f, d);
        p.armR = Mix(p.armR, -0.9f, d * 0.7f);
        p.bodyRoll += 0.10f * std::sin(n.hurtT * 32.0f) * d;
        p.offZ -= 0.08f * d;
    }
    if (n.dead) {
        const float e = Smooth(n.deathT / 0.9f);
        p.bodyPitch = -(kPi / 2.0f) * e;              // topples over backwards
        p.offY = -0.40f * e;
        p.offZ = -0.05f * e;
        p.headPitch = Mix(p.headPitch, -0.35f, e);
        p.armL = Mix(p.armL, -0.4f, e);
        p.armR = Mix(p.armR, -0.2f, e);
        p.legL = Mix(p.legL, 0.1f, e);
        p.legR = Mix(p.legR, -0.1f, e);
        p.spreadLegs = e;
    }
    return p;
}

// ---------------------------------------------------------------------------------------------- wolves, rats, spiders, the dragon

AnimalRenderer::Pose AnimalRenderer::AnimatePredator(const Npc& n, float time, Rig rig) const {
    Pose p;
    const float t = time + n.id * 2.3f;
    const float ph = n.walkPhase;
    const bool spider = rig == Rig::Spider, dragon = rig == Rig::Dragon;
    const float amp = spider ? 0.55f : (dragon ? 0.32f : 0.55f);
    p.tailRoll = 0.25f * std::sin(t * 2.0f);
    if (dragon) p.wingRoll = 0.14f + 0.10f * std::sin(t * 1.3f);
    if (n.Moving()) {
        const float s = std::sin(ph);
        p.legFL = p.legBR = s * amp;
        p.legFR = p.legBL = -s * amp;
        p.offY = std::fabs(s) * (spider ? 0.012f : (dragon ? 0.03f : 0.02f));
        p.bodyRoll = (spider ? 0.05f : 0.03f) * s;
        p.tailRoll += 0.3f * s;
        p.headPitch = n.hunting ? 0.20f : 0.05f + 0.06f * std::sin(ph * 2.0f);
        p.bodyPitch = n.hunting && !dragon ? 0.04f : 0.0f;
        if (dragon) p.wingRoll += 0.10f * std::fabs(s);
    } else if (n.hunting) {
        p.headPitch = dragon ? -0.12f : 0.28f + 0.06f * std::sin(t * 6.0f);   // head low and snarling
        p.bodyPitch = dragon ? 0.0f : 0.05f;
        p.legFL = -0.10f;
        p.tailRoll += 0.25f * std::sin(t * 5.0f);
        if (dragon) p.wingRoll = 0.45f + 0.15f * std::sin(t * 2.5f);
    } else {
        const float sniff = std::pow(std::max(0.0f, std::sin(t * 0.8f)), 4.0f);   // now and then a good sniff, otherwise a look about
        p.headPitch = (spider ? 0.05f : 0.55f) * sniff + 0.04f * std::sin(t * 0.9f);
        p.headYaw = 0.45f * std::sin(t * 0.55f) * (1.0f - sniff);
        p.offY = 0.004f * std::sin(t * 2.1f);
    }
    // striking: a lunge and a bite (the dragon rears its head back, then lunges and breathes)
    if (n.attackT < 0.70f) {
        const float u = n.attackT / 0.70f;
        const float wind = Smooth(u / 0.30f) * (1.0f - Smooth((u - 0.30f) / 0.10f));
        const float strike = Smooth((u - 0.30f) / 0.10f) * (1.0f - Smooth((u - 0.45f) / 0.55f));
        if (dragon) {
            p.headPitch = Mix(p.headPitch, -0.55f * wind + 0.45f * strike, std::min(1.0f, wind + strike));
            p.bodyPitch += -0.10f * wind + 0.10f * strike;
            p.wingRoll += 0.6f * wind;
        } else {
            p.headPitch = Mix(p.headPitch, -0.30f * wind + 0.80f * strike, std::min(1.0f, wind + strike));
            p.offZ += -0.08f * wind + (spider ? 0.20f : 0.34f) * strike;
            p.bodyPitch += -0.08f * wind + 0.12f * strike;
            p.legFL += 0.5f * strike;
            p.legFR += 0.4f * strike;
        }
    }
    if (n.hurtT < 0.5f) {
        const float d = 1.0f - n.hurtT / 0.5f;
        p.bodyPitch -= 0.14f * d;
        p.headPitch -= 0.35f * d;
        p.bodyRoll += 0.10f * std::sin(n.hurtT * 30.0f) * d;
        p.tailRoll += 0.6f * d;
        if (dragon) p.wingRoll += 0.5f * d;
    }
    if (n.dead) {
        const float e = Smooth(n.deathT / (dragon ? 1.6f : 0.9f));
        p.bodyRoll = (kPi / 2.0f) * e;
        p.offY = -(spider ? 0.30f : (dragon ? 0.85f : 0.42f)) * e;
        p.headPitch = Mix(p.headPitch, 0.35f, e);
        p.headRoll = 0.5f * e;
        p.spreadLegs = e;
        p.legFL = 0.3f * e;
        p.legFR = -0.2f * e;
        p.legBL = -0.2f * e;
        p.legBR = 0.2f * e;
        if (dragon) p.wingRoll = 0.9f * e;
    }
    return p;
}

// ---------------------------------------------------------------------------------------------- draw

void AnimalRenderer::Draw(const Npc& n, float time) {
    const SpeciesInfo& info = GetSpecies(n.species);
    const char* sp = info.model ? info.model : info.key;
    const auto cwIt = colorways_.find(sp);
    const int cw = cwIt != colorways_.end() ? n.colorway % cwIt->second : 0;
    auto J = [&](const char* part) {
        auto it = joints_.find(std::string(sp) + "." + part);
        return it != joints_.end() ? it->second : Joint{};
    };
    const std::string pre = std::string(sp) + "_" + std::to_string(cw) + "_";
    const bool chicken = info.rig == Rig::Bird;
    const bool goblin = info.rig == Rig::Biped;
    const bool spider = info.rig == Rig::Spider, dragon = info.rig == Rig::Dragon;
    const bool predator = spider || dragon || (info.rig == Rig::Quad && n.species != Species::Cow && n.species != Species::Calf);

    const Pose p = chicken ? AnimateChicken(n, time) : (goblin ? AnimateGoblin(n, time)
                   : (predator ? AnimatePredator(n, time, info.rig) : AnimateBovine(n, time, n.species == Species::Calf)));
    const Joint bj = J("body");

    // body transform: tilt about the body centre, nudge, then turn to face and place in the world
    Matrix body = MatrixTranslate(-bj.x, -bj.y, -bj.z);
    body = MatrixMultiply(body, MatrixRotateX(p.bodyPitch));
    body = MatrixMultiply(body, MatrixRotateZ(p.bodyRoll));
    body = MatrixMultiply(body, MatrixTranslate(bj.x, bj.y + p.offY, bj.z + p.offZ));
    const float grow = (n.dead ? 1.0f : Mix(0.35f, 1.0f, Smooth(n.spawnT / 0.9f))) * info.scale;
    body = MatrixMultiply(body, MatrixScale(grow, grow, grow));
    body = MatrixMultiply(body, MatrixRotateY(n.facing));
    body = MatrixMultiply(body, MatrixTranslate(n.x, 0.0f, n.z));

    // a soft blob shadow on the ground (not for corpses that have sunk into the grass)
    if (!n.dead || n.deathT < 1.5f) {
        const float r = (chicken ? 0.22f : (goblin ? 0.34f : std::min(1.6f, 0.46f * info.height))) * grow;
        DrawCylinder({n.x, 0.02f, n.z}, r, r, 0.01f, 14, Fade(BLACK, n.dead ? 0.2f : 0.26f));
    }

    DrawPart(pre + "body", J("body"), 0, 0, 0, body);
    if (goblin) {
        DrawPart(pre + "head", J("head"), p.headPitch, p.headYaw, p.headRoll, body);
        DrawPart(pre + "arm_l", J("arm_l"), p.armL, 0, p.spreadLegs * 0.5f, body);
        DrawPart(pre + "arm_r", J("arm_r"), p.armR, 0, -p.spreadLegs * 0.5f, body);
        DrawPart(pre + "leg_l", J("leg_l"), p.legL, 0, p.spreadLegs * 0.3f, body);
        DrawPart(pre + "leg_r", J("leg_r"), p.legR, 0, -p.spreadLegs * 0.3f, body);
    } else if (chicken) {
        DrawPart(pre + "head", J("head"), p.headPitch, p.headYaw, p.headRoll, body);
        const float spread = p.wingRoll;
        DrawPart(pre + "wing_l", J("wing_l"), 0, 0, spread, body);
        DrawPart(pre + "wing_r", J("wing_r"), 0, 0, -spread, body);
        DrawPart(pre + "leg_l", J("leg_l"), p.legL, 0, p.spreadLegs * 0.5f, body);
        DrawPart(pre + "leg_r", J("leg_r"), p.legR, 0, -p.spreadLegs * 0.5f, body);
    } else {
        DrawPart(pre + "head", J("head"), p.headPitch, p.headYaw, p.headRoll, body);
        if (!spider) DrawPart(pre + "tail", J("tail"), 0, 0, p.tailRoll, body);
        if (dragon) {
            DrawPart(pre + "wing_l", J("wing_l"), 0, 0, p.wingRoll, body);
            DrawPart(pre + "wing_r", J("wing_r"), 0, 0, -p.wingRoll, body);
        }
        DrawPart(pre + "leg_fl", J("leg_fl"), p.legFL, 0, p.spreadLegs * 0.15f, body);
        DrawPart(pre + "leg_fr", J("leg_fr"), p.legFR, 0, -p.spreadLegs * 0.15f, body);
        DrawPart(pre + "leg_bl", J("leg_bl"), p.legBL, 0, p.spreadLegs * 0.15f, body);
        DrawPart(pre + "leg_br", J("leg_br"), p.legBR, 0, -p.spreadLegs * 0.15f, body);
    }
}

}  // namespace journey
