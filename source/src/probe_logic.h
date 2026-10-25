#pragma once
#include <cstdint>
#include "audio_config.h"
namespace legio_nam {
// Sampled on the callback's 1 kHz control tick, independent of HAL_GetTick/SysTick. The exact
// wall time may lengthen during an overload; one sustained press still fires once.
class ProbeButton {
 public:
  bool Update(bool held) {
    history_=static_cast<std::uint8_t>((history_<<1)|(held?1:0));
    if(history_==0) down_=false;
    if(history_==0x7f && !down_) { down_=true; return true; }
    return false;
  }
 private:
  std::uint8_t history_=0;
  bool down_=false;
};
inline bool DeadlineExceeded(std::uint32_t cycles,std::uint32_t cpu_hz) {
  return cpu_hz!=0 && cycles>AudioCycleBudget(cpu_hz);
}
inline bool Within70Percent(std::uint32_t cycles,std::uint32_t cpu_hz) {
  return cpu_hz!=0 && cycles<=(7u*AudioCycleBudget(cpu_hz))/10u;
}
}
