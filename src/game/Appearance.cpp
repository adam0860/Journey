#include "game/Appearance.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace journey {

namespace {
Color Hex(unsigned v) { return {(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, 255}; }
int Clamp(int v, int n) { return n <= 0 ? 0 : std::max(0, std::min(v, n - 1)); }
}  // namespace

const std::vector<Color>& SkinTones() {
    static const std::vector<Color> v = {Hex(0xF6D9C4), Hex(0xEEBB98), Hex(0xE0A97C), Hex(0xC68E5F),
                                         Hex(0xA8714A), Hex(0x7E4F33), Hex(0x5A3623), Hex(0x3D2418)};
    return v;
}
const std::vector<Color>& HairColors() {
    static const std::vector<Color> v = {Hex(0x1C1A1A), Hex(0x3B2A20), Hex(0x5E3D28), Hex(0x7A4A2A), Hex(0x8E3B24), Hex(0xB4552A),
                                         Hex(0xD8B25A), Hex(0xECE0B8), Hex(0x9A9A9A), Hex(0x3A5FB0), Hex(0xD65A93), Hex(0x3C9A5A)};
    return v;
}
const std::vector<Color>& ClothColors() {
    static const std::vector<Color> v = {Hex(0xF2F2F2), Hex(0xE8DCC0), Hex(0xB8A06A), Hex(0x7A5230), Hex(0xC0392B), Hex(0xE07B2E), Hex(0xE8C547),
                                         Hex(0x4C9A4F), Hex(0x2A9D8F), Hex(0x3A6FC4), Hex(0x24376E), Hex(0x7B4BA8), Hex(0x7C8088), Hex(0x2A2A2E)};
    return v;
}
const std::vector<Color>& ShoeColors() {
    static const std::vector<Color> v = {Hex(0x7A4A2A), Hex(0x3E2616), Hex(0x222226), Hex(0xB8925A), Hex(0x7C8088), Hex(0xEDEDED), Hex(0xB02A2A), Hex(0x2F5FB0)};
    return v;
}

void Sanitize(Appearance& a, int hc, int sc, int pc, int shc) {
    a.skin = Clamp(a.skin, (int)SkinTones().size());
    a.hair = Clamp(a.hair, hc);
    a.hairColor = Clamp(a.hairColor, (int)HairColors().size());
    a.shirt = Clamp(a.shirt, sc);
    a.shirtColor = Clamp(a.shirtColor, (int)ClothColors().size());
    a.pants = Clamp(a.pants, pc);
    a.pantsColor = Clamp(a.pantsColor, (int)ClothColors().size());
    a.shoes = Clamp(a.shoes, shc);
    a.shoesColor = Clamp(a.shoesColor, (int)ShoeColors().size());
}

void Randomize(Appearance& a, int hc, int sc, int pc, int shc) {
    auto r = [](int n) { return n <= 0 ? 0 : GetRandomValue(0, n - 1); };
    a.female = GetRandomValue(0, 1) == 1;
    a.skin = r((int)SkinTones().size());
    a.hair = r(hc);
    a.hairColor = r((int)HairColors().size());
    a.shirt = r(sc);
    a.shirtColor = r((int)ClothColors().size());
    a.pants = r(pc);
    a.pantsColor = r((int)ClothColors().size());
    a.shoes = r(shc);
    a.shoesColor = r((int)ShoeColors().size());
}

std::string Serialize(const Appearance& a) {
    std::ostringstream o;
    o << "name=" << a.name << "\nfemale=" << (a.female ? 1 : 0) << "\nskin=" << a.skin << "\nhair=" << a.hair
      << "\nhairColor=" << a.hairColor << "\nshirt=" << a.shirt << "\nshirtColor=" << a.shirtColor << "\npants=" << a.pants
      << "\npantsColor=" << a.pantsColor << "\nshoes=" << a.shoes << "\nshoesColor=" << a.shoesColor << "\n";
    return o.str();
}

bool Parse(const std::string& text, Appearance* out) {
    Appearance a;
    std::istringstream in(text);
    std::string line;
    bool gotName = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        const int n = std::atoi(v.c_str());
        if (k == "name") { a.name = v; gotName = true; }
        else if (k == "female") a.female = n != 0;
        else if (k == "skin") a.skin = n;
        else if (k == "hair") a.hair = n;
        else if (k == "hairColor") a.hairColor = n;
        else if (k == "shirt") a.shirt = n;
        else if (k == "shirtColor") a.shirtColor = n;
        else if (k == "pants") a.pants = n;
        else if (k == "pantsColor") a.pantsColor = n;
        else if (k == "shoes") a.shoes = n;
        else if (k == "shoesColor") a.shoesColor = n;
    }
    if (!gotName || !ValidName(a.name)) return false;
    *out = a;
    return true;
}

bool ValidName(const std::string& n) {
    int visible = 0;
    if (n.size() > 12) return false;
    for (char c : n) {
        if (!(std::isalnum((unsigned char)c) || c == ' ')) return false;
        if (c != ' ') ++visible;
    }
    return visible >= 2;
}

}  // namespace journey
