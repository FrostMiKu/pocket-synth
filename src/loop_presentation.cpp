#include "loop_presentation.hpp"
namespace ps {
LoopActions loopActions(const LoopView& loop,Mode mode) {
    const int track=int(mode);
    const bool waiting=loop.armed;
    const bool active=waiting||loop.recording;
    LoopActions actions;
    actions.play=loop.playing?"PAUSE":"PLAY";
    actions.record=waiting?"CANCEL":loop.recording?"FINISH":"RECORD";
    actions.undo=active&&loop.recordTrack==track?"CANCEL":"UNDO";
    actions.clear="CLEAR";
    actions.canRecord=active||loop.recordable[track];
    actions.canUndo=(active&&loop.recordTrack==track)||loop.undo[track];
    actions.canClear=loop.events[track]>0||(active&&loop.recordTrack==track);
    return actions;
}
std::string loopPosition(const LoopView& loop) {
    std::string bars=std::to_string(loop.bars);
    if(loop.requestedBars!=loop.bars) bars+=">"+std::to_string(loop.requestedBars);
    return "BAR "+std::to_string(loop.bar())+"/"+bars+"  BEAT "+std::to_string(loop.beat())+"/"+std::to_string(loopBeatsPerBar);
}
bool recordStarting(const LoopView& loop) {
    return loop.recording&&loop.takeProgress*float(loop.bars*loopBeatsPerBar)<1;
}
std::string loopState(const LoopView& loop) {
    if(loop.savedUnavailable) return "CHECK LOOP";
    if(loop.full) return "TRACK FULL";
    if(loop.recording) return loop.overdubbing?"OVERDUBBING":"RECORDING";
    if(loop.armed) return "WAITING";
    if(loop.starting) return "PREPARING";
    return loop.playing?"PLAYING":"PAUSED";
}
std::string loopNotice(const LoopView& loop) {
    switch(loop.notice) {
    case LoopNotice::Recorded: return "TAKE RECORDED";
    case LoopNotice::Undone: return "TAKE UNDONE";
    case LoopNotice::Cleared: return "TRACK CLEARED";
    case LoopNotice::Cancelled: return "RECORDING CANCELLED";
    case LoopNotice::EmptyTake: return "PLAY NOTES TO RECORD";
    case LoopNotice::None: return "";
    }
    return "";
}
}
