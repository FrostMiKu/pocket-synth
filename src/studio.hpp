#pragma once
#include "engine.hpp"
#include "loop_cache.hpp"
namespace ps {
class Studio {
public:
    Studio();
    void configure(const Settings&);
    void apply(const Event&);
    void render(float*,size_t);
    void prepareCaches(); // Blocking setup for offline demos and measurements.
    LoopView view() const;
    Looper& looper() { return looper_; }
private:
    Looper looper_;
    std::array<Engine,modeCount> live_{},delta_{};
    std::unique_ptr<LoopCache> cache_;
    Settings desired_{};
    Mode mode_=Mode::Guitar;
    int tempo_=120,bars_=1,clickLeft_=0;
    float clickPitch_=1000,masterVolume_=.65f,masterGain_=0;
    bool configured_=false,pendingPlay_=false,preparing_=false;
    std::array<bool,modeCount> emptyBaseline_{},prefix_{},useCache_{},useDelta_{};
    std::array<int,modeCount> deltaFade_{};
    std::array<float,modeCount> cacheGain_{};
    void serviceCaches(bool boundary);
    bool allReady(int tempo,int bars) const;
    bool canPlayNow() const;
};
}
