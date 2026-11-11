#pragma once
#include "performance.hpp"
#include <algorithm>
namespace ps {
inline constexpr int loopFileVersion=2;
inline constexpr char loopFileMagic[]="POCKET_LOOP";
constexpr int loopBeatsPerBar=4,loopStepsPerBeat=4;
constexpr int loopStepsPerBar=loopBeatsPerBar*loopStepsPerBeat,loopEventLimit=4096;
struct LoopRecord { double step=0; Event event; unsigned order=0; bool wraps=false; };
inline int recordedLayers(const LoopRecord* records,size_t count,Mode mode) {
    if(mode==Mode::Drums) return 0;
    const int voices=mode==Mode::Guitar?stringCount:melodyNotesPerLayer;
    int layers=0;
    for(size_t n=0;n<count;++n) {
        const auto& event=records[n].event;
        if(event.type==EventType::Pluck||event.type==EventType::NoteOn) layers=std::max(layers,event.id/voices+1);
    }
    return layers;
}
struct LoopTrackSnapshot {
    std::array<LoopRecord,loopEventLimit> events{};
    size_t count=0;
    uint64_t revision=1;
};
}
