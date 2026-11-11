#pragma once
#include "ui_style.hpp"
#include <cmath>
namespace ps::ui {
inline void fill(SDL_Renderer* r,int x,int y,int w,int h,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    SDL_Rect rect{x,y,w,h}; SDL_RenderFillRect(r,&rect);
}
inline void outlineCircle(SDL_Renderer* r,int x,int y,int radius,SDL_Color color,int segments=96) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    for(int n=0;n<segments;++n) {
        const float a=n*6.2831853f/segments,b=(n+1)*6.2831853f/segments;
        SDL_RenderDrawLine(r,x+int(std::cos(a)*radius),y+int(std::sin(a)*radius),x+int(std::cos(b)*radius),y+int(std::sin(b)*radius));
    }
}
}
