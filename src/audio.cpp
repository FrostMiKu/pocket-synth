#include "audio.hpp"
#include "audio_float.hpp"
#include "loop_demo.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace ps {
Audio::Audio():studio_(std::make_unique<Studio>()) {}
LoopView Audio::loopView() const {
    if(device_) SDL_LockAudioDevice(device_);
    auto view=studio_->view();
    if(device_) SDL_UnlockAudioDevice(device_);
    return view;
}
bool Audio::loadLoop(const std::string& path) { return !device_&&studio_->looper().load(path); }
bool Audio::saveLoop(const std::string& path) {
    if(device_) SDL_LockAudioDevice(device_);
    const bool ok=studio_->looper().save(path);
    if(device_) SDL_UnlockAudioDevice(device_);
    return ok;
}
Audio::~Audio() { if(device_) SDL_CloseAudioDevice(device_); }
void Audio::prepareDemo() { if(!device_) prepareLoopDemo(studio_->looper()); studio_->prepareCaches(); }
bool Audio::open() {
    SDL_AudioSpec wanted{},actual{};
    wanted.freq=sampleRate; wanted.format=AUDIO_F32SYS;
    wanted.channels=2; wanted.samples=audioFrames;
    wanted.callback=callback; wanted.userdata=this;
    device_=SDL_OpenAudioDevice(nullptr,0,&wanted,&actual,0);
    if(!device_) return false;
    std::printf("audio driver=%s rate=%d frames=%d channels=%d\n",SDL_GetCurrentAudioDriver(),actual.freq,actual.samples,actual.channels);
    SDL_PauseAudioDevice(device_,0); return true;
}
bool Audio::submit(const Events& e, const Settings& s) {
    unsigned w=write_.load(std::memory_order_relaxed);
    unsigned next=(w+1)%capacity;
    if(next==read_.load(std::memory_order_acquire)) { panic_.store(true); ++overruns_; return false; }
    queue_[w]={e,s};
    write_.store(next,std::memory_order_release); return true;
}
void Audio::callback(void* userdata, Uint8* stream, int bytes) {
    const auto started=SDL_GetPerformanceCounter();
    configureAudioFloat();
    auto& a=*static_cast<Audio*>(userdata);
    if(a.panic_.exchange(false)) {
        a.read_.store(a.write_.load(std::memory_order_acquire),std::memory_order_release);
        a.studio_->apply({EventType::Loop,int(LoopCommand::Stop)});
    }
    unsigned r=a.read_.load(std::memory_order_relaxed);
    unsigned end=a.write_.load(std::memory_order_acquire);
    while(r!=end) {
        auto& p=a.queue_[r];
        a.studio_->configure(p.settings);
        for(size_t n=0;n<p.events.count;++n) a.studio_->apply(p.events.data[n]);
        r=(r+1)%capacity;
    }
    a.read_.store(r,std::memory_order_release);
    float* samples=reinterpret_cast<float*>(stream);
    size_t frames=size_t(bytes)/(sizeof(float)*2);
    a.studio_->render(samples,frames);
    float peak=0; for(size_t n=0;n<frames*2;++n) peak=std::max(peak,std::abs(samples[n]));
    if(peak>a.maxPeak_.load()) a.maxPeak_.store(peak);
    a.peak_.store(peak); ++a.callbacks_;
    uint64_t micros=(SDL_GetPerformanceCounter()-started)*1000000/SDL_GetPerformanceFrequency();
    if(micros>a.maxMicros_.load()) a.maxMicros_.store(micros);
    if(micros>frames*1000000/sampleRate) ++a.lateCallbacks_;
}
}
