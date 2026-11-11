#include "music.hpp"
#include <cmath>
#include <algorithm>
namespace ps {
static constexpr int majorIntervals[]={0,2,4,5,7,9,11};
static constexpr int minorIntervals[]={0,2,3,5,7,8,10};
static constexpr int pentatonicIntervals[]={0,2,4,7,9};
const std::array<const char*, modeCount> modeNames = {"GUITAR", "EXPRESSIVE", "KEYS", "DRUM MACHINE"};
const std::array<const char*, scaleCount> scaleNames = {"MAJOR", "MINOR", "PENTATONIC"};
const std::array<std::array<Chord, wheelSize>, 2> chordBanks = {{
 {{{"C", {48,52,55,60,64,67}}, {"G", {43,47,50,55,59,67}},
   {"AM", {45,52,57,60,64,69}}, {"F", {41,48,53,57,60,65}},
   {"DM", {50,53,57,62,65,69}}, {"EM", {40,47,52,55,59,64}},
   {"E", {40,47,52,56,59,64}}, {"D", {50,54,57,62,66,69}}}},
 {{{"CMAJ7", {48,52,55,59,64,67}}, {"G7", {43,47,50,53,59,67}},
   {"AM7", {45,52,55,60,64,69}}, {"FMAJ7", {41,48,52,57,60,65}},
   {"DM7", {50,53,57,60,65,69}}, {"EM7", {40,47,50,55,59,64}},
   {"E7", {40,47,50,56,59,64}}, {"DSUS2", {50,52,57,62,64,69}}}}
}};
int scaleNote(int degree, const Settings& s) {
    const int* steps = majorIntervals; int size = 7;
    if(s.scale == Scale::Minor) steps = minorIntervals;
    if(s.scale == Scale::Pentatonic) { steps = pentatonicIntervals; size = 5; }
    return 12*(s.octave+1)+s.root+12*(degree/size)+steps[degree%size];
}
int guitarNote(const Chord& chord,int string,const Settings& s) {
    return chord.notes[string]+s.root+12*(s.octave-middleOctave);
}
namespace {
constexpr float tau=6.28318530718f;
float wheelAngle(float x,float y) { return std::atan2(x,-y); }
}
int sector(float x,float y) {
    return (int(std::floor(wheelAngle(x,y)*wheelSize/tau+.5f))+wheelSize)%wheelSize;
}
int stableSector(float x,float y,int previous) {
    if(previous>=0) {
        const float delta=std::remainder(wheelAngle(x,y)-previous*tau/wheelSize,tau);
        constexpr float margin=3.f*tau/360.f;
        if(std::abs(delta)<=tau/(2*wheelSize)+margin) return previous;
    }
    return sector(x,y);
}
float continuousWheelNote(float x,float y,const Settings& s) {
    float angle=wheelAngle(x,y); if(angle<0) angle+=tau;
    const float position=angle*wheelSize/tau;
    const int low=std::min(int(position),wheelSize-1),high=(low+1)%wheelSize;
    const float fraction=position-low;
    return scaleNote(low,s)*(1-fraction)+scaleNote(high,s)*fraction;
}
float frequency(float midi) {
    constexpr int resolution=64,top=144;
    auto exact=[](float note){return 440.f*std::exp2((note-69.f)/12.f);};
    static const auto table=[&] {
        std::array<float,top*resolution+1> values{};
        for(size_t n=0;n<values.size();++n) values[n]=exact(float(n)/resolution);
        return values;
    }();
    if(midi<0||midi>=top||!std::isfinite(midi)) return exact(midi);
    const float position=midi*resolution; const size_t index=size_t(position);
    return table[index]+(table[index+1]-table[index])*(position-float(index));
}
std::string pitchClassName(int midi) {
    static constexpr const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return names[(midi%12+12)%12];
}
std::string chordName(const Chord& chord, int transpose) {
    std::string root=pitchClassName(chord.notes[0]+transpose);
    return root+std::string(chord.name).substr(1);
}

std::string noteName(int midi) {
    return pitchClassName(midi)+std::to_string(midi/12-1);
}
NoteLabel noteLabel(int midi,const Settings& s) {
    if(s.noteDisplay==NoteDisplay::Names) return {noteName(midi),0};
    int relative=midi-(12*(middleOctave+1)+s.root);
    int semitone=(relative%12+12)%12;
    int dots=(relative-semitone)/12;
    for(int n=0;n<7;++n)
        if(semitone==majorIntervals[n]) return {std::to_string(n+1),dots};
    if(s.scale==Scale::Minor)
        for(int n=0;n<7;++n)
            if(semitone==majorIntervals[n]-1) return {"b"+std::to_string(n+1),dots};
    for(int n=0;n<7;++n)
        if(semitone==majorIntervals[n]+1) return {"#"+std::to_string(n+1),dots};
    return {"1",dots};
}
std::string chordLabel(const Chord& chord,const Settings& s,int semitoneOffset) {
    if(s.noteDisplay==NoteDisplay::Names) return chordName(chord,s.root+semitoneOffset);
    return noteLabel(chord.notes[0]+s.root+semitoneOffset,s).text+std::string(chord.name).substr(1);
}

}
