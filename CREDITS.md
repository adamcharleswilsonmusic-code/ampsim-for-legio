# Credits and permissions

AmpSim for Legio builds on the work of the people and projects below. This page
is a readable overview. The actual license texts in [licenses/](licenses/) and
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) contain the applicable terms.

## Neural Amp Modeler

**Steven Atkinson** created Neural Amp Modeler. This firmware uses
[NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore), which
runs trained models to reproduce the response of captured audio gear.

- Copyright © 2023 Steven Atkinson; [included MIT license](licenses/NAM-Core-MIT.txt).
- Pinned source: [`0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`](https://github.com/sdatkinson/NeuralAmpModelerCore/tree/0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842).
- The build generates two platform adaptations: replacing a thread-local prewarm
  default with a main-thread variable, and shortening a trailing-weight error
  message. The source/build tools disclose these changes.
- Keep the copyright notice and full MIT license with redistributed copies and
  derivatives.

## Included capture: RiffSmith Clean

**Matt Hartman** made the RiffSmith Clean capture included in the firmware.

- [RiffSmith project](https://github.com/hartmamt/riffsmith).
- [Exact upstream capture](https://github.com/hartmamt/riffsmith/blob/228b801ccc8b518d0a1fb53a8e63355a2fbd153f/public/nam/models/riffsmith-clean.nam).
- [Pinned upstream license](https://github.com/hartmamt/riffsmith/blob/228b801ccc8b518d0a1fb53a8e63355a2fbd153f/LICENSE)
  and [README describing the capture grant](https://github.com/hartmamt/riffsmith/blob/228b801ccc8b518d0a1fb53a8e63355a2fbd153f/README.md).
- Copyright © 2026 Matt Hartman; [included MIT license](licenses/RiffSmith-captures-MIT.txt).

The upstream author explicitly includes the bundled captures in the MIT grant,
including their weights. This package extracts the compatible A2-Lite submodel
without changing its configuration or weights. The original capture, extracted
model and [provenance record](approved-model/UPSTREAM-PROVENANCE.json) are included
in [approved-model/](approved-model/).

The exact amplifier make/model is not documented by the available upstream
information. “RiffSmith Clean” is the supported credit; it should not be renamed
a Marshall capture.

The MIT permission includes redistribution, subject to keeping Matt Hartman's
copyright notice and full license text. A name credit or video link alone does
not replace those included license files. The permission covers this capture;
it does not grant rights to other captures you may download.

## Legio and Daisy hardware support

**Noise Engineering** makes the [Legio platform](https://noiseengineering.us/).
This project uses Legio board support through **Electrosmith's**
[libDaisy](https://github.com/electro-smith/libDaisy).

- libDaisy: copyright © 2019 Electrosmith;
  [included MIT license](licenses/libDaisy-MIT.txt).
- Pinned libDaisy source:
  [`f715e4875b2d28445d0c11a0a17f064a248f69d0`](https://github.com/electro-smith/libDaisy/tree/f715e4875b2d28445d0c11a0a17f064a248f69d0).
- The Legio board-support header credits **Eris Fairbanks / Olivia Artz Modular
  + Noise Engineering**. Those upstream credits and notices are retained.

The module's hardware and firmware ecosystem made this experiment possible.
Manufacturer printable panel artwork is not part of this firmware release; the
included guide uses a custom reference drawing.

## TONE3000

[TONE3000](https://www.tone3000.com/) provides NAM-related tools and a capture
platform. Its [nam-pedal project](https://github.com/tone-3000/nam-pedal) was a
reference during development; the applicable
[TONE3000 MIT notice](licenses/TONE3000-reference-MIT.txt) is retained.

Legio runs the NAM engine on the module. It does not run the desktop TONE3000
plug-in. The earlier nam-pedal binary-loader and generic-engine experiments are
not linked into this firmware.

## Supporting dependencies

| Component | Included terms / material |
| --- | --- |
| Eigen | [MPL-2.0](licenses/Eigen-MPL-2.0.txt), with file-specific [BSD](licenses/Eigen-BSD.txt), [MINPACK](licenses/Eigen-MINPACK.txt) and [Apache](licenses/Eigen-Apache.txt) notices; exact covered source is included under [source/vendor/](source/vendor/) |
| STM32 HAL/startup | [BSD notices](licenses/STM32-HAL-BSD.txt) |
| STM32 device headers, CMSIS, CMSIS-DSP | [STM32 device](licenses/STM32-device-Apache-2.0.txt), [CMSIS](licenses/CMSIS-Apache-2.0.txt) and [CMSIS-DSP](licenses/CMSIS-DSP-Apache-2.0.txt) Apache notices/licenses |
| ST USB Device Library | [ST SLA0044](licenses/STM32-USB-SLA0044.txt), including its ST-microcontroller execution restriction |
| nlohmann JSON — Niels Lohmann | [MIT license](licenses/nlohmann-json-MIT.txt) |
| GCC target runtime | [GPL text](licenses/GCC-GPL-3.0.txt) and [GCC Runtime Library Exception 3.1](licenses/GCC-runtime-exception-3.1.txt); the compiler itself is not included |
| newlib / libm / libnosys | [Upstream notice collection](licenses/newlib-notices.txt) |
| Liberation fonts in the guide | [Font license](licenses/Liberation-font-license.txt) |

The build uses `EIGEN_MPL2_ONLY`. No GPL-only Eigen modules are enabled. Retaining
the exact covered source and all applicable notices remains part of the release
packaging; static linking is not a reason to drop them.

## Original project contributions

The firmware controls, stereo chorus, auto-swell, metering, model-preparation
helper and application glue are local project contributions, developed with
AI-assisted coding. The source/license review found no imported commercial
chorus implementation in the reviewed build inputs.

[LICENSE.original-contributions.txt](LICENSE.original-contributions.txt) applies
to those contributions. It does not relicense the capture, adapted NAM code,
board libraries, fonts or other third-party material. Standard DSP techniques
described in the source are not an exclusive ownership claim.

## When sharing this firmware

Keep the complete package together: firmware, notices, full license texts and
the included covered source. If you change the capture or dependencies, review
the replacement's terms and update the credits/notices before sharing the result.
A free download still requires redistribution rights for embedded model weights.

This review documents the permissions and obligations identified for this exact
package; it is not an unlimited legal guarantee for other files or future changes.

AmpSim for Legio is independent firmware. It is not affiliated with or endorsed
by Noise Engineering, Electrosmith, TONE3000, Steven Atkinson or other NAM authors.
Names and links identify compatibility, provenance and credit.
