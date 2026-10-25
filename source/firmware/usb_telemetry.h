#pragma once
#include "audio_meters.h"
#if LEGIO_USB_TELEMETRY
#include "telemetry_protocol.h"
namespace legio_nam {
void InitUsbTelemetry();
void PublishUsbTelemetry(const TelemetryValues& values); // Audio callback only.
void ServiceUsbTelemetry(); // Main loop only, no wait for host.
}
#endif
