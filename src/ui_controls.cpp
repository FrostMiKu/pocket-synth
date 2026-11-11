#include "ui_controls.hpp"
#include "sound_catalog.hpp"
#include "ui_draw.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
using namespace ui;
namespace {
void dpad(SDL_Renderer* r,int x,int y,Button direction,SDL_Color color,bool bothVertical=false) {
    fill(r,x+5,y,5,15,line); fill(r,x,y+5,15,5,line);
    const int dx=direction==Right?10:direction==Left?0:5;
    const int dy=direction==Down?10:direction==Up?0:5;
    fill(r,x+dx,y+dy,5,5,color);
    if(bothVertical) fill(r,x+5,y+10,5,5,color);
}
int action(SDL_Renderer* r,int x,int y,const std::string& key,const std::string& label,bool enabled=true) {
    text(r,x,y,key,1,mint);
    const int labelX=x+textSize(key,1).x+6;
    text(r,labelX,y,label,1,enabled?foreground:muted);
    return labelX+textSize(label,1).x;
}
}
void stickHint(SDL_Renderer* r,int x,int y,bool rightStick,const std::string& label) {
    outlineCircle(r,x+9,y+9,9,mint,24);
    centeredText(r,x+9,y+9,rightStick?"R":"L",1,mint);
    text(r,x+28,y+2,label,2,foreground);
}
void pitchHints(SDL_Renderer* r,int x,int y) {
    dpad(r,x,y,Up,mint,true);
    text(r,x+24,y+3,"HOLD OCTAVE",1,muted);
    dpad(r,x+130,y,Left,mint); text(r,x+154,y+3,"HOLD +1 SEMITONE",1,muted);
}
void drawFooter(SDL_Renderer* r,const Settings& s,const Controller& c,const Input& in,const LoopView& loop,float peak) {
    fill(r,margin,footerLineY,contentWidth,1,line);
    if(!in.ready) text(r,margin,footerTextY,"CENTER STICKS AND RELEASE BUTTONS",1,gold);
    else if(c.menu()) {
        text(r,margin,footerTextY,"D-PAD SELECT / CHANGE",1,muted);
        text(r,240,footerTextY,"START RETURN TO PLAY",1,mint);
    } else if(in.held[Right]) {
        const auto a=loopActions(loop,s.mode);
        const auto name=[&](Button b){return std::string(buttonNames[mappedButton(b,s)]);};
        constexpr int gap=8;
        int x=margin;
        x=action(r,x,footerTextY,name(A),a.play)+gap;
        x=action(r,x,footerTextY,name(B),a.record,a.canRecord)+gap;
        x=action(r,x,footerTextY,name(X),a.undo,a.canUndo)+gap;
        x=action(r,x,footerTextY,name(Y),a.clear,a.canClear)+gap;
        x=action(r,x,footerTextY,"L1/R1","BPM")+gap;
        x=action(r,x,footerTextY,"L2/R2","SOUND",soundCount(s.mode)>1)+gap;
        dpad(r,x,footerTextY-4,Up,mint,true);
        text(r,x+20,footerTextY,"OCTAVE",1,foreground);
    } else {
        int x=action(r,margin,footerTextY,"SELECT","MODE")+24;
        x=action(r,x,footerTextY,"START","SETTINGS")+24;
        dpad(r,x,footerTextY-4,Right,mint);
        text(r,x+20,footerTextY,"HOLD SHORTCUTS",1,mint);
    }
    constexpr int meterX=500,meterWidth=right-meterX;
    fill(r,meterX,footerTextY,meterWidth,7,line);
    fill(r,meterX,footerTextY,int(meterWidth*std::clamp(peak*4,0.f,1.f)),7,mint);
    const float clearing=c.clearProgress();
    std::string feedback=clearing>0&&clearing<1?"CLEARING TRACK":loopNotice(loop);
    if(!feedback.empty()&&!c.menu()) {
        fill(r,margin,391,contentWidth,48,background);
        if(loop.noticeTrack>=0&&!(clearing>0&&clearing<1)) feedback=std::string(modeNames[loop.noticeTrack])+"  "+feedback;
        centeredText(r,width/2,407,feedback,2,gold);
        if(clearing>0&&clearing<1) { fill(r,margin,432,contentWidth,3,line); fill(r,margin,432,int(contentWidth*clearing),3,gold); }
    }
}
}
