#include "input.hpp"
#include <cstdio>
#include <algorithm>
namespace ps {
InputDevice::InputDevice() { connect(); }
InputDevice::~InputDevice() { if(controller_) SDL_GameControllerClose(controller_); }
void InputDevice::connect() {
    for(int n=0;n<SDL_NumJoysticks();++n) {
        char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(n),guid,sizeof(guid));
        std::printf("input index=%d guid=%s name=%s mapped=%d\n",n,guid,SDL_JoystickNameForIndex(n),SDL_IsGameController(n));
        if(!controller_&&SDL_IsGameController(n)) controller_=SDL_GameControllerOpen(n);
    }
}
Input InputDevice::poll(bool& quit, bool& lost) {
    Input i; lost=false; SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT) quit=true;
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { lost=true; focused_=false; guard_.suspend(); }
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) focused_=true;
        if(e.type==SDL_CONTROLLERDEVICEADDED&&!controller_) { connect(); guard_.suspend(); }
        if(e.type==SDL_CONTROLLERDEVICEREMOVED&&controller_&&
           e.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
            SDL_GameControllerClose(controller_); controller_=nullptr; lost=true; guard_.suspend(); connect();
        }
    }
    if(lost||!focused_) return i;
    const Uint8* k=SDL_GetKeyboardState(nullptr);
    constexpr SDL_Scancode keys[]={SDL_SCANCODE_Z,SDL_SCANCODE_X,SDL_SCANCODE_A,SDL_SCANCODE_S,
      SDL_SCANCODE_Q,SDL_SCANCODE_E,SDL_SCANCODE_1,SDL_SCANCODE_3,
      SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_RETURN,SDL_SCANCODE_TAB};
    for(int b=0;b<ButtonCount;++b) i.held[b]=k[keys[b]]!=0;
    if(k[SDL_SCANCODE_ESCAPE]) quit=true;
    i.lx=float(k[SDL_SCANCODE_L])-float(k[SDL_SCANCODE_J]);
    i.ly=float(k[SDL_SCANCODE_K])-float(k[SDL_SCANCODE_I]);
    i.rx=float(k[SDL_SCANCODE_H])-float(k[SDL_SCANCODE_F]);
    i.ry=float(k[SDL_SCANCODE_G])-float(k[SDL_SCANCODE_T]);
    if(controller_) {
        constexpr SDL_GameControllerButton buttons[]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,
          SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
          SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_INVALID,SDL_CONTROLLER_BUTTON_INVALID,
          SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,
          SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_BACK};
        for(int b=0;b<ButtonCount;++b) if(buttons[b]!=SDL_CONTROLLER_BUTTON_INVALID)
            i.held[b]=i.held[b]||SDL_GameControllerGetButton(controller_,buttons[b]);
        i.held[L2]=i.held[L2]||SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
        i.held[R2]=i.held[R2]||SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
        auto axis=[&](SDL_GameControllerAxis a){ return std::clamp(SDL_GameControllerGetAxis(controller_,a)/32767.f,-1.f,1.f); };
        i.lx+=axis(SDL_CONTROLLER_AXIS_LEFTX); i.ly+=axis(SDL_CONTROLLER_AXIS_LEFTY);
        i.rx+=axis(SDL_CONTROLLER_AXIS_RIGHTX); i.ry+=axis(SDL_CONTROLLER_AXIS_RIGHTY);
    }
    i.lx=std::clamp(i.lx,-1.f,1.f); i.ly=std::clamp(i.ly,-1.f,1.f);
    i.rx=std::clamp(i.rx,-1.f,1.f); i.ry=std::clamp(i.ry,-1.f,1.f);
    return guard_.filter(i);
}
}
