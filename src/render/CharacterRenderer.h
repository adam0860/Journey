// Draws the modular character: body + hair + shirt + pants + shoes, each a small glTF in pivot space,
// tinted with the colours the player picked and animated as rigid parts (walk swing, bob, breathing).
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/Appearance.h"
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

struct CharacterPose {
    float x = 0, z = 0;      // world position (tile units)
    float facing = 0;        // radians, 0 = facing +z
    float phase = 0;         // walk phase
    bool moving = false;
    float time = 0;          // for idle breathing
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
    void DrawPart(const std::string& file, const char* joint, float swing, float bob, const CharacterPose& p, Color tint);

    std::string dir_;
    const LitShader* shader_ = nullptr;
    CharacterCatalog catalog_;
    std::map<std::string, Joint> joints_;
    std::map<std::string, std::unique_ptr<Model>> models_;
    TextureCache textures_;
};

}  // namespace journey
