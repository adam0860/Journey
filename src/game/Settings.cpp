#include "game/Settings.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace journey {

std::string SerializeSettings(const Settings& s) {
    std::ostringstream out;
    out << "music=" << s.music << "\nsfx=" << s.sfx << "\n";
    return out.str();
}

Settings ParseSettings(const std::string& text) {
    Settings s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        char* end = nullptr;
        const std::string val = line.substr(eq + 1);
        const float v = std::strtof(val.c_str(), &end);
        if (end == val.c_str()) continue;
        const float c = std::clamp(v, 0.0f, 1.0f);
        if (key == "music") s.music = c;
        else if (key == "sfx") s.sfx = c;
    }
    return s;
}

}  // namespace journey
