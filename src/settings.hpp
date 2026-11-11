#pragma once
#include "music.hpp"
#include <string>
namespace ps {
Settings loadSettings(const std::string& path);
bool saveSettings(const std::string& path, const Settings&);
}
