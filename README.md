# AmpSim for Legio

Independent amp-simulation firmware for the **Noise Engineering Legio** platform,
with a Neural Amp Modeler capture, stereo chorus, auto-swell and Eurorack CV
control. Built with AI-assisted coding as an experiment in bringing a small NAM
model into a guitar-and-modular setup.

The ready-made firmware includes **RiffSmith Clean by Matt Hartman**, used under
the MIT license with the author's copyright and license included. The exact
amplifier make and model behind this capture are not documented.

## Start here

- **Download the complete package:** [firmware, source, guides and licenses](https://github.com/adamcharleswilsonmusic-code/ampsim-for-legio/blob/main/legio-amp-sim-release.zip?raw=true). Keep the included source, copyright notices and licenses with the firmware.
- **Install and play:** [quick-start panel guide](PANEL-GUIDE.pdf).
- **Firmware:** [`legio-riffsmith-clean.bin`](legio-riffsmith-clean.bin).
- **Bring your own capture:** [compatibility, build and installation guide](CUSTOM_CAPTURE.md).
- **Build the included firmware:** [source and pinned dependency setup](source/REBUILD.txt).
- **People, projects and permissions:** [CREDITS.md](CREDITS.md),
  [full third-party notices](THIRD_PARTY_NOTICES.txt) and [license texts](licenses/).

Download the complete repository/release package and keep its licenses, notices
and included source with the firmware. The `.bin` file is the module's firmware;
the other files explain how to use it, rebuild it and meet redistribution terms.

### Install the supplied firmware

1. Mute monitoring and turn OUTPUT down. Connect Legio to your computer by USB.
2. Hold BOOT, press and release RESET, then release BOOT to enter the ROM USB
   firmware loader.
3. Follow [the installation procedure](CUSTOM_CAPTURE.md#4-install-your-build),
   using the supplied `legio-riffsmith-clean.bin` as the file to install. The guide
   explains how to identify the correct device and write only its application
   image. Keep a copy of this firmware for recovery.
4. Leave the **left chorus-depth CV jack unplugged during startup/reset** for its
   zero calibration. Confirm the normal purple bypass LEDs, then engage at low
   monitoring volume. If the module stays in the loader after flashing, press
   RESET once.

## Panel controls

| Control | Function |
| --- | --- |
| Top encoder | Amp input gain; press and release for bypass |
| Middle knob | Tone |
| Bottom knob | Output volume, including bypass |
| Left three-position toggle | Chorus: down off, centre wide, up stronger |
| Right three-position toggle | Auto-swell: down off, centre 0.9 seconds, up 1.8 seconds |
| Top-center gate jack | **Chorus clock:** eight incoming pulses per modulation cycle |
| Left CV jack | Chorus depth: zero preserves the base sound, positive deepens, negative reduces |
| Lower-center CV jack | Tone, summed with the middle knob |
| Right CV jack | Output, summed with the bottom knob |

Purple LEDs mean bypass. Engaged colors are orange/red; in swell mode the right
LED follows the swell's loudness and dims out in red.

The tone and output CVs share their knobs' analog paths. There is no cable
recognition or independent CV takeover. Output CV can raise or lower the combined
setting; it has **no separate knob-set volume ceiling**. For tremolo, set the
external LFO's attenuation and offset to the range you want.

## Audio routing

**IN L feeds the mono amp model. IN L and IN R are not summed.** Chorus creates a
stereo processed output, so connect both OUT L and OUT R to hear the spread. In
bypass, each input passes to its matching output, with the output control still
applied.

Feed the module suitable Eurorack-level audio. A guitar needs an instrument
input/preamp that provides the right impedance and level. Use suitable output
conversion or an audio interface for monitoring.

## Using another capture

The capture is embedded when the firmware is built. This version does not upload
or select model files while running.

It supports one specific **48 kHz mono A2-Lite WaveNet layout: 3 channels,
23 layers and 1,871 weights**. A file being labelled “NAM” or “Lite” does not by
itself make it compatible. The included checker verifies the complete supported
configuration and can extract a compatible Lite submodel from a NAM
SlimmableContainer. It cannot convert an unsupported model into a supported one.

Follow [CUSTOM_CAPTURE.md](CUSTOM_CAPTURE.md) to check, embed, build and flash a
compatible capture. Test your resulting firmware on the module before relying on
it. If you distribute that build, the replacement capture also needs explicit
redistribution rights and its required notices.

## Validation scope

The supplied firmware was installed on one Legio, read back byte for byte, and
confirmed running through USB diagnostics. The initial startup sample reported
no faults or missed deadlines. Computer checks cover model equivalence, chorus
depth bounds, unchanged unpatched chorus, bypass and telemetry. Packaged source
rebuilt to the exact installed image using the same toolchain and pinned local
upstream dependencies.

Longer engaged playing, external CV/clock response and a separate clean-machine
setup test remain pending for this packaged build. These checks do not establish
performance on every hardware setup or with other captures. See the included
verification records and the custom-capture guide for their exact scope.

## Credits and redistribution

**NAM Core is by Steven Atkinson. RiffSmith Clean is by Matt Hartman.** Their MIT
licenses permit redistribution subject to retaining the copyright and license
texts. RiffSmith's upstream grant explicitly covers its bundled capture weights.
See [CREDITS.md](CREDITS.md) for source links, versions and the rest of the credits.

The original application contributions have [their own MIT license](LICENSE.original-contributions.txt).
Third-party components keep their respective licenses: **the complete firmware
bundle is not uniformly MIT**. Keep [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt),
[all license texts](licenses/) and the exact covered Eigen source in
[`source/vendor/`](source/vendor/) with redistributions. ST's USB component also
retains its ST-microcontroller execution restriction. Review those terms before
porting to other hardware or replacing components.

This is independent firmware, not affiliated with or endorsed by Noise
Engineering, Electrosmith, TONE3000, or the NAM authors. Names identify
compatibility and give credit; their use does not imply endorsement.
