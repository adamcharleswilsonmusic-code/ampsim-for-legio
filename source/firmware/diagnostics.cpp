#include "diagnostics.h"
#include "probe_logic.h"
#if LEGIO_DIAGNOSTICS
#include "stm32h7xx_hal.h"
extern "C" {
extern volatile std::uint32_t legio_cpu_hz;
volatile std::uint32_t legio_dma_cycles_max=0,legio_dma_overruns=0,legio_dma_calls=0;
volatile std::uint32_t legio_dma_cycles_last=0;
void __real_HAL_DMA_IRQHandler(DMA_HandleTypeDef* handle);
// Linker --wrap redirects the existing, unmodified libDaisy calls here. SAI1
// A/B use DMA1 streams 0/1 in this pinned board library. Include both since the
// codec configuration selects which block receives and invokes audio processing.
void __wrap_HAL_DMA_IRQHandler(DMA_HandleTypeDef* handle) {
  if(handle->Instance!=DMA1_Stream0 && handle->Instance!=DMA1_Stream1) {
    __real_HAL_DMA_IRQHandler(handle);
    return;
  }
  const std::uint32_t start=DWT->CYCCNT;
  __real_HAL_DMA_IRQHandler(handle);
  const std::uint32_t cycles=DWT->CYCCNT-start;
  legio_dma_cycles_last=cycles;
  if(cycles>legio_dma_cycles_max) legio_dma_cycles_max=cycles;
  if(legio_nam::DeadlineExceeded(cycles,legio_cpu_hz)) ++legio_dma_overruns;
  ++legio_dma_calls;
}
}
#endif
