#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace legio_nam {
struct Preset { float drive=0,tone=.5f; std::uint32_t swell=0; };
struct PresetBank { std::uint32_t valid=0; Preset slots[3]{}; };
inline bool ValidPreset(const Preset& p) {
  return std::isfinite(p.drive)&&p.drive>=-24&&p.drive<=24
      &&std::isfinite(p.tone)&&p.tone>=0&&p.tone<=1&&p.swell<=2;
}
// Control-tick input at 1 kHz. A short press fires on release, never on press;
// a held save fires once and suppresses the subsequent bypass action.
class PresetButton {
 public:
  enum Event {None,Tap,Save};
  Event Update(bool held) {
    history_=std::uint8_t((history_<<1)|(held?1:0));
    if(history_==0x7f&&!down_) {down_=true;elapsed_=0;saved_=false;}
    if(down_&&history_==0) {down_=false;return saved_?None:Tap;}
    if(down_&&elapsed_<5000&&++elapsed_==5000) {saved_=true;return Save;}
    return None;
  }
 private:
  std::uint8_t history_=0; unsigned elapsed_=0; bool down_=false,saved_=false;
};
class PresetControls {
 public:
  void Init(const PresetBank& bank,unsigned slot,unsigned physical_swell,float knob) {
    bank_=bank;slot_=std::min(slot,2u);last_swell_=std::min(physical_swell,2u);
    current_={0,knob,last_swell_};Recall(slot_,knob);
  }
  void Update(unsigned slot,unsigned physical_swell,float knob,int increment) {
    slot=std::min(slot,2u);physical_swell=std::min(physical_swell,2u);
    if(slot!=slot_) Recall(slot,knob);
    if(physical_swell!=last_swell_) current_.swell=physical_swell;
    last_swell_=physical_swell;
    if(!pickup_ && (std::abs(knob-current_.tone)<.025f
       || (last_knob_<=current_.tone&&knob>=current_.tone)
       || (last_knob_>=current_.tone&&knob<=current_.tone))) pickup_=true;
    if(pickup_) current_.tone=std::clamp(knob,0.f,1.f);
    last_knob_=knob;
    current_.drive=std::clamp(current_.drive+float(increment),-24.f,24.f);
  }
  PresetBank Capture() {
    bank_.slots[slot_]=current_;bank_.valid|=1u<<slot_;return bank_;
  }
  const Preset& Current() const {return current_;}
  unsigned Slot() const {return slot_;}
  bool TonePickedUp() const {return pickup_;}
 private:
  void Recall(unsigned slot,float knob) {
    slot_=slot;
    if(bank_.valid&(1u<<slot)) current_=bank_.slots[slot];
    // Empty slots retain the current sound until explicitly saved.
    pickup_=std::abs(knob-current_.tone)<.025f;last_knob_=knob;
  }
  PresetBank bank_{};Preset current_{};
  unsigned slot_=0,last_swell_=0;float last_knob_=.5f;bool pickup_=true;
};
// RGB LED driver cubes channels. Cube-root compensation makes perceived PWM
// brightness follow the actual wet gain rather than its cube.
inline float SwellLedScale(float gain,unsigned mode) {
  return mode?std::cbrt(std::clamp(gain,0.f,1.f)):1.f;
}
}
