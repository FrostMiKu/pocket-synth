#pragma once
#include "sound_catalog.hpp"
#include "Control/adsr.h"
namespace ps {
enum class PitchTransition { Stepped, Legato };
class MelodyVoice {
public:
    MelodyVoice();
    void noteOn(float note,float velocity,const SoundPreset&,PitchTransition);
    void noteOff(bool fast=false);
    void transpose(float delta) { note_+=delta; pitch_+=delta; }
    float process(float timbre,float bend,float vibrato);
    bool active() const { return envelope_.IsRunning(); }
    SoundId sound() const { return preset_?preset_->id:0; }
private:
    const SoundPreset* preset_=nullptr;
    daisysp::Adsr envelope_;
    std::array<float,soundPartials> levels_{},decays_{};
    float note_=60,pitch_=60,velocity_=1,targetVelocity_=1,phase_=0,lastPitch_=-1000,increment_=0;
    float normalization_=1,noiseLow_=0,attackNoise_=1,fmDepth_=0;
    size_t partialCount_=0;
    static constexpr int transitionFrames=64;
    float lastOutput_=0,startOutput_=0;
    int transitionLeft_=0;
    PitchTransition transition_=PitchTransition::Stepped;
    bool held_=false;
    uint32_t noiseState_=0x7133af5d;
};
}
