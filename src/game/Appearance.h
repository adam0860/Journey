// What the player chose in character creation. Plain data + palettes + save/load.
#pragma once
#include <string>
#include <vector>

#include "raylib.h"

namespace journey {

struct Appearance {
    std::string name = "Adventurer";
    bool female = false;
    int skin = 2;
    int hair = 0, hairColor = 2;
    int shirt = 0, shirtColor = 7;
    int pants = 0, pantsColor = 3;
    int shoes = 1, shoesColor = 0;
};

// Colour palettes (sRGB, as you see them on screen). The renderer converts to linear for lighting.
const std::vector<Color>& SkinTones();
const std::vector<Color>& HairColors();
const std::vector<Color>& ClothColors();
const std::vector<Color>& ShoeColors();

// Clamp every index into range for the given catalog sizes (guards against old/edited saves).
void Sanitize(Appearance& a, int hairCount, int shirtCount, int pantsCount, int shoesCount);
void Randomize(Appearance& a, int hairCount, int shirtCount, int pantsCount, int shoesCount);

std::string Serialize(const Appearance& a);
bool Parse(const std::string& text, Appearance* out);

// Names: 2..12 letters/digits/spaces.
bool ValidName(const std::string& n);

}  // namespace journey
