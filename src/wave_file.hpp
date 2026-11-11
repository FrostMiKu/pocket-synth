#pragma once
#include <fstream>
#include <string>
namespace ps {
class WaveFile {
public:
    WaveFile(const std::string& path,int frames);
    void write(const float* stereo,int frames);
    bool finish() { out_.flush(); return bool(out_); }
private:
    std::ofstream out_;
    void word(uint32_t,int bytes);
};
}
