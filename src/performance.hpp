#pragma once
#include "music.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
namespace ps {
enum Button { A, B, X, Y, L1, R1, L2, R2, Up, Down, Left, Right, Start, Select, ButtonCount };
inline constexpr std::array<Button,8> instrumentButtons={A,B,X,Y,L1,L2,R1,R2};
inline constexpr std::array<const char*,ButtonCount> buttonNames={
    "A","B","X","Y","L1","R1","L2","R2","UP","DOWN","LEFT","RIGHT","START","SELECT"};
inline constexpr Button expressionGateButton=L1;
inline constexpr float stickNeutral=.22f;
inline Button mappedButton(Button button,const Settings& s) {
    constexpr std::array<Button,4> swapped={B,A,Y,X};
    return s.swapFaceButtons&&button<=Y?swapped[size_t(button)]:button;
}
inline Button instrumentButton(size_t degree,const Settings& s) { return mappedButton(instrumentButtons[degree],s); }
enum class LoopCommand { Play, Record, Undo, Clear, Stop };
struct Input {
    float lx=0, ly=0, rx=0, ry=0;
    bool ready=true;
    std::array<bool, ButtonCount> held{};
};
// Event identities are persisted in performance files.
enum class EventType { Pluck=0, NoteOn=2, NoteOff=3, Expression=4, Silence=5, Drum=6, Loop=7, LayerStop=8 };
struct Event {
    EventType type=EventType::Silence;
    int id=0;
    float note=60, velocity=1, timbre=.5f, vibrato=0, bend=0;
    uint32_t delay=0;
    SoundId sound=0;
};
struct Events {
    std::array<Event, 64> data{};
    size_t count=0;
    void add(Event e) { if(count<data.size()) data[count++]=e; }
};
struct StringStrike {
    int note=60;
    float strength=0;
    double startsAt=-1;
};
class Controller {
public:
    Events update(const Input&, Settings&, double seconds);
    Events reset();
    Settings performanceSettings(const Settings&) const;
    const std::array<StringStrike,stringCount>& stringStrikes() const { return stringStrikes_; }
    double seconds() const { return lastTime_; }
    float previewPitch() const { return previewPitch_; }
    int chord() const { return chord_; }
    int wheel() const { return wheel_; }
    int stroke() const { return stroke_; }
    float strength() const { return strength_; }
    bool menu() const { return menu_; }
    bool quit() const { return quit_; }
    bool performing(Button button,const Input& input) const { return input.held[button]&&!metaUsed_[button]; }
    int menuRow() const { return menuRow_; }
    float clearProgress() const;
private:
    std::array<StringStrike,stringCount> stringStrikes_{};
    Input previous_{};
    bool armed_=true, menu_=false, quit_=false, expressionSounding_=false, chordStickActive_=false, hasMode_=false;
    int chord_=0, wheel_=-1, stroke_=0, menuRow_=0, temporaryOctave_=0, temporarySemitone_=0, expressionTranspose_=0;
    Mode observedMode_=Mode::Guitar;
    float strength_=0,previewPitch_=60;
    double lastTime_=0;
    std::array<bool,instrumentButtons.size()> notes_{};
    std::array<bool,ButtonCount> metaUsed_{};
    double tempoRepeat_=0,clearSince_=-1;
    bool clearSent_=false;
    Input loopControls(const Input&,Settings&,double,Events&);
    void edit(Settings&, int direction);
    void playGuitar(const Input&,const Input&,Settings&,const Settings&,double,float,Events&);
    bool metaUpUsed_=false, metaDownUsed_=false;
    void updatePitchControls(const Input&, Settings&);
    void readPitchModifiers(const Input&);
};
}
