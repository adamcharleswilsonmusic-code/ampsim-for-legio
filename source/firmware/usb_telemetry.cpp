#include "usb_telemetry.h"
#if LEGIO_USB_TELEMETRY
#include "daisy_seed.h"
extern "C" {
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
}
static_assert(!LEGIO_THREE_MODELS,
              "USB diagnostics require one fixed amp: capacity and heap ownership");
namespace legio_nam {
namespace {
TelemetryMailbox mailbox;
daisy::UsbHandle usb;
std::uint8_t transmit[kTelemetryBytes]; // Persistent until transfer completes.
std::uint32_t sequence=0,last_tick=0;
}
void InitUsbTelemetry() {
  // Called after model allocation/prewarm and before audio starts. With one amp
  // no main-loop model replacement allocates concurrently with USB enumeration.
  usb.Init(daisy::UsbHandle::FS_INTERNAL);
  // Audio DMA remains priority 0. USB must not block a new audio deadline.
  NVIC_SetPriority(OTG_FS_IRQn,5);
  NVIC_SetPriority(OTG_FS_EP1_OUT_IRQn,5);
  NVIC_SetPriority(OTG_FS_EP1_IN_IRQn,5);
}
void PublishUsbTelemetry(const TelemetryValues& values) { mailbox.Publish(values); }
void ServiceUsbTelemetry() {
  // DWT wraps after ~9 s at 480 MHz; unsigned subtraction handles that. Avoid
  // dependence on SysTick and never wait for a cable, terminal or busy endpoint.
  const std::uint32_t now=DWT->CYCCNT;
  const std::uint32_t interval=daisy::System::GetSysClkFreq()/5u;
  if(std::uint32_t(now-last_tick)<interval) return;
  last_tick=now;
  TelemetryValues values;
  if(!mailbox.Read(values)) return;
  std::uint8_t next[kTelemetryBytes];
  EncodeTelemetry(next,sequence,values);
  // Guard class teardown and endpoint state against the USB ISR. Audio remains
  // enabled and may preempt this short copy/transmit. Do not modify an in-flight
  // buffer; a slow/disconnected host simply loses a report.
  NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_DisableIRQ(OTG_FS_EP1_OUT_IRQn);
  NVIC_DisableIRQ(OTG_FS_EP1_IN_IRQn);
  __DSB(); __ISB();
  auto* cdc=static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceFS.pClassData);
  if(hUsbDeviceFS.dev_state==USBD_STATE_CONFIGURED && cdc && !cdc->TxState) {
    std::memcpy(transmit,next,kTelemetryBytes);
    if(usb.TransmitInternal(transmit,kTelemetryBytes)==daisy::UsbHandle::Result::OK)
      ++sequence;
  }
  NVIC_EnableIRQ(OTG_FS_EP1_IN_IRQn);
  NVIC_EnableIRQ(OTG_FS_EP1_OUT_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);
}
}
#endif
