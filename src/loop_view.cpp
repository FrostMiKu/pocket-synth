#include "looper.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
void Looper::notify(LoopNotice notice,int track) { notice_=notice; noticeTrack_=track; noticeFrames_=sampleRate*2; }
int Looper::layers(Mode mode) const {
    const auto& track=tracks_[int(mode)]; return recordedLayers(track.events.data(),track.count,mode);
}
bool Looper::canRecord(Mode mode) const {
    return tracks_[int(mode)].count<loopEventLimit&&(mode==Mode::Drums||layers(mode)<loopLayerCount);
}
LoopView Looper::view() const {
    LoopView view; view.tempo=view.requestedTempo=tempo_; view.bars=view.requestedBars=bars_; view.step=std::min(int(length())-1,int(position_+1.e-9));
    view.playing=playing_; view.recording=recording_; view.armed=armed_; view.recordTrack=recordTrack_; view.full=full_;
    view.overdubbing=recording_&&recordTrack_>=0&&tracks_[recordTrack_].count>0;
    view.progress=float(countIn_>0?(loopStepsPerBar-countIn_)/loopStepsPerBar:phase());
    view.notice=notice_; view.noticeTrack=noticeTrack_;
    view.countBeat=countIn_>0?std::min(loopBeatsPerBar,int((loopStepsPerBar-countIn_)/loopStepsPerBeat)+1):0;
    for(int mode=0;mode<modeCount;++mode) { view.recordable[mode]=canRecord(Mode(mode)); view.layers[mode]=layers(Mode(mode)); view.events[mode]=int(tracks_[mode].count); view.undo[mode]=tracks_[mode].canUndo; }
    if(recording_&&recordTrack_>=0) view.events[recordTrack_]+=int(takeCount_);
    auto mark=[&](const Recorded& e) {
        if(e.event.type==EventType::Drum&&e.event.id>=0&&e.event.id<drumCount&&e.step<length())
            view.drumSteps[e.event.id]|=1u<<std::clamp(int(std::round(e.step))%int(length()),0,31);
    };
    const auto& drums=tracks_[int(Mode::Drums)]; for(size_t n=0;n<drums.count;++n) mark(drums.events[n]);
    if(recording_&&recordTrack_==int(Mode::Drums)) for(size_t n=0;n<takeCount_;++n) mark(take_[n]);
    return view;
}
}
