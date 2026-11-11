#pragma once
#include "looper.hpp"
#include <SDL.h>
namespace ps {
void draw(SDL_Renderer*,const Settings&,const Controller&,const Input&,float peak,const LoopView& loop={});
bool screenshot(SDL_Renderer*,const char* path);
}
