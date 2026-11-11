#include "performance.hpp"
#include "engine.hpp"
#include "settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <fstream>
using namespace ps;
static int checks=0;
static void expect(bool ok,const char* behavior) {
    ++checks; if(!ok) { std::fprintf(stderr,"FAIL: %s\n",behavior); std::exit(1); }
}
static float rms(const std::vector<float>& a) {
    double sum=0; for(float f:a) { expect(std::isfinite(f)&&std::abs(f)<=1,"audio stays finite and bounded"); sum+=f*f; }
    return float(std::sqrt(sum/a.size()));
}
static std::vector<float> sound(Engine& e,int frames) {
    std::vector<float> out(size_t(frames)*2); e.render(out.data(),frames); return out;
}
static void guitar() {
    Settings s; Controller c; Input i;
    auto initial=c.update(i,s,0); expect(initial.count==0,"rest stays silent");
    i.ry=.9; auto down=c.update(i,s,.01);
    expect(down.count==6,"one stroke plays six strings");
    expect(c.stringStrikes()[0].startsAt==c.seconds(),"first string highlights at the start of a downstroke");
    expect(c.stringStrikes()[5].startsAt>c.seconds(),"later strings wait for their scheduled stroke positions");
    expect(c.stringStrikes()[0].note==down.data[0].note,"string feedback displays the performed pitch");
    for(size_t n=1;n<6;++n) {
        expect(down.data[n].note>down.data[n-1].note,"downstroke moves from low to high strings");
        expect(down.data[n].delay>down.data[n-1].delay,"strings start in sequence");
    }
    expect(c.update(i,s,.02).count==0,"holding a stroke sustains the chord");
    i.ry=.4; expect(c.update(i,s,.03).count==0,"threshold jitter keeps the stroke stable");
    i.ry=0; c.update(i,s,.04); i.ry=-.9;
    auto up=c.update(i,s,.05);
    expect(up.count==6&&up.data[0].note>up.data[5].note,"upstroke reverses the string order");
    expect(c.stringStrikes()[5].startsAt<c.stringStrikes()[0].startsAt,"upstroke feedback follows high strings before low strings");
    i={}; i.lx=1; c.update(i,s,.06); i={}; c.update(i,s,.07);
    expect(c.chord()==2,"chord remains selected after the stick returns");
    i.ry=.6; auto slow=c.update(i,s,.17);
    expect(slow.data[0].velocity<down.data[0].velocity,"faster strokes have greater strength");
    expect(slow.data[5].delay>down.data[5].delay,"gentle strokes spread the string attacks");
    i.held[R2]=true; auto mute=c.update(i,s,.18);
    expect(mute.count==1&&mute.data[0].type==EventType::Silence,"mute releases strings and scheduled attacks");
    for(const auto& strike:c.stringStrikes()) expect(strike.startsAt<0&&strike.strength==0,"mute clears active and scheduled string feedback");
    Engine e; for(size_t n=0;n<down.count;++n) e.apply(down.data[n]);
    expect(rms(sound(e,24000))>.001,"a chord produces audible output");
    e.apply({EventType::Silence}); sound(e,24000);
    expect(rms(sound(e,2048))<.00001,"muting reaches silence");
    Engine delayed; Event p=down.data[0]; p.delay=480; delayed.apply(p);
    expect(rms(sound(delayed,480))==0,"scheduled string respects its start time");
    expect(rms(sound(delayed,4096))>.0001,"scheduled string sounds at its start time");
    Engine canceled; canceled.apply(p); canceled.apply({EventType::Silence});
    expect(rms(sound(canceled,4096))==0,"mute cancels a future string");
}
static int period(const std::vector<float>& audio,int low,int high) {
    double best=-2; int lag=0;
    for(int shift=low;shift<=high;++shift) {
        double product=0,left=0,right=0;
        for(size_t n=size_t(shift)*2;n<audio.size();n+=2) {
            double a=audio[n],b=audio[n-size_t(shift)*2];
            product+=a*b; left+=a*a; right+=b*b;
        }
        double score=left*right>0?product/std::sqrt(left*right):-2;
        if(score>best) { best=score; lag=shift; }
    }
    return lag;
}
static void guitarOctaves() {
    Settings s; Controller c; Input i; i.ry=.9f;
    auto original=c.update(i,s,.01);
    int first=int(original.data[0].note);
    double struck=c.stringStrikes()[0].startsAt;
    i.held[Up]=true; auto raised=c.update(i,s,.02);
    expect(raised.count==1&&raised.data[0].type==EventType::RetuneStrings&&raised.data[0].bend==12,
           "holding up retunes ringing guitar strings by one octave");
    expect(s.octave==4&&c.performanceSettings(s).octave==5,"temporary guitar octave preserves the base register");
    expect(c.stringStrikes()[0].note==first+12&&c.stringStrikes()[0].startsAt==struck,
           "string labels retune while the existing pluck continues");
    expect(c.update(i,s,.03).count==0,"holding up maintains a stable guitar register");
    i.held[Up]=false;
    expect(c.update(i,s,.04).data[0].bend==-12,"releasing up restores ringing guitar pitches");
    i.held[Right]=true;
    expect(c.update(i,s,.05).count==0&&s.octave==4,"right preserves the guitar register for future controls");
    expect(c.update(i,s,.06).count==0,"holding right preserves the saved register");
    i.held[Right]=false; i.ry=0; c.update(i,s,.07); i.ry=-.9;
    auto next=c.update(i,s,.08);
    expect(next.data[0].note==original.data[5].note,"subsequent strums use the fixed register");
    i.held[Down]=true;
    expect(c.update(i,s,.09).data[0].bend==-12&&s.octave==4,"down temporarily lowers the guitar register");
    i.held[Up]=true;
    expect(c.update(i,s,.1).data[0].bend==12,"opposite temporary controls restore the fixed guitar register");
    Engine e; Event p; p.type=EventType::Pluck; p.note=57; p.velocity=.9f;
    e.apply(p); sound(e,12000);
    int before=period(sound(e,8192),200,240);
    Event retune; retune.type=EventType::RetuneStrings; retune.bend=12;
    e.apply(retune); sound(e,4000);
    int after=period(sound(e,8192),95,125);
    expect(before>0&&after>0&&std::abs(float(before)/after-2)<.15f,"ringing guitar audio rises by an octave");
    Engine delayed; p.delay=480; delayed.apply(p); delayed.apply(retune);
    expect(rms(sound(delayed,480))==0,"retuning keeps a scheduled string's attack time");
    sound(delayed,4000);
    expect(std::abs(period(sound(delayed,8192),95,125)-after)<4,"scheduled strings adopt the newly selected octave");
}
static void guitarDecay() {
    for(int octave:{2,4,6}) {
        for(float strength:{.2f,1.f}) {
            Engine e;
            Settings s; s.octave=octave; s.chordBank=1;
            const auto& chord=chordBanks[s.chordBank][0];
            for(int n=0;n<stringCount;++n) {
                Event p; p.type=EventType::Pluck; p.id=n;
                p.note=float(guitarNote(chord,n,s)); p.velocity=strength;
                p.delay=uint32_t(n*.006f*sampleRate); e.apply(p);
            }
            float attack=rms(sound(e,sampleRate/4));
            sound(e,sampleRate/4);
            float middle=rms(sound(e,sampleRate/4));
            sound(e,sampleRate*3/4);
            float tail=rms(sound(e,sampleRate/4));
            expect(attack>.0001f,"one guitar strum has an audible attack across registers and strengths");
            expect(middle<attack*.65f,"a strummed chord becomes quieter after its attack");
            expect(tail<attack*.12f,"a strummed chord has a fading tail instead of a sustained plateau");
            sound(e,sampleRate*2);
            expect(rms(sound(e,4096))==0,"one strum naturally reaches silence without a mute input");
            Event p; p.type=EventType::Pluck; p.note=69; e.apply(p);
            expect(rms(sound(e,4096))>.0001f,"a new pluck sounds again after the previous chord has ended");
        }
    }
    Settings s; Controller c; Engine e; Input i; i.ry=.95f;
    int plucks=0; float peakAfterRest=0;
    constexpr int frames=480;
    std::vector<float> block(frames*2);
    for(int tick=0;tick<500;++tick) {
        auto events=c.update(i,s,tick*.01);
        for(size_t n=0;n<events.count;++n) {
            if(events.data[n].type==EventType::Pluck) ++plucks;
            e.apply(events.data[n]);
        }
        e.render(block.data(),frames);
        if(tick>=300) for(float x:block) peakAfterRest=std::max(peakAfterRest,std::abs(x));
    }
    expect(plucks==stringCount,"holding the strum stick produces one stroke");
    expect(peakAfterRest==0,"holding the strum stick lets the chord decay to silence");
    i.ry=0; c.update(i,s,5.01); i.ry=-.95f;
    auto again=c.update(i,s,5.02); for(size_t n=0;n<again.count;++n) e.apply(again.data[n]);
    expect(rms(sound(e,8192))>.0001f,"returning to center and strumming again starts a new chord");
}
static void voice() {
    Settings s; s.mode=Mode::Expressive; s.holdGate=false; Controller c; Input i;
    i.ry=-.8; auto on=c.update(i,s,.01);
    expect(on.data[0].type==EventType::NoteOn&&on.data[0].note==60,"wheel starts with the root at twelve oclock");
    for(int n=0;n<8;++n) {
        float angle=n*6.2831853f/8; i.rx=std::sin(angle)*.9f; i.ry=-std::cos(angle)*.9f;
        auto e=c.update(i,s,.02+n*.01);
        expect(e.data[0].note==scaleNote(n,s),"each wheel sector follows the selected scale");
    }
    i={}; auto off=c.update(i,s,.2);
    expect(off.data[0].type==EventType::NoteOff,"centering the wheel releases the voice");
    s.holdGate=true; i.ry=-1; auto gated=c.update(i,s,.21);
    expect(gated.data[0].type==EventType::Expression,"breath gate waits for L1");
    expect(c.wheel()==0,"wheel previews the selected root before the trigger is pressed");
    i.rx=1; i.ry=0;
    expect(c.update(i,s,.212).data[0].type==EventType::Expression&&c.wheel()==2,"silent selection follows the stick immediately");
    i.rx=0; i.ry=-1;
    i.held[R2]=true; expect(c.update(i,s,.215).data[0].type==EventType::Expression,"right trigger leaves the breath gate at rest");
    i.held[L2]=true;
    expect(c.update(i,s,.216).data[0].type==EventType::Expression,"left trigger preserves silent note preview");
    i.held[L1]=true; expect(c.update(i,s,.22).data[0].type==EventType::NoteOn,"left shoulder opens the breath gate");
    s.continuousPitch=true; i.rx=.3f; i.ry=-.9f;
    float pitch=c.update(i,s,.23).data[0].note;
    expect(pitch>60&&pitch<62,"continuousPitch interpolates pitch between scale tones");
    i.held[L1]=false;
    expect(c.update(i,s,.24).data[0].type==EventType::NoteOff,"releasing the left shoulder releases the voice");
    expect(c.wheel()==0,"releasing the trigger preserves the selected note preview");
    i={}; c.update(i,s,.25);
    expect(c.wheel()==-1,"centering the stick clears the selection");
    Engine engine; engine.setMode(s.mode); Event note; note.type=EventType::NoteOn; note.note=69;
    engine.apply(note); sound(engine,24000); auto a=sound(engine,48000);
    int crossings=0; for(size_t n=2;n<a.size();n+=2) if(a[n-2]<=0&&a[n]>0) ++crossings;
    expect(std::abs(crossings-440)<=2,"A4 sounds at 440 Hz");
    expect(rms(a)>.01,"held note remains audible");
    engine.apply({EventType::NoteOff,0}); sound(engine,48000);
    expect(rms(sound(engine,4096))<.00001,"note release reaches silence");
}
static void temporaryVoiceOctave() {
    Settings s; s.mode=Mode::Expressive; Controller c; Input i;
    i.ry=-1;
    c.update(i,s,.01);
    i.held[Up]=true; c.update(i,s,.02);
    expect(s.octave==4&&c.performanceSettings(s).octave==5,"silent octave preview keeps the saved octave");
    i.held[L1]=true;
    expect(c.update(i,s,.03).data[0].note==72,"held D-pad up performs an octave higher");
    expect(c.update(i,s,.04).data[0].note==72,"holding the octave modifier remains stable");
    i.held[Up]=false;
    expect(c.update(i,s,.05).data[0].note==60,"releasing D-pad returns a sounding note to its base octave");
    i.held[Down]=true;
    expect(c.update(i,s,.06).data[0].note==48,"held D-pad down performs an octave lower");
    i.held[Up]=true;
    expect(c.update(i,s,.07).data[0].note==60,"opposite octave modifiers balance at the base octave");
    i.held[Down]=false; i.held[Right]=true;
    expect(c.update(i,s,.08).data[0].note==72&&s.octave==5,"right plus held up makes the octave fixed");
    i.held[Up]=false; i.held[Right]=false;
    expect(c.update(i,s,.09).data[0].note==72,"releasing the octave shortcut preserves the fixed register");
    s.continuousPitch=true; i.rx=.3f; i.ry=-.9f;
    float base=c.update(i,s,.1).data[0].note;
    i.held[Up]=true;
    expect(std::abs(c.update(i,s,.11).data[0].note-base-12)<.001,"temporary octave also transposes continuous pitch");
    s.octave=6;
    expect(c.update(i,s,.12).data[0].note<86,"temporary octave respects the upper playable range");
    c.reset(); expect(c.performanceSettings(s).octave==6,"reset clears the temporary octave");
    s.octave=2; i.held[Up]=false; i.held[Down]=true; s.continuousPitch=false; i.rx=0; i.ry=-1;
    expect(c.update(i,s,.13).data[0].note==36&&s.octave==2,"temporary octave respects the lower playable range");
}
static void ocarina() {
    Settings s; s.mode=Mode::Keys; Controller c; Input i;
    constexpr Button keys[]={A,B,X,Y,L1,L2,R1,R2};
    constexpr int pitches[]={60,62,64,65,67,69,71,72};
    for(int n=0;n<8;++n) i.held[keys[n]]=true;
    auto on=c.update(i,s,.01);
    expect(on.count==9,"eight note keys play independently alongside expression");
    Engine engine; engine.setMode(s.mode);
    for(int n=0;n<8;++n) {
        expect(on.data[n].note==pitches[n],"eight note keys span a complete major octave");
        engine.apply(on.data[n]);
    }
    expect(rms(sound(engine,12000))>.01,"eight simultaneous notes remain audible and bounded");
    i.held[Right]=true; auto higher=c.update(i,s,.02);
    expect(higher.data[0].note==60&&higher.data[7].note==72,"D-pad right preserves held notes");
    expect(c.update(i,s,.03).data[0].note==60,"holding right preserves the octave");
    i.held[Right]=false; i.held[Up]=true;
    auto raised=c.update(i,s,.04);
    expect(raised.data[0].note==72&&raised.data[7].note==84&&s.octave==4,"D-pad up temporarily raises all eight held notes");
    expect(s.scale==Scale::Major,"octave modifiers preserve the selected scale");
    expect(c.update(i,s,.041).data[0].note==72,"holding up keeps a stable temporary octave");
    i.held[Up]=false;
    expect(c.update(i,s,.042).data[0].note==60,"releasing up restores the fixed octave");
    i.held[Down]=true;
    expect(c.update(i,s,.043).data[0].note==48,"D-pad down temporarily lowers a held note");
    i.held[Up]=true;
    expect(c.update(i,s,.044).data[0].note==60,"opposite temporary modifiers use the fixed octave");
    i.held[Up]=false; i.held[Down]=true; i.held[Left]=true;
    auto lower=c.update(i,s,.05);
    expect(s.scale==Scale::Major&&lower.data[0].note==49&&s.octave==4,"semitone and temporary octave controls combine independently");
    i.held[Down]=false; i.held[Left]=false;
    expect(c.update(i,s,.055).data[0].note==60,"releasing the modifiers restores the saved octave");
    i.held[A]=false; auto release=c.update(i,s,.06);
    expect(release.data[0].type==EventType::NoteOff&&release.data[1].type==EventType::NoteOn,"releasing one key preserves other held notes");
    i={}; auto rest=c.update(i,s,.07);
    expect(rest.count==8,"remaining seven notes release independently");
    i.held[Select]=true; auto change=c.update(i,s,.08);
    expect(change.data[0].type==EventType::Silence&&s.mode==Mode::Drums,"mode change releases the previous instrument");
    i={}; c.update(i,s,.09); i.held[Start]=true; auto menu=c.update(i,s,.1);
    expect(c.menu()&&menu.data[0].type==EventType::Silence,"settings opens with a clean release");
    i.held[Select]=true; c.update(i,s,.11); expect(c.quit(),"exit chord exits from settings");
}
static void numberedNotation() {
    Settings s; s.noteDisplay=NoteDisplay::Numbered;
    for(int n=0;n<7;++n)
        expect(noteLabel(scaleNote(n,s),s).text==std::to_string(n+1),"major notes use relative degrees one through seven");
    expect(noteLabel(72,s).text=="1"&&noteLabel(72,s).octaveDots==1,"upper tonic uses an octave dot");
    expect(noteLabel(48,s).octaveDots==-1,"lower tonic uses a lower octave dot");
    s.root=2;
    expect(noteLabel(62,s).text=="1"&&noteLabel(62,s).octaveDots==0,"relative tonic follows a transposed root");
    expect(noteLabel(60,s).text=="#6"&&noteLabel(60,s).octaveDots==-1,"notes below a transposed tonic retain their register");
    s.scale=Scale::Minor;
    expect(noteLabel(scaleNote(2,s),s).text=="b3"&&noteLabel(scaleNote(5,s),s).text=="b6","minor intervals retain their relative alterations");
    s.scale=Scale::Pentatonic;
    expect(noteLabel(scaleNote(3,s),s).text=="5"&&noteLabel(scaleNote(4,s),s).text=="6","pentatonic labels retain their musical degrees");
    expect(noteLabel(63,s).text=="#1","semitone notes show their alterations");
    expect(chordLabel(chordBanks[0][2],s)=="6M","relative chord labels retain chord quality");
    Settings names=s; names.noteDisplay=NoteDisplay::Names;
    expect(noteLabel(62,names).text=="D4"&&noteLabel(62,names).octaveDots==0,"note-name display retains absolute pitch names");
    Input performance; performance.rx=.9f; performance.ry=.8f; performance.held[L1]=true;
    for(Mode mode:{Mode::Guitar,Mode::Expressive,Mode::Keys}) {
        names.mode=mode; s.mode=mode; Controller namedController,numberedController;
        auto named=namedController.update(performance,names,.01);
        auto numbered=numberedController.update(performance,s,.01);
        expect(named.count==numbered.count,"display selection preserves the performed event count");
        for(size_t n=0;n<named.count;++n)
            expect(named.data[n].type==numbered.data[n].type&&named.data[n].id==numbered.data[n].id&&named.data[n].note==numbered.data[n].note,
                   "display selection preserves notes and expression in every instrument");
    }
    Controller c; Input i; i.held[Start]=true; c.update(i,s,.01);
    double time=.02;
    for(int n=0;n<6;++n) { i={}; c.update(i,s,time+=.01); i.held[Down]=true; c.update(i,s,time+=.01); }
    expect(c.menuRow()==int(SettingRow::NoteDisplay),"display choice is reachable in settings");
    i={}; c.update(i,s,time+=.01); i.held[Right]=true; c.update(i,s,time+=.01);
    expect(s.noteDisplay==NoteDisplay::Names,"settings switches between names and numbered notation");
}
static void scalesAndSettings() {
    Settings s; expect(chordName(chordBanks[0][0],1)=="C#","chord label follows the sounding transposition");
    expect(scaleNote(7,s)==72,"major scale spans an octave");
    s.scale=Scale::Pentatonic; expect(scaleNote(5,s)==72,"pentatonic repeats after five tones");
    const std::string path="behavior-settings.cfg";
    s.mode=Mode::Keys; s.root=11; s.volume=.35f; s.holdGate=true; s.noteDisplay=NoteDisplay::Numbered;
    expect(saveSettings(path,s),"settings save succeeds"); auto loaded=loadSettings(path);
    expect(loaded.noteDisplay==NoteDisplay::Numbered&&loaded.mode==s.mode&&loaded.root==s.root&&loaded.holdGate&&std::abs(loaded.volume-s.volume)<.001,"settings survive reopening");
    { std::ofstream out(path); out<<"mode 90\nvolume -4\noctave 10000000000000000\nroot -30\n"; }
    loaded=loadSettings(path);
    expect(loaded.mode==Mode::Drums&&loaded.volume==0&&loaded.octave==6&&loaded.root==0,"edited settings remain in playable ranges");
    std::remove(path.c_str());
}
int main() {
    guitar(); guitarOctaves(); guitarDecay(); voice(); temporaryVoiceOctave(); ocarina(); numberedNotation(); scalesAndSettings();
    std::printf("PASS: %d checks across guitar, expressive, keys, audio, scales and settings\n",checks);
}
