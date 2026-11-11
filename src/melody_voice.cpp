#include "melody_voice.hpp"
#include "Synthesis/sine_lookup.h"
#include <algorithm>
#include <cmath>
namespace ps {
MelodyVoice::MelodyVoice() { envelope_.Init(sampleRate); }
void MelodyVoice::noteOn(float note,float velocity,const SoundPreset& preset,PitchTransition transition) {
    note_=note; targetVelocity_=velocity;
    if(held_) return;
    startOutput_=lastOutput_; transitionLeft_=active()?transitionFrames:0;
    transition_=transition;
    preset_=&preset; pitch_=note; velocity_=velocity; phase_=0; lastPitch_=-1000;
    held_=true; attackNoise_=1; fmDepth_=preset.fmIndex*(.3f+.7f*velocity);
    envelope_.SetAttackTime(preset.attack); envelope_.SetDecayTime(preset.decay);
    envelope_.SetSustainLevel(preset.sustain); envelope_.SetReleaseTime(preset.release);
    envelope_.Retrigger(true);
    float energy=0; partialCount_=0;
    for(size_t n=0;n<soundPartials;++n) {
        levels_[n]=preset.partials[n]; energy+=levels_[n]*levels_[n];
        if(levels_[n]!=0) partialCount_=n+1;
        decays_[n]=std::exp(-(1+float(n)*.65f)/(preset.tailSeconds*sampleRate));
    }
    normalization_=1/std::sqrt(std::max(energy,.001f));
}
void MelodyVoice::noteOff(bool fast) {
    held_=false;
    if(active()) { envelope_.SetReleaseTime(fast?silenceReleaseSeconds:preset_->release); envelope_.Release(); }
}
float MelodyVoice::process(float timbre,float bend,float vibrato) {
    if(!preset_||!active()) return 0;
    const float envelope=envelope_.Process(held_);
    if(!active()) { lastOutput_=0; transitionLeft_=0; return 0; }
    if(transition_==PitchTransition::Stepped) pitch_=note_;
    else pitch_+=(note_-pitch_)*.0015f; velocity_+=(targetVelocity_-velocity_)*.003f;
    const float pitch=pitch_+bend+vibrato;
    if(pitch!=lastPitch_) { lastPitch_=pitch; increment_=std::min(frequency(pitch)/sampleRate,.45f); }
    using daisysp::SineLookup;
    const float sine=SineLookup(phase_),cosine=SineLookup(phase_+.25f);
    float previous=0,current=sine,tone=0;
    for(size_t n=0;n<partialCount_;++n) {
        const float antialias=std::clamp((.49f-increment_*float(n+1))/.04f,0.f,1.f);
        const float brightness=n?(.6f+.8f*timbre):1;
        tone+=current*levels_[n]*antialias*brightness;
        const float next=2*cosine*current-previous; previous=current; current=next;
        if(preset_->model==SoundModel::Struck) levels_[n]*=decays_[n];
    }
    if(preset_->fmIndex>0) {
        const float fm=SineLookup(phase_+fmDepth_*SineLookup(phase_*2)/6.2831853f);
        tone+=(fm-sine)*levels_[0]; fmDepth_*=.99998f;
    }
    if(preset_->noise>0) {
        noiseState_^=noiseState_<<13; noiseState_^=noiseState_>>17; noiseState_^=noiseState_<<5;
        const float noise=float(noiseState_&0xffffff)/8388607.5f-1;
        noiseLow_+=.12f*(noise-noiseLow_);
        tone+=noiseLow_*preset_->noise*(preset_->model==SoundModel::Struck?attackNoise_:1);
    }
    attackNoise_=attackNoise_>.00001f?attackNoise_*.997f:0;
    phase_+=increment_; if(phase_>=1) phase_-=1;
    float output=tone*normalization_*envelope*velocity_*.13f;
    if(transitionLeft_>0) {
        const float blend=1-float(transitionLeft_--)/transitionFrames;
        output=startOutput_+(output-startOutput_)*blend;
    }
    lastOutput_=output; return output;
}
}
