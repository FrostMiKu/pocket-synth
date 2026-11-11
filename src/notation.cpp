#include "notation.hpp"
#include "font.hpp"
#include <cstdlib>
namespace ps {
void drawNote(SDL_Renderer* r,int cx,int cy,int midi,const Settings& s,int size,SDL_Color color) {
    const auto label=noteLabel(midi,s);
    centeredText(r,cx,cy,label.text,size,color);
    if(label.octaveDots==0) return;
    const int dotX=cx+(textSize(label.text,size).x-textSize(label.text.substr(label.text.size()-1),size).x)/2;
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    const int direction=label.octaveDots>0?-1:1;
    const int halfHeight=textSize(label.text,size).y/2;
    for(int n=0;n<std::abs(label.octaveDots);++n) {
        const int y=cy+direction*(halfHeight+(size+1)*(n+1));
        SDL_Rect dot{dotX-size/2,y-size/2,size,size}; SDL_RenderFillRect(r,&dot);
    }
}
}
