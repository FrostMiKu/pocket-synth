#pragma once
namespace ps {
// Normalize resonator energy before applying musical velocity and decay.
class StringLevel {
public:
    void reset(float midi);
    float process(float sample);
private:
    static constexpr float referenceRms=.12f,minGain=.25f,maxGain=4.f;
    float energy_=referenceRms*referenceRms,gain_=1;
};
}
