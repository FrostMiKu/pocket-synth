#include "loop_ui.hpp"
#include "sound_catalog.hpp"
#include "loop_presentation.hpp"
#include "font.hpp"
#include "ui_controls.hpp"
#include "ui_draw.hpp"
#include <algorithm>
namespace ps {
using namespace ui;
void drawLoopStatus(SDL_Renderer* r,const Settings& s,const Settings& playing,const LoopView& loop,bool tracks) {
    const std::string sound=selectedSound(s,s.mode).name;
    std::string pitch=s.mode==Mode::Drums?sound:
        (s.noteDisplay==NoteDisplay::Numbered?"1=":"")+pitchClassName(60+s.root)+" / "+scaleNames[int(s.scale)]+" / OCTAVE "+std::to_string(playing.octave)+(playing.root!=s.root?" / +1":"");
    if(s.mode!=Mode::Drums) pitch+=" / "+sound;
    std::string tempo=std::to_string(loop.tempo);
    if(loop.requestedTempo!=loop.tempo) tempo+=">"+std::to_string(loop.requestedTempo);
    tempo+=" BPM";
    const std::string bars=loopPosition(loop);
    const std::string state=loopState(loop);
    text(r,margin,infoY,fitText(pitch,loopTempoX-margin-loopInfoGap),1,muted);
    text(r,loopTempoX,infoY,tempo,1,muted);
    text(r,loopPositionX,infoY,bars,1,muted);
    fill(r,loopStatusX,infoY-4,loopStatusWidth,15,selected);
    centeredText(r,loopStatusX+loopStatusWidth/2,infoY+3,state,1,loop.playing&&!loop.recording&&!loop.countBeat&&!loop.full&&!loop.savedUnavailable?mint:gold);
    fill(r,margin,headerLineY,contentWidth,2,line);
    if(loop.playing) fill(r,margin,headerLineY,int(contentWidth*loop.progress),2,loop.recording?gold:mint);
    if(!tracks) return;
    for(int mode=0;mode<modeCount;++mode) {
        const int x=margin+mode*152,w=contentWidth-3*152;
        const bool selectedMode=int(s.mode)==mode;
        fill(r,x,86,w,24,selectedMode?selected:card);
        text(r,x+8,90,modeNames[mode],1,loop.recordTrack==mode&&(loop.recording||loop.armed)?gold:loop.events[mode]?mint:muted);
        const std::string detail=mode==int(Mode::Drums)?(loop.events[mode]?"PATTERN":"EMPTY"):std::to_string(loop.layers[mode])+" / "+std::to_string(loopLayerCount)+" LAYERS";
        text(r,x+8,101,detail,1,muted);
    }
}
void drawRecordingCue(SDL_Renderer* r,const LoopView& loop) {
    const int beat=recordCountdownBeat(loop);
    const bool start=loop.recording&&loop.bar()==1&&loop.beat()==1;
    if(!beat&&!start) return;
    fill(r,margin,392,contentWidth,48,background);
    if(start) {
        const auto color=loop.beatProgress<.25f?gold:mint;
        centeredText(r,width/2,412,"RECORDING",3,color);
        fill(r,margin,392,contentWidth,2,color);
    } else {
        for(int n=1;n<=loopBeatsPerBar;++n) {
            const int x=margin+(2*n-1)*contentWidth/(2*loopBeatsPerBar);
            if(n==beat) fill(r,x-24,396,48,26,selected);
            centeredText(r,x,409,std::to_string(n),3,n==beat?gold:n<beat?mint:muted);
        }
        centeredText(r,width/2,433,"RECORD ON NEXT 1",1,gold);
    }
}
void drawDrumPanel(SDL_Renderer* r,const Settings& s,const Controller& c,const Input& in,const LoopView& loop) {
    const int page=loop.bar()-1;

    text(r,right-60,headingY+3,"BAR "+std::to_string(page+1)+" / "+std::to_string(loop.bars),1,muted);
    constexpr int gridX=128,cellWidth=30,top=152,rowHeight=29;
    for(int step=0;step<loopStepsPerBar;++step) {
        if(step%loopStepsPerBeat==0) text(r,gridX+step*cellWidth+10,136,std::to_string(step/loopStepsPerBeat+1),1,mint);
        for(int pad=0;pad<drumCount;++pad) {
            const int y=top+pad*rowHeight;
            const bool set=loop.drumSteps[pad]&(1u<<(page*loopStepsPerBar+step));
            const bool playhead=loop.playing&&!loop.countBeat&&loop.step%loopStepsPerBar==step;
            fill(r,gridX+step*cellWidth,y,cellWidth-3,rowHeight-4,set?mint:playhead?selected:card);
        }
    }
    for(int pad=0;pad<drumCount;++pad) {
        const bool held=c.performing(instrumentButton(size_t(pad),s),in);
        text(r,margin,top+pad*rowHeight+4,drumNames[pad],1,held?gold:muted);
        text(r,102,top+pad*rowHeight+4,buttonNames[instrumentButton(size_t(pad),s)],1,held?gold:mint);
    }
    stickHint(r,margin,controlsY,false,"DYNAMICS / TONE");
    text(r,margin,controlsY+hintLine,"UP/DOWN DYNAMICS   LEFT/RIGHT TONE",1,muted);
}
}
