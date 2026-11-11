#include "engine.hpp"
#include "settings.hpp"
#include "input_guard.hpp"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <vector>
using namespace ps;
static int failures=0,checks=0;
static void check(bool ok,const char* message) {
    ++checks; if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
}
static bool has(const Events& events,EventType type) {
    for(size_t n=0;n<events.count;++n) if(events.data[n].type==type) return true;
    return false;
}
static float power(Engine& engine,int frames) {
    std::vector<float> audio(size_t(frames)*2); engine.render(audio.data(),frames);
    double sum=0; bool finite=true;
    for(float sample:audio) { finite=finite&&std::isfinite(sample)&&std::abs(sample)<=1; sum+=sample*sample; }
    check(finite,"audio remains finite and bounded");
    return float(sum/audio.size());
}
static void muteAndExit() {
    Settings s; Controller c; Input i; i.ry=.9f; c.update(i,s,.01);
    i.held[R2]=true; c.update(i,s,.02); i.held[R2]=false;
    check(!has(c.update(i,s,.03),EventType::Pluck),"releasing mute with the stick held stays quiet");
    i.ry=0; c.update(i,s,.04); i.ry=-.9f;
    check(has(c.update(i,s,.05),EventType::Pluck),"a fresh stroke after centering plays normally");
    Controller exit; i={}; i.ry=.9f; i.held[Select]=true; i.held[Start]=true;
    auto events=exit.update(i,s,.01);
    check(exit.quit()&&events.count==1&&events.data[0].type==EventType::Silence,"exit produces a release without a new strum");
}
static void modeChanges() {
    Settings s; s.mode=Mode::Expressive; Controller c; Input i; i.ry=-1; i.held[L1]=true;
    c.update(i,s,.01); s.mode=Mode::Keys; i={};
    auto changed=c.update(i,s,.02);
    check(has(changed,EventType::Silence),"changing the selected instrument releases the previous voice");
    check(!has(c.update(i,s,.03),EventType::Silence),"instrument release happens once per change");
}
static Input direction(float degrees,bool chord=false) {
    Input i; float angle=degrees*3.14159265f/180;
    if(chord) { i.lx=.9f*std::sin(angle); i.ly=-.9f*std::cos(angle); }
    else { i.rx=.9f*std::sin(angle); i.ry=-.9f*std::cos(angle); i.held[L1]=true; }
    return i;
}
static void wheelStability() {
    for(Mode mode:{Mode::Guitar,Mode::Expressive}) {
        Settings s; s.mode=mode; Controller c;
        c.update(direction(20,mode==Mode::Guitar),s,.01);
        c.update(direction(24,mode==Mode::Guitar),s,.02);
        check((mode==Mode::Guitar?c.chord():c.wheel())==0,"small direction jitter preserves the selected sector");
        c.update(direction(28,mode==Mode::Guitar),s,.03);
        check((mode==Mode::Guitar?c.chord():c.wheel())==1,"deliberately entering the next sector selects it");
    }
    Settings s; s.mode=Mode::Expressive; s.continuousPitch=true; Controller c;
    auto left=c.update(direction(-.01f),s,.01); auto right=c.update(direction(.01f),s,.02);
    check(std::abs(left.data[0].note-right.data[0].note)<.02f,"continuous pitch joins smoothly at the top of the wheel");
}
static void damagedSettings() {
    const char* path="audit-settings.cfg";
    { std::ofstream out(path); out<<"root 2\nvolume broken\noctave 5\nunknown nan\nscale 1\nnoteDisplay 1\n"; }
    auto s=loadSettings(path);
    check(s.root==2&&s.octave==5&&s.scale==Scale::Minor&&s.noteDisplay==NoteDisplay::Numbered,"a malformed settings row preserves later valid settings");
    check(s.volume==Settings{}.volume,"a malformed value retains its default");
    std::remove(path);
}
static void inputRecovery() {
    InputGuard guard; Input held; held.ry=.9f; held.held[L1]=true;
    auto initial=guard.filter(held);
    check(!initial.ready&&initial.ry==0&&!initial.held[L1],"startup waits for held performance controls to be released");
    check(guard.filter({}).ready,"neutral controls make the instrument ready");
    check(guard.filter(held).held[L1],"fresh controls pass after neutral input");
    guard.suspend();
    auto resumed=guard.filter(held);
    check(!resumed.ready&&resumed.ry==0,"focus or controller recovery suppresses a held stroke");
    guard.filter({});
    check(guard.filter(held).ry==held.ry,"centering restores performance after recovery");
}
static void releaseTone() {
    Engine original,switched; original.setMode(Mode::Keys); switched.setMode(Mode::Keys);
    Event on; on.type=EventType::NoteOn; on.note=69;
    original.apply(on); switched.apply(on); power(original,12000); power(switched,12000);
    original.apply({EventType::NoteOff,0}); switched.apply({EventType::NoteOff,0});
    switched.setMode(Mode::Guitar);
    std::vector<float> a(8192),b(8192); original.render(a.data(),4096); switched.render(b.data(),4096);
    float difference=0; for(size_t n=0;n<a.size();++n) difference=std::max(difference,std::abs(a[n]-b[n]));
    check(difference<1.e-6f,"a releasing note keeps its timbre when the selected instrument changes");
}
static void globalStop() {
    Engine e; e.setMode(Mode::Expressive); Event on; on.type=EventType::NoteOn; on.note=69;
    e.apply(on); power(e,12000); e.apply({EventType::Silence}); power(e,4800);
    check(power(e,4096)==0,"global stop finishes every voice before the audio device closes");
    e.apply(on);
    check(power(e,4096)>.0001f,"a new note starts normally after global stop");
    e.apply({EventType::NoteOff,0});
    check(power(e,2400)>0,"ordinary note release retains its musical tail");
}
static void settingsFailures() {
    const std::string path="audit-save.cfg"; Settings s; s.root=9; s.octave=5; s.volume=.37f;
    check(saveSettings(path,s),"settings write successfully");
    auto saved=loadSettings(path);
    check(saved.root==9&&saved.octave==5&&std::abs(saved.volume-.37f)<1.e-6f,"saved settings retain their values");
    std::filesystem::create_directory("audit-save-directory");
    check(!saveSettings("audit-save-directory",s),"an invalid destination reports a save failure");
    bool residue=false;
    for(const auto& file:std::filesystem::directory_iterator("."))
        if(file.path().filename().string().rfind("audit-save-directory.tmp.",0)==0) residue=true;
    check(!residue,"a failed save cleans up its temporary file");
    std::filesystem::remove("audit-save-directory"); std::remove(path.c_str());
}
static void dynamicsAndMelodyLevels() {
    for(int note:{36,60,84,102}) {
        double soft=0,strong=0;
        for(int take=0;take<8;++take) for(float velocity:{.3f,1.f}) {
            Engine e; Event p; p.type=EventType::Pluck; p.note=float(note); p.velocity=velocity;
            e.apply(p); float pwr=power(e,12000); if(velocity<1) soft+=pwr; else strong+=pwr;
        }
        check(strong>soft*2.5,"loudness balancing preserves stronger guitar strokes");
    }
    for(Mode mode:{Mode::Expressive,Mode::Keys}) {
        float low=100,high=0;
        for(int note=36;note<=108;note+=12) {
            Engine e; e.setMode(mode); Event on; on.type=EventType::NoteOn; on.note=float(note); e.apply(on);
            float rms=std::sqrt(power(e,12000)); low=std::min(low,rms); high=std::max(high,rms);
        }
        check(20*std::log10(high/low)<.3f,"melody notes preserve their level across registers");
    }
}
static void levels() {
    for(float velocity:{.3f,1.f}) {
        float low=100,high=0;
        for(int note:{16,24,36,48,60,72,84,96,104}) {
            double sum=0;
            for(int take=0;take<8;++take) {
                Engine e; Event pluck; pluck.type=EventType::Pluck; pluck.note=float(note); pluck.velocity=velocity; pluck.timbre=.5f;
                e.apply(pluck); sum+=power(e,12000);
            }
            float rms=std::sqrt(float(sum/8)); low=std::min(low,rms); high=std::max(high,rms);
        }
        check(20*std::log10(high/low)<6,"equal-strength guitar notes have balanced output across registers");
    }
}
static void retuneLevel() {
    for(int note:{45,69,81}) {
        Engine baseline; Event p; p.type=EventType::Pluck; p.note=float(note);
        baseline.apply(p); power(baseline,12000);
        Engine shifted=baseline;
        Event transpose; transpose.type=EventType::RetuneStrings; transpose.bend=12; shifted.apply(transpose);
        power(baseline,6000); power(shifted,6000);
        const float reference=power(baseline,6000),changed=power(shifted,6000);
        check(reference>1.e-9f&&changed>1.e-9f&&std::abs(10*std::log10(changed/reference))<3,
              "retuning a ringing string preserves its level within musical variation");
    }
}
static void randomPerformance() {
    std::mt19937 rng(46791); Settings s; Controller c; Engine e;
    for(int tick=0;tick<1200;++tick) {
        if(tick%17==0) s.mode=Mode(rng()%modeCount);
        if(tick%23==0) { s.root=int(rng()%12); s.octave=2+int(rng()%5); s.scale=Scale(rng()%scaleCount); }
        Input i;
        i.lx=float(int(rng()%201)-100)/100; i.ly=float(int(rng()%201)-100)/100;
        i.rx=float(int(rng()%201)-100)/100; i.ry=float(int(rng()%201)-100)/100;
        for(int key=0;key<int(Start);++key) i.held[key]=rng()%5==0;
        auto events=c.update(i,s,tick*.01); e.setMode(s.mode);
        for(size_t n=0;n<events.count;++n) e.apply(events.data[n]);
        power(e,480);
    }
    auto stop=c.reset(); e.apply(stop.data[0]); power(e,sampleRate*2);
    check(power(e,4096)==0,"a mixed performance reaches silence after release");
}
static void semitoneControls() {
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys}) {
        for(int scale=0;scale<scaleCount;++scale) {
            Settings s; s.mode=mode; s.scale=Scale(scale); s.root=11;
            Controller c; Input i;
            if(mode==Mode::Guitar) i.ry=.9f;
            else if(mode==Mode::Expressive) { i.ry=-1; i.held[L1]=true; }
            else for(Button key:instrumentButtons) i.held[key]=true;
            c.update(i,s,.01);
            i.held[Left]=true; auto raised=c.update(i,s,.02);
            Settings playing=c.performanceSettings(s);
            check(s.root==11&&s.octave==4&&playing.root==12,"held semitone preserves saved root across the octave boundary");
            check(playing.scale==s.scale,"held semitone preserves the selected scale");
            if(mode==Mode::Guitar) {
                check(raised.count==1&&raised.data[0].type==EventType::RetuneStrings&&raised.data[0].bend==1,"left raises ringing guitar strings by a semitone without a new stroke");
                check(c.stringStrikes()[0].note==guitarNote(chordBanks[0][0],0,playing),"guitar feedback follows the semitone modifier");
                check(chordLabel(chordBanks[0][0],s,1)=="C","guitar chord preview follows the semitone across B to C");
                check(c.update(i,s,.03).count==0,"holding left keeps a stable guitar pitch");
            } else {
                const int count=mode==Mode::Keys?8:1;
                for(int n=0;n<count;++n)
                    check(raised.data[n].note==scaleNote(n,playing),"held melody notes adopt the semitone modifier");
                if(mode==Mode::Expressive) check(c.previewPitch()==raised.data[0].note,"voice preview matches the raised sounding note");
            }
            i.held[Up]=true; c.update(i,s,.04);
            playing=c.performanceSettings(s);
            check(scaleNote(0,playing)==84,"octave and semitone modifiers combine");
            i.held[Left]=false; auto released=c.update(i,s,.05);
            check(c.performanceSettings(s).root==11,"releasing left restores the saved root");
            if(mode==Mode::Guitar) check(released.data[0].bend==-1,"releasing left restores ringing guitar strings");
            i.held[Right]=true; c.update(i,s,.06);
            check(s.root==11&&s.octave==5&&c.performanceSettings(s).root==11,"right plus up fixes the octave while preserving the root");
            c.reset(); check(c.performanceSettings(s).root==s.root,"reset clears the temporary semitone");
        }
    }
    Settings s; std::array<bool,12> pitches{};
    for(int n=0;n<7;++n) for(int shift:{0,1}) pitches[(scaleNote(n,s)+shift)%12]=true;
    check(std::all_of(pitches.begin(),pitches.end(),[](bool pitch){return pitch;}),"major scale plus held semitone reaches all twelve pitch classes");
    s.noteDisplay=NoteDisplay::Numbered;
    check(noteLabel(61,s).text=="#1"&&chordLabel(chordBanks[0][0],s,1)=="#1","numbered notes and chords show the raised pitch relative to the saved tonic");
    Settings voice; voice.mode=Mode::Expressive; voice.continuousPitch=true; voice.holdGate=false;
    Controller c; Input i; i.rx=.3f; i.ry=-.9f;
    float base=c.update(i,voice,.01).data[0].note;
    i.held[Left]=true;
    check(std::abs(c.update(i,voice,.02).data[0].note-base-1)<.001f,"continuous Theremin pitch supports the same semitone modifier");
}
static void faceButtonLayout() {
    constexpr std::array<Button,8> buttons={A,B,X,Y,L1,L2,R1,R2};
    constexpr std::array<int,8> standard={60,62,64,65,67,69,71,72};
    constexpr std::array<int,8> swapped={62,60,65,64,67,69,71,72};
    for(bool swap:{false,true}) {
        Settings s; s.mode=Mode::Keys; s.swapFaceButtons=swap;
        Controller c; double t=0;
        for(size_t key=0;key<buttons.size();++key) {
            Input i; i.held[buttons[key]]=true;
            auto on=c.update(i,s,t+=.01); int played=-1,noteOn=0;
            for(size_t n=0;n<on.count;++n) if(on.data[n].type==EventType::NoteOn) { played=int(on.data[n].note); ++noteOn; }
            check(noteOn==1&&played==(swap?swapped[key]:standard[key]),"each physical button plays its selected layout note");
            auto released=c.update({},s,t+=.01);
            check(has(released,EventType::NoteOff),"releasing a remapped button releases its sounding note");
        }
        Input chord; chord.held[A]=chord.held[X]=true;
        auto together=c.update(chord,s,t+=.01);
        std::vector<int> notes;
        for(size_t n=0;n<together.count;++n) if(together.data[n].type==EventType::NoteOn) notes.push_back(int(together.data[n].note));
        check(notes==std::vector<int>{swap?62:60,swap?65:64},"remapped face buttons retain independent simultaneous notes");
        const std::string path="layout-settings.cfg";
        check(saveSettings(path,s)&&loadSettings(path).swapFaceButtons==swap,"face button layout survives reopening");
        std::remove(path.c_str());
    }
    Settings s; Controller c; double t=0;
    auto tap=[&](Button button) { Input i; i.held[button]=true; c.update(i,s,t+=.01); c.update({},s,t+=.01); };
    tap(Start); tap(Right);
    check(s.root==1&&s.mode==Mode::Guitar,"the first settings row adjusts the root");
    for(int n=0;n<7;++n) tap(Down); tap(Right);
    check(s.swapFaceButtons,"face button swapping is available in settings");
    tap(Start); tap(Select);
    check(s.mode==Mode::Expressive,"Select keeps quick instrument switching available");
    tap(Select); Input i; i.held[A]=true;
    auto on=c.update(i,s,t+=.01);
    check(has(on,EventType::NoteOn),"returning from settings plays with the selected layout");
}
static void metaOctaveControls() {
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys}) for(Button direction:{Up,Down}) {
        Settings s; s.mode=mode; Controller c; Input i;
        if(mode==Mode::Guitar) i.ry=.9f;
        else if(mode==Mode::Expressive) { i.ry=-1; i.held[L1]=true; }
        else i.held[A]=true;
        c.update(i,s,.01);
        i.held[Right]=true; c.update(i,s,.02);
        check(s.octave==4&&c.performanceSettings(s).octave==4,"Meta alone preserves the octave");
        i.held[direction]=true; auto shifted=c.update(i,s,.03);
        const int octave=direction==Up?5:3,delta=direction==Up?12:-12;
        check(s.octave==octave&&c.performanceSettings(s).octave==octave,"Meta plus direction changes only the fixed octave");
        if(mode==Mode::Guitar) check(shifted.count==1&&shifted.data[0].type==EventType::RetuneStrings&&shifted.data[0].bend==delta,"Meta retunes ringing guitar strings exactly one octave");
        else check(shifted.data[0].note==60+delta,"Meta retunes a held melody exactly one octave");
        c.update(i,s,.04);
        check(s.octave==octave,"holding a Meta shortcut changes the octave once");
        i.held[Right]=false; c.update(i,s,.05);
        check(c.performanceSettings(s).octave==octave,"releasing Meta first preserves the fixed octave until direction release");
        i.held[direction]=false; c.update(i,s,.06);
        i.held[direction]=true; c.update(i,s,.07);
        check(s.octave==octave&&c.performanceSettings(s).octave==octave+(direction==Up?1:-1),"a fresh direction press restores temporary octave control");
        i.held[Right]=true; c.update(i,s,.08);
        check(s.octave==octave+(direction==Up?1:-1)&&c.performanceSettings(s).octave==s.octave,"pressing Meta after a direction converts it into a fixed octave");
        i.held[direction]=false; c.update(i,s,.09);
        check(c.performanceSettings(s).octave==s.octave,"releasing the direction first preserves the fixed octave");
        const int before=s.octave;
        i.held[direction]=true; c.update(i,s,.1);
        check(s.octave==std::clamp(before+(direction==Up?1:-1),minOctave,maxOctave),"a fresh direction press with Meta held changes the fixed octave again");
    }
    for(int octave:{minOctave,maxOctave}) {
        Settings s; s.octave=octave; Controller c; Input i;
        i.held[Right]=true; i.held[octave==minOctave?Down:Up]=true; c.update(i,s,.01);
        i.held[Right]=false; c.update(i,s,.02);
        check(s.octave==octave&&c.performanceSettings(s).octave==octave,"Meta respects register bounds and stable release");
    }
    Settings s; Controller c; Input i; i.held[Right]=i.held[Up]=i.held[Down]=true;
    c.update(i,s,.01); check(s.octave==4,"opposite Meta directions preserve the fixed octave");
    i.held[Down]=false; c.update(i,s,.02);
    check(s.octave==4,"releasing one opposite direction waits for a fresh shortcut");
    i={}; c.update(i,s,.03); i.held[Right]=i.held[Up]=i.held[Left]=true; c.update(i,s,.04);
    check(s.octave==5&&c.performanceSettings(s).root==s.root+1,"Meta octave and the semitone modifier combine");
    check(saveSettings("meta-settings.cfg",s)&&loadSettings("meta-settings.cfg").octave==5,"the fixed octave survives reopening");
    std::remove("meta-settings.cfg");
    i={}; i.held[Start]=true; c.update(i,s,.05); i={}; c.update(i,s,.06);
    i.held[Right]=i.held[Up]=true; c.update(i,s,.07);
    check(s.octave==5&&s.quantize==Quantize::All,"settings keep their navigation and value controls");
}
int main() {
    metaOctaveControls(); faceButtonLayout(); semitoneControls(); muteAndExit(); modeChanges(); wheelStability(); damagedSettings(); settingsFailures(); inputRecovery(); releaseTone(); globalStop(); levels(); dynamicsAndMelodyLevels(); retuneLevel(); randomPerformance();
    std::printf("Audit: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
