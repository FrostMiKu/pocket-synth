#include "loop_presentation.hpp"
#include <cstdio>
#include <memory>
using namespace ps;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) { ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); } }
void advance(Looper& loop,int frames) {
    for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) {} loop.advance(); }
}
void recordingJourney() {
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; s.mode=Mode::Keys; loop->configure(s);
    auto actions=loopActions(loop->view(),s.mode);
    check(actions.play=="PLAY"&&actions.record=="RECORD"&&!actions.canUndo&&!actions.canClear,"an empty track offers recording and shows undo/clear as unavailable");
    loop->command(LoopCommand::Record,s.mode);
    check(loopState(loop->view())=="COUNT 1/4"&&loopActions(loop->view(),s.mode).record=="CANCEL","count-in shows its beat and offers cancellation");
    advance(*loop,sampleRate/2);
    check(loop->view().progress>.49f&&loop->view().progress<.51f,"count-in progress advances through the four beats");
    loop->command(LoopCommand::Record,s.mode);
    check(!loop->view().playing&&loop->view().notice==LoopNotice::Cancelled,"canceling the initial wait leaves a paused empty track and confirms cancellation");
    advance(*loop,sampleRate*2);
    check(loopNotice(loop->view()).empty(),"confirmation expires even while paused");
    loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate);
    check(loopState(loop->view())=="RECORDING"&&loopActions(loop->view(),s.mode).record=="FINISH","recording starts after count-in and offers early finish");
    Event note; note.type=EventType::NoteOn; loop->record(s.mode,note); advance(*loop,12000);
    check(loopState(loop->view())=="RECORDING"&&!loop->view().overdubbing,"a first take remains recording after its first note");
    check(loop->view().progress>.24f&&loop->view().progress<.26f,"record progress shows the elapsed part of the loop");
    loop->command(LoopCommand::Record,s.mode);
    check(loop->view().notice==LoopNotice::Recorded&&loop->view().layers[int(s.mode)]==1,"finishing a take confirms it and shows one committed layer");
    loop->command(LoopCommand::Record,s.mode);
    check(loopState(loop->view())=="NEXT LOOP"&&loopActions(loop->view(),s.mode).record=="CANCEL","arming during playback explains the next-loop start");
    advance(*loop,sampleRate-12000);
    check(loopState(loop->view())=="OVERDUBBING","recording over an existing track is labeled overdubbing");
    check(loopActions(loop->view(),s.mode).undo=="CANCEL","undo during recording offers cancellation of the current take");
    loop->command(LoopCommand::Undo,s.mode);
    check(loop->view().layers[int(s.mode)]==1&&loop->view().notice==LoopNotice::Cancelled,"canceling an overdub preserves the committed layer");
    loop->command(LoopCommand::Clear,s.mode);
    check(loop->view().notice==LoopNotice::Cleared&&loop->view().layers[int(s.mode)]==0,"clear confirms the selected track and resets its layer count");
    check(loopActions(loop->view(),s.mode).canUndo&&!loopActions(loop->view(),s.mode).canClear,"clear offers undo and shows the empty track's clear as unavailable");
    loop->command(LoopCommand::Undo,s.mode);
    check(loop->view().notice==LoopNotice::Undone&&loop->view().layers[int(s.mode)]==1,"undo confirms restoration and restores the visible layer count");
    loop->command(LoopCommand::Play,s.mode);
    check(loopActions(loop->view(),s.mode).play=="PLAY","pause changes the offered action to play");
}
void capacity() {
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; loop->configure(s);
    for(int layer=0;layer<loopLayerCount;++layer) {
        loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate);
        Event e; e.type=EventType::Pluck; loop->record(s.mode,e); advance(*loop,sampleRate);
        check(loop->view().layers[int(s.mode)]==layer+1,"each committed take adds one visible pitched layer");
    }
    check(!loopActions(loop->view(),s.mode).canRecord,"a four-layer track shows record as unavailable");
    loop->command(LoopCommand::Record,s.mode);
    check(loop->view().full&&!loop->view().armed,"recording a full track reports capacity immediately");
    check(loopActions(loop->view(),Mode::Drums).canRecord,"pitched-layer capacity preserves drum recording availability");
}
void playbackPosition() {
    constexpr int framesPerBeat=sampleRate/4;
    for(int bars:{1,2}) {
        auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; s.loopBars=bars; loop->configure(s);
        loop->command(LoopCommand::Play,Mode::Drums);
        for(int cycle=0;cycle<2;++cycle) for(int bar=1;bar<=bars;++bar) for(int beat=1;beat<=4;++beat) {
            const auto expected="BAR "+std::to_string(bar)+"/"+std::to_string(bars)+"  BEAT "+std::to_string(beat)+"/4";
            check(loopPosition(loop->view())==expected,"playback shows each bar and beat at its exact musical boundary");
            advance(*loop,framesPerBeat-1);
            check(loopPosition(loop->view())==expected,"the current beat remains visible until its final sample");
            loop->command(LoopCommand::Play,Mode::Drums); advance(*loop,4000);
            check(loopPosition(loop->view())==expected,"pausing freezes the visible bar and beat");
            loop->command(LoopCommand::Play,Mode::Drums); advance(*loop,1);
        }
        check(loopPosition(loop->view())=="BAR 1/"+std::to_string(bars)+"  BEAT 1/4","the end of a loop returns the display to its first bar and beat");
        loop->command(LoopCommand::Stop,Mode::Drums); loop->command(LoopCommand::Record,Mode::Drums);
        check(loopPosition(loop->view())=="BAR -/"+std::to_string(bars)+"  BEAT -/4","count-in is distinct from the upcoming playback position");
    }
}
void clearGesture() {
    for(bool swap:{false,true}) {
        Controller c; Settings s; s.mode=Mode::Drums; s.swapFaceButtons=swap; Input i;
        i.held[Right]=true; c.update(i,s,0); i.held[mappedButton(Y,s)]=true; c.update(i,s,.1);
        auto events=c.update(i,s,.6);
        check(c.clearProgress()>.49f&&c.clearProgress()<.51f&&events.count==0,"holding clear shows half progress before committing");
        i.held[Right]=false; c.update(i,s,.7);
        check(c.clearProgress()==0,"releasing the modifier cancels clear progress");
        i={}; c.update(i,s,.8); i.held[Right]=true; c.update(i,s,.9); i.held[mappedButton(Y,s)]=true; c.update(i,s,1);
        events=c.update(i,s,2.01);
        check(c.clearProgress()==1&&events.count==1&&events.data[0].type==EventType::Loop&&events.data[0].id==int(LoopCommand::Clear),"a complete hold emits exactly one clear with either face-button layout");
        check(c.update(i,s,3).count==0,"holding after completion keeps clear a single action");
        c.reset(); check(c.clearProgress()==0,"focus reset cancels a pending clear gesture");
    }
}
}
int main() {
    recordingJourney(); capacity(); playbackPosition(); clearGesture();
    std::printf("Loop experience: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
