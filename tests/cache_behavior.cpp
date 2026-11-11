#include "studio.hpp"
#include "loop_demo.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
using namespace ps;
static int failures=0,checks=0;
static void check(bool ok,const char* message) { ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); } }
static std::vector<float> render(Studio& studio,int frames) {
    std::vector<float> out(size_t(frames)*2); studio.render(out.data(),frames);
    check(std::all_of(out.begin(),out.end(),[](float value){return std::isfinite(value)&&std::abs(value)<=1;}),"cached output stays finite and bounded");
    return out;
}
static float rms(const std::vector<float>& out) { double sum=0; for(float f:out) sum+=f*f; return float(std::sqrt(sum/out.size())); }
static void advance(Looper& loop,int frames) { for(int n=0;n<frames;++n) { Mode mode; Event event; while(loop.next(mode,event)) {} loop.advance(); } }
static std::unique_ptr<Studio> tone(float pitch,int tempo) {
    auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Expressive; s.tempo=tempo;
    auto& loop=studio->looper(); loop.configure(s);
    const int frames=int(std::ceil(double(sampleRate)*240/tempo));
    loop.command(LoopCommand::Record,Mode::Expressive); advance(loop,frames);
    Event expression; expression.type=EventType::Expression; expression.timbre=0; loop.record(Mode::Expressive,expression);
    Event note; note.type=EventType::NoteOn; note.note=pitch; note.velocity=.7f; loop.record(Mode::Expressive,note);
    advance(loop,frames); studio->configure(s); studio->prepareCaches(); return studio;
}
static void transportAndEdits() {
    auto studio=std::make_unique<Studio>(); prepareLoopDemo(studio->looper()); Settings s; s.mode=Mode::Keys;
    studio->configure(s); studio->prepareCaches();
    check(!studio->view().preparing,"a prepared song is ready for playback");
    check(rms(render(*studio,24000))>.01f,"four cached instrument tracks play together");
    const auto counts=studio->view().events;
    studio->apply({EventType::Loop,int(LoopCommand::Clear)}); render(*studio,2400);
    auto cleared=studio->view().events;
    check(cleared[int(Mode::Keys)]==0&&cleared[int(Mode::Guitar)]==counts[int(Mode::Guitar)]&&cleared[int(Mode::Drums)]==counts[int(Mode::Drums)],"clearing a cached track preserves other tracks");
    studio->apply({EventType::Loop,int(LoopCommand::Undo)}); render(*studio,512);
    check(studio->view().events==counts,"undo restores the cached track's score");
    check(rms(render(*studio,4096))>.01f,"undo restores audible cached playback");
    s.volume=0; studio->configure(s); render(*studio,24000);
    check(rms(render(*studio,2048))<1.e-7f,"master volume mutes cached audio");
    s.volume=.65f; studio->configure(s); render(*studio,4800);
    const int oldTempo=studio->view().tempo;
    s.tempo=137; studio->configure(s); render(*studio,128);
    check(studio->view().tempo==oldTempo&&studio->view().requestedTempo==137,"requested tempo keeps the old backing stable during preparation");
    bool changed=false;
    for(int n=0;n<2000;++n) {
        render(*studio,480);
        if(studio->view().tempo==137) { changed=true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(changed&&studio->looper().phase()<.02,"prepared tracks change tempo together at a loop boundary");
    for(int tempo:{145,129,151,118,132}) { s.tempo=tempo; studio->configure(s); render(*studio,128); }
    studio->prepareCaches();
    check(studio->view().tempo==132&&!studio->view().preparing,"rapid changes retain the newest requested tempo");
    check(rms(render(*studio,4096))>.005f,"playback continues after obsolete renders are cancelled");
    studio->apply({EventType::Loop,int(LoopCommand::Stop)}); render(*studio,12000);
    check(rms(render(*studio,2048))==0,"cached playback reaches complete silence after stop");
}
static void pitchAndSeams() {
    auto studio=tone(69,120); render(*studio,4800);
    Settings s; s.mode=Mode::Expressive; s.tempo=180; studio->configure(s); studio->prepareCaches();
    render(*studio,4800); auto audio=render(*studio,4800); int crossings=0;
    for(size_t n=2;n<audio.size();n+=2) if(audio[n-2]<=0&&audio[n]>0) ++crossings;
    check(crossings>=43&&crossings<=45,"regenerated tempo preserves the cached 440 Hz pitch");
    auto seam=tone(69.5f,137);
    const double framesPerLoop=double(sampleRate)*240/137;
    auto wave=render(*seam,int(std::ceil(framesPerLoop*3)));
    float largest=0;
    for(int cycle=1;cycle<=2;++cycle) {
        const int boundary=int(std::llround(framesPerLoop*cycle));
        for(int n=boundary-4;n<boundary+5;++n) largest=std::max(largest,std::abs(wave[size_t(n)*2]-wave[size_t(n-1)*2]));
    }
    check(largest<.02f,"non-integer waveform periods wrap without an abrupt seam");
}
static void naturalTails() {
    // The fastest one-bar loop is shorter than a crash one-shot.
    constexpr int cycle=sampleRate,hitFrame=cycle*15/16,window=2400;
    for(Mode mode:{Mode::Drums,Mode::Guitar,Mode::Expressive,Mode::Keys}) {
        const int pads=mode==Mode::Drums?drumCount:1;
        for(int pad=0;pad<pads;++pad) {
            auto studio=std::make_unique<Studio>(); Settings s; s.mode=mode; s.tempo=240;
            auto& loop=studio->looper(); loop.configure(s);
            loop.command(LoopCommand::Record,mode); advance(loop,cycle);
            advance(loop,hitFrame);
            Event hit; hit.type=mode==Mode::Drums?EventType::Drum:mode==Mode::Guitar?EventType::Pluck:EventType::NoteOn;
            hit.id=pad; hit.note=60; hit.velocity=.8f; hit.timbre=.5f; loop.record(mode,hit);
            advance(loop,cycle-hitFrame);
            studio->configure(s); studio->prepareCaches();
            // Compare the audible tail with an uninterrupted instrument, including
            // two previous repetitions so long one-shots overlap naturally.
            Engine reference; reference.setMode(mode); reference.setVolumeImmediate(1);
            std::vector<float> discard(size_t(cycle)*2),expected(size_t(window)*2);
            for(int n=0;n<3;++n) {
                reference.render(discard.data(),hitFrame,false); reference.apply(hit);
                const bool melody=mode==Mode::Expressive||mode==Mode::Keys;
                reference.render(discard.data(),cycle-hitFrame-(melody?1:0),false);
                if(melody) { reference.apply({EventType::NoteOff,0}); reference.render(discard.data(),1,false); }
            }
            reference.render(expected.data(),window,false);
            auto audio=render(*studio,cycle*2+window);
            // Skip the 64-frame seam blend and master gain startup.
            double actualPower=0,expectedPower=0;
            for(int n=128;n<window;++n) for(int ch=0;ch<2;++ch) {
                const float a=audio[size_t(cycle*2+n)*2+ch];
                const float b=expected[size_t(n)*2+ch]*s.volume;
                actualPower+=a*a; expectedPower+=b*b;
            }
            check(expectedPower>1.e-8,"a final-step instrument has a measurable natural tail");
            const double ratio=actualPower/expectedPower;
            if(ratio<.75||ratio>1.25) std::fprintf(stderr,"tail mode=%d pad=%d power ratio=%.5f\n",int(mode),pad,ratio);
            check(ratio>.75&&ratio<1.25,"cached final-step tails preserve uninterrupted instrument energy across the boundary");
            // The event-stream fallback must also leave tails running at wraps.
            loop.takeStops(); advance(loop,cycle); check(loop.takeStops()==0,"normal loop wraps preserve instrument tails");
            loop.command(LoopCommand::Stop,mode); check(loop.takeStops()!=0,"explicit transport stop still stops instruments");
        }
    }
}
static void savedProject() {
    auto original=tone(69,120); check(original->looper().save("cache-project.pi"),"cached scores save");
    auto loaded=std::make_unique<Studio>(); check(loaded->looper().load("cache-project.pi"),"cached scores reopen");
    Settings s; s.mode=Mode::Expressive; loaded->configure(s); loaded->prepareCaches();
    check(!loaded->view().playing,"preparing a reopened project keeps it paused");
    check(rms(render(*loaded,4800))==0,"background preparation stays silent while paused");
    loaded->apply({EventType::Loop,int(LoopCommand::Play)});
    check(rms(render(*loaded,4800))>.01f,"a reopened cached project starts with play");
    std::remove("cache-project.pi");
}
int main() {
    transportAndEdits(); pitchAndSeams(); naturalTails(); savedProject();
    std::printf("Cache behavior: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
