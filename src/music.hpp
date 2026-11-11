#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace ps {
using SoundId=uint16_t;
constexpr float silenceReleaseSeconds=.006f;
constexpr int sampleRate = 48000;
constexpr int audioFrames = 512;
constexpr int stringCount = 6;
constexpr int wheelSize = 8;
constexpr int loopLayerCount=4,guitarVoiceCount=stringCount*loopLayerCount;
constexpr int melodyLayerCount=loopLayerCount,melodyNotesPerLayer=8,melodyVoiceCount=melodyLayerCount*melodyNotesPerLayer;
enum class Mode { Guitar, Expressive, Keys, Drums, Count };
constexpr int modeCount=int(Mode::Count);
enum class Quantize { Drums, All, Off };
constexpr int minTempo=40,maxTempo=240;
enum class Scale { Major, Minor, Pentatonic, Count };
constexpr int scaleCount=int(Scale::Count);
enum class NoteDisplay { Names, Numbered };
enum class SettingRow { Root, Scale, Octave, Volume, ExpressionPitch, ExpressionGate, NoteDisplay, FaceButtons, Tempo, LoopBars, Quantize, Count };
constexpr int settingCount=int(SettingRow::Count);
constexpr int middleOctave=4,minOctave=2,maxOctave=6;
struct Settings {
    Mode mode = Mode::Guitar;
    Scale scale = Scale::Major;
    NoteDisplay noteDisplay = NoteDisplay::Names;
    int root = 0, octave = middleOctave, chordBank = 0;
    int tempo=120, loopBars=1;
    Quantize quantize=Quantize::Drums;
    float volume = .65f;
    std::array<SoundId,modeCount> sounds{};
    bool continuousPitch = false, holdGate = true, swapFaceButtons = false;
};
struct NoteLabel { std::string text; int octaveDots=0; };
struct Chord { const char* name; std::array<int, stringCount> notes; };
extern const std::array<std::array<Chord, wheelSize>, 2> chordBanks;
extern const std::array<const char*, modeCount> modeNames;
extern const std::array<const char*, scaleCount> scaleNames;
int scaleNote(int degree, const Settings& settings);
int guitarNote(const Chord&,int string,const Settings&);
int sector(float x, float y);
int stableSector(float x,float y,int previous);
float continuousWheelNote(float x,float y,const Settings&);
float frequency(float midi);
std::string pitchClassName(int midi);
std::string noteName(int midi);
NoteLabel noteLabel(int midi, const Settings&);
std::string chordLabel(const Chord&, const Settings&, int semitoneOffset=0);
std::string chordName(const Chord&, int transpose);
}
