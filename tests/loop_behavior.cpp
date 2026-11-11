#include "studio.hpp"
#include "loop_demo.hpp"
#include "settings.hpp"
#include <memory>
#include <vector>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <algorithm>
using namespace ps;
static int checks=0,failures=0;
static void check(bool ok,const char* message) { ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); } }
static std::vector<float> render(Studio& studio,int frames) {
    std::vector<float> out(size_t(frames)*2); studio.render(out.data(),frames);
    check(std::all_of(out.begin(),out.end(),[](float f){return std::isfinite(f)&&std::abs(f)<=1;}),"mixed output stays finite and bounded");
    return out;
}
static float rms(const std::vector<float>& out) { double sum=0; for(float f:out) sum+=f*f; return float(std::sqrt(sum/out.size())); }
static void advance(Looper& loop,int frames,std::vector<Event>* emitted=nullptr) {
    for(int n=0;n<frames;++n) {
        Mode mode; Event e;
        while(loop.next(mode,e)) if(emitted) emitted->push_back(e);
        loop.advance();
    }
}
static void pitchAccuracyAndMute() {
    double largestCents=0;
    for(int n=0;n<=14400;++n) {
        const float note=float(n)/100;
        const double expected=440*std::exp2((double(note)-69)/12);
        largestCents=std::max(largestCents,std::abs(1200*std::log2(frequency(note)/expected)));
    }
    check(largestCents<.001,"continuous pitch stays within one thousandth of a cent of equal temperament");
    auto studio=std::make_unique<Studio>(); Settings s; s.volume=0; studio->configure(s);
    studio->apply({EventType::Loop,int(LoopCommand::Record)});
    check(rms(render(*studio,4800))==0,"master volume also mutes the count-in");
}
static void shortGatePulses() {
    for(Mode mode:{Mode::Expressive,Mode::Keys}) for(EventType stop:{EventType::NoteOff,EventType::Silence}) {
        Engine engine; engine.setMode(mode); engine.apply({EventType::NoteOn,0}); engine.apply({stop,0});
        std::vector<float> audio(9600); engine.render(audio.data(),4800);
        check(rms(audio)==0&&!engine.active(),"a gate that ends before the first audio sample leaves no stuck voice");
    }
    Engine guitar; guitar.apply({EventType::Pluck,0}); guitar.apply({EventType::LayerStop,0});
    std::vector<float> audio(9600); guitar.render(audio.data(),4800);
    check(rms(audio)==0&&!guitar.active(),"a muted pluck before rendering stays silent");
}
static void drums() {
    Settings s; s.mode=Mode::Drums; Controller c; double t=0;
    for(int pad=0;pad<drumCount;++pad) {
        Input i; i.held[instrumentButtons[size_t(pad)]]=true;
        auto e=c.update(i,s,t+=.01);
        check(e.count==1&&e.data[0].type==EventType::Drum&&e.data[0].id==pad,"each drum pad has its own one-shot voice");
        check(c.update(i,s,t+=.01).count==0,"holding a drum pad plays one hit");
        c.update({},s,t+=.01);
        Engine engine; engine.setMode(Mode::Drums); engine.apply(e.data[0]);
        std::vector<float> audio(8192); engine.render(audio.data(),4096);
        std::printf("%s attack RMS %.6f\n",drumNames[pad],double(rms(audio)));
        check(rms(audio)>.002f,"each drum has an audible attack");
        audio.resize(sampleRate*4); engine.render(audio.data(),sampleRate*2);
        audio.resize(2048); engine.render(audio.data(),1024);
        check(rms(audio)==0,"every drum naturally reaches silence");
    }
    auto power=[](int pad,float velocity,bool choke) {
        Engine engine; Event e; e.type=EventType::Drum; e.id=pad; e.velocity=velocity; engine.apply(e);
        std::vector<float> audio(9600); engine.render(audio.data(),4800);
        if(choke) { e.id=2; engine.apply(e); }
        audio.resize(24000); engine.render(audio.data(),12000);
        audio.resize(4096); engine.render(audio.data(),2048); return rms(audio);
    };
    check(power(3,1,true)<power(3,1,false)*.1f,"closed hi-hat chokes a ringing open hi-hat");
    Engine soft,loud; Event hit; hit.type=EventType::Drum; hit.velocity=.2f; soft.apply(hit); hit.velocity=1; loud.apply(hit);
    std::vector<float> a(8192),b(8192); soft.render(a.data(),4096); loud.render(b.data(),4096);
    check(rms(b)>rms(a)*3,"drum velocity retains expressive dynamics");
    s.swapFaceButtons=true; Input i; i.held[A]=true;
    check(c.update(i,s,t+=.01).data[0].id==1,"drums share the face button layout setting");
}
static void controls() {
    for(int mode=0;mode<modeCount;++mode) {
        Settings s; s.mode=Mode(mode); Controller c; Input i; i.held[Right]=true;
        c.update(i,s,0);
        i.held[B]=true; auto record=c.update(i,s,.01);
        bool found=false,played=false;
        for(size_t n=0;n<record.count;++n) {
            found|=record.data[n].type==EventType::Loop&&record.data[n].id==int(LoopCommand::Record);
            played|=record.data[n].type==EventType::Drum||record.data[n].type==EventType::Pluck||record.data[n].type==EventType::NoteOn;
        }
        check(found&&!played,"Meta record starts recording while consuming the performance button");
        i.held[Right]=false; auto released=c.update(i,s,.02);
        check(!c.performing(B,i),"releasing Meta first keeps its command button consumed until release");
        for(size_t n=0;n<released.count;++n) check(released.data[n].type!=EventType::Drum&&released.data[n].type!=EventType::NoteOn,"Meta release creates no stray note");
        i={}; c.update(i,s,.03); i.held[Right]=true; c.update(i,s,.04);
        i.held[R1]=true; const int bank=s.chordBank; auto faster=c.update(i,s,.05);
        check(s.tempo==121&&s.chordBank==bank,"Meta R1 raises tempo once and preserves the chord bank");
        check(!c.performing(R1,i),"tempo controls consume the performance button");
        c.update(i,s,.2); check(s.tempo==121,"tempo repeat waits before repeating");
        c.update(i,s,.46); check(s.tempo==122,"holding tempo repeats after the initial delay");
        i={}; c.update(i,s,.5); i.held[Right]=true; c.update(i,s,.51); i.held[Y]=true;
        auto early=c.update(i,s,.52); bool cleared=false;
        for(size_t n=0;n<early.count;++n) cleared|=early.data[n].type==EventType::Loop&&early.data[n].id==int(LoopCommand::Clear);
        check(!cleared,"clearing waits for a deliberate hold");
        auto clear=c.update(i,s,1.53); cleared=false;
        for(size_t n=0;n<clear.count;++n) cleared|=clear.data[n].type==EventType::Loop&&clear.data[n].id==int(LoopCommand::Clear);
        check(cleared,"holding Meta Y clears the current track");
    }
    Settings s; s.mode=Mode::Expressive; Controller c; Input i; i.ry=-1; i.held[L1]=true; c.update(i,s,0);
    i.held[Right]=true; c.update(i,s,.01);
    check(s.tempo==120,"holding a singing key before Meta preserves tempo");
    i.held[R1]=true; auto events=c.update(i,s,.02);
    check(s.tempo==121&&events.data[0].type==EventType::NoteOn,"tempo adjustment can preserve an already held voice gate");
}
static void transport() {
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; s.mode=Mode::Drums; loop->configure(s);
    loop->command(LoopCommand::Record,s.mode);
    int clicks=0; bool accent=false;
    for(int frame=0;frame<sampleRate;++frame) { if(loop->takeClick(accent)) ++clicks; loop->advance(); }
    check(clicks==4&&loop->view().recording,"first recording follows four count-in beats");
    advance(*loop,1800); Event hit; hit.type=EventType::Drum; hit.id=0; loop->record(s.mode,hit);
    advance(*loop,sampleRate-1800);
    check(!loop->view().recording&&loop->view().events[int(s.mode)]==1,"one recorded cycle commits automatically");
    check(loop->view().drumSteps[0]==2,"drums quantize to the nearest sixteenth note");
    std::vector<Event> played; advance(*loop,sampleRate,&played);
    check(played.size()==1&&played[0].type==EventType::Drum,"a recorded drum hit repeats once per cycle");
    loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate);
    hit.id=1; loop->record(s.mode,hit); advance(*loop,sampleRate);
    check(loop->view().events[int(s.mode)]==2,"a second take overdubs the existing track");
    loop->command(LoopCommand::Undo,s.mode);
    check(loop->view().events[int(s.mode)]==1,"undo removes the last overdub and retains the earlier take");
    loop->command(LoopCommand::Record,Mode::Keys); advance(*loop,sampleRate);
    Event note; note.type=EventType::NoteOn; note.note=64; loop->record(Mode::Keys,note);
    advance(*loop,sampleRate/4); note.type=EventType::NoteOff; loop->record(Mode::Keys,note); advance(*loop,sampleRate*3/4);
    check(loop->view().events[int(Mode::Keys)]==2&&loop->view().events[int(Mode::Drums)]==1,"instruments retain independent synchronized tracks");
    loop->command(LoopCommand::Clear,Mode::Keys);
    check(loop->view().events[int(Mode::Keys)]==0&&loop->view().events[int(Mode::Drums)]==1,"clear affects only the selected instrument");
    loop->command(LoopCommand::Undo,Mode::Keys);
    check(loop->view().events[int(Mode::Keys)]==2,"undo restores a cleared track");
    advance(*loop,sampleRate/4); const int position=loop->view().step;
    loop->command(LoopCommand::Play,Mode::Drums); advance(*loop,10000);
    check(!loop->view().playing&&loop->view().step==position,"pause freezes the shared playhead");
    loop->command(LoopCommand::Play,Mode::Drums); advance(*loop,3000);
    check(loop->view().playing&&loop->view().step==position+1,"play resumes from the paused position");
    check(loop->save("loop-test.pi"),"loop project saves atomically");
    auto loaded=std::make_unique<Looper>(); check(loaded->load("loop-test.pi"),"loop project reopens");
    check(loaded->view().events==loop->view().events&&!loaded->view().playing,"reopening preserves all tracks and waits for play");
    { std::ofstream out("loop-broken.pi"); out<<"POCKET_LOOP 2\n999999999\n"; }
    check(!loaded->load("loop-broken.pi")&&loaded->view().events==loop->view().events,"malformed project preserves the current tracks");
    std::remove("loop-test.pi"); std::remove("loop-broken.pi");
    s.loopBars=2; loop->configure(s); loop->command(LoopCommand::Stop,s.mode);
    loop->command(LoopCommand::Record,s.mode); advance(*loop,sampleRate);
    check(loop->view().recording&&loop->view().bars==2,"two-bar recording keeps a four-beat count-in");
    advance(*loop,sampleRate); check(loop->view().recording,"two-bar recording continues through the first bar");
    advance(*loop,sampleRate); check(!loop->view().recording,"two-bar recording finishes at its shared boundary");
}
static void audioLoop() {
    auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Expressive; s.tempo=240; studio->configure(s);
    studio->apply({EventType::Loop,int(LoopCommand::Record)}); render(*studio,sampleRate);
    Event expression; expression.type=EventType::Expression; expression.timbre=0; studio->apply(expression);
    Event note; note.type=EventType::NoteOn; note.note=69; studio->apply(note); render(*studio,sampleRate/4);
    note.type=EventType::NoteOff; studio->apply(note); render(*studio,sampleRate*3/4);
    check(studio->looper().view().events[int(Mode::Expressive)]==3,"recording preserves note duration and expression events");
    auto first=render(*studio,4800);
    check(rms(first)>.01f,"the committed voice loop produces sound");
    s.tempo=120; s.mode=Mode::Drums; studio->configure(s);
    auto second=render(*studio,4800);
    int crossings=0; for(size_t n=2;n<second.size();n+=2) if(second[n-2]<=0&&second[n]>0) ++crossings;
    check(crossings>=43&&crossings<=45,"tempo changes and instrument switching preserve the loop's 440 Hz pitch");
    check(rms(second)>.01f,"switching instruments preserves the backing loop");
    studio->apply({EventType::Loop,int(LoopCommand::Play)}); render(*studio,12000);
    check(rms(render(*studio,2048))==0,"pausing a loop quickly releases its held sound");
    studio->apply({EventType::Loop,int(LoopCommand::Play)});
    check(rms(render(*studio,4800))>.01f,"resuming restores a melody held at the paused position");
    studio->apply({EventType::Loop,int(LoopCommand::Stop)}); render(*studio,12000);
    check(rms(render(*studio,2048))==0,"global stop releases every loop and live instrument");
}
static void strumPitches() {
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; loop->configure(s);
    loop->command(LoopCommand::Record,Mode::Guitar); advance(*loop,sampleRate);
    Controller controller; Input input; input.held[Up]=true; input.ry=.9f;
    const auto stroke=controller.update(input,s,0);
    for(size_t n=0;n<stroke.count;++n) loop->record(Mode::Guitar,stroke.data[n]);
    advance(*loop,48); input.held[Up]=false;
    const auto release=controller.update(input,s,.001);
    for(size_t n=0;n<release.count;++n) loop->record(Mode::Guitar,release.data[n]);
    advance(*loop,sampleRate-48);
    std::vector<Event> events; advance(*loop,4000,&events);
    check(events.size()==stringCount,"one recorded stroke retains its six string attacks");
    for(size_t n=0;n<events.size();++n) check(events[n].note==stroke.data[n].note,"loop playback retains every string pitch after modifier release");
}
static void melodicOverdubs() {
    auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Expressive; s.tempo=240; studio->configure(s);
    Event expression; expression.type=EventType::Expression; expression.timbre=0;
    studio->apply({EventType::Loop,int(LoopCommand::Record)}); render(*studio,sampleRate);
    studio->apply(expression); Event note; note.type=EventType::NoteOn; note.note=60; note.velocity=.5f;
    studio->apply(note); render(*studio,6000); studio->apply({EventType::NoteOff,0}); render(*studio,42000);
    studio->apply({EventType::Loop,int(LoopCommand::Record)}); render(*studio,sampleRate);
    studio->apply(expression); note.note=69; studio->apply(note); render(*studio,36000);
    studio->apply({EventType::NoteOff,0}); render(*studio,12000);
    render(*studio,34800); auto harmony=render(*studio,1024);
    int crossings=0; for(size_t n=2;n<harmony.size();n+=2) if(harmony[n-2]<=0&&harmony[n]>0) ++crossings;
    check(rms(harmony)>.01f&&crossings>=8&&crossings<=11,"an earlier take's note-off preserves the independently held 440 Hz overdub");
    auto loop=std::make_unique<Looper>(); loop->configure(s);
    for(int take=0;take<melodyLayerCount;++take) {
        loop->command(LoopCommand::Record,Mode::Keys); advance(*loop,sampleRate);
        loop->record(Mode::Keys,note); advance(*loop,sampleRate/2);
        loop->record(Mode::Keys,{EventType::NoteOff,0}); advance(*loop,sampleRate/2);
    }
    check(loop->view().events[int(Mode::Keys)]==melodyLayerCount*2,"melody tracks support four independent overdub layers");
    loop->command(LoopCommand::Record,Mode::Keys); advance(*loop,sampleRate);
    check(loop->view().full&&!loop->view().recording,"a full melody track preserves its existing harmonies");
}
static void guitarOverdubs() {
    Engine engine; Event low; low.type=EventType::Pluck; low.id=0; low.note=45; engine.apply(low);
    Event high=low; high.id=stringCount; high.note=69; engine.apply(high);
    std::vector<float> audio(4800); engine.render(audio.data(),2400);
    engine.apply({EventType::LayerStop,0}); engine.render(audio.data(),2400); engine.render(audio.data(),2400);
    check(rms(audio)>.001f,"muting one guitar take preserves another ringing guitar take");
    engine.apply({EventType::Silence}); engine.render(audio.data(),2400); engine.render(audio.data(),2400);
    check(rms(audio)<.001f,"global silence releases all guitar layers");
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; loop->configure(s);
    for(int take=0;take<2;++take) {
        loop->command(LoopCommand::Record,Mode::Guitar); advance(*loop,sampleRate);
        loop->record(Mode::Guitar,low); advance(*loop,6000);
        loop->record(Mode::Guitar,{EventType::Silence}); advance(*loop,sampleRate-6000);
    }
    std::vector<Event> events; advance(*loop,sampleRate,&events);
    bool first=false,second=false,scoped=false;
    for(const auto& event:events) {
        first|=event.type==EventType::Pluck&&event.id==0;
        second|=event.type==EventType::Pluck&&event.id==stringCount;
        scoped|=event.type==EventType::LayerStop&&event.id==stringCount;
    }
    check(first&&second&&scoped,"guitar overdubs keep independent strings and mute controls");
    check(loop->save("guitar-layers.pi"),"guitar layers save");
    auto reopened=std::make_unique<Looper>(); check(reopened->load("guitar-layers.pi"),"guitar layers reopen with their scoped mute events");
    std::remove("guitar-layers.pi");
}
static void tempoAndCapacity() {
    auto loop=std::make_unique<Looper>(); Settings s; s.tempo=240; loop->configure(s);
    loop->command(LoopCommand::Record,Mode::Drums); advance(*loop,sampleRate);
    advance(*loop,3000); Event e; e.type=EventType::Drum; loop->record(Mode::Drums,e); advance(*loop,sampleRate-3000);
    s.tempo=120; loop->configure(s);
    std::vector<int> hits;
    for(int frame=0;frame<sampleRate*4;++frame) {
        Mode mode; Event event;
        while(loop->next(mode,event)) if(event.type==EventType::Drum) hits.push_back(frame);
        loop->advance();
    }
    check(hits.size()==2&&std::abs(hits[0]-6000)<=1&&std::abs(hits[1]-hits[0]-96000)<=1,"tempo changes scale event spacing and keep all tracks on the shared sample clock");
    loop->command(LoopCommand::Record,Mode::Drums); advance(*loop,sampleRate*2);
    for(int n=0;n<loopEventLimit+1;++n) loop->record(Mode::Drums,e);
    check(loop->view().full&&loop->view().events[int(Mode::Drums)]==1,"a full overdub preserves the existing track and reports capacity");
    loop->command(LoopCommand::Stop,Mode::Drums);
    check(!loop->view().recording&&!loop->view().playing,"a capacity warning still allows a clean stop");
}
static void mixedPlayback() {
    auto studio=std::make_unique<Studio>(); prepareLoopDemo(studio->looper()); studio->prepareCaches();
    auto view=studio->looper().view();
    check(std::all_of(view.events.begin(),view.events.end(),[](int count){return count>0;}),"the full-loop demo contains all four instrument tracks");
    Settings s; s.mode=Mode::Guitar; studio->configure(s);
    for(int block=0;block<80;++block) {
        if(block%10==0) for(int n=0;n<stringCount;++n) {
            Event e; e.type=EventType::Pluck; e.id=n; e.note=float(chordBanks[0][0].notes[n]); e.velocity=.8f; studio->apply(e);
        }
        if(block==40) { s.tempo=160; studio->configure(s); }
        check(rms(render(*studio,2400))>.001f,"four loop tracks and live guitar keep producing bounded musical output");
    }
    studio->apply({EventType::Loop,int(LoopCommand::Stop)}); render(*studio,12000);
    check(rms(render(*studio,2048))==0,"stopping a layered performance reaches silence");
}
int main() {
    pitchAccuracyAndMute(); shortGatePulses(); drums(); controls(); transport(); audioLoop(); strumPitches(); melodicOverdubs(); guitarOverdubs(); tempoAndCapacity(); mixedPlayback();
    std::printf("Loop and drum behavior: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
