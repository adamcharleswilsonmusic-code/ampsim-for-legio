#pragma once
#include "a2_lite_factory.h"
#include "audio_meters.h"
#include "audio_config.h"
#include "drive_range.h"
#include "performance_controls.h"
#if LEGIO_CHORUS
#include "stereo_chorus.h"
#endif
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef LEGIO_THREE_MODELS
#define LEGIO_THREE_MODELS 1
#endif
namespace legio_nam {
static_assert(std::atomic<unsigned>::is_always_lock_free &&
              std::atomic<bool>::is_always_lock_free,
              "Model handoff must not acquire a lock in the audio callback");
constexpr unsigned kModelCount = LEGIO_THREE_MODELS ? 3 : 1;
struct Controls {
  float drive_db = 0;       // kMinimumDriveDb .. kMaximumDriveDb before the capture.
  float tone = .5f;         // 0 dark, .5 neutral, 1 bright.
  float output = 0;         // 0..1 final gain, applies to dry bypass too.
  unsigned voicing = 1;     // 0 Tight, 1 Normal, 2 Full.
  unsigned model = 0;
  unsigned mids = 1;        // 0 cut, 1 neutral, 2 boost; optional one-amp layout.
  unsigned swell = 0;       // 0 off, 1 short, 2 long; optional one-amp layout.
  unsigned chorus = 0;      // 0 off, 1 moderate, 2 stronger stereo spread.
  float chorus_depth = 1;   // 0..2 depth scale; zero also fades chorus wet mix out.
  float chorus_rate = 0;    // 0 uses slow mode defaults; override bounded .03..1 Hz.
  bool bypass = true;
};
enum class FaultReason : unsigned { None,NoMemory,ModelLoad,NonFinite,BlockSize,Deadline };
class LiteProcessor {
 public:
  // Init and Service are main-loop only. Audio is stopped for Init.
  bool Init(unsigned initial_model = 0);
  // True if a load was attempted, including a failed attempt. Main can measure
  // the actual call, without racing a separate Detached() snapshot.
  bool Service();
  // Process and controls belong exclusively to the audio callback.
  void Process(const float* left, const float* right, float* out_left,
               float* out_right, std::size_t frames, const Controls& controls);
  bool Ready() const { return state_.load(std::memory_order_acquire) == ReadyState; }
  unsigned ActiveModel() const { return active_.load(std::memory_order_acquire); }
  bool Fault() const { return FaultCode()!=FaultReason::None; }
  FaultReason FaultCode() const { return fault_.load(std::memory_order_acquire); }
  // Does not release, replace or touch model ownership. The next Process skips
  // neural inference and fades to dry. First failure stays latched until restart.
  void LatchFault(FaultReason reason) {
    auto expected=FaultReason::None;
    fault_.compare_exchange_strong(expected,reason,std::memory_order_acq_rel);
  }
  bool Detached() const { return state_.load(std::memory_order_acquire) == DetachedState; }
#if LEGIO_USB_TELEMETRY
  const AudioMeters& Meters() const { return meters_; } // Audio callback only.
#endif
#if LEGIO_MIDS_SWELL
  const AutoSwell& Swell() const {return swell_;} // Audio callback only.
#endif
 private:
  enum State : unsigned { DetachedState, ReadyState, FailedState };
  bool Load(unsigned id);
  std::unique_ptr<nam::DSP> model_;
  std::atomic<unsigned> state_{DetachedState}, active_{0}, requested_{0};
  std::atomic<FaultReason> fault_{FaultReason::None};
  // Callback-owned state. Service never touches these variables.
  float drive_ = 1, level_ = 0, wet_ = 0, hp_low_ = 0;
  float hp_coefficient_ = 0, tone_low_ = 0, tone_low_gain_ = 1;
  float input_[kFrames]{}, neural_[kFrames]{};
#if LEGIO_MIDS_SWELL
  MidEq mids_;
  AutoSwell swell_;
#endif
#if LEGIO_CHORUS
  StereoChorus chorus_;
#endif
#if LEGIO_USB_TELEMETRY
  AudioMeters meters_;
#endif
};
}
