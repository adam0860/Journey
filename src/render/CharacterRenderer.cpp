#include "render/CharacterRenderer.h"

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
                                 Color tint) {
    Model* m = Get(file);
    if (!m) return;
    const Joint& j = joints_[joint];
    Matrix t = MatrixRotateX(swing);
    t = MatrixMultiply(t, MatrixTranslate(j.x, j.y + bob, j.z));
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

    const Color skin = ToLinear(SkinTones()[a.skin]);
    const Color hair = ToLinear(HairColors()[a.hairColor]);
    const Color shirt = ToLinear(ClothColors()[a.shirtColor]);
    const Color pants = ToLinear(ClothColors()[a.pantsColor]);
    const Color shoes = ToLinear(ShoeColors()[a.shoesColor]);

    const StyleInfo* hs = a.hair < (int)catalog_.hair.size() ? &catalog_.hair[a.hair] : nullptr;
    const StyleInfo* ss = a.shirt < (int)catalog_.shirt.size() ? &catalog_.shirt[a.shirt] : nullptr;
    const StyleInfo* ps = a.pants < (int)catalog_.pants.size() ? &catalog_.pants[a.pants] : nullptr;
    const StyleInfo* fs = a.shoes < (int)catalog_.shoes.size() ? &catalog_.shoes[a.shoes] : nullptr;

    // forward swing is a negative rotation about +x; legs and arms alternate
    const float legL = -swing, legR = swing, armL = swing + sway, armR = -swing - sway;
    const std::string gs(1, g);

    // ---- body
    DrawPart("body_" + gs + "_torso", "torso", 0, bob, p, skin);
    DrawPart("body_" + gs + "_head", "head", 0, bob, p, skin);
    DrawPart("body_arm_l", "arm_l", armL, bob, p, skin);
    DrawPart("body_arm_r", "arm_r", armR, bob, p, skin);
    DrawPart("body_leg_l", "leg_l", legL, bob, p, skin);
    DrawPart("body_leg_r", "leg_r", legR, bob, p, skin);
    // ---- hair
    if (hs) DrawPart("hair_" + hs->id, "head", 0, bob, p, hair);
    // ---- shirt
    if (ss) {
        DrawPart("shirt_" + ss->id + "_torso_" + gs, "torso", 0, bob, p, shirt);
        if (ss->hasSleeves) {
            DrawPart("shirt_" + ss->id + "_sleeve_l", "arm_l", armL, bob, p, shirt);
            DrawPart("shirt_" + ss->id + "_sleeve_r", "arm_r", armR, bob, p, shirt);
        }
    }
    // ---- pants
    if (ps) {
        DrawPart("pants_" + ps->id + "_hips_" + gs, "torso", 0, bob, p, pants);
        if (ps->hasLegs) {
            DrawPart("pants_" + ps->id + "_leg_l", "leg_l", legL, bob, p, pants);
            DrawPart("pants_" + ps->id + "_leg_r", "leg_r", legR, bob, p, pants);
        }
    }
    // ---- shoes
    if (fs && fs->id != "none") {
        DrawPart("shoes_" + fs->id + "_l", "leg_l", legL, bob, p, shoes);
        DrawPart("shoes_" + fs->id + "_r", "leg_r", legR, bob, p, shoes);
    }
}

}  // namespace journey
