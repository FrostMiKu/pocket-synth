#include "engine.hpp"
#include <vector>
#include <cstdio>
#include <cmath>
int main(){
 std::puts("mode,note,velocity,rms,peak");
 for(auto mode:{ps::Mode::Guitar,ps::Mode::Expressive,ps::Mode::Keys})
 for(float velocity:{.3f,1.f})
 for(int note=16;note<=111;note+=1) {
  double energy=0; float peak=0; const int frames=ps::sampleRate/4;
  for(int take=0;take<8;++take) {
   ps::Engine e; e.setMode(mode);
   ps::Event event; event.type=mode==ps::Mode::Guitar?ps::EventType::Pluck:ps::EventType::NoteOn;
   event.note=float(note); event.velocity=velocity; event.timbre=.5;
   e.apply(event); std::vector<float> out(frames*2); e.render(out.data(),frames);
   for(float f:out) { energy+=f*f; peak=std::fmax(peak,std::abs(f)); }
  }
  std::printf("%d,%d,%.1f,%.8f,%.6f\n",int(mode),note,velocity,std::sqrt(energy/(8*frames*2)),peak);
 }
}
