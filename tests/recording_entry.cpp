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
void advance(Looper& loop,int frames,std::vector<Event>* emitted=nullptr) {
    for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) if(emitted) emitted->push_back(e); loop.advance(); }
}
Event attack(Mode mode) {
    Event e; e.type=mode==Mode::Drums?EventType::Drum:mode==Mode::Guitar?EventType::Pluck:EventType::NoteOn;
    e.note=60; e.velocity=.8f; e.gesture=1; return e;
}
bool isAttack(const Event& e) { return e.type==EventType::Drum||e.type==EventType::Pluck||e.type==EventType::NoteOn; }
int attacks(const Looper& loop,Mode mode,double* firstStep=nullptr) {
    LoopTrackSnapshot snapshot; loop.snapshot(mode,snapshot); int count=0;
    for(size_t n=0;n<snapshot.count;++n) if(isAttack(snapshot.events[n].event)) {
        if(!count&&firstStep) *firstStep=snapshot.events[n].step;
        ++count;
    }
    return count;
}
// Stopped: Record waits indefinitely, and the first attack is the first downbeat.
void firstAttackStarts() {
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys,Mode::Drums}) for(int bars:{1,2}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.mode=mode; s.tempo=240; s.loopBars=bars; s.quantize=Quantize::Off;
        loop->configure(s); loop->command(LoopCommand::Record,mode);
        bool accent=false;
        check(loop->view().armed&&!loop->view().playing&&!loop->takeClick(accent),"stopped recording waits silently for the first input");
        check(loopState(loop->view())=="WAITING"&&loopActions(loop->view(),mode).record=="CANCEL","waiting is labeled and can be cancelled");
        advance(*loop,sampleRate*5);
        check(loop->view().armed&&loop->view().step==0,"the wait has no time limit and keeps the playhead at the start");
        loop->record(mode,attack(mode));
        check(loop->view().recording&&loop->view().playing&&loop->view().step==0,"the first attack starts playback and recording at the downbeat");
        check(loop->takeClick(accent)&&accent,"the first attack sounds the accented recording downbeat");
        check(recordStarting(loop->view()),"the screen confirms the first beat of the take");
        const int cycle=sampleRate*bars;
        advance(*loop,cycle-1);
        check(loop->view().recording,"the take continues through its final frame");
        advance(*loop,1);
        double first=-1;
        check(!loop->view().recording&&attacks(*loop,mode,&first)==1&&first==0,"one complete loop commits with the first attack at its downbeat");
    }
}
// Right is a silent downbeat: the take starts, but no note enters the score.
void restStarts() {
    for(Mode mode:{Mode::Guitar,Mode::Keys,Mode::Drums}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.mode=mode; s.tempo=240; s.quantize=Quantize::Off;
        loop->configure(s); loop->command(LoopCommand::Record,mode);
        loop->command(LoopCommand::Cue,mode);
        check(loop->view().recording&&loop->view().events[int(mode)]==0,"a rest cue starts recording without recording a note");
        advance(*loop,sampleRate/4); loop->record(mode,attack(mode));
        advance(*loop,sampleRate*3/4);
        double first=-1;
        check(attacks(*loop,mode,&first)==1&&std::abs(first-4)<1.e-6,"a note after the rest cue keeps its place on the second beat");
    }
    auto loop=std::make_unique<Looper>(); Settings s; loop->configure(s);
    loop->command(LoopCommand::Cue,s.mode);
    check(!loop->view().playing&&!loop->view().recording,"the rest cue does nothing unless a take is waiting");
    loop->command(LoopCommand::Record,s.mode); loop->command(LoopCommand::Cue,s.mode);
    advance(*loop,sampleRate*2);
    check(loop->view().notice==LoopNotice::EmptyTake&&loop->eventCount(s.mode)==0,"a take with only rests commits nothing and explains why");
}
// Stick state and held gates while waiting do not start a take.
void waitingInputs() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Expressive; s.tempo=240; s.quantize=Quantize::Off; loop->configure(s);
    Event held=attack(s.mode); held.gesture=7;
    loop->record(s.mode,held);
    loop->command(LoopCommand::Record,s.mode);
    Event expression; expression.type=EventType::Expression; expression.timbre=.2f; expression.vibrato=.4f;
    loop->record(s.mode,expression); loop->record(s.mode,held); advance(*loop,4800);
    check(loop->view().armed&&!loop->view().recording,"a voice already sounding before Record and stick movement keep the take waiting");
    loop->record(s.mode,{EventType::NoteOff,0});
    Event fresh=attack(s.mode); fresh.gesture=8; loop->record(s.mode,fresh);
    check(loop->view().recording,"a fresh voice attack starts the take");
    advance(*loop,2400); loop->record(s.mode,{EventType::NoteOff,0}); advance(*loop,sampleRate);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count>=3&&snapshot.events[0].event.type==EventType::Expression&&snapshot.events[0].event.timbre==.2f&&snapshot.events[0].step==0,
        "the take opens with the stick expression held while waiting");
    auto keys=std::make_unique<Looper>(); s.mode=Mode::Keys; keys->configure(s);
    keys->command(LoopCommand::Record,s.mode); keys->record(s.mode,{EventType::NoteOff,0}); keys->record(s.mode,{EventType::Silence});
    check(keys->view().armed,"releases and silence keep the take waiting");
}
// Playing: a waiting overdub starts in place and records exactly one loop.
void inPlaceOverdub() {
    for(Quantize quantize:{Quantize::Off,Quantize::All}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Keys; s.tempo=240; s.quantize=quantize; loop->configure(s);
        loop->command(LoopCommand::Record,Mode::Drums); loop->record(Mode::Drums,attack(Mode::Drums)); advance(*loop,sampleRate);
        loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate/2+100);
        bool accent=true;
        check(loop->view().armed&&loop->takeClick(accent)&&!accent,"waiting during playback keeps the beat clicks running");
        loop->record(s.mode,attack(s.mode));
        check(loop->view().recording&&loopState(loop->view())=="RECORDING","the first attack during playback starts recording in place");
        advance(*loop,sampleRate/4); loop->record(s.mode,{EventType::NoteOff,0});
        advance(*loop,sampleRate/2);
        check(loop->view().recording,"an in-place take continues across the loop start");
        Event later=attack(s.mode); later.gesture=2; later.note=64; loop->record(s.mode,later);
        advance(*loop,sampleRate/8); loop->record(s.mode,{EventType::NoteOff,0});
        advance(*loop,sampleRate/8-200);
        check(loop->view().recording,"an in-place take lasts one full loop");
        std::vector<Event> played; advance(*loop,200,&played);
        check(!loop->view().recording,"an in-place take finishes one loop after it began");
        LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot); std::vector<double> steps;
        for(size_t n=0;n<snapshot.count;++n) if(snapshot.events[n].event.type==EventType::NoteOn) steps.push_back(snapshot.events[n].step);
        const double offset=quantize==Quantize::All?0:100.*loopStepsPerBar/sampleRate;
        check(steps.size()==2&&std::abs(steps[0]-(4+offset))<1.e-6&&std::abs(steps[1]-(8+offset))<1.e-6,
            "an in-place take keeps the notes on both sides of the loop start");
        played.clear(); advance(*loop,sampleRate,&played);
        const int replayed=int(std::count_if(played.begin(),played.end(),[](const Event& e){return e.type==EventType::NoteOn;}));
        check(replayed==2,"the next loop repeats every note of the in-place take once");
    }
}
// A take that wraps keeps a held melody note sounding until its own start.
void heldThroughEnd() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Keys; s.tempo=240; s.quantize=Quantize::Off; loop->configure(s);
    loop->command(LoopCommand::Record,s.mode); loop->record(s.mode,attack(s.mode)); advance(*loop,sampleRate);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count==2&&snapshot.events[1].event.type==EventType::NoteOff&&snapshot.events[1].step>15.9,"a note held through the end of a take releases at the loop end");
}
void cancellation() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Keys; loop->configure(s);
    loop->command(LoopCommand::Record,s.mode); loop->command(LoopCommand::Record,s.mode);
    check(!loop->view().armed&&!loop->view().playing&&loop->view().notice==LoopNotice::Cancelled,"Record again cancels a waiting take");
    loop->command(LoopCommand::Record,s.mode); loop->command(LoopCommand::Undo,s.mode);
    check(!loop->view().armed&&loop->view().notice==LoopNotice::Cancelled,"Undo cancels a waiting take");
    loop->record(s.mode,attack(s.mode));
    check(!loop->view().playing&&loop->eventCount(s.mode)==0,"playing after cancelling a wait stays live only");
    loop->command(LoopCommand::Record,Mode::Drums); loop->record(s.mode,attack(s.mode));
    check(loop->view().armed,"only the waiting track's instrument starts its take");
}
void pitchCorrection() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Keys; s.tempo=240; s.quantize=Quantize::Off; loop->configure(s);
    loop->command(LoopCommand::Record,s.mode); loop->record(s.mode,attack(s.mode));
    Event correction; correction.type=EventType::AttackPitch; correction.gesture=1; correction.bend=12;
    advance(*loop,480); loop->record(s.mode,correction); advance(*loop,2400);
    loop->record(s.mode,{EventType::NoteOff,0}); loop->command(LoopCommand::Record,s.mode);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count>=2&&snapshot.events[0].event.note==72,"a late pitch modifier completes the take's first note");
}
void strum() {
    auto loop=std::make_unique<Looper>(); Settings s; s.mode=Mode::Guitar; s.tempo=240; s.quantize=Quantize::Off; loop->configure(s);
    loop->command(LoopCommand::Record,s.mode);
    for(int n=0;n<6;++n) { Event e=attack(s.mode); e.id=n; e.note=float(48+n*4); e.delay=n*192; loop->record(s.mode,e); }
    advance(*loop,4800); loop->command(LoopCommand::Record,s.mode);
    LoopTrackSnapshot snapshot; loop->snapshot(s.mode,snapshot);
    check(snapshot.count==6,"a strum that starts a take records all six strings");
    for(size_t n=0;n<snapshot.count;++n) check(std::abs(snapshot.events[n].step-double(n)*192*16/sampleRate)<1.e-6,
        "the first strum retains its string spacing");
}
float energy(const std::vector<float>& audio) { double sum=0; for(float x:audio) sum+=x*x; return float(sum); }
void audibleDownbeat() {
    auto studio=std::make_unique<Studio>(); Settings s; s.tempo=240; s.mode=Mode::Drums; studio->configure(s);
    studio->apply({EventType::Loop,int(LoopCommand::Record)});
    std::vector<float> audio(4800); studio->render(audio.data(),2400);
    check(energy(audio)==0,"a waiting take stays silent");
    studio->apply({EventType::Loop,int(LoopCommand::Cue)}); studio->render(audio.data(),2400);
    check(energy(audio)>.001f,"the rest cue's downbeat click is audible through the actual mixer");
    check(studio->looper().eventCount(s.mode)==0,"cue clicks stay outside the recorded drum score");
    studio->apply({EventType::Loop,int(LoopCommand::Stop)}); studio->render(audio.data(),2400);
    check(energy(audio)==0,"stopping clears the audible cue tail");
}
}
int main() {
    firstAttackStarts(); restStarts(); waitingInputs(); inPlaceOverdub(); heldThroughEnd(); cancellation(); pitchCorrection(); strum(); audibleDownbeat();
    std::printf("Recording entry: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
