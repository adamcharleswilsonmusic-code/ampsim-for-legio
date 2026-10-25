#pragma once
#include <algorithm>
#include <cmath>
#include "drive_range.h"
namespace legio_nam {
// The dedicated pitch input spans nominally -2..+5 V. Capture its idle zero
// after ADC settling; boot/reset with that jack unpatched. No plug detection.
// Nominal +5 V still adds 48 dB; voltage scaling does not change when the manual
// range is extended. From -36 dB, +5 V therefore reaches +12 dB, not the maximum.
class GainCvOffset {
 public:
  void CalibrateZero(float raw) {zero_=std::isfinite(raw)?std::clamp(raw,0.f,1.f):2.f/7.f;}
  float OffsetDb(float raw) const {
    if(!std::isfinite(raw))return 0;
    const float delta=std::clamp(raw,0.f,1.f)-zero_;
    return std::abs(delta)<=.002f?0.f:std::clamp(delta*67.2f,-48.f,48.f);
  }
  float Zero() const {return zero_;}
  float DriveDb(float base,float raw) const {
    if(!std::isfinite(base))base=0;
    return std::clamp(base+OffsetDb(raw),kMinimumDriveDb,kMaximumDriveDb);
  }
 private:
  float zero_=2.f/7.f;
};
}
