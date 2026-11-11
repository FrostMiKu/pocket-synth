#include "performance.hpp"
#include "sound_catalog.hpp"
#include <algorithm>
namespace ps {
namespace { constexpr double clearHoldSeconds=1; }
float Controller::clearProgress() const {
    return clearSince_<0?0:float(std::clamp((lastTime_-clearSince_)/clearHoldSeconds,0.,1.));
}
Input Controller::loopControls(const Input& raw,Settings& s,double now,Events& out) {
    Input playing=raw;
    auto command=[&](LoopCommand command) { Event e; e.type=EventType::Loop; e.id=int(command); out.add(e); };
    auto edge=[&](Button button) { return raw.held[button]&&!previous_.held[button]; };
    for(int b=0;b<ButtonCount;++b) if(!raw.held[b]) metaUsed_[b]=false;
    if(raw.held[Right]) {
        for(Button button:{A,B,X,Y,L1,R1,L2,R2}) if(edge(button)) metaUsed_[button]=true;
        if(edge(L2)) cycleSound(s,s.mode,-1);
        if(edge(R2)) cycleSound(s,s.mode,1);
        if(edge(mappedButton(A,s))) command(LoopCommand::Play);
        if(edge(mappedButton(B,s))) command(LoopCommand::Record);
        if(edge(mappedButton(X,s))) command(LoopCommand::Undo);
        const int tempoDirection=int(raw.held[R1]&&metaUsed_[R1])-int(raw.held[L1]&&metaUsed_[L1]);
        if(tempoDirection&&(edge(L1)||edge(R1)||now>=tempoRepeat_)) {
            const bool first=edge(L1)||edge(R1);
            s.tempo=std::clamp(s.tempo+tempoDirection,minTempo,maxTempo);
            tempoRepeat_=now+(first?.4:.08);
        }
    } else tempoRepeat_=now+.4;
    const bool clearing=raw.held[Right]&&raw.held[mappedButton(Y,s)]&&metaUsed_[mappedButton(Y,s)];
    if(!clearing) { clearSince_=-1; clearSent_=false; }
    else {
        if(clearSince_<0) clearSince_=now;
        if(!clearSent_&&now-clearSince_>=clearHoldSeconds) { command(LoopCommand::Clear); clearSent_=true; }
    }
    for(int b=0;b<ButtonCount;++b) if(metaUsed_[b]) playing.held[b]=false;
    return playing;
}
}
