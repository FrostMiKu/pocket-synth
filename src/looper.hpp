#pragma once
#include "loop_snapshot.hpp"
#include "drums.hpp"
#include <array>
#include <string>
namespace ps {
enum class LoopNotice { None, Recorded, Undone, Cleared, Cancelled, EmptyTake };
struct LoopView {
    int tempo=120,bars=1,step=0,countBeat=0,recordTrack=-1;
    bool overdubbing=false,playing=false,recording=false,armed=false,full=false,savedUnavailable=false,preparing=false,starting=false;
    int requestedTempo=120,requestedBars=1;
    float progress=0,beatProgress=0;
    LoopNotice notice=LoopNotice::None;
    int noticeTrack=-1;
    std::array<int,modeCount> layers{};
    std::array<bool,modeCount> recordable{};
    std::array<int,modeCount> events{};
    std::array<bool,modeCount> undo{};
    std::array<uint32_t,drumCount> drumSteps{};
    int bar() const { return step/loopStepsPerBar+1; }
    int beat() const { return (step%loopStepsPerBar)/loopStepsPerBeat+1; }
};
class Looper {
public:
    Looper();
    void configure(const Settings&);
    void command(LoopCommand,Mode);
    void record(Mode,const Event&);
    bool next(Mode&,Event&,unsigned* order=nullptr);
    void advance();
    size_t framesToNext(size_t maximum) const;
    unsigned takeStops();
    bool takeClick(bool& accent);
    std::array<Events,modeCount> takeResume();
    LoopView view() const;
    void snapshot(Mode,LoopTrackSnapshot&) const;
    uint64_t revision(Mode mode) const { return tracks_[int(mode)].revision; }
    uint64_t undoRevision(Mode mode) const { return tracks_[int(mode)].undoRevision; }
    size_t eventCount(Mode mode) const { return tracks_[int(mode)].count; }
    size_t prefixCount(Mode,unsigned maxOrder) const;
    double phase() const { return position_/length(); }
    bool running() const { return playing_&&countIn_<=0; }
    bool busyRecording() const { return recording_||armed_; }
    bool save(const std::string&);
    bool load(const std::string&);
private:
    using Recorded=LoopRecord;
    struct Track {
        std::array<Recorded,loopEventLimit> events{},undo{};
        size_t count=0,undoCount=0,cursor=0;
        bool canUndo=false;
        uint64_t revision=1,undoRevision=0;
    };
    std::array<Track,modeCount> tracks_{};
    std::array<Recorded,loopEventLimit> take_{},merged_{};
    std::array<Event,8> lastNotes_{};
    std::array<bool,8> noteHeld_{};
    Event lastExpression_{};
    Events pickup_{};
    size_t takeCount_=0;
    int recordTrack_=-1,takeLayer_=0,tempo_=120,bars_=1,dispatchTrack_=0,lastBeat_=-1;
    Quantize quantize_=Quantize::Drums;
    double position_=0,countIn_=0;
    bool playing_=false,armed_=false,recording_=false,full_=false,expressionSet_=false,resume_=false;
    unsigned stops_=0,order_=0;
    uint64_t nextRevision_=2;
    LoopNotice notice_=LoopNotice::None;
    int noticeTrack_=-1,noticeFrames_=0;
    void notify(LoopNotice notice,int track);
    int layers(Mode) const;
    bool canRecord(Mode) const;
    void cancelTake();
    double length() const { return bars_*loopStepsPerBar; }
    double stepPerFrame() const { return double(tempo_)*loopStepsPerBeat/(60*sampleRate); }
    bool capturePickup(Mode,const Event&);
    void beginTake();
    void finishTake();
    void rewind();
};
}
