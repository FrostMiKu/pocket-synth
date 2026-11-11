#include "font.hpp"
#include <array>
namespace ps {
static constexpr int glyphWidth=5, glyphHeight=7, glyphAdvance=6, lineAdvance=9;
// Original 5x7 bitmap alphabet. Each row occupies five bits.
static const std::array<std::array<unsigned char,7>,46> glyphs={{
 {{14,17,17,31,17,17,17}},{{30,17,17,30,17,17,30}},{{14,17,16,16,16,17,14}},
 {{30,17,17,17,17,17,30}},{{31,16,16,30,16,16,31}},{{31,16,16,30,16,16,16}},
 {{14,17,16,23,17,17,15}},{{17,17,17,31,17,17,17}},{{14,4,4,4,4,4,14}},
 {{7,2,2,2,18,18,12}},{{17,18,20,24,20,18,17}},{{16,16,16,16,16,16,31}},
 {{17,27,21,21,17,17,17}},{{17,25,21,19,17,17,17}},{{14,17,17,17,17,17,14}},
 {{30,17,17,30,16,16,16}},{{14,17,17,17,21,18,13}},{{30,17,17,30,20,18,17}},
 {{15,16,16,14,1,1,30}},{{31,4,4,4,4,4,4}},{{17,17,17,17,17,17,14}},
 {{17,17,17,17,17,10,4}},{{17,17,17,21,21,27,17}},{{17,17,10,4,10,17,17}},
 {{17,17,10,4,4,4,4}},{{31,1,2,4,8,16,31}},
 {{14,17,19,21,25,17,14}},{{4,12,4,4,4,4,14}},{{14,17,1,2,4,8,31}},
 {{30,1,1,14,1,1,30}},{{2,6,10,18,31,2,2}},{{31,16,16,30,1,1,30}},
 {{14,16,16,30,17,17,14}},{{31,1,2,4,8,8,8}},{{14,17,17,14,17,17,14}},
 {{14,17,17,15,1,1,14}},{{0,0,0,31,0,0,0}},{{0,4,4,31,4,4,0}},
 {{1,2,2,4,8,8,16}},{{0,4,4,0,4,4,0}},{{10,10,31,10,31,10,10}},
 {{0,0,0,0,0,6,6}},{{0,0,0,0,0,0,0}},
 {{16,16,16,22,25,17,30}},{{0,0,31,0,31,0,0}},{{16,8,4,2,4,8,16}}
}};
void text(SDL_Renderer* r,int x,int y,const std::string& value,int size,SDL_Color c) {
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); int start=x;
    for(char ch:value) {
        if(ch=='\n') { x=start; y+=lineAdvance*size; continue; }
        int index=42;
        if(ch=='b') index=43;
        else if(ch>='a'&&ch<='z') ch=char(ch-'a'+'A');
        if(ch=='=') index=44;
        if(ch=='>') index=45;
        if(ch>='A'&&ch<='Z') index=ch-'A';
        if(ch>='0'&&ch<='9') index=26+ch-'0';
        const std::string symbols="-+/:#.";
        auto p=symbols.find(ch); if(p!=std::string::npos) index=36+int(p);
        for(int row=0;row<glyphHeight;++row) for(int col=0;col<glyphWidth;++col)
            if(glyphs[index][row]&(1<<(glyphWidth-1-col))) { SDL_Rect rect{x+col*size,y+row*size,size,size}; SDL_RenderFillRect(r,&rect); }
        x+=glyphAdvance*size;
    }
}
SDL_Point textSize(const std::string& value,int size) {
    return {value.empty()?0:(int(value.size())*glyphAdvance-(glyphAdvance-glyphWidth))*size,glyphHeight*size};
}
std::string fitText(const std::string& value,int maxWidth,int size) {
    if(textSize(value,size).x<=maxWidth) return value;
    std::string clipped=value;
    while(!clipped.empty()&&textSize(clipped+"...",size).x>maxWidth) clipped.pop_back();
    return textSize(clipped+"...",size).x<=maxWidth?clipped+"...":"";
}
void centeredText(SDL_Renderer* r,int cx,int cy,const std::string& value,int size,SDL_Color color) {
    const auto bounds=textSize(value,size);
    text(r,cx-bounds.x/2,cy-bounds.y/2,value,size,color);
}

}
