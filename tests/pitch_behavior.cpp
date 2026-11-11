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
struct Performance {
    Settings settings;
    Controller controller;
    Input input;
    Engine engine;
    double seconds=0;
    explicit Performance(const SoundPreset& sound) {
        settings.mode=sound.mode; settings.sounds[int(sound.mode)]=sound.id;
        engine.setMode(sound.mode); engine.setVolumeImmediate(1);
    }
    void update() {
        auto events=controller.update(input,settings,seconds);
        seconds+=.01;
        for(size_t n=0;n<events.count;++n) {
            auto e=events.data[n]; e.sound=selectedSound(settings,settings.mode).id;
            engine.apply(e);
        }
    }
    void gate(bool on) {
        if(settings.mode==Mode::Guitar) input.ry=on?.9f:0;
        else if(settings.mode==Mode::Expressive) { input.ry=-1; input.held[L1]=on; }
        else input.held[A]=on;
        update();
    }
    std::vector<float> render(int frames=2048) {
        std::vector<float> out(size_t(frames)*2); engine.render(out.data(),frames,false); return out;
    }
};
void sameAudio(Performance& actual,Performance& reference,const char* message,int frames=2048) {
    const auto a=actual.render(frames),b=reference.render(frames);
    double energy=0; float error=0;
    for(size_t n=0;n<a.size();++n) { error=std::max(error,std::abs(a[n]-b[n])); energy+=b[n]*b[n]; }
    check(energy>1.e-10&&error<1.e-7f,message);
}
void retainedPitch() {
    const auto catalog=soundCatalog();
    for(auto preset=catalog.begin;preset!=catalog.end;++preset) if(preset->mode!=Mode::Drums) {
        for(Button modifier:{Up,Down,Left}) {
            for(bool shiftedAttack:{false,true}) {
                Performance actual(*preset),reference(*preset);
                actual.input.held[modifier]=reference.input.held[modifier]=shiftedAttack;
                actual.gate(true); reference.gate(true);
                if(preset->mode==Mode::Guitar) {
                    // Compare one pluck against its own continuation after the
                    // random excitation, so natural attack variation stays musical.
                    actual.render(4096); reference.engine=actual.engine;
                }
                sameAudio(actual,reference,"matching attacks produce matching audio",48);
                actual.seconds=reference.seconds=.1;
                actual.input.held[modifier]=!shiftedAttack; actual.update();
                sameAudio(actual,reference,"pressing or releasing a modifier preserves held audio");
                actual.gate(false); reference.gate(false);
                actual.input.held[modifier]=shiftedAttack; actual.update();
                sameAudio(actual,reference,"modifier changes preserve released audio tails");
            }
        }
    }
}
void newAttacks() {
    for(Mode mode:{Mode::Guitar,Mode::Keys,Mode::Expressive}) for(Button modifier:{Up,Down,Left}) {
        Settings s; s.mode=mode; Controller c; Input input; double time=0;
        auto gate=[&](bool on) {
            if(mode==Mode::Guitar) input.ry=on?.9f:0;
            else if(mode==Mode::Expressive) { input.ry=-1; input.held[L1]=on; }
            else input.held[A]=on;
            time+=.1; return c.update(input,s,time);
        };
        input.held[modifier]=true;
        auto shifted=gate(true);
        const int delta=modifier==Up?12:modifier==Down?-12:1;
        const float base=float(mode==Mode::Guitar?guitarNote(chordBanks[0][0],0,s):scaleNote(0,s));
        check(shifted.data[0].note==base+delta,"a new attack includes the held modifier");
        input.held[modifier]=false; gate(true); gate(false);
        auto normal=gate(true);
        check(normal.data[0].note==base,"the next attack after modifier release uses the saved register");
        gate(false); input.held[Up]=input.held[Left]=true;
        check(gate(true).data[0].note==base+13,"octave and semitone combine on a fresh attack");
    }
}
void independentKeys() {
    Settings s; s.mode=Mode::Keys; Controller c; Input input; input.held[A]=true;
    c.update(input,s,0); input.held[Up]=true; input.held[B]=true;
    auto second=c.update(input,s,.1);
    check(second.data[0].type==EventType::NoteOn&&second.data[0].id==1&&second.data[0].note==74,
          "a new upper-octave note joins the sustained lower-octave note");
    input.held[Up]=false; input.held[X]=true;
    auto third=c.update(input,s,.2);
    check(third.data[0].id==2&&third.data[0].note==64,"each note in a chord captures its own starting register");
}
void expressivePhrase() {
    for(bool continuous:{false,true}) for(bool holdGate:{false,true}) {
        Settings s; s.mode=Mode::Expressive; s.continuousPitch=continuous; s.holdGate=holdGate;
        Controller c; Input input; input.ry=-1; input.held[L1]=true; input.held[Up]=input.held[Left]=true;
        const float first=c.update(input,s,0).data[0].note;
        input.held[Up]=input.held[Left]=false; input.rx=.3f; input.ry=-.9f;
        const auto moving=c.update(input,s,.01);
        Settings phrase=s; phrase.octave+=1; phrase.root+=1;
        const float expected=continuous?continuousWheelNote(input.rx,input.ry,phrase):float(scaleNote(c.wheel(),phrase));
        check(first==73&&std::abs(moving.data[0].note-expected)<.001f,"wheel motion keeps the starting register in either gate mode");
        if(holdGate) input.held[L1]=false; else input.rx=input.ry=0;
        c.update(input,s,.02); input.rx=0; input.ry=-1; input.held[L1]=true;
        check(c.update(input,s,.03).data[0].note==60,"a fresh expressive phrase adopts the current modifiers");
    }
}
void recordedPitch() {
    for(Mode mode:{Mode::Keys,Mode::Expressive}) {
        auto studio=std::make_unique<Studio>(); Settings s; s.mode=mode; s.tempo=240;
        studio->configure(s); studio->apply({EventType::Loop,int(LoopCommand::Record)});
        std::vector<float> audio(size_t(sampleRate)*2); studio->render(audio.data(),sampleRate);
        Controller c; Input input; input.held[Up]=input.held[Left]=true;
        if(mode==Mode::Keys) input.held[A]=true; else { input.ry=-1; input.held[L1]=true; }
        auto submit=[&](double time) {
            const auto events=c.update(input,s,time);
            for(size_t n=0;n<events.count;++n) studio->apply(events.data[n]);
        };
        submit(0); studio->render(audio.data(),2400);
        input.held[Up]=input.held[Left]=false; submit(.05); studio->render(audio.data(),2400);
        input={}; submit(.1); studio->apply({EventType::Loop,int(LoopCommand::Record)});
        LoopTrackSnapshot snapshot; studio->looper().snapshot(mode,snapshot);
        int attacks=0;
        for(size_t n=0;n<snapshot.count;++n) if(snapshot.events[n].event.type==EventType::NoteOn) {
            ++attacks; check(snapshot.events[n].event.note==73,"the recorded score retains the performed pitch");
        }
        check(attacks==1,"modifier release records one sustained note");
        studio->prepareCaches(); studio->apply({EventType::Loop,int(LoopCommand::Play)});
        studio->render(audio.data(),sampleRate);
        check(std::any_of(audio.begin(),audio.end(),[](float x){return std::abs(x)>.001f;}),"the retained pitch remains audible in cached playback");
    }
}
}
int main() {
    retainedPitch(); newAttacks(); independentKeys(); expressivePhrase(); recordedPitch();
    std::printf("Pitch behavior: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
