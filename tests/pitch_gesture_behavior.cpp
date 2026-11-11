#include "studio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace ps;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) {
    ++checks;
    if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
}
int corrections(const Events& events,float* delta=nullptr) {
    int count=0;
    for(size_t n=0;n<events.count;++n) if(events.data[n].type==EventType::AttackPitch) {
        ++count; if(delta) *delta+=events.data[n].bend;
    }
    return count;
}
Input attack(Mode mode,bool strum=false) {
    Input input;
    if(mode==Mode::Expressive) { input.ry=-1; input.held[L1]=true; }
    else if(mode==Mode::Guitar&&strum) input.ry=.9f;
    else input.held[A]=true;
    return input;
}
void timing() {
    for(Mode mode:{Mode::Guitar,Mode::Keys,Mode::Expressive}) for(Button modifier:{Up,Down,Left})
    for(double delay:{.005,.015,.029,.031,.080}) for(bool strum:{false,true}) {
        if(strum&&mode!=Mode::Guitar) continue;
        Settings s; s.mode=mode; Controller c; Input input=attack(mode,strum);
        const auto onset=c.update(input,s,0);
        check(onset.count>0&&(onset.data[0].type==EventType::Pluck||onset.data[0].type==EventType::NoteOn),"a new note starts immediately");
        input.held[modifier]=true; float delta=0;
        const int count=corrections(c.update(input,s,delay),&delta);
        const int voices=strum?6:1;
        const int shift=modifier==Up?12:modifier==Down?-12:1;
        check(delay<.030?count==voices&&delta==voices*shift:count==0,
              "near-simultaneous modifiers join the attack while later presses preserve sustain");
        input.held[modifier]=false;
        check(corrections(c.update(input,s,delay+.001))==0,"releasing a modifier keeps the established pitch");
        input.held[modifier]=true;
        check(corrections(c.update(input,s,delay+.002))==0,"a modifier joins a gesture once");
    }
}
void chordsAndRelease() {
    Settings s; s.mode=Mode::Keys; Controller c; Input input; input.held[A]=true;
    c.update(input,s,0); input.held[B]=true; c.update(input,s,.1); input.held[Up]=true;
    auto events=c.update(input,s,.105); int count=0;
    for(size_t n=0;n<events.count;++n) if(events.data[n].type==EventType::AttackPitch) {
        ++count; check(events.data[n].id==1,"a late modifier affects the recent chord note while the older note sustains");
    }
    check(count==1,"independent chord notes have independent combination windows");
    for(Mode mode:{Mode::Guitar,Mode::Keys,Mode::Expressive}) {
        s.mode=mode; Controller released; input=attack(mode); released.update(input,s,0);
        input={}; released.update(input,s,.003); input.held[Up]=true;
        check(corrections(released.update(input,s,.005))==0,"released notes retain their tail even during the combination window");
    }
    s.mode=Mode::Keys; Controller combined; input=attack(s.mode); combined.update(input,s,0);
    input.held[Up]=true; float delta=0; corrections(combined.update(input,s,.005),&delta);
    input.held[Left]=true; corrections(combined.update(input,s,.010),&delta);
    check(delta==13,"late octave and semitone presses combine on one attack");
    input.held[Up]=input.held[Left]=false;
    check(corrections(combined.update(input,s,.015))==0,"releasing a combined modifier retains both pitch changes");
}
float periodicity(const std::vector<float>& audio,float hz) {
    const double lag=sampleRate/double(hz); double xy=0,xx=0,yy=0;
    for(int i=0;i<960-int(std::ceil(lag))-1;++i) {
        const double p=i+lag; const size_t n=size_t(p); const double f=p-n;
        const double a=audio[size_t(i)*2],b=audio[n*2]*(1-f)+audio[(n+1)*2]*f;
        xy+=a*b; xx+=a*a; yy+=b*b;
    }
    return float(xy/std::sqrt(xx*yy));
}
void immediatePitch() {
    const auto catalog=soundCatalog();
    for(auto sound=catalog.begin;sound!=catalog.end;++sound) if(sound->mode==Mode::Keys) for(Button modifier:{Up,Down,Left}) {
        Settings s; s.mode=Mode::Keys; Controller c; Engine engine; engine.setMode(s.mode); engine.setVolumeImmediate(1);
        Input input=attack(s.mode);
        auto submit=[&](double time) {
            const auto events=c.update(input,s,time);
            for(size_t n=0;n<events.count;++n) { auto event=events.data[n]; event.sound=sound->id; engine.apply(event); }
        };
        std::vector<float> audio(4096); submit(0); engine.render(audio.data(),384,false);
        input.held[modifier]=true; submit(.008); engine.render(audio.data(),2048,false);
        const float correlation=periodicity(audio,frequency(float(scaleNote(0,c.performanceSettings(s)))));
        check(correlation>.99f,"a late keyboard modifier reaches its target pitch directly during the first 20 ms");
        if(correlation<=.99f) std::fprintf(stderr,"%s modifier %d correlation %.5f\n",sound->key,int(modifier),correlation);
    }
}
void expressiveAndScheduled() {
    Settings s; s.mode=Mode::Expressive; Controller c; Input input=attack(s.mode);
    c.update(input,s,0); input.held[Up]=true;
    auto events=c.update(input,s,.005);
    check(events.data[events.count-2].note==72,"the expressive phrase immediately adopts a near-simultaneous octave");
    input.held[Up]=false; input.rx=1; input.ry=0;
    check(c.update(input,s,.01).data[0].note==76,"expressive wheel motion retains the completed gesture register");
    s.mode=Mode::Guitar; Controller guitar; input=attack(s.mode,true); const auto stroke=guitar.update(input,s,0);
    input.held[Left]=true; guitar.update(input,s,.005);
    for(int n=0;n<6;++n) check(guitar.stringStrikes()[n].note==stroke.data[n].note+1,"the whole scheduled strum shares the corrected pitch and visual feedback");
}
void boundaryGesture() {
    auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Keys; s.tempo=240; s.quantize=Quantize::Off;
    studio->configure(s); studio->apply({EventType::Loop,int(LoopCommand::Record)});
    std::vector<float> audio(size_t(sampleRate)*2); studio->render(audio.data(),sampleRate);
    studio->render(audio.data(),sampleRate-240);
    Controller c; Input input=attack(s.mode);
    auto submit=[&](double time) {
        const auto events=c.update(input,s,time);
        for(size_t n=0;n<events.count;++n) studio->apply(events.data[n]);
    };
    submit(0); studio->render(audio.data(),480);
    studio->prepareCaches();
    const auto revision=studio->looper().revision(s.mode);
    input.held[Up]=true; submit(.01);
    LoopTrackSnapshot snapshot; studio->looper().snapshot(s.mode,snapshot);
    check(snapshot.count>0&&snapshot.events[0].event.note==72,"a gesture completed across a loop boundary updates the committed attack");
    check(studio->looper().revision(s.mode)!=revision,"a boundary correction refreshes cached playback");
    studio->prepareCaches();
    check(studio->looper().save("gesture-boundary.performance"),"the completed boundary gesture saves");
    auto reopened=std::make_unique<Looper>(); check(reopened->load("gesture-boundary.performance"),"the completed boundary gesture reopens");
    reopened->snapshot(s.mode,snapshot);
    check(snapshot.events[0].event.note==72,"saved gestures keep the final pitch directly");
    std::remove("gesture-boundary.performance");
}
void recordedGesture() {
    for(Mode mode:{Mode::Guitar,Mode::Keys,Mode::Expressive}) {
        auto studio=std::make_unique<Studio>(); Settings s; s.mode=mode; s.tempo=240; s.quantize=Quantize::Off;
        studio->configure(s); studio->apply({EventType::Loop,int(LoopCommand::Record)});
        std::vector<float> audio(size_t(sampleRate)*2); studio->render(audio.data(),sampleRate);
        Controller c; Input input=attack(mode,mode==Mode::Guitar);
        auto submit=[&](double time) {
            const auto events=c.update(input,s,time);
            for(size_t n=0;n<events.count;++n) studio->apply(events.data[n]);
        };
        submit(0); studio->render(audio.data(),240); input.held[Up]=true; submit(.005);
        studio->render(audio.data(),2400); input={}; submit(.055);
        studio->apply({EventType::Loop,int(LoopCommand::Record)});
        LoopTrackSnapshot snapshot; studio->looper().snapshot(mode,snapshot); int attacks=0;
        for(size_t n=0;n<snapshot.count;++n) {
            const auto& e=snapshot.events[n].event;
            check(e.type!=EventType::AttackPitch,"recordings store final pitches directly");
            if(e.type==EventType::Pluck||e.type==EventType::NoteOn) {
                ++attacks;
                const int expected=mode==Mode::Guitar?guitarNote(chordBanks[0][0],e.id,s)+12:72;
                check(e.note==expected,"the recorded gesture uses the completed pitch for every attack");
            }
        }
        check(attacks==(mode==Mode::Guitar?6:1),"completing a gesture records one attack per note");
        studio->prepareCaches(); studio->apply({EventType::Loop,int(LoopCommand::Play)}); studio->render(audio.data(),sampleRate);
        check(std::any_of(audio.begin(),audio.end(),[](float x){return std::abs(x)>.001f;}),"completed gestures remain audible in cached playback");
    }
}
}
int main() {
    timing(); chordsAndRelease(); immediatePitch(); expressiveAndScheduled(); recordedGesture(); boundaryGesture();
    std::printf("Pitch gestures: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
