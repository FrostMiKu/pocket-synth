#pragma once
#include "ui_style.hpp"
#include <string>
namespace ps {
std::string fitText(const std::string&,int maxWidth,int size=1);
SDL_Point textSize(const std::string&,int size=2);
void centeredText(SDL_Renderer*,int centerX,int centerY,const std::string&,int size=2,SDL_Color color=ui::foreground);
void text(SDL_Renderer*,int x,int y,const std::string&,int size=2,SDL_Color color=ui::foreground); }
