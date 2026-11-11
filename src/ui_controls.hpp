#pragma once
#include "loop_presentation.hpp"
#include "font.hpp"
namespace ps {
void stickHint(SDL_Renderer*,int x,int y,bool rightStick,const std::string& label);
void pitchHints(SDL_Renderer*,int x,int y);
void drawFooter(SDL_Renderer*,const Settings&,const Controller&,const Input&,const LoopView&,float peak);
}
