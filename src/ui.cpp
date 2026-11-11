#include "ui.hpp"
#include "product.hpp"
#include "sound_catalog.hpp"
#include "font.hpp"
#include "notation.hpp"
#include "guitar_ui.hpp"
#include "guitar_controls.hpp"
#include "loop_ui.hpp"
#include "ui_controls.hpp"
#include "ui_draw.hpp"
#include <cmath>
#include <algorithm>
namespace ps {
using namespace ui;
static void activityBar(SDL_Renderer* r,float amount) {
    fill(r,detailX,activityY,detailWidth,activityHeight,line);
    fill(r,detailX,activityY,int(detailWidth*std::clamp(amount,0.f,1.f)),activityHeight,mint);
}
static void settingsPanel(SDL_Renderer* r,const Settings& s,const Controller& c) {
    const std::array<std::string,settingCount> values={pitchClassName(60+s.root),scaleNames[int(s.scale)],std::to_string(s.octave),
      std::to_string(int(s.volume*100))+" / 100",s.continuousPitch?"CONTINUOUS":"SCALE WHEEL",s.holdGate?std::string("HOLD ")+buttonNames[expressionGateButton]:"STICK TO PLAY",s.noteDisplay==NoteDisplay::Names?"NOTE NAMES":"NUMBERED",s.swapFaceButtons?"SWAP AB / XY":"STANDARD",std::to_string(s.tempo)+" BPM",std::to_string(s.loopBars),s.quantize==Quantize::All?"ALL":s.quantize==Quantize::Off?"OFF":"DRUMS"};
    const std::array<const char*,settingCount> labels={"ROOT","SCALE","OCTAVE","VOLUME","PITCH MODE","EXPRESSION GATE","NOTE DISPLAY","FACE BUTTONS","TEMPO","LOOP BARS","QUANTIZE"};
    constexpr int rowTop=100,rowHeight=36;
    const int first=std::clamp(c.menuRow()-3,0,std::max(0,settingCount-8));
    for(int n=first;n<std::min(first+8,settingCount);++n) {
        int top=rowTop+(n-first)*rowHeight;
        if(n==c.menuRow()) fill(r,margin,top,contentWidth,rowHeight-2,selected);
        text(r,margin+16,top+10,labels[n],2,n==c.menuRow()?mint:muted);
        text(r,detailX,top+10,values[n],2);
    }
    text(r,right-40,92,std::to_string(c.menuRow()+1)+"/"+std::to_string(settingCount),1,muted);

}
static void noteWheel(SDL_Renderer* r,const Settings& s,const Settings& playing,const Controller& c) {
    int active=s.mode==Mode::Guitar?c.chord():c.wheel();
    outlineCircle(r,wheelX,wheelY,wheelRadius,circle);
    outlineCircle(r,wheelX,wheelY,wheelInnerRadius,circle);
    for(int n=0;n<wheelSize;++n) {
        float a=n*6.2831853f/8;
        int x=wheelX+int(std::sin(a)*wheelLabelRadius),y=wheelY-int(std::cos(a)*wheelLabelRadius);
        if(s.mode==Mode::Guitar) centeredText(r,x,y,chordLabel(chordBanks[s.chordBank][n],s,playing.root-s.root),2,n==active?mint:muted);
        else drawNote(r,x,y,scaleNote(n,playing),s,2,n==active?mint:muted);
    }
    if(s.mode==Mode::Guitar) centeredText(r,wheelX,wheelY,chordLabel(chordBanks[s.chordBank][active],s,playing.root-s.root),3,mint);
    else if(active>=0) drawNote(r,wheelX,wheelY,int(std::lround(c.previewPitch())),s,3,mint);
    else centeredText(r,wheelX,wheelY,"READY",2,mint);
}
static void keysPanel(SDL_Renderer* r,const Settings& s,const Settings& playing,const Controller& c,const Input& in) {
    for(size_t n=0;n<instrumentButtons.size();++n) {
        int x=margin+int(n%4)*(cardWidth+cardGap);
        int y=cardTop+int(n/4)*(cardHeight+cardRowGap);
        fill(r,x,y,cardWidth,cardHeight,c.performing(instrumentButton(n,s),in)?cardHeld:card);
        centeredText(r,x+cardWidth/2,y+26,buttonNames[instrumentButton(n,s)],2,mint);
        drawNote(r,x+cardWidth/2,y+79,scaleNote(int(n),playing),s,4,foreground);
    }
    stickHint(r,margin,controlsY,false,"BEND / VIBRATO");
    pitchHints(r,margin,playHintY);
}
void draw(SDL_Renderer* r,const Settings& s,const Controller& c,const Input& in,float peak,const LoopView& loop) {
    const Settings playing=c.performanceSettings(s);
    SDL_SetRenderDrawColor(r,background.r,background.g,background.b,background.a); SDL_RenderClear(r);
    text(r,margin,titleY,product::name,2,mint);
    const std::string panel=modeNames[int(s.mode)];
    const int panelX=right-textSize(panel,2).x;
    text(r,panelX,titleY,panel,2);
    drawLoopStatus(r,s,playing,loop,!c.menu());
    if(c.menu()) settingsPanel(r,s,c);
    else if(s.mode==Mode::Drums) drawDrumPanel(r,s,c,in,loop);
    else if(s.mode==Mode::Keys) keysPanel(r,s,playing,c,in);
    else {
        noteWheel(r,s,playing,c);
        if(s.mode==Mode::Guitar) {
            text(r,detailX,headingY,c.stroke()>0?"DOWNSTROKE":c.stroke()<0?"UPSTROKE":"PICK / STRUM",2,gold);
            activityBar(r,drawGuitarStrings(r,s,c));
            stickHint(r,margin,controlsY,false,"CHORD");
            stickHint(r,190,controlsY,true,"STRUM");
            text(r,detailX,controlsY+3,std::string(buttonNames[guitarBankButton])+" CHORD BANK   "+buttonNames[guitarMuteButton]+" MUTE",1,muted);
            pitchHints(r,margin,playHintY);
        } else {
            text(r,detailX,headingY,s.continuousPitch?"CONTINUOUS PITCH":"SCALE WHEEL",2,gold);
            stickHint(r,detailX,216,true,"NOTE");
            stickHint(r,detailX,252,false,"TONE / VIBRATO");
            text(r,detailX,300,s.holdGate?std::string("HOLD ")+buttonNames[expressionGateButton]+" TO PLAY":"CENTER TO REST",2,gold);
            activityBar(r,std::hypot(in.rx,in.ry));
            pitchHints(r,margin,playHintY);
        }
    }
    drawFooter(r,s,c,in,loop,peak);
}
bool screenshot(SDL_Renderer* r,const char* path) {
    auto* s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);
    if(!s) return false;
    bool ok=SDL_RenderReadPixels(r,nullptr,s->format->format,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path)==0;
    SDL_FreeSurface(s); return ok;
}
}
