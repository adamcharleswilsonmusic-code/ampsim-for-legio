#pragma once
#include <cstdint>
#ifndef LEGIO_DIAGNOSTICS
#define LEGIO_DIAGNOSTICS 0
#endif
#ifndef LEGIO_PROBE
#define LEGIO_PROBE 0
#endif
#ifndef LEGIO_ENCODER_ONLY
#define LEGIO_ENCODER_ONLY 0
#endif
#if LEGIO_DIAGNOSTICS
extern "C" {
extern volatile std::uint32_t legio_dma_cycles_last,legio_dma_cycles_max,legio_dma_overruns,legio_dma_calls;
}
#endif
