#include "studio.hpp"
#include "loop_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace ps;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) {
    ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
}
void advance(Looper& loop,int frames) {
    for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) {} loop.advance(); }
}
Event attack(Mode mode) {
    Event e; e.type=mode==Mode::Drums?EventType::Drum:mode==Mode::Guitar?EventType::Pluck:EventType::NoteOn;
    e.note=60; e.velocity=.8f; e.gesture=1; return e;
}
void cues() {
    for(int tempo:{60,120,240}) for(int bars:{1,2}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Drums; s.tempo=tempo; s.loopBars=bars;
        loop->configure(s); loop->command(LoopCommand::Record,s.mode); const int beatFrames=sampleRate*60/tempo;
        for(int beat=1;beat<=4;++beat) {
            bool accent=true;
            check(recordCountdownBeat(loop->view())==beat,"the visual lead-in counts four beats at the song tempo");
            check(loop->takeClick(accent)&&!accent,"each count-in beat produces a regular click");
            check(!loop->takeClick(accent),"a beat produces one audible cue");
            advance(*loop,beatFrames);
        }
        check(loop->view().recording&&recordCountdownBeat(loop->view())==0,"the next beat starts recording after the four-beat lead-in");
        bool accent=false;
        check(loop->takeClick(accent)&&accent,"the recording downbeat has a distinct accented click");
        check(loop->view().beatProgress<.001f,"the visual pulse starts at the recording downbeat");
        advance(*loop,beatFrames/2);
        check(loop->view().beatProgress>.49f&&loop->view().beatProgress<.51f,"the visual pulse follows the audio clock between beats");
        advance(*loop,beatFrames-beatFrames/2);
        check(loop->takeClick(accent)&&!accent,"recording retains regular beat clicks after its downbeat");
        loop->command(LoopCommand::Stop,s.mode); check(!loop->takeClick(accent),"stop ends the recording metronome");
    }
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; loop->configure(s);
    loop->command(LoopCommand::Play,Mode::Drums); advance(*loop,1000);
    loop->command(LoopCommand::Record,Mode::Drums); bool accent=false;
    check(!loop->takeClick(accent),"arming in the middle of a beat waits for the next beat click");
    advance(*loop,11000);
    check(loop->takeClick(accent)&&recordCountdownBeat(loop->view())==2,"an overdub has audible and visual cues before the next loop");
}
void earlyAttacks() {
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys,Mode::Drums}) for(int earlyMs:{1,20,49,60}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.mode=mode; s.tempo=240; s.quantize=Quantize::Off;
        loop->configure(s); loop->command(LoopCommand::Record,mode);
        const int leadFrames=sampleRate*earlyMs/1000;
        advance(*loop,sampleRate-leadFrames); loop->record(mode,attack(mode));
        advance(*loop,leadFrames);
        check(loop->view().recording,"an early attack preserves the count-in timing");
        advance(*loop,2400);
        if(mode==Mode::Keys||mode==Mode::Expressive) loop->record(mode,{EventType::NoteOff,0});
        loop->command(LoopCommand::Record,mode);
        LoopTrackSnapshot snapshot; loop->snapshot(mode,snapshot);
        int attacks=0;
        for(size_t n=0;n<snapshot.count;++n) {
            const auto& r=snapshot.events[n];
            if(r.event.type==EventType::Drum||r.event.type==EventType::Pluck||r.event.type==EventType::NoteOn) {
                ++attacks; check(r.step==0&&r.event.note==60,"a near-downbeat early attack records at the first beat");
            }
        }
        check(attacks==(earlyMs<50?1:0),"the pickup captures near-downbeat attacks and keeps earlier practice separate");
    }
}
void pickupLifecycle() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Keys; s.tempo=240; s.quantize=Quantize::Off;
    loop->configure(s); loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate-960);
    loop->record(s.mode,attack(s.mode)); loop->record(s.mode,{EventType::NoteOff,0});
    advance(*loop,960); loop->command(LoopCommand::Record,s.mode);
    check(loop->eventCount(s.mode)==0,"a keyboard preview released before recording stays separate from the take");
    loop->command(LoopCommand::Stop,s.mode); loop->command(LoopCommand::Record,s.mode);
    advance(*loop,sampleRate-960); loop->record(s.mode,attack(s.mode));
    Event correction; correction.type=EventType::AttackPitch; correction.gesture=1; correction.bend=12;
    loop->record(s.mode,correction); advance(*loop,960); advance(*loop,2400);
    loop->record(s.mode,{EventType::NoteOff,0}); loop->command(LoopCommand::Record,s.mode);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count>=2&&snapshot.events[0].event.note==72,"a late pitch modifier completes the captured first-beat note");
    auto cancelled=std::make_unique<Looper>(); cancelled->configure(s);
    cancelled->command(LoopCommand::Record,s.mode); advance(*cancelled,sampleRate-960);
    cancelled->record(s.mode,attack(s.mode)); cancelled->command(LoopCommand::Undo,s.mode);
    cancelled->command(LoopCommand::Record,s.mode); advance(*cancelled,sampleRate);
    cancelled->command(LoopCommand::Record,s.mode);
    check(cancelled->eventCount(s.mode)==0,"canceling a count-in clears its pending attacks");
}
void strumAndOverdub() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Guitar; s.tempo=240; s.quantize=Quantize::Off;
    loop->configure(s); loop->command(LoopCommand::Play,s.mode); loop->command(LoopCommand::Record,s.mode);
    advance(*loop,sampleRate-480);
    for(int n=0;n<6;++n) { Event e=attack(s.mode); e.id=n; e.note=float(48+n*4); e.delay=n*192; loop->record(s.mode,e); }
    advance(*loop,480); advance(*loop,4800); loop->command(LoopCommand::Record,s.mode);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count==6,"a strum just before an overdub starts records all six strings");
    for(size_t n=0;n<snapshot.count;++n) check(std::abs(snapshot.events[n].step-double(n)*192*16/sampleRate)<1.e-6,
        "the captured first-beat strum retains its string spacing");
}
float energy(const std::vector<float>& audio) { double sum=0; for(float x:audio) sum+=x*x; return float(sum); }
void audibleDownbeat() {
    auto studio=std::make_unique<Studio>(); Settings s; s.tempo=240; s.mode=Mode::Drums; studio->configure(s);
    studio->apply({EventType::Loop,int(LoopCommand::Record)});
    std::vector<float> audio(size_t(sampleRate)*2); studio->render(audio.data(),sampleRate);
    audio.resize(4800); studio->render(audio.data(),2400);
    check(energy(audio)>.001f,"the recording start is audible through the actual mixer");
    check(studio->looper().eventCount(s.mode)==0,"cue clicks stay outside the recorded drum score");
    studio->apply({EventType::Loop,int(LoopCommand::Stop)}); studio->render(audio.data(),2400);
    check(energy(audio)==0,"stopping clears the audible cue tail");
}
}
int main() {
    cues(); earlyAttacks(); pickupLifecycle(); strumAndOverdub(); audibleDownbeat();
    std::printf("Recording entry: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
