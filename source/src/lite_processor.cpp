#include "lite_processor.h"
#include "model_data.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <new>

namespace legio_nam {
namespace {
constexpr float kSmooth = .004158f; // ~5 ms exponential controls.
constexpr float kWetStep = 1.0f / 240.0f; // Exact 5 ms linear bypass/switch fade.
constexpr float kToneSplit = .09942313f; // 1-exp(-2*pi*800/48000).
float Clean(float value) { return std::isfinite(value) ? value : 0; }
float Clamp(float value, float lo, float hi) { return std::clamp(Clean(value), lo, hi); }
float Towards(float value, float target, float step) {
  return value < target ? std::min(value + step, target) : std::max(value - step, target);
}
}
bool LiteProcessor::Load(unsigned id) {
  // Caller must own DetachedState; the ISR uses only its dry path in that state.
  model_.reset();
  try {
#if LEGIO_CHORUS
    chorus_.Init(); // One allocation before audio, included in failure handling.
#endif
    auto candidate = CreateLite(Weights(id), kLiteWeights);
    candidate->ResetAndPrewarm(48000.0, kFrames);
    model_ = std::move(candidate);
    active_.store(id, std::memory_order_relaxed);
    state_.store(ReadyState, std::memory_order_release);
    return true;
  } catch (const std::bad_alloc&) {
    LatchFault(FaultReason::NoMemory);
    state_.store(FailedState,std::memory_order_release);
    return false;
  } catch (const std::exception&) {
    LatchFault(FaultReason::ModelLoad);
    state_.store(FailedState, std::memory_order_release);
    return false;
  }
}
bool LiteProcessor::Init(unsigned initial_model) {
  initial_model=std::min(initial_model,kModelCount-1);
  requested_.store(initial_model,std::memory_order_relaxed);
  return Load(initial_model);
}
bool LiteProcessor::Service() {
  if(Fault()) return false;
  if (state_.load(std::memory_order_acquire) != DetachedState) return false;
  Load(requested_.load(std::memory_order_acquire));
  return true;
}
void LiteProcessor::Process(const float* left, const float* right, float* out_left,
                            float* out_right, std::size_t frames, const Controls& c) {
  if (frames > kFrames) {
    LatchFault(FaultReason::BlockSize);
    for (std::size_t i=0; i<frames; ++i) out_left[i]=out_right[i]=0;
    return;
  }
  const unsigned requested = std::min(c.model, kModelCount-1);
  requested_.store(requested, std::memory_order_release);
  const bool attached = state_.load(std::memory_order_acquire) == ReadyState;
  const bool switching = requested != active_.load(std::memory_order_relaxed);
  const float target_wet = attached && !switching && !c.bypass && !Fault() ? 1 : 0;
  const float target_drive = std::pow(10.f, Clamp(c.drive_db,kMinimumDriveDb,kMaximumDriveDb)/20.f);
  const float target_level = Clamp(c.output,0,1);
  const float target_tone = 2*Clamp(c.tone,0,1)-1;
  const float target_low_gain = std::pow(10.f,-.3f*target_tone);
  // Full is original low end, Normal 45 Hz, Tight 110 Hz. Provisional by ear.
  constexpr float hp[] = {.01429649f, .00587318f, 0};
  const float target_hp = hp[std::min(c.voicing,2u)];
  for (std::size_t i=0; i<frames; ++i) {
    drive_ += kSmooth*(target_drive-drive_);
    hp_coefficient_ += kSmooth*(target_hp-hp_coefficient_);
    // Keep a valid lowpass history in Full mode, fade its subtraction to zero.
    const float x=Clean(left[i]);
    const float coefficient=std::max(hp_coefficient_, .00587318f);
    hp_low_ += coefficient*(x-hp_low_);
    const float lowcut_mix=std::min(hp_coefficient_/.00587318f,1.f);
    input_[i]=(x-lowcut_mix*hp_low_)*drive_;
#if LEGIO_USB_TELEMETRY
    meters_.Input(left[i],right[i],input_[i]);
#endif
  }
  if (attached && !Fault()) {
    float* in=input_; float* out=neural_;
    model_->process(&in,&out,static_cast<int>(frames));
    for (std::size_t i=0; i<frames; ++i)
      if (!std::isfinite(neural_[i])) { LatchFault(FaultReason::NonFinite); break; }
  }
  const bool fault=Fault();
  for (std::size_t i=0; i<frames; ++i) {
    tone_low_gain_ += kSmooth*(target_low_gain-tone_low_gain_);
    level_ += kSmooth*(target_level-level_);
    wet_=Towards(wet_,fault?0:target_wet,kWetStep);
    const float n=attached&&!fault ? neural_[i] : 0;
    tone_low_ += kToneSplit*(n-tone_low_);
    // Complementary one-pole split, opposite shelves +/-6 dB at the extremes.
    const float low_gain=tone_low_gain_;
    const float high_gain=1.f/low_gain;
    float voiced=low_gain*tone_low_+high_gain*(n-tone_low_);
#if LEGIO_CHORUS
    float wet_l=voiced,wet_r=voiced;
    if(!fault) chorus_.Process(voiced,c.chorus,wet_l,wet_r,c.chorus_depth,c.chorus_rate);
    const float swell_gain=swell_.Process(Clean(left[i]),c.swell);
    wet_l*=swell_gain;wet_r*=swell_gain;
    swell_.TrackOutput(.5f*(std::abs(wet_l)+std::abs(wet_r)));
#else
#if LEGIO_MIDS_SWELL
    voiced=mids_.Process(voiced,c.mids)*swell_.Process(Clean(left[i]),c.swell);
#if LEGIO_PRESETS
    swell_.TrackOutput(voiced); // Actual post-swell wet level, before final volume.
#endif
#endif
    const float wet_l=voiced,wet_r=voiced;
#endif
    const float l=level_*((1-wet_)*Clean(left[i])+wet_*wet_l);
    const float r=level_*((1-wet_)*Clean(right[i])+wet_*wet_r);
#if LEGIO_USB_TELEMETRY
    meters_.Output(n,l,r);
#endif
    out_left[i]=Clamp(l,-1,1);
    out_right[i]=Clamp(r,-1,1);
  }
  // Publish only after the final model access in this callback. Main may now
  // delete/recreate/prewarm it; future callbacks remain dry until ReadyState.
  if (attached && switching && wet_==0 && !Fault())
    state_.store(DetachedState,std::memory_order_release);
}
}
