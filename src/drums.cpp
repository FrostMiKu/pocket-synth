#include "drums.hpp"
#include "Synthesis/oscillator.h"
#include <algorithm>
#include <cmath>
namespace ps {
const std::array<const char*,drumCount> drumNames={"KICK","SNARE","CLOSED HAT","OPEN HAT","CLAP","LOW TOM","HIGH TOM","CRASH"};
namespace {
// Original electronic one-shots, generated once before audio starts.
// Playback shares this immutable kit across live instruments and loop tracks.
struct Kit {
    std::array<std::vector<float>,drumCount> clips;
    Kit() {
        constexpr std::array<float,drumCount> durations={.55f,.34f,.10f,.65f,.32f,.55f,.4f,1.4f};
        uint32_t seed=0x93ab145d;
        for(int pad=0;pad<drumCount;++pad) {
            auto& clip=clips[pad]; clip.resize(size_t(durations[pad]*sampleRate));
            daisysp::Oscillator osc; osc.Init(sampleRate); osc.SetAmp(1);
            float low=0,metalPhase=0,peak=0;
            for(size_t n=0;n<clip.size();++n) {
                const float t=float(n)/sampleRate;
                seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
                const float noise=float(seed&0xffffff)/8388607.5f-1;
                low+=.16f*(noise-low); const float high=noise-low;
                float value=0;
                if(pad==0) {
                    osc.SetFreq(48+110*std::exp(-t*65));
                    value=osc.Process()*std::exp(-t*12)+high*std::exp(-t*300)*.3f;
                } else if(pad==1) {
                    osc.SetFreq(175+55*std::exp(-t*70));
                    value=osc.Process()*.4f*std::exp(-t*24)+high*.85f*std::exp(-t*19);
                } else if(pad==2||pad==3||pad==7) {
                    metalPhase+=1.f/sampleRate;
                    float metal=0;
                    for(float hz:{317.f,503.f,811.f,1297.f,1913.f,2909.f})
                        metal+=std::sin(metalPhase*hz*6.2831853f)>0?1.f:-1.f;
                    const float decay=pad==2?78.f:pad==3?10.f:5.5f;
                    value=(high*.65f+metal*.08f)*std::exp(-t*decay);
                    if(pad==7) value*=std::min(1.f,t*160);
                } else if(pad==4) {
                    float envelope=0;
                    for(float start:{0.f,.012f,.024f}) if(t>=start) envelope+=std::exp(-(t-start)*170);
                    value=high*(envelope*.55f+std::exp(-t*22)*.25f);
                } else {
                    osc.SetFreq((pad==5?100.f:155.f)+70*std::exp(-t*55));
                    value=osc.Process()*std::exp(-t*(pad==5?13.f:18.f))+high*.12f*std::exp(-t*90);
                }
                const float fade=std::min(1.f,float(clip.size()-n)/240);
                clip[n]=value*fade; peak=std::max(peak,std::abs(clip[n]));
            }
            constexpr std::array<float,drumCount> levels={.65f,.8f,.6f,.4f,.85f,.55f,.6f,.45f};
            const float level=levels[pad];
            for(float& sample:clip) sample*=level/std::max(peak,.01f);
        }
    }
};
const Kit& kit() { static const Kit k; return k; }
}
Drums::Drums() { (void)kit(); }
void Drums::hit(int pad,float velocity,float tone) {
    if(pad<0||pad>=drumCount) return;
    if(pad==2) for(auto& voice:voices_) if(voice.pad==3&&voice.active) voice.release=.975f;
    auto& voice=voices_[size_t(pad)*2+(next_[pad]++%2)];
    if(!voice.active) ++activeCount_;
    voice={0,pad,std::clamp(velocity,0.f,1.f),0,std::clamp(tone,0.f,1.f),1,true};
}
void Drums::silence() { for(auto& voice:voices_) if(voice.active) voice.release=.975f; }
float Drums::process() {
    float sum=0;
    for(auto& voice:voices_) if(voice.active) {
        const auto& clip=kit().clips[voice.pad];
        if(voice.cursor>=clip.size()||voice.gain<.00001f) { voice.active=false; --activeCount_; continue; }
        const float sample=clip[voice.cursor++];
        voice.low+=(.08f+voice.tone*.85f)*(sample-voice.low);
        sum+=voice.low*voice.gain;
        voice.gain*=voice.release;
    }
    return sum;
}
}
