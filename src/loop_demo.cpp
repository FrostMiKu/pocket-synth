#include "loop_demo.hpp"
namespace ps {
void prepareLoopDemo(Looper& loop) {
    Settings s; loop.configure(s);
    auto advance=[&](int frames) {
        for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) {} loop.advance(); }
    };
    constexpr int framesPerStep=sampleRate/8,framesPerBar=framesPerStep*loopStepsPerBar;
    auto begin=[&](Mode mode) { loop.command(LoopCommand::Record,mode); advance(framesPerBar); };
    begin(Mode::Drums);
    for(int step=0;step<loopStepsPerBar;++step) {
        auto hit=[&](int pad,float velocity) { Event e; e.type=EventType::Drum; e.id=pad; e.velocity=velocity; loop.record(Mode::Drums,e); };
        if(step%8==0||step==10) hit(0,.85f);
        if(step%8==4) hit(1,.8f);
        if(step%2==0) hit(2,step%4==0?.6f:.4f);
        if(step==14) hit(3,.5f);
        advance(framesPerStep);
    }
    begin(Mode::Guitar);
    for(int step=0;step<loopStepsPerBar;++step) {
        if(step==0||step==8) for(int string=0;string<stringCount;++string) {
            Event e; e.type=EventType::Pluck; e.id=string; e.note=float(chordBanks[0][step==0?0:2].notes[string]);
            e.velocity=.6f; e.delay=uint32_t(string*.006f*sampleRate); loop.record(Mode::Guitar,e);
        }
        advance(framesPerStep);
    }
    begin(Mode::Expressive);
    Event expression; expression.type=EventType::Expression; expression.timbre=.25f; expression.vibrato=.2f;
    loop.record(Mode::Expressive,expression);
    constexpr int voiceNotes[]={64,67,69,67};
    for(int step=0;step<loopStepsPerBar;++step) {
        if(step%4==0) { Event e; e.type=EventType::NoteOn; e.note=float(voiceNotes[step/4]); e.velocity=.35f; loop.record(Mode::Expressive,e); }
        if(step==15) loop.record(Mode::Expressive,{EventType::NoteOff,0});
        advance(framesPerStep);
    }
    begin(Mode::Keys);
    loop.record(Mode::Keys,expression);
    constexpr int melody[]={72,76,79,76,74,72,69,71};
    for(int step=0;step<loopStepsPerBar;++step) {
        if(step%2==0) { Event e; e.type=EventType::NoteOn; e.note=float(melody[step/2]); e.velocity=.45f; loop.record(Mode::Keys,e); }
        else loop.record(Mode::Keys,{EventType::NoteOff,0});
        advance(framesPerStep);
    }
    loop.command(LoopCommand::Stop,Mode::Drums);
    loop.command(LoopCommand::Play,Mode::Drums);
}
}
