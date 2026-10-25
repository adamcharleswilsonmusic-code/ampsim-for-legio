#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#ifndef LEGIO_MIDS_SWELL
#define LEGIO_MIDS_SWELL 0
#endif
#ifndef LEGIO_PRESETS
#define LEGIO_PRESETS 0
#endif
#ifndef LEGIO_CHORUS
#define LEGIO_CHORUS 0
#endif
#ifndef LEGIO_CHORUS_CLOCK
#define LEGIO_CHORUS_CLOCK 0
#endif
namespace legio_nam {
// Fixed 750 Hz/Q=.8 bandpass in a parallel EQ; centre position is unity.
// Preset mix changes are smoothed, keeping all poles fixed and stable.
class MidEq {
 public:
  float Process(float x,unsigned position) {
    constexpr float amounts[]={-.3690426555f,0.f,.5848931925f}; // -4/0/+4 dB at centre.
    amount_+=.004158f*(amounts[std::min(position,2u)]-amount_);
    const float band=.0577244705f*x+z1_;
    z1_=1.875476431f*band+z2_;
    z2_=-.0577244705f*x-.884551059f*band;
    return x+amount_*band;
  }
 private:
  float z1_=0,z2_=0,amount_=0;
};
class AutoSwell {
 public:
  // Modes: 0 off; short/long are 0.9/1.8 s in chorus/preset builds and
  // 0.18/0.9 s in the original mids/swell build. Detector uses raw input before
  // drive; gain acts on wet audio, never on bypass/fault fallback.
  float Process(float input,unsigned mode) {
    if(mode>2) mode=0;
    const float x=std::isfinite(input)?std::abs(input):0.f;
    const float coefficient=x>envelope_?.0206178187f:.0010411247f; // 1/20 ms.
    envelope_+=coefficient*(x-envelope_);
    if(envelope_<1e-12f) envelope_=0; // No denormal tail during long silence.
    // Entering the effect mid-note leaves that note open; the next quiet gap
    // rearms it. This avoids unexpectedly chopping an already ringing chord.
    if(previous_mode_==0 && mode && !armed_) phase_=1;
    previous_mode_=mode;
    // Original detector sensitivity restored at Adam's request. Hysteresis and
    // the quiet gap prevent chatter; no extra sustain/hold behavior is added.
    if(envelope_<=.0025f) {
      if(quiet_samples_<1920) ++quiet_samples_; // 40 ms below release threshold.
      if(quiet_samples_==1920) {armed_=true;phase_=0;}
    } else quiet_samples_=0;
    if(armed_ && envelope_>=.008f) {armed_=false;phase_=0;}
    if(!armed_) {
      const float step=(LEGIO_PRESETS||LEGIO_CHORUS) ? (mode==2?1.f/86400.f:1.f/43200.f)
                                    : (mode==2?1.f/43200.f:1.f/8640.f);
      phase_=std::min(1.f,phase_+step);
    }
    const float curve=phase_*phase_*(3.f-2.f*phase_);
    const float target=mode==0?1.f:curve;
    // Bound mode/rearm changes to a 5 ms fade; the slower swell curve passes
    // unchanged. Once fully open, a held note decays naturally with the input.
    gain_+=std::clamp(target-gain_,-1.f/240.f,1.f/240.f);
    return gain_;
  }
  float Envelope() const {return envelope_;}
  float Gain() const {return gain_;}
  bool Armed() const {return armed_;}
#if LEGIO_PRESETS || LEGIO_CHORUS
  void TrackOutput(float wet) {
    const float x=std::isfinite(wet)?std::abs(wet):0.f;
    output_envelope_+=(x>output_envelope_?.0206178187f:.0002603828f)*(x-output_envelope_);
    if(output_envelope_<1e-12f) output_envelope_=0;
  }
  float OutputEnvelope() const {return output_envelope_;}
#endif
 private:
  float envelope_=0,phase_=0,gain_=1;
  std::uint32_t quiet_samples_=1920;
  unsigned previous_mode_=0;
  bool armed_=true;
#if LEGIO_PRESETS || LEGIO_CHORUS
  float output_envelope_=0;
#endif
};
}
