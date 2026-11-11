#pragma once
#include "music.hpp"
#include <array>
#include <vector>
namespace ps {
constexpr int drumCount=8;
extern const std::array<const char*,drumCount> drumNames;
class Drums {
public:
    Drums();
    void hit(int pad,float velocity,float tone);
    void silence();
    float process();
    bool active() const { return activeCount_>0; }
private:
    struct Voice { size_t cursor=0; int pad=0; float gain=0,low=0,tone=.5f,release=1; bool active=false; };
    std::array<Voice,drumCount*2> voices_{};
    std::array<unsigned,drumCount> next_{};
    int activeCount_=0;
};
}
