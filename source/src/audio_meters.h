#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#ifndef LEGIO_USB_TELEMETRY
#define LEGIO_USB_TELEMETRY 0
#endif
namespace legio_nam {
// Callback-owned. Peaks and event counts latch since boot; no allocation or I/O.
// A sample near a converter rail is a warning, not proof of analog clipping.
struct AudioMeters {
  float input_left=0,input_right=0,model_input=0,model_output=0,output_unclamped=0;
  std::uint32_t input_near_rail_frames=0,output_clamped_frames=0;
  static void Peak(float& maximum,float sample) {
    if(std::isfinite(sample)) maximum=std::max(maximum,std::abs(sample));
  }
  void Input(float left,float right,float driven) {
    Peak(input_left,left); Peak(input_right,right); Peak(model_input,driven);
    if((std::isfinite(left)&&std::abs(left)>=.999f) ||
       (std::isfinite(right)&&std::abs(right)>=.999f)) ++input_near_rail_frames;
  }
  void Output(float neural,float left,float right) {
    Peak(model_output,neural); Peak(output_unclamped,left); Peak(output_unclamped,right);
    if((std::isfinite(left)&&std::abs(left)>1.f) ||
       (std::isfinite(right)&&std::abs(right)>1.f)) ++output_clamped_frames;
  }
};
}
