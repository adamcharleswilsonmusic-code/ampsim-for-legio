// Keep upstream inference intact, expose its internal fixed Lite specialization.
// Upstream MIT license: host-research/NeuralAmpModelerCore-current/LICENSE.
#include "a2_lite_factory.h"
#include "../firmware/generated/a2_fast_embedded.cpp"
namespace nam::wavenet::a2_fast {
std::unique_ptr<DSP> CreateEmbeddedLite(const float* weights, std::size_t count) {
  if (count != 1871) throw std::runtime_error("Expected exactly 1871 Lite weights");
  return std::make_unique<A2FastModel<3>>(
      std::vector<float>(weights, weights + count), 48000.0);
}
}
namespace legio_nam {
std::unique_ptr<nam::DSP> CreateLite(const float* weights, std::size_t count) {
  return nam::wavenet::a2_fast::CreateEmbeddedLite(weights, count);
}
}
