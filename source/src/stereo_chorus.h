#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
namespace legio_nam {
// One mono delay history with independently modulated left/right taps. No
// feedback or lookahead; dry amp audio remains in both outputs at zero delay.
class StereoChorus {
 public:
  // Reserve once during model setup, using the existing bounded internal heap.
  // Keeping the zero-filled history out of the global processor object avoids
  // storing an 8 KiB zero initializer in scarce internal firmware flash.
  void Init() {if(!history_) history_.reset(new float[2048]{});}
  void Process(float input,unsigned mode,float& left,float& right,
               float depth_scale=1.f,float rate_hz=0.f) {
    if(!std::isfinite(input)) input=0;
    if(!history_) {left=right=input;return;}
    mode=std::min(mode,2u);
    if(!std::isfinite(depth_scale))depth_scale=1;
    depth_scale=std::clamp(depth_scale,0.f,2.f);
    if(!std::isfinite(rate_hz))rate_hz=0;
    // Fuller unpatched sound with left CV at zero (depth_scale=1): medium
    // 45% wet / 4.5 ms excursion, strong 55% / 6 ms. Even at full depth (2x),
    // taps stay between 96 and 1248 samples inside the 2048-sample history.
    constexpr float mix[]={0,.45f,.55f},depth[]={0,216.f,288.f},rate[]={.15f,.15f,.20f};
    const float target_mix=mix[mode]*std::min(depth_scale,1.f);
    const float target_rate=rate_hz>0?std::clamp(rate_hz,.03f,1.f):rate[mode];
    mix_+=.0010411247f*(target_mix-mix_);
    depth_+=.0010411247f*(depth[mode]*depth_scale-depth_);
    rate_+=.0010411247f*(target_rate-rate_);
    if(target_mix==0&&mix_<1e-8f) mix_=0;
    history_[write_]=input;
    if(mix_==0) left=right=input;
    else {
      float other=phase_+.5f;if(other>=1)other-=1; // Opposed motion gives more width.
      const float l=Read(672.f+depth_*Lfo(phase_));
      const float r=Read(672.f+depth_*Lfo(other));
      left=input+mix_*(l-input);right=input+mix_*(r-input);
    }
    write_=(write_+1)&2047u;
    phase_+=rate_/48000.f;if(phase_>=1)phase_-=1;
  }
 private:
  static float Lfo(float phase) {
    // Smooth the triangle's turning points without a per-sample sin() call.
    const float t=phase<.5f?4.f*phase-1.f:3.f-4.f*phase;
    return t*(1.5f-.5f*t*t);
  }
  float Read(float delay) const {
    const auto whole=unsigned(delay);const float fraction=delay-float(whole);
    const auto index=(write_+2048u-whole)&2047u;
    const float a=history_[index],b=history_[(index-1)&2047u];
    return a+fraction*(b-a);
  }
  std::unique_ptr<float[]> history_;
  unsigned write_=0;float phase_=0,mix_=0,depth_=0,rate_=.15f;
};
struct SwellColor {float red,green,blue;};
inline SwellColor SwellLedColor(float wet_envelope,unsigned mode) {
  if(mode==0) return {1,.6f,0};
  if(!std::isfinite(wet_envelope)) wet_envelope=0;
  const float brightness=std::clamp(16.f*wet_envelope,0.f,1.f);
  if(brightness<.004f) return {0,0,0}; // End the dim tail cleanly.
  const float scale=std::cbrt(brightness);
  return {scale,0,0}; // Requested red-only swell fade; no green/yellow transition.
}
}
