#include "sound_catalog.hpp"
#include <iterator>
namespace ps {
namespace {
// Stable identities and all preset data live here. Add a row to extend a family.
// IDs and keys remain stable as rows are reordered or new sounds are introduced.
constexpr SoundPreset presets[]={
    {1,Mode::Guitar,"acoustic","ACOUSTIC",SoundModel::Guitar,true,.001f,.42f,0,.006f,{1}},
    {10,Mode::Expressive,"flute","FLUTE",SoundModel::Sustained,true,.025f,.12f,.85f,.16f,{1,.08f,.13f,.025f},1,.012f},
    {11,Mode::Expressive,"violin","VIOLIN",SoundModel::Sustained,false,.075f,.18f,.82f,.22f,{1,.62f,.42f,.3f,.22f,.15f,.1f,.07f},1,.004f},
    {20,Mode::Keys,"piano","PIANO",SoundModel::Struck,true,.002f,.1f,1,.18f,{1,.46f,.28f,.16f,.09f,.05f},1.3f,.035f},
    {21,Mode::Keys,"electric-piano","ELECTRIC PIANO",SoundModel::Struck,false,.003f,.1f,1,.22f,{1,.16f,.04f},1.8f,0,2.8f},
    {30,Mode::Drums,"electronic-kit","ELECTRONIC KIT",SoundModel::DrumKit,true,.001f,.1f,0,.006f,{1}},
};
}
SoundCatalog soundCatalog() { return {std::begin(presets),std::end(presets)}; }
const SoundPreset* findSound(SoundId id) {
    for(const auto& preset:presets) if(preset.id==id) return &preset;
    return nullptr;
}
const SoundPreset* findSound(std::string_view key,Mode mode) {
    for(const auto& preset:presets) if(preset.mode==mode&&key==preset.key) return &preset;
    return nullptr;
}
const SoundPreset& defaultSound(Mode mode) {
    for(const auto& preset:presets) if(preset.mode==mode&&preset.isDefault) return preset;
    return presets[0];
}
const SoundPreset& resolveSound(SoundId id,Mode mode) {
    const auto* preset=findSound(id);
    return preset&&preset->mode==mode?*preset:defaultSound(mode);
}
const SoundPreset& selectedSound(const Settings& s,Mode mode) { return resolveSound(s.sounds[int(mode)],mode); }
size_t soundCount(Mode mode) { size_t count=0; for(const auto& p:presets) count+=p.mode==mode; return count; }
void cycleSound(Settings& s,Mode mode,int direction) {
    const auto& current=selectedSound(s,mode); size_t index=size_t(&current-presets);
    const size_t count=std::size(presets);
    for(size_t n=0;n<count;++n) {
        index=direction>0?(index+1)%count:(index+count-1)%count;
        if(presets[index].mode==mode) { s.sounds[int(mode)]=presets[index].id; return; }
    }
}
}
