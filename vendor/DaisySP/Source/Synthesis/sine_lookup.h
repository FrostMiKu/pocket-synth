#pragma once
#include <array>
#include <cmath>
namespace daisysp {
// Pocket Instrument: interpolated full-cycle sine, shared by oscillators and
// vibrato. The electronic kit primes this immutable table before audio starts.
inline float SineLookup(float phase) {
    constexpr int size=4096;
    static const auto table=[] {
        std::array<float,size+1> values{};
        for(int n=0;n<=size;++n) values[n]=std::sin(float(n)*6.28318530718f/size);
        return values;
    }();
    if(!std::isfinite(phase)) return 0;
    phase-=std::floor(phase);
    const float position=phase*size;
    const int index=int(position);
    const float fraction=position-index;
    const int wrapped=index&(size-1);
    return table[wrapped]+(table[wrapped+1]-table[wrapped])*fraction;
}
}
