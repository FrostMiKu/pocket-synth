#pragma once
#include <SDL.h>
namespace ps::ui {
inline constexpr int width=640,height=480,margin=24,right=width-margin;
inline constexpr int contentWidth=right-margin;
inline constexpr int titleY=20,infoY=52,headerLineY=74,footerLineY=444,footerTextY=457;
inline constexpr int detailX=336,detailWidth=right-detailX;
inline constexpr int headingY=124,activityY=380,activityHeight=6;
inline constexpr int wheelX=168,wheelY=256,wheelRadius=128,wheelInnerRadius=68,wheelLabelRadius=105;
inline constexpr int controlsY=400,hintLine=24,playHintY=424;
inline constexpr int stringTop=166,stringGap=38,stringNumberX=detailX;
inline constexpr int stringLeft=detailX+20,stringRight=548,stringPitchX=596;
inline constexpr int cardGap=16,cardWidth=(contentWidth-3*cardGap)/4;
inline constexpr int cardTop=124,cardHeight=120,cardRowGap=16;
inline constexpr SDL_Color background{15,24,35,255},foreground{222,233,239,255};
inline constexpr SDL_Color mint{88,225,182,255},muted{140,163,181,255},gold{255,202,111,255};
inline constexpr SDL_Color line{41,58,74,255},circle{43,64,82,255};
inline constexpr SDL_Color card{31,48,64,255},cardHeld{36,105,95,255},selected{36,68,75,255};
}
