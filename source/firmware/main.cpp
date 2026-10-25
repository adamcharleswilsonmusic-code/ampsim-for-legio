#include "daisy_legio.h"
#include "lite_processor.h"
#include "model_data.h"
#include "diagnostics.h"
#include "probe_logic.h"
#include "usb_telemetry.h"
#include <algorithm>
#include <cstdint>
#if LEGIO_CHORUS_CLOCK
#include "chorus_clock.h"
#include "chorus_depth_cv.h"
static_assert(LEGIO_CHORUS,"Chorus clock requires stereo chorus");
#endif
#if LEGIO_PRESETS
#include "preset_controls.h"
#include "preset_flash.h"
static_assert(LEGIO_PROBE&&LEGIO_MIDS_SWELL&&LEGIO_USB_TELEMETRY,
              "Checked preset configuration requires raw encoder input, swell and USB diagnostics");
#endif
#if LEGIO_CHORUS
static_assert(LEGIO_PROBE&&LEGIO_MIDS_SWELL&&!LEGIO_PRESETS,
              "Chorus layout replaces presets and requires checked encoder/swell controls");
#endif

using namespace daisy;
namespace {
DaisyLegio board;
legio_nam::LiteProcessor processor;
legio_nam::Controls controls;
bool latched_bypass = true; // Safe dry startup; final output level still applies.
legio_nam::ControlClock control_clock;
#if LEGIO_CHORUS_CLOCK
legio_nam::ChorusClock chorus_clock;
legio_nam::ChorusDepthCv depth_cv;
float manual_drive_db=0;
#endif
#if LEGIO_PRESETS
legio_nam::PresetControls preset_controls;
legio_nam::PresetButton preset_button;
legio_nam::PresetFlash preset_flash(board.seed.qspi);
legio_nam::PresetStorage<legio_nam::PresetFlash> preset_storage(preset_flash);
legio_nam::PresetBank save_request;
// Release/acquire mailbox: 0 idle, 1 requested, 2 main writing, 3 verified, 4 failed.
std::atomic<unsigned> preset_save_state{0};
std::atomic<unsigned> preset_save_ok{0},preset_save_errors{0};
unsigned save_feedback_ticks=0;bool save_feedback_success=false;
int encoder_pending=0;
#endif
#if LEGIO_MIDS_SWELL
static_assert(legio_nam::kModelCount==1,"Mids/swell toggle layout requires one fixed amp");
#endif
#if LEGIO_PROBE || LEGIO_ENCODER_ONLY
GPIO probe_button;
legio_nam::ProbeButton probe_debounce;
#endif
struct Toggle {
  int candidate=-1, stable=Switch3::POS_CENTER;
  unsigned count=0;
  void Seed(int value) { candidate=stable=value; count=0; }
  int Update(int value) {
    if(value!=candidate) { candidate=value; count=0; }
    if(count<5) ++count;
    if(count>=5) stable=candidate; // ~5 ms at 1 kHz control tick.
    return stable;
  }
} toggles[2];
unsigned Index(int position) {
  // Official Switch3 names/pins. Physical panel orientation must be checked.
  return position==Switch3::POS_UP?0:position==Switch3::POS_DOWN?2:1;
}
void SetControlColor(unsigned led,unsigned index,bool engaged) {
  constexpr float colors[3][3]={{0,.15f,1},{1,.3f,0},{0,1,.1f}};
  board.SetLed(led,engaged?colors[index][0]:0,
                    engaged?colors[index][1]:0,
                    engaged?colors[index][2]:0);
}
}
// Read via debugger or the optional main-loop USB diagnostics. No USB I/O
// or allocation in the audio callback. App cycles include controls, DSP and LEDs,
// excluding libDaisy input/output conversion. The separate diagnostic build also
// measures the encompassing SAI DMA handler bodies.
extern "C" {
volatile std::uint32_t legio_app_cycles_max=0,legio_app_overruns=0;
volatile std::uint32_t legio_app_cycles_last=0;
volatile std::uint32_t legio_app_callback_count=0,legio_cpu_hz=0;
volatile std::uint32_t legio_warmup_us_max=0,legio_active_model=0;
volatile std::uint32_t legio_model_fault=0;
#if LEGIO_USB_TELEMETRY
extern volatile std::uint32_t legio_heap_high_water,legio_heap_failed_count;
extern volatile std::uint32_t legio_heap_largest_failed_request;
#endif
}
void Audio(AudioHandle::InputBuffer in,AudioHandle::OutputBuffer out,size_t frames) {
  const std::uint32_t started=DWT->CYCCNT;
  const bool control_tick=control_clock.Advance(frames);
#if LEGIO_PRESETS
  bool save_pressed=false;
#endif
  // A completed overrun must not keep inference running and starving SysTick or
  // the main loop. Retain model ownership; latch a dry fallback on the next block.
  if(legio_nam::DeadlineExceeded(legio_app_cycles_max,legio_cpu_hz)
#if LEGIO_DIAGNOSTICS
     || legio_nam::DeadlineExceeded(legio_dma_cycles_max,legio_cpu_hz)
#endif
    ) processor.LatchFault(legio_nam::FaultReason::Deadline);
  board.ProcessAllControls();
#if LEGIO_CHORUS_CLOCK
  manual_drive_db=std::clamp(manual_drive_db+float(board.encoder.Increment()),legio_nam::kMinimumDriveDb,legio_nam::kMaximumDriveDb);
#elif LEGIO_PRESETS
  encoder_pending+=board.encoder.Increment();
#else
  controls.drive_db=std::clamp(controls.drive_db+float(board.encoder.Increment()),legio_nam::kMinimumDriveDb,legio_nam::kMaximumDriveDb);
#endif
#if LEGIO_PROBE || LEGIO_ENCODER_ONLY
  const bool button_held=!probe_button.Read(); // Official encoder push D1, active low.
#if LEGIO_PRESETS
  if(control_tick) {
    const auto event=preset_button.Update(button_held);
    if(event==legio_nam::PresetButton::Tap) latched_bypass=!latched_bypass;
    if(save_feedback_ticks) --save_feedback_ticks;
    const auto state=preset_save_state.load(std::memory_order_acquire);
    if(state>=3) {
      save_feedback_success=state==3;save_feedback_ticks=900;
      preset_save_state.store(0,std::memory_order_release);
    }
    save_pressed=event==legio_nam::PresetButton::Save && state==0;
  }
#else
  if(control_tick && probe_debounce.Update(button_held)) latched_bypass=!latched_bypass;
#endif
#else
  if(board.encoder.RisingEdge()) latched_bypass=!latched_bypass;
#endif
#if LEGIO_PROBE
  const bool gate=board.Gate(); // Diagnostic bit only; no audio override.
#endif
  controls.bypass=latched_bypass;
#if !LEGIO_PRESETS
  controls.tone=board.GetKnobValue(DaisyLegio::CONTROL_KNOB_TOP);
#endif
  const float output=board.GetKnobValue(DaisyLegio::CONTROL_KNOB_BOTTOM);
  controls.output=output*output; // Useful quiet end of the final level control.
#if LEGIO_CHORUS_CLOCK
  controls.chorus_depth=depth_cv.Scale(board.GetKnobValue(DaisyLegio::CONTROL_PITCH));
#endif
  if(control_tick) {
#if LEGIO_CHORUS_CLOCK
    chorus_clock.Tick(board.Gate());
    controls.chorus_rate=chorus_clock.RateHz();
    controls.drive_db=manual_drive_db; // Gain remains exclusively on the encoder.
#endif
#if LEGIO_MIDS_SWELL
#if LEGIO_CHORUS
    // Adam's physical test establishes POS_UP contacts are panel down here.
    // Index therefore directly yields physical down=0, centre=1, up=2.
    controls.chorus=Index(toggles[0].Update(board.sw[DaisyLegio::SW_LEFT].Read()));
    controls.swell=Index(toggles[1].Update(board.sw[DaisyLegio::SW_RIGHT].Read()));
    controls.mids=1;
#elif LEGIO_PRESETS
    const unsigned slot=Index(toggles[0].Update(board.sw[DaisyLegio::SW_LEFT].Read()));
    const unsigned swell=2u-Index(toggles[1].Update(board.sw[DaisyLegio::SW_RIGHT].Read()));
    preset_controls.Update(slot,swell,board.GetKnobValue(DaisyLegio::CONTROL_KNOB_TOP),encoder_pending);
    encoder_pending=0;
    const auto& current=preset_controls.Current();
    controls.drive_db=current.drive;controls.tone=current.tone;controls.swell=current.swell;
    controls.mids=1; // Left toggle now selects presets; neutral mids for all slots.
#else
    controls.mids=Index(toggles[0].Update(board.sw[DaisyLegio::SW_LEFT].Read()));
    controls.swell=2u-Index(toggles[1].Update(board.sw[DaisyLegio::SW_RIGHT].Read()));
#endif
    controls.voicing=1; // Neutral/Normal low end; left switch now controls mids.
    controls.model=0;
#else
    controls.voicing=Index(toggles[0].Update(board.sw[DaisyLegio::SW_LEFT].Read()));
    const unsigned selected=Index(toggles[1].Update(board.sw[DaisyLegio::SW_RIGHT].Read()));
    controls.model=legio_nam::kModelCount==3?selected:0;
#endif
  }
#if LEGIO_PRESETS
  if(save_pressed) {
    save_request=preset_controls.Capture();
    preset_save_state.store(1,std::memory_order_release);
  }
#endif
  processor.Process(in[0],in[1],out[0],out[1],frames,controls);
  const bool engaged=!controls.bypass && processor.Ready() && !processor.Fault();
#if LEGIO_MIDS_SWELL
  // Performance layout takes precedence over the probe's raw control bits.
  // CPU and fault counters remain available in the USB diagnostic reports.
  if(processor.Fault()) {
    const bool on=((DWT->CYCCNT/(legio_cpu_hz/2u))&1u)==0;
    for(unsigned led=0;led<DaisyLegio::LED_LAST;++led)
      board.SetLed(led,on?1:0,on?1:0,on?1:0); // Blinking white: dry fault fallback.
  } else if(!processor.Ready()) {
    for(unsigned led=0;led<DaisyLegio::LED_LAST;++led) board.SetLed(led,0,0,1);
#if LEGIO_PRESETS
  } else if(save_feedback_ticks) {
    const bool on=(save_feedback_ticks/150u)&1u;
    for(unsigned led=0;led<DaisyLegio::LED_LAST;++led)
      board.SetLed(led,on?1:0,on?(save_feedback_success?.42f:1.f):0,
                       on&&!save_feedback_success?1:0);
#endif
  } else if(!engaged) {
    for(unsigned led=0;led<DaisyLegio::LED_LAST;++led) board.SetLed(led,.8f,0,1);
  } else {
    // SetLed cubes channels; these green values retain visible amber/orange.
#if LEGIO_PRESETS || LEGIO_CHORUS
    constexpr float green[]={.60f,.42f,0.f}; // Orange, orange-red, red; no yellow.
#else
    constexpr float green[]={.82f,.60f,0.f}; // Earlier mids/swell trial palette.
#endif
#if LEGIO_CHORUS
    board.SetLed(DaisyLegio::LED_LEFT,1,green[std::min(controls.chorus,2u)],0);
    const auto color=legio_nam::SwellLedColor(processor.Swell().OutputEnvelope(),controls.swell);
    board.SetLed(DaisyLegio::LED_RIGHT,color.red,color.green,color.blue);
#elif LEGIO_PRESETS
    board.SetLed(DaisyLegio::LED_LEFT,1,green[preset_controls.Slot()],0);
    const float brightness=16.f*processor.Swell().OutputEnvelope();
    const float scale=legio_nam::SwellLedScale(brightness,controls.swell);
    board.SetLed(DaisyLegio::LED_RIGHT,scale,green[std::min(controls.swell,2u)]*scale,0);
#else
    board.SetLed(DaisyLegio::LED_LEFT,1,green[std::min(controls.mids,2u)],0);
    board.SetLed(DaisyLegio::LED_RIGHT,1,green[std::min(controls.swell,2u)],0);
#endif
  }
#elif LEGIO_PROBE
  // Special probe only: status remains visible even while dry/bypassed/faulted.
  const auto fault=processor.FaultCode();
  if(fault==legio_nam::FaultReason::NoMemory)
    board.SetLed(DaisyLegio::LED_LEFT,1,0,1); // Magenta: allocation failed.
  else if(fault==legio_nam::FaultReason::ModelLoad)
    board.SetLed(DaisyLegio::LED_LEFT,1,1,1); // White: other construction error.
  else if(fault==legio_nam::FaultReason::Deadline) {
    const bool on=((DWT->CYCCNT/(legio_cpu_hz/2u))&1u)==0;
    board.SetLed(DaisyLegio::LED_LEFT,on?1:0,0,0); // Red blink: dry CPU fallback.
  } else if(fault!=legio_nam::FaultReason::None)
    board.SetLed(DaisyLegio::LED_LEFT,1,0,0); // Solid red: other runtime fault.
  else if(!processor.Ready()) board.SetLed(DaisyLegio::LED_LEFT,0,0,1);
  else {
    const bool headroom=legio_nam::Within70Percent(legio_dma_cycles_max,legio_cpu_hz);
    board.SetLed(DaisyLegio::LED_LEFT,headroom?0:1,1,0); // Yellow stays distinct from fault red after cubing.
  }
  // RGB bits make controls readable without SysTick or a debugger:
  // red=raw push held, green=latched bypass, blue=unassigned Gate() reading.
  board.SetLed(DaisyLegio::LED_RIGHT,button_held?1:0,latched_bypass?1:0,gate?1:0);
#elif LEGIO_DIAGNOSTICS
  // Separate benchmark image only: green <=70%, amber >70%, red if any measured
  // SAI DMA handler body exceeded 1 ms. Maxima latch until reboot, even in bypass.
  if(engaged) {
    const bool overrun=legio_nam::DeadlineExceeded(legio_dma_cycles_max,legio_cpu_hz);
    const bool headroom=legio_nam::Within70Percent(legio_dma_cycles_max,legio_cpu_hz);
    board.SetLed(DaisyLegio::LED_LEFT,overrun||!headroom?1:0,overrun?0:headroom?1:.3f,0);
  } else board.SetLed(DaisyLegio::LED_LEFT,0,0,0);
#else
  SetControlColor(DaisyLegio::LED_LEFT,controls.voicing,engaged);
#endif
#if !LEGIO_PROBE && !LEGIO_MIDS_SWELL
  SetControlColor(DaisyLegio::LED_RIGHT,legio_nam::kModelCount==3?processor.ActiveModel():LEGIO_FIXED_MODEL,engaged);
#endif
  board.UpdateLeds();
  legio_active_model=processor.ActiveModel();
  legio_model_fault=processor.Fault();
#if LEGIO_USB_TELEMETRY
  using namespace legio_nam;
  TelemetryValues report{};
  report[Callbacks]=legio_app_callback_count+1;
  report[CpuHz]=legio_cpu_hz;
  // Last/max timing refers to completed work; the current enclosing DMA handler
  // cannot be timed until after this callback returns.
  report[AppLastCycles]=legio_app_cycles_last;
  report[AppMaxCycles]=legio_app_cycles_max;
  report[AppOverruns]=legio_app_overruns;
  report[DmaLastCycles]=legio_dma_cycles_last;
  report[DmaMaxCycles]=legio_dma_cycles_max;
  report[DmaOverruns]=legio_dma_overruns;
  report[DmaCalls]=legio_dma_calls;
  report[HeapHighWater]=legio_heap_high_water;
  report[HeapFailures]=legio_heap_failed_count;
  report[HeapLargestFailure]=legio_heap_largest_failed_request;
  report[WarmupMaxUs]=legio_warmup_us_max;
  report[Amp]=LEGIO_PUBLIC_CAPTURE?kCaptureId:(kModelCount==3?processor.ActiveModel():LEGIO_FIXED_MODEL);
  report[Ready]=processor.Ready(); report[Fault]=unsigned(processor.FaultCode());
  report[Bypass]=latched_bypass; report[DriveDb]=FloatBits(controls.drive_db);
  report[Tone]=FloatBits(controls.tone); report[OutputGain]=FloatBits(controls.output);
  report[Contour]=controls.voicing;
  const auto& meters=processor.Meters();
  report[InputLeftPeak]=FloatBits(meters.input_left);
  report[InputRightPeak]=FloatBits(meters.input_right);
  report[ModelInputPeak]=FloatBits(meters.model_input);
  report[ModelOutputPeak]=FloatBits(meters.model_output);
  report[OutputUnclampedPeak]=FloatBits(meters.output_unclamped);
  report[InputNearRailFrames]=meters.input_near_rail_frames;
  report[OutputClampedFrames]=meters.output_clamped_frames;
  report[BlockFrames]=frames; report[SampleRate]=48000;
#if LEGIO_MIDS_SWELL
  report[MidPosition]=controls.mids; report[SwellMode]=controls.swell;
  report[InputEnvelope]=FloatBits(processor.Swell().Envelope());
  report[SwellGain]=FloatBits(processor.Swell().Gain());
  report[SwellArmed]=processor.Swell().Armed();
#if LEGIO_PRESETS
  report[PresetSlot]=preset_controls.Slot();
  report[PresetSaveState]=preset_save_state.load(std::memory_order_relaxed);
  report[TonePickup]=preset_controls.TonePickedUp();
  report[PresetSaveCount]=preset_save_ok.load(std::memory_order_relaxed);
  report[PresetSaveErrors]=preset_save_errors.load(std::memory_order_relaxed);
  report[WetEnvelope]=FloatBits(processor.Swell().OutputEnvelope());
#endif
#if LEGIO_CHORUS
  report[ChorusMode]=controls.chorus;
  report[WetEnvelope]=FloatBits(processor.Swell().OutputEnvelope());
#endif
#if LEGIO_CHORUS_CLOCK
  report[ClockLocked]=chorus_clock.Locked();report[ClockPeriodMs]=chorus_clock.PeriodMs();
  report[ClockEdges]=chorus_clock.Edges();report[ChorusRate]=FloatBits(controls.chorus_rate);
  report[GateHigh]=board.Gate();
  report[PitchAdc]=FloatBits(board.GetKnobValue(DaisyLegio::CONTROL_PITCH));
  report[TopAdc]=FloatBits(board.GetKnobValue(DaisyLegio::CONTROL_KNOB_TOP));
  report[BottomAdc]=FloatBits(board.GetKnobValue(DaisyLegio::CONTROL_KNOB_BOTTOM));
  report[BaseDriveDb]=FloatBits(manual_drive_db);
  report[GainCvDb]=FloatBits(0.f); // Retired field; left jack now controls depth.
  report[GainCvZero]=FloatBits(depth_cv.Zero()); // v8 reader names this depth_cv_zero.
  report[ChorusDepth]=FloatBits(controls.chorus_depth);
  report[OutputLevelDb]=FloatBits(controls.output>0?20.f*std::log10(controls.output):-120.f);
#endif
#endif
  PublishUsbTelemetry(report);
#endif
  const std::uint32_t cycles=DWT->CYCCNT-started;
  legio_app_cycles_last=cycles;
  if(cycles>legio_app_cycles_max) legio_app_cycles_max=cycles;
  if(legio_nam::DeadlineExceeded(cycles,legio_cpu_hz)) ++legio_app_overruns;
  ++legio_app_callback_count;
}
int main() {
  board.Init(true); // Official supported 480 MHz boost; measure actual clock below.
  board.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  board.SetAudioBlockSize(legio_nam::kFrames);
  legio_cpu_hz=System::GetSysClkFreq();
  CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT=0;
  DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
#if LEGIO_PROBE || LEGIO_ENCODER_ONLY
  probe_button.Init(seed::D1,GPIO::Mode::INPUT,GPIO::Pull::PULLUP);
#endif
  // Read actual switches before constructing any model. Five stable 1 ms
  // samples avoid a synthetic center/Plexi request while up/down settles.
  for(unsigned i=0;i<2;++i) toggles[i].Seed(board.sw[i].Read());
  bool settled=false;
  while(!settled) {
    System::Delay(1);
    settled=true;
    for(unsigned i=0;i<2;++i) {
      toggles[i].Update(board.sw[i].Read());
      settled=settled && toggles[i].count>=5;
    }
  }
  controls.model=legio_nam::kModelCount==3?Index(toggles[1].stable):0;
#if LEGIO_MIDS_SWELL
#if LEGIO_CHORUS
  controls.chorus=Index(toggles[0].stable);
  controls.swell=Index(toggles[1].stable);
  controls.mids=1;
#else
  controls.mids=Index(toggles[0].stable);
  controls.swell=2u-Index(toggles[1].stable);
#endif
  controls.voicing=1;
#endif
  const std::uint32_t start=System::GetUs();
  processor.Init(controls.model);
  legio_warmup_us_max=System::GetUs()-start;
#if LEGIO_USB_TELEMETRY
  legio_nam::InitUsbTelemetry();
#endif
  board.StartAdc();
#if LEGIO_PRESETS || LEGIO_CHORUS_CLOCK
  // Let ADC and knob smoothing settle before tone pickup is initialized.
  for(unsigned i=0;i<64;++i) {System::Delay(1);board.ProcessAllControls();}
#if LEGIO_CHORUS_CLOCK
  depth_cv.CalibrateZero(board.GetKnobValue(DaisyLegio::CONTROL_PITCH));
#endif
#if LEGIO_PRESETS
  legio_nam::PresetBank bank;
  preset_storage.Load(bank); // Read only; empty storage remains untouched.
  preset_controls.Init(bank,Index(toggles[0].stable),2u-Index(toggles[1].stable),
                       board.GetKnobValue(DaisyLegio::CONTROL_KNOB_TOP));
#endif
#endif
  board.StartAudio(Audio);
  while(true) {
    const std::uint32_t before=System::GetUs();
    const bool loading=processor.Service();
    if(loading) {
      const std::uint32_t elapsed=System::GetUs()-before;
      if(elapsed>legio_warmup_us_max) legio_warmup_us_max=elapsed;
    }
#if LEGIO_USB_TELEMETRY
    legio_nam::ServiceUsbTelemetry();
#endif
#if LEGIO_PRESETS
    if(preset_save_state.load(std::memory_order_acquire)==1) {
      preset_save_state.store(2,std::memory_order_release);
      const bool saved=preset_storage.Save(save_request);
      if(saved) preset_save_ok.fetch_add(1,std::memory_order_relaxed);
      else preset_save_errors.fetch_add(1,std::memory_order_relaxed);
      preset_save_state.store(saved?3:4,std::memory_order_release);
    }
#endif
    System::Delay(1);
  }
}
