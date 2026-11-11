#pragma once
#include "studio.hpp"
#include <memory>
#include <SDL.h>
#include <atomic>
namespace ps {
class Audio {
public:
    Audio();
    ~Audio();
    bool open();
    void prepareDemo();
    LoopView loopView() const;
    bool loadLoop(const std::string&);
    bool saveLoop(const std::string&);
    bool submit(const Events&, const Settings&);
    uint64_t callbacks() const { return callbacks_.load(); }
    uint64_t overruns() const { return overruns_.load(); }
    uint64_t maxMicros() const { return maxMicros_.load(); }
    uint64_t lateCallbacks() const { return lateCallbacks_.load(); }
    float maxPeak() const { return maxPeak_.load(); }
    float peak() const { return peak_.load(); }
private:
    struct Packet { Events events; Settings settings; };
    static constexpr unsigned capacity=128;
    std::array<Packet, capacity> queue_{};
    std::atomic<unsigned> read_{0},write_{0};
    std::atomic<uint64_t> callbacks_{0},overruns_{0};
    std::atomic<uint64_t> maxMicros_{0},lateCallbacks_{0};
    std::atomic<bool> panic_{false};
    std::atomic<float> peak_{0},maxPeak_{0};
    SDL_AudioDeviceID device_=0;
    std::unique_ptr<Studio> studio_;
    static void callback(void*, Uint8*, int);
};
}
