#pragma once
#include "performance.hpp"
#include "string_level.hpp"
#include "drums.hpp"
#include "melody_voice.hpp"
#include "Control/adsr.h"
#include "PhysicalModeling/stringvoice.h"
#include <array>
namespace ps {
class Engine {
public:
    Engine();
    Event apply(Event);
    void render(float* interleaved, size_t frames,bool limit=true);
    bool active() const { return active_; }
    void setMode(Mode mode) { mode_=mode; }
    void setVolumeImmediate(float volume) { volume_=targetVolume_=volume; }
    void setVolume(float volume) { targetVolume_=volume; }
private:
    struct String {
        daisysp::StringVoice voice;
        daisysp::Adsr envelope;
        StringLevel level;
        float gain=0, note=60;
        bool gate=false;
    };
    struct Pending { Event event; uint64_t due=0; bool active=false; };
    std::array<String, guitarVoiceCount> strings_{};
    std::array<MelodyVoice, melodyVoiceCount> melody_{};
    struct Expression { float timbre=.5f,vibrato=0,bend=0; };
    std::array<Expression,melodyLayerCount> expression_{};
    std::array<Pending, 96> pending_{};
    bool active_=false;
    Drums drums_;
    Mode mode_=Mode::Guitar;
    uint64_t clock_=0, nextDue_=UINT64_MAX;
    float phase_=0, volume_=0, targetVolume_=.65f;
    void pluck(const Event&);
};
}
