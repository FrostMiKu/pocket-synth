#include "string_level.hpp"
#include "music.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
namespace {
const float energyRate=1.f-std::exp(-1.f/(sampleRate*.04f));
const float gainRate=1.f-std::exp(-1.f/(sampleRate*.012f));
}
void StringLevel::reset(float midi) {
    // A conservative starting estimate; measured resonator energy takes over.
    gain_=std::clamp(std::exp2((60.f-midi)/36.f),minGain,maxGain);
    energy_=referenceRms*referenceRms/(gain_*gain_);
}
float StringLevel::process(float sample) {
    energy_+=energyRate*(sample*sample-energy_);
    const float target=std::clamp(referenceRms/std::sqrt(std::max(energy_,1.e-6f)),minGain,maxGain);
    gain_+=gainRate*(target-gain_);
    return sample*gain_;
}
}
