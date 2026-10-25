#pragma once
#include <memory>
#include "NAM/dsp.h"
namespace legio_nam {
// Fixed topology verified on the host by tools/prepare_capture.py. No JSON parser or
// Full model is needed on the module. The inference implementation is upstream.
std::unique_ptr<nam::DSP> CreateLite(const float* weights, std::size_t count);
}
