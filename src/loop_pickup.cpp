#include "looper.hpp"
#include <algorithm>
namespace ps {
bool Looper::capturePickup(Mode mode,const Event& event) {
    if(!armed_||recording_||int(mode)!=recordTrack_) return false;
    const double remaining=countIn_>0?countIn_:length()-position_;
    if(remaining>loopPickupSeconds*sampleRate*stepPerFrame()+1.e-9) return false;
    if(event.type==EventType::Silence) { pickup_.count=0; return true; }
    if(event.type==EventType::NoteOff) {
        const auto end=std::remove_if(pickup_.data.begin(),pickup_.data.begin()+pickup_.count,[&](const Event& note) {
            return note.type==EventType::NoteOn&&note.id==event.id;
        });
        pickup_.count=size_t(end-pickup_.data.begin()); return true;
    }
    if(event.type!=EventType::NoteOn&&event.type!=EventType::Pluck&&event.type!=EventType::Drum&&event.type!=EventType::Expression) return true;
    if(event.type==EventType::NoteOn||event.type==EventType::Expression) {
        for(size_t n=0;n<pickup_.count;++n) if(pickup_.data[n].type==event.type&&pickup_.data[n].id==event.id) {
            pickup_.data[n]=event; return true;
        }
    }
    pickup_.add(event); return true;
}
}
