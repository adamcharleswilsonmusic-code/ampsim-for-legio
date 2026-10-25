#pragma once
#include <cstddef>
#include <cstdint>
#ifndef LEGIO_BLOCK_FRAMES
#define LEGIO_BLOCK_FRAMES 48
#endif
namespace legio_nam {
constexpr std::uint32_t kSampleRate=48000;
constexpr std::size_t kFrames=LEGIO_BLOCK_FRAMES;
static_assert(kFrames==24 || kFrames==48,"Only checked 24/48-frame configurations are supported");
constexpr std::uint32_t AudioCycleBudget(std::uint32_t cpu_hz) {
  return cpu_hz/(kSampleRate/kFrames);
}
// Keep switch/button sampling at 1 kHz as audio callback frequency changes.
class ControlClock {
 public:
  bool Advance(std::size_t frames) {
    pending_+=frames;
    if(pending_<kSampleRate/1000) return false;
    pending_-=kSampleRate/1000;
    return true;
  }
 private:
  std::size_t pending_=0;
};
}
