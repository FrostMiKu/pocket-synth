#pragma once
#include "performance.hpp"
#include "input_guard.hpp"
#include <SDL.h>
namespace ps {
class InputDevice {
public:
    InputDevice();
    ~InputDevice();
    Input poll(bool& quit, bool& lost);
private:
    SDL_GameController* controller_=nullptr;
    bool focused_=true;
    InputGuard guard_;
    void connect();
};
}
