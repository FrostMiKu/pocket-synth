#include "studio.hpp"
#include "settings.hpp"
#include "sound_catalog.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <vector>
using namespace ps;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) { ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); } }
std::vector<float> render(Engine& engine,int frames) {
    std::vector<float> out(size_t(frames)*2); engine.render(out.data(),frames,false);
    check(std::all_of(out.begin(),out.end(),[](float f){return std::isfinite(f)&&std::abs(f)<2;}),"every sound produces finite bounded output");
    return out;
}
float rms(const std::vector<float>& audio) { double power=0; for(float v:audio) power+=v*v; return float(std::sqrt(power/audio.size())); }
void advance(Looper& loop,int frames) {
    for(int n=0;n<frames;++n) { Mode mode; Event e; while(loop.next(mode,e)) {} loop.advance(); }
}
Event note(const SoundPreset& preset) { Event e; e.type=EventType::NoteOn; e.note=69; e.velocity=.8f; e.sound=preset.id; return e; }
void catalogAndSelection() {
    std::set<SoundId> ids; std::set<std::string> keys;
    const auto catalog=soundCatalog();
    for(auto p=catalog.begin;p!=catalog.end;++p) {
        check(p->id>0&&ids.insert(p->id).second&&keys.insert(p->key).second,"catalog identities are unique and stable");
        check(findSound(p->id)==p&&findSound(p->key,p->mode)==p,"sound identities resolve through both memory and saved keys");
        check(p->attack>0&&p->release>0&&p->tailSeconds>0,"sound envelopes have valid times");
    }
    Settings s;
    for(int mode=0;mode<modeCount;++mode) {
        const auto instrument=Mode(mode); int defaults=0;
        for(auto p=catalog.begin;p!=catalog.end;++p) defaults+=p->mode==instrument&&p->isDefault;
        check(defaults==1&&soundCount(instrument)>0,"each instrument has one explicit default sound");
        const auto first=selectedSound(s,instrument).id;
        for(size_t n=0;n<soundCount(instrument);++n) cycleSound(s,instrument,1);
        check(selectedSound(s,instrument).id==first,"sound navigation wraps using the catalog size");
        cycleSound(s,instrument,-1); cycleSound(s,instrument,1);
        check(selectedSound(s,instrument).id==first,"previous and next sound navigation are inverses");
        cycleSound(s,instrument,1);
    }
    check(saveSettings("sounds.cfg",s),"independent instrument sound selections save");
    const auto loaded=loadSettings("sounds.cfg");
    for(int mode=0;mode<modeCount;++mode) check(selectedSound(loaded,Mode(mode)).id==selectedSound(s,Mode(mode)).id,"reopening settings restores each instrument sound");
    std::remove("sounds.cfg");
}
void shortcuts() {
    for(int mode=0;mode<modeCount;++mode) for(Button key:{L2,R2}) {
        Controller c; Settings s; s.mode=Mode(mode); Input input; input.held[Right]=true; c.update(input,s,0);
        auto expected=s; cycleSound(expected,s.mode,key==R2?1:-1);
        input.held[key]=true; auto events=c.update(input,s,.01);
        check(selectedSound(s,s.mode).id==selectedSound(expected,s.mode).id,"Meta triggers the requested sound direction");
        for(size_t n=0;n<events.count;++n) check(events.data[n].type!=EventType::Silence&&events.data[n].type!=EventType::Pluck&&events.data[n].type!=EventType::Drum&&events.data[n].type!=EventType::NoteOn,"sound navigation preserves guitar resonance and produces no stray notes");
        const auto selected=selectedSound(s,s.mode).id; c.update(input,s,2);
        check(selectedSound(s,s.mode).id==selected,"holding a sound shortcut selects once");
        input.held[Right]=false; events=c.update(input,s,2.01);
        for(size_t n=0;n<events.count;++n) check(events.data[n].type!=EventType::Silence&&events.data[n].type!=EventType::Drum&&events.data[n].type!=EventType::NoteOn,"releasing Meta first keeps the shoulder key consumed");
    }
}
void soundIdentity() {
    const auto& flute=*findSound("flute",Mode::Expressive); const auto& violin=*findSound("violin",Mode::Expressive);
    Engine changed,reference; changed.setMode(Mode::Expressive); reference.setMode(Mode::Expressive);
    changed.setVolumeImmediate(1); reference.setVolumeImmediate(1); changed.apply(note(flute)); reference.apply(note(flute));
    render(changed,12000); render(reference,12000);
    check(changed.apply(note(violin)).sound==flute.id,"an already held note keeps its original sound");
    auto a=render(changed,4096),b=render(reference,4096); float delta=0;
    for(size_t n=0;n<a.size();++n) delta=std::max(delta,std::abs(a[n]-b[n]));
    check(delta<1.e-6f,"selecting another sound leaves a held note audibly unchanged");
    changed.apply({EventType::NoteOff,0});
    check(changed.apply(note(violin)).sound==violin.id,"a new attack adopts the newly selected sound");
    auto transition=render(changed,1);
    check(std::abs(transition[0]-a[a.size()-2])<1.e-6f,"rearticulating a sound preserves continuity at the first sample");
    std::vector<std::vector<float>> sounds;
    std::map<std::string,size_t> soundIndex;
    const auto catalog=soundCatalog();
    for(auto p=catalog.begin;p!=catalog.end;++p) if(p->model==SoundModel::Sustained||p->model==SoundModel::Struck) {
        Engine engine; engine.setMode(p->mode); engine.setVolumeImmediate(1); engine.apply(note(*p));
        render(engine,2400); auto early=render(engine,12000); check(rms(early)>.003f,"each melodic sound is audible");
        render(engine,sampleRate*3); auto late=render(engine,12000);
        if(p->model==SoundModel::Struck) check(rms(late)<rms(early)*.5f,"struck sounds decay naturally while their key remains held");
        else check(rms(late)>rms(early)*.6f,"expressive sounds sustain during a held note");
        soundIndex[p->key]=sounds.size(); sounds.push_back(early); engine.apply({EventType::Silence}); render(engine,4800);
        check(rms(render(engine,1024))==0,"global stop silences every melodic sound");
    }
    // The initial instrument families have distinct identities; future close
    // variations remain free to share a character with an existing preset.
    for(const auto& pair: {std::pair{"flute","violin"},std::pair{"piano","electric-piano"},
                          std::pair{"flute","piano"},std::pair{"violin","electric-piano"}}) {
        const size_t a=soundIndex.at(pair.first),b=soundIndex.at(pair.second);
        double difference=0,power=0;
        for(size_t n=0;n<sounds[a].size();++n) { const double d=sounds[a][n]-sounds[b][n]; difference+=d*d; power+=sounds[a][n]*sounds[a][n]; }
        check(difference>power*.08,"different catalog sounds produce clearly distinct waveforms");
    }
}
void steppedKeys() {
    const auto catalog=soundCatalog();
    for(auto p=catalog.begin;p!=catalog.end;++p) if(p->mode==Mode::Keys) for(Button modifier:{Up,Down,Left}) {
        auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Keys; s.sounds[int(s.mode)]=p->id;
        Controller controller; Input input; input.held[A]=true; studio->configure(s);
        auto submit=[&](double time) { auto events=controller.update(input,s,time); studio->configure(s); for(size_t i=0;i<events.count;++i) studio->apply(events.data[i]); };
        std::vector<float> audio(24000); submit(0); studio->render(audio.data(),12000);
        input.held[modifier]=true; submit(.25);
        const float expected=frequency(float(scaleNote(0,controller.performanceSettings(s))));
        audio.resize(4096); studio->render(audio.data(),2048);
        // A stepped note is periodic at the target frequency from the change.
        // Compare waveform periods during the first 20 ms, before a glide could settle.
        auto periodicity=[&](float hz) {
            const double lag=double(sampleRate)/hz; double xy=0,xx=0,yy=0;
            for(int i=0;i<960-int(std::ceil(lag))-1;++i) {
                const double position=i+lag; const size_t j=size_t(position); const double f=position-j;
                const double x=audio[size_t(i)*2],y=audio[j*2]*(1-f)+audio[(j+1)*2]*f;
                xy+=x*y; xx+=x*x; yy+=y*y;
            }
            return xy/std::sqrt(xx*yy);
        };
        const double correlation=periodicity(expected);
        if(correlation<.995) std::fprintf(stderr,"key pitch %s modifier=%d correlation=%.5f\n",p->key,int(modifier),correlation);
        check(correlation>.995,"a late octave or semitone modifier reaches the target keyboard pitch immediately");
        input.held[modifier]=false; submit(.3); studio->render(audio.data(),2048);
        check(periodicity(frequency(float(scaleNote(0,s))))>.995,"releasing a temporary modifier immediately restores the keyboard pitch");
    }
}
void recordedSounds() {
    auto studio=std::make_unique<Studio>(); Settings s; s.mode=Mode::Keys; s.tempo=240; studio->configure(s);
    const auto piano=selectedSound(s,s.mode).id; studio->apply({EventType::Loop,int(LoopCommand::Record)});
    std::vector<float> scratch(size_t(sampleRate)*2); studio->render(scratch.data(),sampleRate);
    Event on; on.type=EventType::NoteOn; studio->apply(on); studio->render(scratch.data(),2400);
    cycleSound(s,s.mode,1); studio->configure(s); studio->apply(on);
    studio->apply({EventType::NoteOff,0}); studio->apply(on);
    studio->apply({EventType::Loop,int(LoopCommand::Record)});
    LoopTrackSnapshot snapshot; studio->looper().snapshot(s.mode,snapshot);
    std::vector<SoundId> recorded;
    for(size_t n=0;n<snapshot.count;++n) if(snapshot.events[n].event.type==EventType::NoteOn) recorded.push_back(snapshot.events[n].event.sound);
    check(recorded.size()==2&&recorded[0]==piano&&recorded[1]==selectedSound(s,s.mode).id,"a take retains the actual sound of each new attack, including mid-take selection");
    check(studio->looper().save("sounds.performance"),"a multi-sound performance saves");
    auto loaded=std::make_unique<Looper>(); check(loaded->load("sounds.performance"),"a multi-sound performance reopens");
    loaded->snapshot(s.mode,snapshot); size_t n=0;
    for(size_t i=0;i<snapshot.count;++i) if(snapshot.events[i].event.type==EventType::NoteOn) check(snapshot.events[i].event.sound==recorded[n++],"saved performances retain stable sound identities");
    cycleSound(s,s.mode,1); studio->configure(s); studio->looper().snapshot(s.mode,snapshot);
    check(snapshot.events[0].event.sound==piano,"changing the live sound preserves recorded sound identities");
    auto first=std::make_unique<Studio>(),second=std::make_unique<Studio>();
    check(first->looper().load("sounds.performance")&&second->looper().load("sounds.performance"),"the same recorded sounds reopen for comparison");
    Settings other=s; cycleSound(other,other.mode,1); first->configure(s); second->configure(other);
    first->prepareCaches(); second->prepareCaches(); first->apply({EventType::Loop,int(LoopCommand::Play)}); second->apply({EventType::Loop,int(LoopCommand::Play)});
    std::vector<float> left(24000),right(24000); first->render(left.data(),12000); second->render(right.data(),12000);
    float error=0; for(size_t i=0;i<left.size();++i) error=std::max(error,std::abs(left[i]-right[i]));
    check(error<1.e-6f,"cached playback preserves the recorded sounds regardless of the current live selection");
    std::remove("sounds.performance");
}
}
int main() {
    catalogAndSelection(); shortcuts(); soundIdentity(); steppedKeys(); recordedSounds();
    std::printf("Sound behavior: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
