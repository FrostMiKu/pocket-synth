#include "loop_cache.hpp"
#include "engine.hpp"
#include "audio_float.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
namespace ps {
uint64_t LoopCache::key(uint64_t revision,int tempo,int bars) { return (revision<<12)|uint64_t(tempo<<2)|uint64_t(bars); }
bool LoopCache::matches(const Slot& slot,uint64_t revision,int tempo,int bars) {
    return slot.source.revision==revision&&slot.tempo==tempo&&slot.bars==bars;
}
LoopCache::LoopCache() {
    active_.fill(-1); previous_.fill(-1); fade_.fill(0);
    for(int n=0;n<int(workers_.size());++n) workers_[n]=std::thread([this,n]{worker(n);});
}
LoopCache::~LoopCache() { stop_.store(true); wake_.notify_all(); for(auto& thread:workers_) thread.join(); }
void LoopCache::request(const Looper& loop,Mode mode,int tempo,int bars) {
    const int track=int(mode); const uint64_t revision=loop.revision(mode),desired=key(revision,tempo,bars);
    wanted_[track].store(desired,std::memory_order_release);
    if(!loop.eventCount(mode)) return;
    for(const auto& slot:slots_[track]) {
        const int state=slot.state.load(std::memory_order_acquire);
        if(state!=Free&&state!=Writing&&matches(slot,revision,tempo,bars)) return;
    }
    for(int bank=0;bank<banks;++bank) {
        auto& slot=slots_[track][bank]; int state=slot.state.load(std::memory_order_acquire);
        if(state!=Free&&state!=Ready&&state!=Queued) continue;
        if(state==Ready&&slot.source.revision==loop.undoRevision(mode)&&bank!=active_[track]) {
            // Preserve one useful undo cache while another slot is available.
            bool alternative=false;
            for(int other=0;other<banks;++other) if(other!=bank) {
                int s=slots_[track][other].state.load(std::memory_order_acquire);
                if(s==Free||s==Queued||(s==Ready&&slots_[track][other].source.revision!=loop.undoRevision(mode))) alternative=true;
            }
            if(alternative) continue;
        }
        if(!slot.state.compare_exchange_strong(state,Writing,std::memory_order_acq_rel)) continue;
        loop.snapshot(mode,slot.source); slot.track=track; slot.tempo=tempo; slot.bars=bars; slot.key=desired;
        slot.state.store(Queued,std::memory_order_release); wake_.notify_one(); return;
    }
}
bool LoopCache::ready(Mode mode,uint64_t revision,int tempo,int bars) const {
    for(const auto& slot:slots_[int(mode)]) {
        const int state=slot.state.load(std::memory_order_acquire);
        if((state==Ready||state==Active||state==Fading)&&matches(slot,revision,tempo,bars)) return true;
    }
    return false;
}
bool LoopCache::exact(Mode mode,uint64_t revision,int tempo,int bars) const {
    const int bank=active_[int(mode)]; return bank>=0&&matches(slots_[int(mode)][bank],revision,tempo,bars);
}
bool LoopCache::hasAudio(Mode mode) const { return active_[int(mode)]>=0; }
unsigned LoopCache::coveredOrder(Mode mode) const { const int bank=active_[int(mode)]; return bank<0?0:slots_[int(mode)][bank].maxOrder; }
int LoopCache::coveredLayers(Mode mode) const { const int bank=active_[int(mode)]; return bank<0?0:slots_[int(mode)][bank].layers; }
bool LoopCache::prefix(const Looper& loop,Mode mode,int tempo,int bars) const {
    const int bank=active_[int(mode)]; if(bank<0) return false;
    const auto& slot=slots_[int(mode)][bank];
    return slot.tempo==tempo&&slot.bars==bars&&loop.prefixCount(mode,slot.maxOrder)==slot.source.count;
}
bool LoopCache::activate(Mode mode,uint64_t revision,int tempo,int bars) {
    const int track=int(mode);
    if(exact(mode,revision,tempo,bars)) return false;
    for(int bank=0;bank<banks;++bank) {
        auto& slot=slots_[track][bank]; int state=Ready;
        if(!matches(slot,revision,tempo,bars)||!slot.state.compare_exchange_strong(state,Active,std::memory_order_acq_rel)) continue;
        if(previous_[track]>=0) slots_[track][previous_[track]].state.store(Ready,std::memory_order_release);
        previous_[track]=active_[track]; fade_[track]=fadeFrames;
        if(previous_[track]>=0) slots_[track][previous_[track]].state.store(Fading,std::memory_order_release);
        active_[track]=bank; return true;
    }
    return false;
}
void LoopCache::read(const Slot& slot,double phase,float& left,float& right) {
    const double position=phase*slot.frames;
    const size_t a=std::min(size_t(position),slot.frames-1),b=(a+1)%slot.frames;
    const float fraction=float(position-double(a));
    left=slot.pcm[a*2]+(slot.pcm[b*2]-slot.pcm[a*2])*fraction;
    right=slot.pcm[a*2+1]+(slot.pcm[b*2+1]-slot.pcm[a*2+1])*fraction;
}
void LoopCache::sample(Mode mode,double phase,float& left,float& right) {
    const int track=int(mode),bank=active_[track]; left=right=0;
    if(bank<0) return;
    read(slots_[track][bank],phase,left,right);
    if(fade_[track]>0) {
        float a=0,b=0;
        if(previous_[track]>=0) read(slots_[track][previous_[track]],phase,a,b);
        const float mix=1-float(fade_[track])/fadeFrames;
        left=a+(left-a)*mix; right=b+(right-b)*mix;
        if(--fade_[track]==0&&previous_[track]>=0) {
            slots_[track][previous_[track]].state.store(Ready,std::memory_order_release); previous_[track]=-1;
        }
    }
}
void LoopCache::worker(int offset) {
    configureAudioFloat();
    while(!stop_.load(std::memory_order_acquire)) {
        bool found=false;
        for(int step=0;step<modeCount*banks;++step) {
            const int index=(step+offset*banks)%(modeCount*banks);
            auto& slot=slots_[index/banks][index%banks]; int state=Queued;
            if(!slot.state.compare_exchange_strong(state,Rendering,std::memory_order_acq_rel)) continue;
            found=true; bool ok=slot.key==wanted_[slot.track].load(std::memory_order_acquire)&&render(slot);
            slot.state.store(ok?Ready:Free,std::memory_order_release);
        }
        if(!found) { std::unique_lock<std::mutex> lock(waitMutex_); wake_.wait_for(lock,std::chrono::milliseconds(5)); }
    }
}
bool LoopCache::render(Slot& slot) {
    const double framesPerStep=double(sampleRate)*60/(slot.tempo*loopStepsPerBeat);
    slot.frames=size_t(std::llround(slot.bars*loopStepsPerBar*framesPerStep));
    slot.pcm.resize((slot.frames+fadeFrames)*2);
    slot.maxOrder=0; slot.layers=recordedLayers(slot.source.events.data(),slot.source.count,Mode(slot.track));
    for(size_t n=0;n<slot.source.count;++n) {
        const auto& record=slot.source.events[n]; slot.maxOrder=std::max(slot.maxOrder,record.order);
    }
    auto engine=std::make_unique<Engine>(); engine->setMode(Mode(slot.track)); engine->setVolumeImmediate(1);
    std::array<float,512> scratch{};
    // Two uninterrupted priming cycles include tails longer than the shortest
    // loop. Retain the third cycle and its natural continuation on the worker.
    constexpr int retainedCycle=2;
    for(int cycle=0;cycle<=retainedCycle;++cycle) {
        size_t cursor=0,position=0;
        while(position<slot.frames) {
            if(stop_.load(std::memory_order_relaxed)||slot.key!=wanted_[slot.track].load(std::memory_order_acquire)) return false;
            while(cursor<slot.source.count&&size_t(std::llround(slot.source.events[cursor].step*framesPerStep))<=position) {
                if(slot.source.events[cursor].step<slot.bars*loopStepsPerBar) engine->apply(slot.source.events[cursor].event);
                ++cursor;
            }
            size_t block=std::min(size_t(256),slot.frames-position);
            if(cursor<slot.source.count) {
                const auto due=size_t(std::llround(slot.source.events[cursor].step*framesPerStep));
                if(due>position) block=std::min(block,due-position);
            }
            float* out=cycle==retainedCycle?slot.pcm.data()+position*2:scratch.data();
            engine->render(out,block,false); position+=block;
        }
    }
    engine->render(slot.pcm.data()+slot.frames*2,fadeFrames,false);
    for(int n=0;n<fadeFrames;++n) {
        const float blend=.5f-.5f*std::cos(float(n)*3.14159265359f/(fadeFrames-1));
        for(int channel=0;channel<2;++channel) {
            const size_t first=size_t(n)*2+channel,tail=(slot.frames+size_t(n))*2+channel;
            slot.pcm[first]=slot.pcm[tail]+(slot.pcm[first]-slot.pcm[tail])*blend;
        }
    }
    return true;
}
}
