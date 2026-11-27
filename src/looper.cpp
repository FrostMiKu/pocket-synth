#include "looper.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
Looper::Looper()=default;
void Looper::configure(const Settings& s) {
    tempo_=std::clamp(s.tempo,minTempo,maxTempo); quantize_=s.quantize;
    if(bars_!=s.loopBars) {
        finishTake(); armed_=false; bars_=std::clamp(s.loopBars,1,2);
        position_=std::fmod(position_,length()); rewind(); stops_=(1u<<modeCount)-1; resume_=true;
        for(auto& track:tracks_) while(track.cursor<track.count&&track.events[track.cursor].step<position_) ++track.cursor;
    }
}
void Looper::rewind() { for(auto& track:tracks_) track.cursor=0; dispatchTrack_=0; }
void Looper::beginTake() {
    // Stopped: the first input is the downbeat. Playing: the take starts in place.
    if(!playing_) { playing_=true; position_=0; lastBeat_=-1; rewind(); }
    // Quantized takes start on the grid, so their first attack repeats exactly one loop later.
    const bool quantize=quantize_==Quantize::All||(quantize_==Quantize::Drums&&recordTrack_==int(Mode::Drums));
    takeCount_=0; takeElapsed_=quantize?position_-std::round(position_):0; recording_=true; armed_=false; full_=false;
    noteHeld_.fill(false); expressionSet_=false;
    takeLayer_=0;
    if(recordTrack_!=int(Mode::Drums)) {
        takeLayer_=layers(Mode(recordTrack_));
        if(takeLayer_>=loopLayerCount) { recording_=false; full_=true; return; }
    }
    // The stick state held while waiting opens the take's expression.
    if(hasArmedExpression_) { hasArmedExpression_=false; record(Mode(recordTrack_),armedExpression_); }
}
void Looper::finishTake() {
    if(!recording_||recordTrack_<0) return;
    recording_=false;
    const bool hasNotes=std::any_of(take_.begin(),take_.begin()+takeCount_,[](const Recorded& r) {
        return r.event.type==EventType::NoteOn||r.event.type==EventType::Pluck||r.event.type==EventType::Drum;
    });
    if(!hasNotes) { takeCount_=0; notify(LoopNotice::EmptyTake,recordTrack_); return; }
    if(recordTrack_==int(Mode::Expressive)||recordTrack_==int(Mode::Keys)) {
        // Notes held through the end of a full take sustain until its start point.
        const double end=std::fmod(position_-stepPerFrame()+length(),length());
        for(int id=0;id<melodyNotesPerLayer;++id) if(noteHeld_[id]) {
            if(takeCount_>=loopEventLimit) { full_=true; takeCount_=0; return; }
            Event off{EventType::NoteOff,id+takeLayer_*melodyNotesPerLayer};
            take_[takeCount_++]={end,off,order_++,false}; noteHeld_[id]=false;
        }
    }
    auto& track=tracks_[recordTrack_];
    if(track.count+takeCount_>loopEventLimit) { full_=true; takeCount_=0; return; }
    auto before=[](const Recorded& a,const Recorded& b) { return a.step<b.step||(a.step==b.step&&a.order<b.order); };
    std::sort(take_.begin(),take_.begin()+takeCount_,before);
    std::copy_n(track.events.begin(),track.count,track.undo.begin()); track.undoCount=track.count; track.canUndo=true; track.undoRevision=track.revision;
    std::merge(track.events.begin(),track.events.begin()+track.count,take_.begin(),take_.begin()+takeCount_,merged_.begin(),before);
    track.revision=nextRevision_++;
    notify(LoopNotice::Recorded,recordTrack_);
    track.count+=takeCount_; std::copy_n(merged_.begin(),track.count,track.events.begin()); takeCount_=0;
    // Already heard live. A full take next repeats from its own start, at most one frame behind;
    // a take finished early resumes after the current position.
    const double from=takeElapsed_+1.e-9>=length()?position_-stepPerFrame()-1.e-9:position_+1.e-9;
    track.cursor=size_t(std::lower_bound(track.events.begin(),track.events.begin()+track.count,from,
       [](const Recorded& e,double step){return e.step<step;})-track.events.begin());
}
void Looper::cancelTake() {
    recording_=armed_=false; takeCount_=0; hasArmedExpression_=false;
}
void Looper::snapshot(Mode mode,LoopTrackSnapshot& out) const {
    const auto& track=tracks_[int(mode)];
    out.count=track.count; out.revision=track.revision;
    std::copy_n(track.events.begin(),track.count,out.events.begin());
}
size_t Looper::prefixCount(Mode mode,unsigned maxOrder) const {
    const auto& track=tracks_[int(mode)]; size_t count=0;
    for(size_t n=0;n<track.count;++n) if(track.events[n].order<=maxOrder) ++count;
    return count;
}
void Looper::command(LoopCommand command,Mode mode) {
    const int selected=int(mode);
    if(command==LoopCommand::Play) {
        if(playing_) { finishTake(); armed_=false; playing_=false; stops_=(1u<<modeCount)-1; }
        else { playing_=true; resume_=true; }
    } else if(command==LoopCommand::Record) {
        if(recording_||armed_) { const bool waiting=armed_; finishTake(); cancelTake(); if(waiting) notify(LoopNotice::Cancelled,recordTrack_); }
        else {
            recordTrack_=selected; full_=false;
            if(!canRecord(mode)) { full_=true; return; }
            armed_=true; notice_=LoopNotice::None; hasArmedExpression_=false;
            lastBeat_=playing_&&std::fmod(position_+1.e-9,loopStepsPerBeat)>1.e-8?int(position_/loopStepsPerBeat):-1;
        }
    } else if(command==LoopCommand::Undo) {
        full_=false;
        if((recording_||armed_)&&recordTrack_==selected) { cancelTake(); notify(LoopNotice::Cancelled,selected); }
        else {
            auto& track=tracks_[selected];
            if(track.canUndo) {
                track.count=track.undoCount; track.revision=track.undoRevision; std::copy_n(track.undo.begin(),track.count,track.events.begin());
                notify(LoopNotice::Undone,selected);
                track.canUndo=false; track.cursor=0; stops_|=1u<<selected; resume_=true;
                while(track.cursor<track.count&&track.events[track.cursor].step<position_) ++track.cursor;
            }
        }
    } else if(command==LoopCommand::Clear) {
        full_=false;
        if(recordTrack_==selected) { cancelTake(); }
        auto& track=tracks_[selected];
        if(track.count) { std::copy_n(track.events.begin(),track.count,track.undo.begin()); track.undoCount=track.count; track.canUndo=true; track.undoRevision=track.revision; }
        track.count=track.cursor=0; track.revision=nextRevision_++; stops_|=1u<<selected;
        notify(LoopNotice::Cleared,selected);
    } else if(command==LoopCommand::Stop) {
        finishTake(); armed_=false; playing_=false; stops_=(1u<<modeCount)-1;
    } else if(command==LoopCommand::Cue) {
        if(armed_) beginTake();
    }
}
void Looper::record(Mode mode,const Event& e) {
    if(e.type==EventType::AttackPitch) {
        if(e.gesture==0||mode==Mode::Drums) return;
        const int voices=mode==Mode::Guitar?stringCount:melodyNotesPerLayer;
        auto correctEvent=[&](Event& note) {
            if(note.gesture!=e.gesture||note.id%voices!=e.id||(note.type!=EventType::Pluck&&note.type!=EventType::NoteOn)) return false;
            note.note+=e.bend; return true;
        };
        auto correct=[&](Recorded* records,size_t count) {
            bool changed=false;
            for(size_t n=0;n<count;++n) changed=correctEvent(records[n].event)||changed;
            return changed;
        };
        if(recording_&&int(mode)==recordTrack_) correct(take_.data(),takeCount_);
        auto& track=tracks_[int(mode)];
        if(correct(track.events.data(),track.count)) {
            // Fresh order identities invalidate an older append-only cache prefix.
            for(size_t n=0;n<track.count;++n) track.events[n].order=order_++;
            track.revision=nextRevision_++;
        }
        if(e.id>=0&&e.id<int(lastNotes_.size())&&lastNotes_[e.id].gesture==e.gesture) lastNotes_[e.id].note+=e.bend;
        return;
    }
    bool fresh=e.type==EventType::Pluck||e.type==EventType::Drum;
    if(e.type==EventType::NoteOn&&e.id>=0&&e.id<8) {
        // A held expressive gate repeats NoteOn with its gesture; only new attacks start a take.
        fresh=!liveHeld_[e.id]||liveGesture_[e.id]!=e.gesture;
        liveHeld_[e.id]=true; liveGesture_[e.id]=e.gesture;
    } else if(e.type==EventType::NoteOff&&e.id>=0&&e.id<8) liveHeld_[e.id]=false;
    else if(e.type==EventType::Silence) liveHeld_.fill(false);
    if(armed_&&int(mode)==recordTrack_) {
        if(e.type==EventType::Expression) { armedExpression_=e; hasArmedExpression_=true; return; }
        if(!fresh) return;
        beginTake();
    }
    if(!recording_||int(mode)!=recordTrack_||e.type==EventType::Loop) return;
    const bool melodic=mode==Mode::Expressive||mode==Mode::Keys;
    if(mode==Mode::Drums&&e.type==EventType::Silence) return;
    if(mode==Mode::Guitar&&e.type==EventType::Silence) {
        record(mode,{EventType::LayerStop,0}); return;
    }
    if(melodic&&e.type==EventType::Silence) {
        for(int id=0;id<melodyNotesPerLayer;++id) if(noteHeld_[id]) record(mode,{EventType::NoteOff,id});
        return;
    }
    if(e.type==EventType::Expression) {
        if(expressionSet_&&std::abs(e.timbre-lastExpression_.timbre)<.01f&&std::abs(e.bend-lastExpression_.bend)<.02f&&std::abs(e.vibrato-lastExpression_.vibrato)<.01f) return;
        lastExpression_=e; expressionSet_=true;
    } else if(e.type==EventType::NoteOn&&e.id>=0&&e.id<8) {
        const auto& last=lastNotes_[e.id];
        if(noteHeld_[e.id]&&std::abs(e.note-last.note)<.02f&&std::abs(e.velocity-last.velocity)<.015f) return;
        noteHeld_[e.id]=true; lastNotes_[e.id]=e;
    } else if(e.type==EventType::NoteOff&&e.id>=0&&e.id<8) noteHeld_[e.id]=false;
    else if(e.type==EventType::Silence) noteHeld_.fill(false);
    if(takeCount_>=loopEventLimit) { full_=true; finishTake(); return; }
    double step=position_+e.delay*stepPerFrame();
    const bool quantize=quantize_==Quantize::All||(quantize_==Quantize::Drums&&mode==Mode::Drums);
    // Expression retains its continuous performance timing.
    if(quantize&&(e.type==EventType::Drum||e.type==EventType::NoteOn||e.type==EventType::NoteOff||e.type==EventType::Pluck)) step=std::round(step);
    const bool wraps=step>=length();
    step=std::fmod(step,length());
    Event event=e; event.delay=0;
    if(melodic) event.id+=takeLayer_*melodyNotesPerLayer;
    else if(mode==Mode::Guitar) event.id+=takeLayer_*stringCount;
    take_[takeCount_++]={step,event,order_++,wraps};
}
bool Looper::next(Mode& mode,Event& event,unsigned* order) {
    if(!playing_) return false;
    while(dispatchTrack_<modeCount) {
        auto& track=tracks_[dispatchTrack_];
        if(track.cursor<track.count&&track.events[track.cursor].step<=position_+1.e-9) {
            const auto& recorded=track.events[track.cursor++];
            if(recorded.step<length()) { event=recorded.event; mode=Mode(dispatchTrack_); if(order) *order=recorded.order; return true; }
        } else ++dispatchTrack_;
    }
    return false;
}
size_t Looper::framesToNext(size_t maximum) const {
    if(!playing_) return maximum;
    double distance=length()-position_;
    if(recording_||armed_) distance=std::min(distance,(std::floor((position_+1.e-9)/loopStepsPerBeat)+1)*loopStepsPerBeat-position_);
    if(recording_) distance=std::min(distance,length()-takeElapsed_);
    for(const auto& track:tracks_) if(track.cursor<track.count&&track.events[track.cursor].step<length())
        distance=std::min(distance,track.events[track.cursor].step-position_);
    const double frames=std::ceil((distance-1.e-9)/stepPerFrame());
    return size_t(std::clamp(frames,1.,double(maximum)));
}
void Looper::advance() {
    if(noticeFrames_>0&&--noticeFrames_==0) notice_=LoopNotice::None;
    dispatchTrack_=0;
    if(!playing_) return;
    position_+=stepPerFrame();
    if(recording_) {
        takeElapsed_+=stepPerFrame();
        if(takeElapsed_+1.e-9>=length()) finishTake();
    }
    if(position_+1.e-9>=length()) {
        position_=std::max(0.,position_-length());
        // A wrap changes score position; ringing instruments continue naturally.
        rewind();
    }
}
unsigned Looper::takeStops() { const unsigned stops=stops_; stops_=0; return stops; }
bool Looper::takeClick(bool& accent) {
    if(!playing_||(!recording_&&!armed_)) return false;
    const int beat=int((position_+1.e-9)/loopStepsPerBeat);
    if(beat==lastBeat_) return false;
    lastBeat_=beat; accent=beat%loopBeatsPerBar==0; return true;
}
std::array<Events,modeCount> Looper::takeResume() {
    std::array<Events,modeCount> result{};
    if(!resume_) return result;
    resume_=false;
    for(int mode=0;mode<modeCount;++mode) {
        const auto& track=tracks_[mode];
        std::array<Event,melodyVoiceCount> notes{}; std::array<bool,melodyVoiceCount> held{};
        std::array<Event,melodyLayerCount> expressions{}; std::array<bool,melodyLayerCount> hasExpression{};
        for(size_t n=0;n<track.count&&track.events[n].step<position_;++n) {
            const Event& e=track.events[n].event;
            if(e.type==EventType::Silence) held.fill(false);
            if(e.id>=0&&e.id<melodyVoiceCount) {
                if(e.type==EventType::NoteOn) { held[e.id]=true; notes[e.id]=e; }
                if(e.type==EventType::NoteOff) held[e.id]=false;
            }
            if(e.type==EventType::Expression) { const int layer=e.id/melodyNotesPerLayer; expressions[layer]=e; hasExpression[layer]=true; }
        }
        for(int layer=0;layer<melodyLayerCount;++layer) if(hasExpression[layer]) result[mode].add(expressions[layer]);
        for(size_t n=0;n<held.size();++n) if(held[n]) result[mode].add(notes[n]);
    }
    return result;
}
}
