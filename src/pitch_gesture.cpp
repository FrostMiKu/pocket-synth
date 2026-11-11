#include "performance.hpp"
#include "guitar_controls.hpp"
#include <cmath>
namespace ps {
void Controller::startAttack(Event& event,const Settings& s,double now,Button key) {
    event.gesture=++nextGesture_;
    recentAttacks_[event.id]={now,s.octave,modifiers_,key,event.gesture};
}
void Controller::completePitchGestures(const Input& raw,const Input& performance,
                                      const Settings& s,double now,int added,Events& out) {
    if(s.mode==Mode::Drums||!added) return;
    for(size_t n=0;n<recentAttacks_.size();++n) {
        auto& attack=recentAttacks_[n];
        if(attack.started<0||now-attack.started>pitchGestureSeconds) continue;
        bool held=false;
        if(s.mode==Mode::Keys) held=notes_[n]&&performance.held[instrumentButton(n,s)];
        else if(s.mode==Mode::Expressive) held=expressionSounding_&&std::hypot(raw.rx,raw.ry)>.25f
            &&(!s.holdGate||performance.held[expressionGateButton]);
        else held=!performance.held[guitarMuteButton]&&(attack.key==ButtonCount
            ?std::abs(raw.ry)>stickNeutral:performance.held[attack.key]&&previous_.held[attack.key]);
        if(!held) continue;
        const int combined=attack.modifiers|added;
        const int delta=modifierPitch(attack.octave,combined)-modifierPitch(attack.octave,attack.modifiers);
        attack.modifiers=combined;
        if(delta==0) continue;
        Event event; event.type=EventType::AttackPitch; event.id=int(n);
        event.gesture=attack.gesture; event.bend=float(delta); out.add(event);
        if(s.mode==Mode::Expressive) expressionTranspose_+=delta;
        else if(s.mode==Mode::Guitar) stringStrikes_[n].note+=delta;
    }
}
}
