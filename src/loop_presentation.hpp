#pragma once
#include "looper.hpp"
namespace ps {
struct LoopActions {
    std::string play,record,undo,clear;
    bool canRecord=true,canUndo=false,canClear=false;
};
LoopActions loopActions(const LoopView&,Mode);
int recordCountdownBeat(const LoopView&);
std::string loopState(const LoopView&);
std::string loopPosition(const LoopView&);
std::string loopNotice(const LoopView&);
}
