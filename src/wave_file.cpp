#include "wave_file.hpp"
#include "music.hpp"
#include <algorithm>
namespace ps {
void WaveFile::word(uint32_t value,int bytes) { for(int n=0;n<bytes;++n) out_.put(char(value>>(8*n))); }
WaveFile::WaveFile(const std::string& path,int frames):out_(path,std::ios::binary) {
    out_.write("RIFF",4); word(36+frames*4,4); out_.write("WAVEfmt ",8);
    word(16,4); word(1,2); word(2,2); word(sampleRate,4); word(sampleRate*4,4);
    word(4,2); word(16,2); out_.write("data",4); word(frames*4,4);
}
void WaveFile::write(const float* audio,int frames) {
    for(int n=0;n<frames*2;++n) word(uint16_t(int16_t(std::clamp(audio[n],-1.f,1.f)*32767)),2);
}
}
