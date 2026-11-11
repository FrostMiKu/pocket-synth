#include "audio.hpp"
#include <cmath>
#include <cstdio>
using namespace ps;
int main() {
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    if(SDL_Init(SDL_INIT_AUDIO)!=0) return 1;
    int failures=0;
    auto check=[&](bool ok,const char* message) {
        if(!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
    };
    {
        Audio audio;
        if(!audio.open()) { std::fprintf(stderr,"Audio: %s\n",SDL_GetError()); return 1; }
        Settings settings; settings.mode=Mode::Expressive;
        Events on; Event note; note.type=EventType::NoteOn; note.note=69; on.add(note);
        for(int n=0;n<20000;++n) audio.submit(on,settings);
        SDL_Delay(100);
        check(audio.overruns()>0,"queue pressure exercises overflow handling");
        Events stop; stop.add({EventType::Silence});
        check(audio.submit(stop,settings),"queue recovers after pressure");
        SDL_Delay(150);
        check(audio.callbacks()>0&&std::isfinite(audio.peak()),"the audio callback remains active and finite");
        check(audio.peak()==0,"release after pressure leaves no stuck note");
        check(audio.submit(on,settings),"fresh notes are accepted after recovery");
        SDL_Delay(100);
        check(audio.peak()>.001f,"a fresh note sounds after recovery");
        audio.submit(stop,settings); SDL_Delay(100);
        check(audio.peak()==0,"global stop completes its short release");
    }
    SDL_Quit();
    std::printf("Audio queue audit: %d failures\n",failures);
    return failures?1:0;
}
