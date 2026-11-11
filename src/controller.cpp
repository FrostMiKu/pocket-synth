#include "performance.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
Events Controller::reset() {
    clearSince_=-1; clearSent_=false;
    chordStickActive_=false;
    stringStrikes_.fill({}); stroke_=0; strength_=0;
    notes_.fill(false); wheel_=-1; armed_=true; expressionSounding_=false; modifiers_=0; recentAttacks_.fill({});
    Events out; out.add({EventType::Silence}); return out;
}
Settings Controller::performanceSettings(const Settings& saved) const {
    Settings playing=saved;
    playing.octave=modifiedOctave(saved.octave,modifiers_);
    playing.root+=int(bool(modifiers_&SemitoneUp));
    return playing;
}
void Controller::readPitchModifiers(const Input& i) {
    modifiers_=int(!i.held[Right]&&i.held[Up]&&!metaUpUsed_)*OctaveUp
        |int(!i.held[Right]&&i.held[Down]&&!metaDownUsed_)*OctaveDown
        |int(i.held[Left])*SemitoneUp;
}
void Controller::updatePitchControls(const Input& i,Settings& s) {
    if(!i.held[Up]) metaUpUsed_=false;
    if(!i.held[Down]) metaDownUsed_=false;
    if(i.held[Right]) {
        const bool upPressed=i.held[Up]&&(!previous_.held[Up]||!previous_.held[Right]);
        const bool downPressed=i.held[Down]&&(!previous_.held[Down]||!previous_.held[Right]);
        const int direction=int(i.held[Up])-int(i.held[Down]);
        if(upPressed||downPressed) s.octave=std::clamp(s.octave+direction,minOctave,maxOctave);
        metaUpUsed_=metaUpUsed_||i.held[Up];
        metaDownUsed_=metaDownUsed_||i.held[Down];
    }
    readPitchModifiers(i);
}
void Controller::edit(Settings& s, int d) {
    switch(SettingRow(menuRow_)) {
    case SettingRow::Root: s.root=(s.root+d+12)%12; break;
    case SettingRow::Scale: s.scale=Scale((int(s.scale)+d+scaleCount)%scaleCount); break;
    case SettingRow::Octave: s.octave=std::clamp(s.octave+d,minOctave,maxOctave); break;
    case SettingRow::Volume: s.volume=std::clamp(s.volume+d*.05f,0.f,1.f); break;
    case SettingRow::ExpressionPitch: s.continuousPitch=!s.continuousPitch; break;
    case SettingRow::ExpressionGate: s.holdGate=!s.holdGate; break;
    case SettingRow::NoteDisplay: s.noteDisplay=s.noteDisplay==NoteDisplay::Names?NoteDisplay::Numbered:NoteDisplay::Names; break;
    case SettingRow::FaceButtons: s.swapFaceButtons=!s.swapFaceButtons; break;
    case SettingRow::Tempo: s.tempo=std::clamp(s.tempo+d,minTempo,maxTempo); break;
    case SettingRow::LoopBars: s.loopBars=s.loopBars==1?2:1; break;
    case SettingRow::Quantize: s.quantize=Quantize((int(s.quantize)+d+3)%3); break;
    case SettingRow::Count: break;
    }
}
Events Controller::update(const Input& i, Settings& s, double now) {
    Events out;
    if(hasMode_&&observedMode_!=s.mode) out=reset();
    observedMode_=s.mode; hasMode_=true;
    auto pressed=[&](Button b){ return i.held[b]&&!previous_.held[b]; };
    const float dt=float(std::clamp(now-lastTime_,.001,.1));
    lastTime_=now;
    if(quit_||(i.held[Select]&&i.held[Start])) {
        quit_=true; out=reset(); previous_=i; return out;
    }
    if(pressed(Start)) { menu_=!menu_; out=reset(); }
    else if(pressed(Select)) { s.mode=Mode((int(s.mode)+1)%modeCount); out=reset(); }
    observedMode_=s.mode;
    if(menu_) {
        if(pressed(Down)) menuRow_=(menuRow_+1)%settingCount;
        if(pressed(Up)) menuRow_=(menuRow_+settingCount-1)%settingCount;
        if(pressed(Left)) edit(s,-1);
        if(pressed(Right)) edit(s,1);
        observedMode_=s.mode; previous_=i; return out;
    }
    const Input performance=loopControls(i,s,now,out);
    const int priorModifiers=modifiers_;
    updatePitchControls(i,s);
    completePitchGestures(i,performance,s,now,modifiers_&~priorModifiers,out);
    const Settings playing=performanceSettings(s);
    if(s.mode==Mode::Guitar) {
        playGuitar(i,performance,s,playing,now,dt,out);
    } else if(s.mode==Mode::Drums) {
        for(size_t n=0;n<instrumentButtons.size();++n) {
            const Button button=instrumentButton(n,s);
            if(performance.held[button]&&!previous_.held[button]) {
                Event e; e.type=EventType::Drum; e.id=int(n);
                e.velocity=std::clamp(.7f-i.ly*.3f,.2f,1.f); e.timbre=(i.lx+1)*.5f; out.add(e);
            }
        }
    } else {
        if(s.mode==Mode::Expressive) {
            const float radius=std::hypot(i.rx,i.ry);
            bool selected=radius>(wheel_>=0?.25f:.4f);
            wheel_=selected?stableSector(i.rx,i.ry,wheel_):-1;
            if(selected) previewPitch_=s.continuousPitch?continuousWheelNote(i.rx,i.ry,playing):float(scaleNote(wheel_,playing));
            bool gate=selected&&(!s.holdGate||performance.held[expressionGateButton]);
            if(gate) {
                const int transpose=playing.root+12*playing.octave;
                if(!expressionSounding_) expressionTranspose_=transpose;
                Event e; e.type=EventType::NoteOn; e.id=0;
                e.note=previewPitch_+float(expressionTranspose_-transpose); e.velocity=std::clamp(radius,.15f,1.f);
                if(!expressionSounding_) startAttack(e,s,now);
                else e.gesture=recentAttacks_[0].gesture;
                out.add(e); expressionSounding_=true;
            } else if(expressionSounding_) { out.add({EventType::NoteOff,0}); expressionSounding_=false; }
        } else {
            for(size_t n=0;n<instrumentButtons.size();++n) {
                const bool held=performance.held[instrumentButton(n,s)];
                if(held&&!notes_[n]) {
                    Event e; e.type=EventType::NoteOn; e.id=int(n);
                    e.note=float(scaleNote(int(n),playing));
                    startAttack(e,s,now,instrumentButton(n,s)); out.add(e);
                } else if(!held&&notes_[n]) out.add({EventType::NoteOff,int(n)});
                notes_[n]=held;
            }
        }
        Event e; e.type=EventType::Expression;
        e.timbre=(i.lx+1)*.5f; e.vibrato=std::max(0.f,-i.ly);
        e.bend=s.mode==Mode::Keys?i.lx*2:0; out.add(e);
    }
    previous_=i;
    return out;
}
}
