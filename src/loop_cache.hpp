#pragma once
#include "looper.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace ps {
class LoopCache {
public:
    LoopCache();
    ~LoopCache();
    LoopCache(const LoopCache&)=delete;
    LoopCache& operator=(const LoopCache&)=delete;
    void request(const Looper&,Mode,int tempo,int bars);
    bool ready(Mode,uint64_t revision,int tempo,int bars) const;
    bool activate(Mode,uint64_t revision,int tempo,int bars);
    bool exact(Mode,uint64_t revision,int tempo,int bars) const;
    bool prefix(const Looper&,Mode,int tempo,int bars) const;
    bool hasAudio(Mode) const;
    unsigned coveredOrder(Mode) const;
    int coveredLayers(Mode) const;
    void sample(Mode,double phase,float& left,float& right);
private:
    enum State { Free,Writing,Queued,Rendering,Ready,Active,Fading };
    struct Slot {
        std::atomic<int> state{Free};
        LoopTrackSnapshot source;
        std::vector<float> pcm;
        int track=0,tempo=120,bars=1,layers=0;
        size_t frames=0;
        unsigned maxOrder=0;
        uint64_t key=0;
    };
    static constexpr int banks=3,fadeFrames=64;
    std::array<std::array<Slot,banks>,modeCount> slots_{};
    std::array<std::atomic<uint64_t>,modeCount> wanted_{};
    std::array<int,modeCount> active_{},previous_{},fade_{};
    std::atomic<bool> stop_{false};
    std::array<std::thread,2> workers_;
    std::mutex waitMutex_;
    std::condition_variable wake_;
    static uint64_t key(uint64_t revision,int tempo,int bars);
    static bool matches(const Slot&,uint64_t revision,int tempo,int bars);
    static void read(const Slot&,double phase,float&,float&);
    void worker(int);
    bool render(Slot&);
};
}
