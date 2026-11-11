#pragma once
#include "music.hpp"
#include <algorithm>
namespace ps {
inline constexpr double pitchGestureSeconds=.030;
enum PitchModifier { OctaveUp=1,OctaveDown=2,SemitoneUp=4 };
inline int modifiedOctave(int base,int modifiers) {
    return std::clamp(base+int(bool(modifiers&OctaveUp))-int(bool(modifiers&OctaveDown)),minOctave,maxOctave);
}
inline int modifierPitch(int base,int modifiers) {
    return 12*(modifiedOctave(base,modifiers)-base)+int(bool(modifiers&SemitoneUp));
}
}
