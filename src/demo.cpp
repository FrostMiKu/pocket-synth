#include "demo.hpp"
#include "wave_file.hpp"
#include "audio_float.hpp"
#include "studio.hpp"
#include "loop_demo.hpp"
#include <memory>
#include <cmath>
namespace ps {
Input demoInput(double t,Settings& s,bool recordLoops) {
    Input i;
    if(t<4) {
        s.mode=Mode::Guitar;
        int chord=int(t); float angle=chord*6.2831853f/8;
        i.lx=std::sin(angle); i.ly=-std::cos(angle);
        double beat=t*4-int(t*4);
        i.ry=beat<.35?(int(t*4)%2?-.95f:.95f):0;
    } else if(t<8) {
        s.mode=Mode::Expressive; s.holdGate=false;
        float a=int((t-4)*2)*6.2831853f/8;
        i.rx=.85f*std::sin(a); i.ry=-.85f*std::cos(a);
        i.lx=std::sin(float(t)); i.ly=-.5;
    } else if(t<12) {
        s.mode=Mode::Keys;
        int step=int((t-8)*3);
        if((t*3-int(t*3))<.8) i.held[instrumentButton(size_t(step)%instrumentButtons.size(),s)]=true;

    }
    if(t>=12&&t<16) {
        s.mode=Mode::Drums;
        int step=int((t-12)*8);
        if(t*8-int(t*8)<.4) i.held[instrumentButton(size_t(step%4==0?0:step%4==2?1:2),s)]=true;
    }
    if(recordLoops&&t<16) {
        const double local=std::fmod(t,4.);
        if(local>=.5&&local<.55) i.held[Right]=i.held[mappedButton(B,s)]=true;
    }
    return i;
}
bool renderDemo(const std::string& path,bool loops) {
    configureAudioFloat();
    constexpr int seconds=18,frames=seconds*sampleRate;
    WaveFile out(path,frames);
    auto studio=std::make_unique<Studio>();
    if(loops) { prepareLoopDemo(studio->looper()); studio->prepareCaches(); }
    Controller controller; Settings s;
    constexpr int block=240; std::array<float,block*2> audio{};
    for(int pos=0;pos<frames;pos+=block) {
        double t=double(pos)/sampleRate; auto i=demoInput(t,s,loops);
        if(pos==16*sampleRate) { auto reset=controller.reset(); studio->apply(reset.data[0]); studio->apply({EventType::Loop,int(LoopCommand::Stop)}); }
        auto events=controller.update(i,s,t); studio->configure(s);
        for(size_t n=0;n<events.count;++n) studio->apply(events.data[n]);
        studio->render(audio.data(),block);
        out.write(audio.data(),block);
    }
    return out.finish();
}
}
