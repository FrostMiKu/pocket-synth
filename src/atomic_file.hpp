#pragma once
#include <cstdio>
#include <functional>
#include <string>
namespace ps { bool atomicWrite(const std::string&,const std::function<bool(FILE*)>&); }
