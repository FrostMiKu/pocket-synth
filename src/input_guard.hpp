#pragma once
#include "performance.hpp"
#include <algorithm>
#include <cmath>
namespace ps {
class InputGuard {
public:
    void suspend() { waiting_=true; }
    Input filter(const Input& input) {
        if(!waiting_) return input;
        const bool held=std::any_of(input.held.begin(),input.held.end(),[](bool b){return b;});
        if(held||std::hypot(input.lx,input.ly)>stickNeutral||std::hypot(input.rx,input.ry)>stickNeutral) { Input blocked; blocked.ready=false; return blocked; }
        waiting_=false; return input;
    }
private:
    bool waiting_=true;
};
}
