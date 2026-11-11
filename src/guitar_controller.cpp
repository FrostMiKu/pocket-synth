#include "guitar_controls.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
void Controller::playGuitar(const Input& raw,const Input& performance,Settings& saved,
                            const Settings& playing,double now,float dt,Events& out) {
    auto pressed=[&](Button button){ return performance.held[button]&&!previous_.held[button]; };
    if(pressed(guitarBankButton)) saved.chordBank=1-saved.chordBank;
    const bool selecting=std::hypot(raw.lx,raw.ly)>(chordStickActive_?.30f:.42f);
    if(selecting) chord_=stableSector(raw.lx,raw.ly,chordStickActive_?chord_:-1);
    chordStickActive_=selecting;
    if(pressed(guitarMuteButton)) { out=reset(); readPitchModifiers(raw); }
    if(performance.held[guitarMuteButton]) { armed_=false; return; }
    const auto& chord=chordBanks[saved.chordBank][chord_];
    auto pluck=[&](int string,float velocity,uint32_t delay) {
        Event e; e.type=EventType::Pluck; e.id=string;
        e.note=float(guitarNote(chord,string,playing));
        e.velocity=velocity; e.timbre=.2f+.7f*velocity; e.delay=delay;
        out.add(e);
        stringStrikes_[string]={int(e.note),velocity,now+double(delay)/sampleRate};
    };
    std::array<bool,stringCount> picked{};
    for(int string=0;string<stringCount;++string) {
        picked[string]=pressed(guitarStringButton(size_t(string),saved));
        if(picked[string]) {
            stroke_=0; strength_=.7f;
            pluck(string,strength_,0);
        }
    }
    if(std::abs(raw.ry)<stickNeutral) armed_=true;
    if(armed_&&std::abs(raw.ry)>.58f) {
        armed_=false; stroke_=raw.ry>0?1:-1;
        strength_=std::clamp(std::abs(raw.ry-previous_.ry)/dt/18.f,.15f,1.f);
        const float interval=.018f-.014f*strength_;
        for(int n=0;n<stringCount;++n) {
            const int string=stroke_>0?n:stringCount-1-n;
            if(!picked[string]) pluck(string,strength_,uint32_t(n*interval*sampleRate));
        }
    }
}
}
