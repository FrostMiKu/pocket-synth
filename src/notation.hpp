#pragma once
#include "music.hpp"
#include <SDL.h>
namespace ps {
void drawNote(SDL_Renderer*,int centerX,int centerY,int midi,const Settings&,int size,SDL_Color);
}
