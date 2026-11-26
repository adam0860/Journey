// The player's settings (volumes), kept in their own small file apart from the saved game. Pure C++ (unit tested).
#pragma once
#include <string>

namespace journey {

struct Settings {
    float music = 0.5f;   // 0..1
    float sfx = 0.8f;     // 0..1
};

std::string SerializeSettings(const Settings& s);
// Reads what SerializeSettings wrote. Bad or unknown lines are ignored and values are clamped to 0..1.
Settings ParseSettings(const std::string& text);

}  // namespace journey
