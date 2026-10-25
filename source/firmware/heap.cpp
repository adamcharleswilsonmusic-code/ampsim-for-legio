// Bounded internal AXI SRAM heap. Never depend on a particular SDRAM variant.
#include <cerrno>
#include <cstddef>
#include <cstdint>
namespace {
alignas(8) std::uint8_t arena[224*1024];
std::ptrdiff_t used=0;
}
extern "C" {
volatile std::uint32_t legio_heap_high_water=0;
volatile std::uint32_t legio_heap_failed_count=0,legio_heap_largest_failed_request=0;
void* _sbrk(std::ptrdiff_t increment) {
  if(increment>static_cast<std::ptrdiff_t>(sizeof(arena))-used || increment< -used) {
    ++legio_heap_failed_count;
    if(increment>0 && static_cast<std::uint32_t>(increment)>legio_heap_largest_failed_request)
      legio_heap_largest_failed_request=static_cast<std::uint32_t>(increment);
    errno=ENOMEM;
    return reinterpret_cast<void*>(-1);
  }
  void* old=arena+used;
  used+=increment;
  if(static_cast<std::uint32_t>(used)>legio_heap_high_water)
    legio_heap_high_water=static_cast<std::uint32_t>(used);
  return old;
}
}
