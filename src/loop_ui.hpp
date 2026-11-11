#pragma once
#include "looper.hpp"
#include <SDL.h>
namespace ps {
void drawLoopStatus(SDL_Renderer*,const Settings&,const Settings& playing,const LoopView&,bool tracks=true);
void drawRecordingCue(SDL_Renderer*,const LoopView&);
void drawDrumPanel(SDL_Renderer*,const Settings&,const Controller&,const Input&,const LoopView&);
}
