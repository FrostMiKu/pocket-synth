#pragma once
#include "performance.hpp"
#include <string>
namespace ps {
Input demoInput(double seconds,Settings&,bool recordLoops=false);
bool renderSounds(const std::string& path);
bool renderDemo(const std::string& path,bool loops=false);
}
