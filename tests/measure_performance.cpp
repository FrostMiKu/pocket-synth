#include "studio.hpp"
#include "audio_float.hpp"
#include "loop_demo.hpp"
#include <chrono>
#include <memory>
#include <cstdio>
#include <string>
using namespace ps;
int main(int argc,char** argv) {
    const std::string scene=argc>1?argv[1]:"full";
    configureAudioFloat();
    auto studio=std::make_unique<Studio>(); auto& loop=studio->looper(); Settings initial; loop.configure(initial);
    auto advance=[&](int frames) { for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) {} loop.advance(); } };
    constexpr int bar=sampleRate*2;
    loop.command(LoopCommand::Record,Mode::Drums); advance(bar);
    for(int step=0;step<16;++step) { Event hit; hit.type=EventType::Drum; hit.id=step%4==0?0:step%4==2?1:2; loop.record(Mode::Drums,hit); advance(bar/16); }
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys}) for(int take=0;take<loopLayerCount-(scene=="live-overdub"&&mode==Mode::Keys?1:0);++take) {
        if(scene=="guitar"&&mode!=Mode::Guitar) continue;
        if(scene=="melody"&&mode==Mode::Guitar) continue;
        if(scene=="drums") continue;
        loop.command(LoopCommand::Record,mode); advance(bar);
        Event expression; expression.type=EventType::Expression; expression.timbre=.3f; expression.vibrato=.5f;
        if(mode!=Mode::Guitar) loop.record(mode,expression);
        const int voices=mode==Mode::Guitar?stringCount:mode==Mode::Keys?8:1;
        for(int n=0;n<voices;++n) {
            Event e; e.type=mode==Mode::Guitar?EventType::Pluck:EventType::NoteOn; e.id=n;
            e.note=mode==Mode::Guitar?float(chordBanks[0][0].notes[n]):float(60+n*2+take); e.velocity=.5f;
            loop.record(mode,e);
        }
        advance(bar*3/4);
        loop.record(mode,{EventType::Silence}); advance(bar/4);
    }
    Settings s;
    if(scene=="live-keys"||scene=="live-overdub") { s.mode=Mode::Keys; s.sounds[int(s.mode)]=findSound("electric-piano",s.mode)->id; }
    if(scene=="live-expressive") { s.mode=Mode::Expressive; s.sounds[int(s.mode)]=findSound("violin",s.mode)->id; }
    studio->configure(s);
    if(s.mode!=Mode::Guitar) { Event e; e.type=EventType::Expression; e.timbre=.8f; e.vibrato=1; studio->apply(e); }
    const auto preparing=std::chrono::steady_clock::now(); studio->prepareCaches();
    const auto prepareMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-preparing).count();
    std::printf("Cache preparation: %lld ms\n",static_cast<long long>(prepareMs));
    std::array<float,audioFrames*2> out{};
    if(scene=="live-overdub") {
        auto renderBar=[&] { for(int n=0;n<bar;n+=audioFrames) studio->render(out.data(),size_t(std::min(audioFrames,bar-n))); };
        studio->apply({EventType::Loop,int(LoopCommand::Record)}); renderBar();
        Event expression; expression.type=EventType::Expression; expression.timbre=.8f; expression.vibrato=1; studio->apply(expression);
        for(int n=0;n<8;++n) { Event e; e.type=EventType::NoteOn; e.id=n; e.note=float(60+n*2); studio->apply(e); }
        renderBar();
    }
    long long maximum=0,total=0; int late=0,blocks=sampleRate*8/audioFrames;
    for(int block=0;block<blocks;++block) {
        if(block%24==0) {
            const int count=s.mode==Mode::Guitar?stringCount:s.mode==Mode::Keys?8:1;
            for(int n=0;n<count;++n) {
                Event e; e.type=s.mode==Mode::Guitar?EventType::Pluck:EventType::NoteOn; e.id=n;
                e.note=s.mode==Mode::Guitar?float(chordBanks[0][0].notes[n]):float(60+n*2);
                if(s.mode!=Mode::Guitar) studio->apply({EventType::NoteOff,n});
                studio->apply(e);
            }
        }
        const auto start=std::chrono::steady_clock::now(); studio->render(out.data(),audioFrames);
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        maximum=std::max(maximum,static_cast<long long>(us)); total+=us; if(us>audioFrames*1000000/sampleRate) ++late;
    }
    std::printf("Scene %s with live instrument: blocks=%d mean_us=%lld max_us=%lld over_budget=%d budget_us=%d\n",scene.c_str(),blocks,total/blocks,maximum,late,audioFrames*1000000/sampleRate);
    return late?1:0;
}
