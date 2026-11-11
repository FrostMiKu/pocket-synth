#include "guitar_ui.hpp"
#include "font.hpp"
#include "notation.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
float drawGuitarStrings(SDL_Renderer* r,const Settings& s,const Controller& c) {
    const Settings playing=c.performanceSettings(s);
    const auto& chord=chordBanks[s.chordBank][c.chord()];
    float strongest=0;
    for(int n=0;n<stringCount;++n) {
        const auto& strike=c.stringStrikes()[n];
        const double age=c.seconds()-strike.startsAt;
        const float level=strike.startsAt>=0&&age>=0
            ?strike.strength*std::exp(-float(age)*3.5f):0;
        strongest=std::max(strongest,level);
        const int y=ui::stringTop+n*ui::stringGap;
        const float amplitude=level*7;
        SDL_Color color{Uint8(86+2*level),Uint8(111+114*level),Uint8(130+52*level),255};
        SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
        const int thickness=1+(stringCount-1-n)/2;
        constexpr int left=ui::stringLeft,right=ui::stringRight,segments=44;
        auto wave=[&](float t) {
            return int(amplitude*std::sin(t*3.14159265f)*std::sin(t*9.424778f-float(age)*72));
        };
        for(int segment=0;segment<segments;++segment) {
            float a=float(segment)/segments,b=float(segment+1)/segments;
            for(int pixel=0;pixel<thickness;++pixel)
                SDL_RenderDrawLine(r,left+int((right-left)*a),y+wave(a)+pixel,
                                     left+int((right-left)*b),y+wave(b)+pixel);
        }
        text(r,ui::stringNumberX,y-3,std::to_string(stringCount-n),1,ui::muted);
        const int pitch=level>.015f?strike.note:guitarNote(chord,n,playing);
        drawNote(r,ui::stringPitchX,y+1,pitch,s,2,level>.015f?ui::mint:ui::muted);
        if(level>.03f) {
            SDL_SetRenderDrawColor(r,ui::mint.r,ui::mint.g,ui::mint.b,255);
            SDL_Rect pick{ui::stringLeft-8,y-3,3,8}; SDL_RenderFillRect(r,&pick);
        }
    }
    return strongest;
}
}
