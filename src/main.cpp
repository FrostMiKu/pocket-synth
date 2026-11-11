#include "audio.hpp"
#include "product.hpp"
#include "input.hpp"
#include "ui.hpp"
#include "ui_style.hpp"
#include "settings.hpp"
#include "demo.hpp"
#include <cstdio>
#include <csignal>
#include <string>
#include <cmath>
#include <stdexcept>
#include <filesystem>
namespace { volatile std::sig_atomic_t stopped=0; void stop(int) { stopped=1; } }
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    bool smoke=false,demo=false,fullscreen=false,loopDemo=false;
    double duration=0; std::string capture,settingsPath="settings.cfg";
    bool customSettings=false;
    for(int n=1;n<argc;++n) {
        std::string a=argv[n];
        if(a=="--render-sounds"&&n+1<argc) return ps::renderSounds(argv[++n])?0:1;
        if(a=="--render-loop-demo"&&n+1<argc) return ps::renderDemo(argv[++n],true)?0:1;
        if(a=="--render-demo"&&n+1<argc) return ps::renderDemo(argv[++n])?0:1;
        if(a=="--smoke") { smoke=true; demo=true; duration=.4; }
        else if(a=="--loop-demo") { loopDemo=true; demo=true; duration=18; }
        else if(a=="--demo") { demo=true; duration=18; }
        else if(a=="--fullscreen") fullscreen=true;
        else if(a=="--seconds") {
            try {
                if(n+1>=argc) throw std::invalid_argument("missing");
                const std::string value=argv[++n]; size_t end=0;
                duration=std::stod(value,&end);
                if(end!=value.size()||!std::isfinite(duration)||duration<=0) throw std::invalid_argument("range");
            } catch(const std::exception&) { std::fputs("--seconds needs a positive finite number.\n",stderr); return 2; }
        }
        else if(a=="--screenshot"&&n+1<argc) capture=argv[++n];
        else if(a=="--settings"&&n+1<argc) { settingsPath=argv[++n]; customSettings=true; }
        else if(a=="--help") {
            std::printf("%s: --fullscreen --demo --loop-demo --render-sounds audio.wav --render-loop-demo audio.wav --seconds N --screenshot image.bmp --settings file --render-demo audio.wav --smoke\n",ps::product::name); return 0;
        } else { std::fprintf(stderr,"Unknown option: %s\n",a.c_str()); return 2; }
    }
#ifdef __APPLE__
    if(!customSettings&&!demo) {
        char* pref=SDL_GetPrefPath(ps::product::identifier,ps::product::identifier);
        if(!pref) { std::fprintf(stderr,"Settings: %s\n",SDL_GetError()); return 1; }
        settingsPath=std::string(pref)+"settings.cfg"; SDL_free(pref);
    }
#else
    (void)customSettings;
#endif
    std::signal(SIGINT,stop); std::signal(SIGTERM,stop);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)!=0) {
        std::fprintf(stderr,"Startup: %s\n",SDL_GetError()); return 1;
    }
    auto* window=SDL_CreateWindow(ps::product::name,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,ps::ui::width,ps::ui::height,
      fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
    if(!window) { std::fprintf(stderr,"Screen: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    auto* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer) { std::fprintf(stderr,"Drawing: %s\n",SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_RenderSetLogicalSize(renderer,ps::ui::width,ps::ui::height);
    std::printf("video=%s\n",SDL_GetCurrentVideoDriver());
    int result=0;
    {
        ps::Settings settings=demo?ps::Settings{}:ps::loadSettings(settingsPath);
        const std::string loopPath=settingsPath+".performance";
        ps::Audio audio;
        bool preserveSavedLoop=false;
        if(!demo) {
            std::error_code error;
            const bool exists=std::filesystem::exists(loopPath,error);
            preserveSavedLoop=(exists||bool(error))&&!audio.loadLoop(loopPath);
            if(preserveSavedLoop) std::fputs("Saved loop unavailable; previous file preserved.\n",stderr);
        }
        if(loopDemo) audio.prepareDemo();
        if(!audio.open()) { std::fprintf(stderr,"Sound: %s\n",SDL_GetError()); result=1; }
        else {
            ps::InputDevice input; ps::Controller controller;
            bool quit=false,captured=false; uint64_t begin=SDL_GetPerformanceCounter(),lastDraw=0;
            while(!quit&&!stopped) {
                uint64_t tick=SDL_GetPerformanceCounter(); double t=double(tick-begin)/SDL_GetPerformanceFrequency();
                if(duration>0&&t>=duration) break;
                bool lost=false; auto in=input.poll(quit,lost);
                if(quit||stopped) break;
                if(demo) in=ps::demoInput(t,settings,loopDemo);
                if((lost&&!demo)||(demo&&t>=16)) { auto stop=controller.reset(); stop.add({ps::EventType::Loop,int(ps::LoopCommand::Stop)}); audio.submit(stop,settings); }
                auto events=controller.update(in,settings,t); audio.submit(events,settings);
                quit=quit||controller.quit();
                if(tick-lastDraw>=SDL_GetPerformanceFrequency()/60) {
                    auto loopView=audio.loopView(); loopView.savedUnavailable=preserveSavedLoop;
                    ps::draw(renderer,settings,controller,in,audio.peak(),loopView);
                    if(!capture.empty()&&!captured&&t>.2) {
                        captured=ps::screenshot(renderer,capture.c_str());
                        if(!captured) { std::fprintf(stderr,"Capture: %s\n",SDL_GetError()); result=1; }
                    }
                    SDL_RenderPresent(renderer); lastDraw=tick;
                }
                SDL_Delay(2);
            }
            auto stop=controller.reset(); stop.add({ps::EventType::Loop,int(ps::LoopCommand::Stop)});
            audio.submit(stop,settings); SDL_Delay(150);
            if(!demo&&!preserveSavedLoop&&!audio.saveLoop(loopPath)) { std::fprintf(stderr,"Loop save failed: %s\n",loopPath.c_str()); result=1; }
            std::printf("callbacks=%llu queue_overruns=%llu max_callback_us=%llu late_callbacks=%llu peak=%.6f max_peak=%.6f\n",
              static_cast<unsigned long long>(audio.callbacks()),static_cast<unsigned long long>(audio.overruns()),static_cast<unsigned long long>(audio.maxMicros()),
              static_cast<unsigned long long>(audio.lateCallbacks()),audio.peak(),audio.maxPeak());
            if(smoke&&audio.callbacks()==0) result=1;
            if(!demo&&!ps::saveSettings(settingsPath,settings)) { std::fprintf(stderr,"Settings save failed: %s\n",settingsPath.c_str()); result=1; }
            if(!capture.empty()&&!captured) result=1;
        }
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return result;
}
