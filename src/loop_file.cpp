#include "looper.hpp"
#include "sound_catalog.hpp"
#include "atomic_file.hpp"
#include <fstream>
#include <memory>
#include <cmath>
namespace ps {
bool Looper::save(const std::string& path) {
    finishTake();
    return atomicWrite(path,[&](FILE* out) {
        if(std::fprintf(out,"%s %d\n",loopFileMagic,loopFileVersion)<0) return false;
        for(int mode=0;mode<modeCount;++mode) {
            const auto& track=tracks_[mode];
            if(std::fprintf(out,"%zu\n",track.count)<0) return false;
            for(size_t n=0;n<track.count;++n) {
                const auto& r=track.events[n]; const auto& e=r.event;
                if(std::fprintf(out,"%.17g %d %d %.9g %.9g %.9g %.9g %.9g %s\n",r.step,int(e.type),e.id,
                   double(e.note),double(e.velocity),double(e.timbre),double(e.vibrato),double(e.bend),resolveSound(e.sound,Mode(mode)).key)<0) return false;
            }
        }
        return true;
    });
}
bool Looper::load(const std::string& path) {
    std::ifstream in(path); std::string magic; int version;
    if(!(in>>magic>>version)||magic!=loopFileMagic||version!=loopFileVersion) return false;
    auto loaded=std::make_unique<Looper>();
    for(int mode=0;mode<modeCount;++mode) {
        auto& track=loaded->tracks_[mode];
        if(!(in>>track.count)||track.count>loopEventLimit) return false;
        double previous=-1;
        for(size_t n=0;n<track.count;++n) {
            auto& r=track.events[n]; auto& e=r.event; int type; std::string sound;
            if(!(in>>r.step>>type>>e.id>>e.note>>e.velocity>>e.timbre>>e.vibrato>>e.bend>>sound)) return false;
            if(!std::isfinite(r.step)||r.step<previous||r.step<0||r.step>=32) return false;
            previous=r.step;
            if(!std::isfinite(e.note)||e.note<0||e.note>144||!std::isfinite(e.velocity)||e.velocity<0||e.velocity>1||
               !std::isfinite(e.timbre)||e.timbre<0||e.timbre>1||!std::isfinite(e.vibrato)||e.vibrato<0||e.vibrato>1||
               !std::isfinite(e.bend)||std::abs(e.bend)>60||e.id<0||e.id>=melodyVoiceCount) return false;
            const auto* preset=findSound(sound,Mode(mode)); if(!preset) return false; e.sound=preset->id;
            e.type=EventType(type);
            switch(e.type) {
            case EventType::Pluck: if(mode!=int(Mode::Guitar)||e.id>=guitarVoiceCount) return false; break;
            case EventType::Drum: if(mode!=int(Mode::Drums)||e.id>=drumCount) return false; break;
            case EventType::NoteOn: case EventType::NoteOff: case EventType::Expression:
                if(mode!=int(Mode::Expressive)&&mode!=int(Mode::Keys)) return false; break;
            case EventType::LayerStop:
                if(mode!=int(Mode::Guitar)||e.id>=guitarVoiceCount||e.id%stringCount!=0) return false; break;
            case EventType::Silence: break;
            default: return false;
            }
            r.order=loaded->order_++;
        }
    }
    in>>std::ws; if(!in.eof()) return false;
    tracks_=std::move(loaded->tracks_); order_=loaded->order_;
    for(auto& track:tracks_) track.revision=nextRevision_++;
    playing_=armed_=recording_=false; position_=countIn_=0; takeCount_=0;
    recordTrack_=-1; rewind(); stops_=(1u<<modeCount)-1;
    return true;
}
}
