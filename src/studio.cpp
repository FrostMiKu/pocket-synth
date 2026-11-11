#include "studio.hpp"
#include "audio_math.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
namespace ps {
Studio::Studio() {
    emptyBaseline_.fill(true);
    for(int mode=0;mode<modeCount;++mode) {
        live_[mode].setMode(Mode(mode)); delta_[mode].setMode(Mode(mode));
        live_[mode].setVolumeImmediate(1); delta_[mode].setVolumeImmediate(1);
    }
    cache_=std::make_unique<LoopCache>();
}
bool Studio::allReady(int tempo,int bars) const {
    for(int mode=0;mode<modeCount;++mode) if(looper_.eventCount(Mode(mode))&&!cache_->ready(Mode(mode),looper_.revision(Mode(mode)),tempo,bars)) return false;
    return true;
}
bool Studio::canPlayNow() const {
    for(int mode=0;mode<modeCount;++mode) if(looper_.eventCount(Mode(mode))&&!emptyBaseline_[mode]
        &&!cache_->prefix(looper_,Mode(mode),tempo_,bars_)) return false;
    return true;
}
void Studio::configure(const Settings& s) {
    if(!configured_) {
        for(int mode=0;mode<modeCount;++mode) emptyBaseline_[mode]=looper_.eventCount(Mode(mode))==0;
        if(looper_.running()) { looper_.command(LoopCommand::Play,s.mode); pendingPlay_=true; }
        tempo_=s.tempo; bars_=s.loopBars; configured_=true;
    }
    if(mode_!=s.mode) {
        const Event silence{EventType::Silence};
        live_[int(mode_)].apply(silence); looper_.record(mode_,silence);
        const auto state=looper_.view();
        if((state.recording||state.armed)&&state.recordTrack==int(mode_)) looper_.command(LoopCommand::Record,mode_);
        mode_=s.mode;
    }
    desired_=s; masterVolume_=s.volume;
    if(!looper_.running()&&!looper_.busyRecording()) { tempo_=s.tempo; bars_=s.loopBars; }
    Settings active=s; active.tempo=tempo_; active.loopBars=bars_; looper_.configure(active);
}
void Studio::apply(const Event& e) {
    if(e.type!=EventType::Loop) {
        Event note=e; note.sound=selectedSound(desired_,mode_).id;
        looper_.record(mode_,live_[int(mode_)].apply(note)); return;
    }
    const auto command=LoopCommand(e.id);
    if(command==LoopCommand::Play&&!looper_.running()&&!looper_.busyRecording()&&!canPlayNow()) {
        pendingPlay_=!pendingPlay_; return;
    }
    if(command==LoopCommand::Stop) {
        pendingPlay_=false; deltaFade_.fill(64);
        for(auto& engine:live_) engine.apply({EventType::Silence});
        clickLeft_=0;
    }
    if(command==LoopCommand::Play&&looper_.running()) deltaFade_.fill(64);
    looper_.command(command,mode_);
}
void Studio::serviceCaches(bool boundary) {
    for(int mode=0;mode<modeCount;++mode) cache_->request(looper_,Mode(mode),desired_.tempo,desired_.loopBars);
    const bool ready=allReady(desired_.tempo,desired_.loopBars);
    if(ready&&!looper_.busyRecording()&&(boundary||!looper_.running())) {
        tempo_=desired_.tempo; bars_=desired_.loopBars;
        Settings active=desired_; active.tempo=tempo_; active.loopBars=bars_; looper_.configure(active);
    }
    preparing_=!ready||tempo_!=desired_.tempo||bars_!=desired_.loopBars;
    for(int mode=0;mode<modeCount;++mode) {
        const Mode instrument=Mode(mode); const auto count=looper_.eventCount(instrument);
        if(!count) emptyBaseline_[mode]=true;
        bool prefix=cache_->prefix(looper_,instrument,tempo_,bars_);
        bool activated=false;
        if(count&&(boundary||!looper_.running()||(!prefix&&!emptyBaseline_[mode])))
            activated=cache_->activate(instrument,looper_.revision(instrument),tempo_,bars_);
        if(activated) { emptyBaseline_[mode]=false; prefix=true; if(looper_.running()) cacheGain_[mode]=1; }
        const bool exact=count&&cache_->exact(instrument,looper_.revision(instrument),tempo_,bars_);
        prefix_[mode]=count&&prefix;
        useCache_[mode]=prefix_[mode];
        const bool delta=count&&!exact&&(prefix||emptyBaseline_[mode]);
        if(useDelta_[mode]&&!delta) deltaFade_[mode]=64;
        useDelta_[mode]=delta;
    }
    if(pendingPlay_&&ready) { pendingPlay_=false; looper_.command(LoopCommand::Play,mode_); }
}
LoopView Studio::view() const {
    auto view=looper_.view(); view.preparing=preparing_||pendingPlay_; view.starting=pendingPlay_; view.requestedTempo=desired_.tempo; view.requestedBars=desired_.loopBars; return view;
}
void Studio::prepareCaches() {
    if(!configured_) configure(desired_);
    serviceCaches(true);
    while(!allReady(desired_.tempo,desired_.loopBars)) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); serviceCaches(true); }
    serviceCaches(true);
}
void Studio::render(float* out,size_t frames) {
    serviceCaches(false);
    unsigned initialStops=looper_.takeStops();
    for(int mode=0;mode<modeCount;++mode) if(initialStops&(1u<<mode)) delta_[mode].apply({EventType::Silence});
    const auto resume=looper_.takeResume();
    for(int mode=0;mode<modeCount;++mode) if(useDelta_[mode]) {
        const int first=prefix_[mode]?cache_->coveredLayers(Mode(mode))*melodyNotesPerLayer:0;
        for(size_t n=0;n<resume[mode].count;++n) if(resume[mode].data[n].id>=first) delta_[mode].apply(resume[mode].data[n]);
    }
    std::fill_n(out,frames*2,0.f);
    constexpr size_t maxBlock=64;
    std::array<float,maxBlock*2> scratch{};
    std::array<std::array<float,maxBlock*2>,modeCount> extra{};
    for(size_t offset=0;offset<frames;) {
        unsigned stops=looper_.takeStops();
        for(int mode=0;mode<modeCount;++mode) if(stops&(1u<<mode)) delta_[mode].apply({EventType::Silence});
        Event event; Mode instrument; unsigned order;
        while(looper_.next(instrument,event,&order)) {
            const int track=int(instrument);
            if(useDelta_[track]&&(!prefix_[track]||order>cache_->coveredOrder(instrument))) delta_[track].apply(event);
        }
        bool accent;
        if(looper_.takeClick(accent)) { clickLeft_=2400; clickPitch_=accent?1500:1000; }
        const size_t block=looper_.framesToNext(std::min(maxBlock,frames-offset));
        for(int mode=0;mode<modeCount;++mode) {
            if(live_[mode].active()) {
                live_[mode].render(scratch.data(),block,false);
                for(size_t n=0;n<block*2;++n) out[offset*2+n]+=scratch[n];
            }
            std::fill_n(extra[mode].data(),block*2,0.f);
            if(delta_[mode].active()) delta_[mode].render(extra[mode].data(),block,false);
        }
        bool boundary=false;
        for(size_t frame=0;frame<block;++frame) {
            float left=0,right=0;
            const double phase=looper_.phase();
            for(int mode=0;mode<modeCount;++mode) {
                const bool playing=looper_.running();
                const float target=playing&&useCache_[mode]?1.f:0.f;
                cacheGain_[mode]+=(target-cacheGain_[mode])*.02f;
                if(target==0&&cacheGain_[mode]<1.e-6f) cacheGain_[mode]=0;
                float a=0,b=0; cache_->sample(Mode(mode),phase,a,b);
                left+=a*cacheGain_[mode]; right+=b*cacheGain_[mode];
                const float gain=playing&&useDelta_[mode]?1.f:deltaFade_[mode]>0?float(deltaFade_[mode]--)/64:0;
                left+=extra[mode][frame*2]*gain; right+=extra[mode][frame*2+1]*gain;
                if(!useDelta_[mode]&&deltaFade_[mode]==1) delta_[mode].apply({EventType::Silence});
            }
            if(clickLeft_>0) {
                const float t=float(2400-clickLeft_--)/sampleRate;
                const float click=std::sin(t*clickPitch_*6.2831853f)*std::exp(-t*100)*.1f;
                left+=click; right+=click;
            }
            masterGain_+=(masterVolume_-masterGain_)*.002f;
            out[(offset+frame)*2]=softLimit((out[(offset+frame)*2]+left)*masterGain_);
            out[(offset+frame)*2+1]=softLimit((out[(offset+frame)*2+1]+right)*masterGain_);
            looper_.advance(); boundary=boundary||looper_.phase()<phase;
        }
        offset+=block;
        if(boundary) serviceCaches(true);
    }
}
}
