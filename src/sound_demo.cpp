#include "demo.hpp"
#include "engine.hpp"
#include "wave_file.hpp"
#include "audio_float.hpp"
namespace ps {
bool renderSounds(const std::string& path) {
    configureAudioFloat(); const auto catalog=soundCatalog(); int count=0;
    for(auto p=catalog.begin;p!=catalog.end;++p) count+=p->model==SoundModel::Sustained||p->model==SoundModel::Struck;
    constexpr int seconds=4,block=240; WaveFile out(path,count*seconds*sampleRate);
    std::array<float,block*2> audio{};
    for(auto p=catalog.begin;p!=catalog.end;++p) if(p->model==SoundModel::Sustained||p->model==SoundModel::Struck) {
        Engine engine; engine.setMode(p->mode); engine.setVolumeImmediate(.85f);
        for(int frame=0;frame<seconds*sampleRate;frame+=block) {
            if(frame==0||frame==sampleRate/2||frame==sampleRate||frame==sampleRate*3/2) {
                engine.apply({EventType::NoteOff,0});
                Event e; e.type=EventType::NoteOn; e.sound=p->id;
                constexpr int notes[]={60,64,67,72}; e.note=float(notes[frame/(sampleRate/2)]); e.velocity=.8f; engine.apply(e);
            }
            if(frame==sampleRate*3) engine.apply({EventType::NoteOff,0});
            engine.render(audio.data(),block); out.write(audio.data(),block);
        }
    }
    return out.finish();
}
}
