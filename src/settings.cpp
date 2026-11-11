#include "settings.hpp"
#include "sound_catalog.hpp"
#include "atomic_file.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
namespace ps {
Settings loadSettings(const std::string& path) {
    Settings s; std::ifstream in(path); std::string line;
    while(std::getline(in,line)) {
        std::istringstream row(line); std::string key; float value;
        if(!(row>>key)) continue;
        bool soundSetting=false;
        for(int mode=0;mode<modeCount;++mode) if(key=="sound."+std::to_string(mode)) {
            std::string name; row>>name;
            if(const auto* preset=findSound(name,Mode(mode))) s.sounds[mode]=preset->id;
            soundSetting=true; break;
        }
        if(soundSetting||!(row>>value)) continue;
        if(!std::isfinite(value)) continue;
        // Bound before integer conversion: settings are editable user data.
        int v=int(std::clamp(value,-1000.f,1000.f));
        if(key=="tempo") s.tempo=std::clamp(v,minTempo,maxTempo);
        if(key=="loopBars") s.loopBars=std::clamp(v,1,2);
        if(key=="quantize") s.quantize=Quantize(std::clamp(v,0,2));
        if(key=="mode") s.mode=Mode(std::clamp(v,0,modeCount-1));
        if(key=="swapFaceButtons") s.swapFaceButtons=v!=0;
        if(key=="noteDisplay") s.noteDisplay=NoteDisplay(std::clamp(v,0,1));
        if(key=="scale") s.scale=Scale(std::clamp(v,0,scaleCount-1));
        if(key=="root") s.root=std::clamp(v,0,11);
        if(key=="octave") s.octave=std::clamp(v,minOctave,maxOctave);
        if(key=="chordBank") s.chordBank=std::clamp(v,0,1);
        if(key=="volume") s.volume=std::clamp(value,0.f,1.f);
        if(key=="continuousPitch") s.continuousPitch=v!=0;
        if(key=="holdGate") s.holdGate=v!=0;
    }
    return s;
}
bool saveSettings(const std::string& path, const Settings& s) {
    return atomicWrite(path,[&](FILE* out) {
        if(std::fprintf(out,"mode %d\nscale %d\nroot %d\noctave %d\nchordBank %d\nvolume %.7g\nnoteDisplay %d\ncontinuousPitch %d\nholdGate %d\nswapFaceButtons %d\ntempo %d\nloopBars %d\nquantize %d\n",
        int(s.mode),int(s.scale),s.root,s.octave,s.chordBank,double(s.volume),int(s.noteDisplay),int(s.continuousPitch),int(s.holdGate),int(s.swapFaceButtons),s.tempo,s.loopBars,int(s.quantize))<0) return false;
        for(int mode=0;mode<modeCount;++mode)
            if(std::fprintf(out,"sound.%d %s\n",mode,selectedSound(s,Mode(mode)).key)<0) return false;
        return true;
    });
}
}
