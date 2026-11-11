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
bool has(const Events& events,EventType type) {
    for(size_t n=0;n<events.count;++n) if(events.data[n].type==type) return true;
    return false;
}
void singleStrings() {
    constexpr Button normal[]={A,B,X,Y,L1,R1},swapped[]={B,A,Y,X,L1,R1};
    constexpr int cMajor[]={48,52,55,60,64,67};
    for(bool swap:{false,true}) for(int string=0;string<6;++string) {
        Settings s; s.swapFaceButtons=swap; Controller controller; Input input;
        const Button key=swap?swapped[string]:normal[string]; input.held[key]=true;
        const auto pick=controller.update(input,s,0);
        check(pick.count==1&&pick.data[0].type==EventType::Pluck&&pick.data[0].id==string
              &&pick.data[0].note==cMajor[string]&&pick.data[0].delay==0,
              "each button immediately picks its assigned string in either face-button layout");
        check(s.chordBank==0,"manual picks preserve the selected chord bank");
        const auto strike=controller.stringStrikes()[string];
        check(strike.startsAt==0&&strike.note==cMajor[string]&&strike.strength>0,"string feedback follows the manual pick");
        check(controller.update(input,s,.01).count==0,"holding a string button produces one attack");
        Engine engine; engine.apply(pick.data[0]);
        std::vector<float> audio(8192); engine.render(audio.data(),4096);
        check(std::any_of(audio.begin(),audio.end(),[](float x){return std::abs(x)>.001f;}),"each manually picked string produces audible output");
        input={}; check(controller.update(input,s,.02).count==0,"releasing the button lets the picked string ring");
        input.held[key]=true;
        check(controller.update(input,s,.03).count==1,"a fresh button press picks the string again");
    }
}
void banksAndModifiers() {
    Settings s; Controller controller; Input input; input.held[L2]=true;
    check(controller.update(input,s,0).count==0&&s.chordBank==1,"L2 selects the next chord bank");
    check(controller.update(input,s,.01).count==0&&s.chordBank==1,"holding L2 selects once");
    input.held[Y]=true;
    check(controller.update(input,s,.02).data[0].note==59,"a manual pick uses the newly selected chord bank");
    input={}; controller.update(input,s,.03); input.held[L2]=true; controller.update(input,s,.04);
    check(s.chordBank==0,"the next L2 press returns to the first chord bank");
    input={}; controller.update(input,s,.05); input.lx=1; input.held[Up]=input.held[Left]=input.held[A]=true;
    auto raised=controller.update(input,s,.06);
    check(raised.count==1&&raised.data[0].note==58,"manual picks follow chord selection and combined pitch modifiers");
    input.held[Up]=input.held[Left]=false;
    check(controller.update(input,s,.07).count==0&&controller.stringStrikes()[0].note==58,"modifier release preserves the picked note and its feedback");
    input.held[A]=false; controller.update(input,s,.08); input.held[A]=true;
    check(controller.update(input,s,.09).data[0].note==45,"the next pick uses the current chord and base register");
}
void combinations() {
    Settings s; Controller controller; Input input;
    input.held[A]=input.held[L1]=input.held[R1]=true;
    auto chord=controller.update(input,s,0);
    check(chord.count==3&&chord.data[0].id==0&&chord.data[1].id==4&&chord.data[2].id==5,"several buttons pick several strings together");
    check(s.chordBank==0,"shoulder picks keep the current chord bank");
    input={}; controller.update(input,s,.01); input.held[A]=true; input.ry=.9f;
    auto mixed=controller.update(input,s,.02); int bassAttacks=0;
    for(size_t n=0;n<mixed.count;++n) if(mixed.data[n].id==0) ++bassAttacks;
    check(mixed.count==6&&bassAttacks==1,"a simultaneous manual pick and sweep attack each string once");
    input.held[R2]=input.held[B]=true; auto mute=controller.update(input,s,.03);
    check(mute.count==1&&mute.data[0].type==EventType::Silence,"R2 mutes both picked strings and the sweep");
    input.held[X]=true;
    check(controller.update(input,s,.04).count==0,"held mute gives priority to silence");
    input={}; controller.update(input,s,.05); input.held[R1]=true;
    check(controller.update(input,s,.06).count==1,"fresh picks resume after mute release");
}
void shortcuts() {
    for(bool swap:{false,true}) for(Button key:{A,B,X,Y,L1,R1,L2,R2}) {
        Settings s; s.swapFaceButtons=swap; Controller c; Input input; input.held[Right]=true;
        c.update(input,s,0); input.held[key]=true; auto command=c.update(input,s,.01);
        check(!has(command,EventType::Pluck)&&s.chordBank==0,"Meta shortcuts keep strings and chord banks at rest");
        input.held[Right]=false; auto released=c.update(input,s,.02);
        check(!has(released,EventType::Pluck)&&s.chordBank==0,"releasing Meta first keeps the command button consumed");
        input.held[key]=false; c.update(input,s,.03); input.held[key]=true;
        auto fresh=c.update(input,s,.04);
        if(key==L2) check(s.chordBank==1,"L2 returns to chord bank control after shortcut release");
        else if(key!=R2) check(has(fresh,EventType::Pluck),"a fresh press after a shortcut returns to manual picking");
    }
}
void recordedPicks() {
    auto studio=std::make_unique<Studio>(); Settings s; s.tempo=240; s.quantize=Quantize::Off;
    studio->configure(s); studio->apply({EventType::Loop,int(LoopCommand::Record)});
    std::vector<float> audio(size_t(sampleRate)*2); studio->render(audio.data(),sampleRate);
    Controller controller; constexpr Button phrase[]={A,X,L1,R1};
    for(int n=0;n<4;++n) {
        Input input; input.held[phrase[n]]=true;
        const auto events=controller.update(input,s,n*.1);
        for(size_t i=0;i<events.count;++i) studio->apply(events.data[i]);
        studio->render(audio.data(),2400); controller.update({},s,n*.1+.05); studio->render(audio.data(),2400);
    }
    studio->apply({EventType::Loop,int(LoopCommand::Record)});
    LoopTrackSnapshot snapshot; studio->looper().snapshot(Mode::Guitar,snapshot);
    check(snapshot.count==4,"a four-pick phrase records four attacks");
    constexpr int strings[]={0,2,4,5};
    for(size_t n=0;n<snapshot.count;++n) check(snapshot.events[n].event.id==strings[n]
        &&std::abs(snapshot.events[n].step-double(n)*1.6)<.001,"recorded fingerpicking retains each string and its timing");
    studio->prepareCaches(); studio->apply({EventType::Loop,int(LoopCommand::Play)});
    studio->render(audio.data(),sampleRate);
    check(std::any_of(audio.begin(),audio.end(),[](float x){return std::abs(x)>.001f;}),"cached Loop playback plays the manually picked phrase");
}
}
int main() {
    singleStrings(); banksAndModifiers(); combinations(); shortcuts(); recordedPicks();
    std::printf("Guitar behavior: %d checks, %d failures\n",checks,failures); return failures?1:0;
}
