#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "performance_controls.h"
namespace legio_nam {
enum TelemetryField : unsigned {
  Callbacks, CpuHz, AppLastCycles, AppMaxCycles, AppOverruns,
  DmaLastCycles, DmaMaxCycles, DmaOverruns, DmaCalls,
  HeapHighWater, HeapFailures, HeapLargestFailure, WarmupMaxUs,
  Amp, Ready, Fault, Bypass, DriveDb, Tone, OutputGain, Contour,
  InputLeftPeak, InputRightPeak, ModelInputPeak, ModelOutputPeak,
  OutputUnclampedPeak, InputNearRailFrames, OutputClampedFrames,
  BlockFrames, SampleRate,
#if LEGIO_MIDS_SWELL
  MidPosition, SwellMode, InputEnvelope, SwellGain, SwellArmed,
#if LEGIO_PRESETS
  PresetSlot, PresetSaveState, TonePickup, PresetSaveCount, PresetSaveErrors, WetEnvelope,
#endif
#if LEGIO_CHORUS
  ChorusMode, WetEnvelope,
#endif
#if LEGIO_CHORUS_CLOCK
  ClockLocked, ClockPeriodMs, ClockEdges, ChorusRate, GateHigh,
  PitchAdc, TopAdc, BottomAdc,
  BaseDriveDb, GainCvDb, GainCvZero,
  ChorusDepth, OutputLevelDb,
#endif
#endif
  TelemetryFieldCount
};
using TelemetryValues=std::array<std::uint32_t,TelemetryFieldCount>;
constexpr std::uint32_t kTelemetryMagic=0x3144474c; // LGD1, little endian.
constexpr std::uint32_t kTelemetryVersion=LEGIO_CHORUS_CLOCK?8:LEGIO_CHORUS?4:LEGIO_PRESETS?3:LEGIO_MIDS_SWELL?2:1;
constexpr std::size_t kTelemetryBytes=(TelemetryFieldCount+5)*4;
inline std::uint32_t FloatBits(float value) {
  std::uint32_t bits; static_assert(sizeof(bits)==sizeof(value));
  std::memcpy(&bits,&value,sizeof(bits)); return bits;
}
inline std::uint32_t TelemetryChecksum(const std::uint8_t* data,std::size_t size) {
  std::uint32_t hash=2166136261u;
  for(std::size_t i=0;i<size;++i) hash=(hash^data[i])*16777619u;
  return hash;
}
inline void EncodeTelemetry(std::uint8_t* out,std::uint32_t sequence,
                            const TelemetryValues& values) {
  auto word=[&](std::size_t index,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) out[index*4+i]=std::uint8_t(value>>(8*i));
  };
  word(0,kTelemetryMagic); word(1,kTelemetryVersion);
  word(2,TelemetryFieldCount*4); word(3,sequence);
  for(unsigned i=0;i<TelemetryFieldCount;++i) word(i+4,values[i]);
  word(TelemetryFieldCount+4,TelemetryChecksum(out,kTelemetryBytes-4));
}
// One ISR writer and one main-loop reader. All payload words are atomic too,
// so a rejected snapshot has no C++ data race. Readers skip, never spin.
class TelemetryMailbox {
 public:
  void Publish(const TelemetryValues& values) {
    sequence_.fetch_add(1,std::memory_order_acq_rel);
    for(unsigned i=0;i<TelemetryFieldCount;++i)
      values_[i].store(values[i],std::memory_order_relaxed);
    sequence_.fetch_add(1,std::memory_order_release);
  }
  bool Read(TelemetryValues& values) const {
    const auto before=sequence_.load(std::memory_order_acquire);
    if(!before || (before&1u)) return false;
    for(unsigned i=0;i<TelemetryFieldCount;++i)
      values[i]=values_[i].load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    return before==sequence_.load(std::memory_order_relaxed);
  }
 private:
  static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
  std::atomic<std::uint32_t> sequence_{0};
  std::array<std::atomic<std::uint32_t>,TelemetryFieldCount> values_{};
};
}
