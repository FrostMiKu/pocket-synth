#include "engine.hpp"
#include "audio_math.hpp"
#include "Synthesis/sine_lookup.h"
#include <cmath>
#include <algorithm>
namespace ps {

Engine::Engine() {
    (void)frequency(69); // Prime the immutable pitch table before audio starts.
    for(auto& s:strings_) {
        s.voice.Init(sampleRate); s.voice.SetStructure(.18f);
        s.envelope.Init(sampleRate);
        s.envelope.SetAttackTime(.001f);
        s.envelope.SetSustainLevel(0);
        s.envelope.SetReleaseTime(silenceReleaseSeconds);
    }

}
void Engine::pluck(const Event& e) {
    active_=true;
    auto& s=strings_[e.id];
    s.note=e.note; s.voice.SetFreq(frequency(s.note)); s.voice.SetAccent(e.velocity);
    const auto& preset=resolveSound(e.sound,Mode::Guitar);
    s.voice.SetStructure(preset.structure);
    s.voice.SetBrightness(std::clamp(e.timbre+preset.brightness-.5f,0.f,1.f)); s.voice.SetDamping(preset.damping);
    if(!s.envelope.IsRunning()) { s.voice.Reset(); s.level.reset(s.note); }
    s.voice.Trig(); s.gain=.3f+.7f*e.velocity; s.gate=true;
    s.envelope.SetAttackTime(preset.attack);
    s.envelope.SetDecayTime(preset.decay*(2.f+e.velocity)/3);
    s.envelope.Retrigger(false);
}
Event Engine::apply(Event e) {
    if(e.type==EventType::Drum) { const auto& preset=resolveSound(e.sound,Mode::Drums); drums_.hit(e.id,e.velocity,std::clamp(e.timbre+preset.brightness-.5f,0.f,1.f)); active_=true; return e; }
    if(e.type==EventType::LayerStop) {
        const int first=(e.id/stringCount)*stringCount;
        if(first<0||first>=guitarVoiceCount) return e;
        for(int n=first;n<first+stringCount;++n) {
            auto& s=strings_[n]; s.gate=false; if(s.envelope.IsRunning()) s.envelope.Release();
        }
        nextDue_=UINT64_MAX;
        for(auto& p:pending_) if(p.active) {
            if(p.event.id>=first&&p.event.id<first+stringCount) p.active=false;
            else nextDue_=std::min(nextDue_,p.due);
        }
    } else if(e.type==EventType::Silence) {
        drums_.silence();
        for(auto& p:pending_) p.active=false;
        nextDue_=UINT64_MAX;
        for(auto& s:strings_) { s.gate=false; if(s.envelope.IsRunning()) s.envelope.Release(); }
        for(auto& m:melody_) m.noteOff(true);
    } else if(e.type==EventType::Pluck&&e.id>=0&&e.id<guitarVoiceCount) {
        if(e.delay==0) pluck(e);
        else for(auto& p:pending_) if(!p.active) { active_=true; p={e,clock_+e.delay,true}; nextDue_=std::min(nextDue_,p.due); break; }
    } else if(e.type==EventType::Expression) {
        if(e.id>=0&&e.id<melodyVoiceCount) expression_[e.id/melodyNotesPerLayer]={e.timbre,e.vibrato,e.bend};
    } else if(e.id>=0&&e.id<int(melody_.size())) {
        auto& m=melody_[e.id];
        if(e.type==EventType::NoteOn) {
            active_=true;
            m.noteOn(e.note,e.velocity,resolveSound(e.sound,mode_),
                mode_==Mode::Keys?PitchTransition::Stepped:PitchTransition::Legato); e.sound=m.sound();
        } else if(e.type==EventType::NoteOff) m.noteOff();
    }
    return e;
}
void Engine::render(float* out, size_t frames,bool limit) {
    std::array<bool,guitarVoiceCount> stringActive{};
    for(size_t n=0;n<strings_.size();++n) stringActive[n]=strings_[n].gate||strings_[n].envelope.IsRunning();
    for(const auto& pending:pending_) if(pending.active) stringActive[size_t(pending.event.id)]=true;
    std::array<size_t,guitarVoiceCount> stringIndices{}; size_t stringVoices=0;
    for(size_t n=0;n<stringActive.size();++n) if(stringActive[n]) stringIndices[stringVoices++]=n;
    std::array<size_t,melodyVoiceCount> melodyIndices{}; size_t melodyVoices=0;
    for(size_t n=0;n<melody_.size();++n) if(melody_[n].active()) melodyIndices[melodyVoices++]=n;
    for(size_t f=0;f<frames;++f,++clock_) {
        if(clock_>=nextDue_) {
            nextDue_=UINT64_MAX;
            for(auto& p:pending_) if(p.active) {
                if(p.due<=clock_) { pluck(p.event); p.active=false; }
                else nextDue_=std::min(nextDue_,p.due);
            }
        }
        float left=0,right=0; bool sounding=false;
        for(size_t voice=0;voice<stringVoices;++voice) {
            const size_t n=stringIndices[voice]; auto& s=strings_[n];
            if(!s.gate&&!s.envelope.IsRunning()) continue;
            const float envelope=s.envelope.Process(s.gate);
            if(!s.envelope.IsRunning()) { s.gate=false; continue; }
            sounding=true;
            float v=s.level.process(s.voice.Process())*envelope*s.gain*.21f;
            float pan=.2f+.6f*float(n%stringCount)/(stringCount-1);
            left+=v*(1-pan); right+=v*pan;
        }
        phase_+=5.2f/sampleRate; if(phase_>=1) phase_-=1;
        for(size_t voice=0;voice<melodyVoices;++voice) {
            const size_t index=melodyIndices[voice]; auto& m=melody_[index]; const auto& expression=expression_[index/melodyNotesPerLayer];
            if(!m.active()) continue;
            const float modulation=expression.vibrato>0?daisysp::SineLookup(phase_)*expression.vibrato*.3f:0;
            const float v=m.process(expression.timbre,expression.bend,modulation);
            sounding=sounding||m.active();
            left+=v; right+=v;
        }
        const float drum=drums_.active()?drums_.process()*.42f:0; left+=drum; right+=drum;
        active_=sounding||drums_.active()||nextDue_!=UINT64_MAX;
        volume_+=(targetVolume_-volume_)*.002f;
        out[f*2]=limit?softLimit(left*volume_):left*volume_;
        out[f*2+1]=limit?softLimit(right*volume_):right*volume_;
    }
}
}
