#pragma once
#include "performance.hpp"
namespace ps {
// Low to high strings, matching the order displayed in the Guitar panel.
inline constexpr std::array<Button,stringCount> guitarStringButtons={A,B,X,Y,L1,R1};
inline constexpr Button guitarBankButton=L2,guitarMuteButton=R2;
inline Button guitarStringButton(size_t string,const Settings& s) {
    return mappedButton(guitarStringButtons[string],s);
}
}
