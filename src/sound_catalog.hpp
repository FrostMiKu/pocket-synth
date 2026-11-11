#pragma once
#include "music.hpp"
#include <string_view>
namespace ps {
inline constexpr size_t soundPartials=8;
enum class SoundModel { Guitar, DrumKit, Sustained, Struck };
struct SoundPreset {
    SoundId id;
    Mode mode;
    const char* key;
    const char* name;
    SoundModel model;
    bool isDefault;
    float attack,decay,sustain,release;
    std::array<float,soundPartials> partials;
    float tailSeconds=1,noise=0,fmIndex=0,brightness=.5f,damping=.68f,structure=.18f;
};
struct SoundCatalog { const SoundPreset* begin; const SoundPreset* end; };
SoundCatalog soundCatalog();
const SoundPreset* findSound(SoundId);
const SoundPreset* findSound(std::string_view key,Mode);
const SoundPreset& defaultSound(Mode);
const SoundPreset& resolveSound(SoundId,Mode);
const SoundPreset& selectedSound(const Settings&,Mode);
void cycleSound(Settings&,Mode,int direction);
size_t soundCount(Mode);
}
