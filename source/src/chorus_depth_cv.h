#pragma once
#include <algorithm>
#include <cmath>
namespace legio_nam {
// Dedicated left CV input, nominal -2..+5 V. Calibrate with the jack unpatched.
// Zero CV preserves the toggle's base sound; positive adds depth, negative reduces it.
class ChorusDepthCv {
 public:
  void CalibrateZero(float raw) {
    zero_=std::isfinite(raw)?std::clamp(raw,0.f,1.f):2.f/7.f;
  }
  float Scale(float raw) const {
    if(!std::isfinite(raw))return 1.f;
    const float delta=std::clamp(raw,0.f,1.f)-zero_;
    if(std::abs(delta)<=.002f)return 1.f;
    // At nominal calibration: -2 V -> 0x, 0 V -> 1x, +5 V -> 2x.
    return std::clamp(1.f+delta*(delta<0?3.5f:1.4f),0.f,2.f);
  }
  float Zero() const {return zero_;}
 private:
  float zero_=2.f/7.f;
};
}
