# Build Legio firmware with your own capture

The downloadable firmware is ready to use with RiffSmith Clean. Replacing the
capture is an optional source-build procedure on a computer. It does not add
model loading to the running module or change the panel controls.

## Compatibility

This build accepts only the exact mono A2-Lite WaveNet layout in
`source/tools/supported_capture_config.json`: 48 kHz, 3 channels, 23 layers and
1,871 weights. A compatible Lite submodel can be extracted from a NAM
SlimmableContainer. Ordinary NAM Standard, Full, LSTM and other WaveNet layouts
are not supported by this firmware. The checker is deliberately conservative;
matching sample rate and weight count alone is insufficient. It cannot shrink,
retrain or resample an incompatible model.

## 1. Prepare the source build

Unzip the release and keep its licenses and notices. Work from its `source`
folder. Use a folder path without spaces, for example `~/legio-build/source`.
You need Python 3, Git, Make, native build tools and Arm GNU Toolchain
14.3.rel1 for the `arm-none-eabi` target. The toolchain is a separate download
from Arm; it is not included in this release. The tested host is macOS; this
is not a Windows setup guide.

From the `source` folder, fetch the pinned dependencies:

```sh
mkdir -p host-research vendor

git clone --recursive https://github.com/sdatkinson/NeuralAmpModelerCore host-research/NeuralAmpModelerCore-current
git -C host-research/NeuralAmpModelerCore-current checkout 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
git -C host-research/NeuralAmpModelerCore-current submodule update --init --recursive

git clone --recursive https://github.com/electro-smith/libDaisy vendor/libDaisy
git -C vendor/libDaisy checkout f715e4875b2d28445d0c11a0a17f064a248f69d0
git -C vendor/libDaisy submodule update --init --recursive
```

The required Eigen source is already included. Keep this source folder as a
working copy; preserve the original release ZIP as your known-working fallback.

## 2. Check your capture

Replace the example path with the `.nam` file you want to use:

```sh
python3 tools/prepare_capture.py /path/to/your-capture.nam --check
```

If it reports an unsupported capture, stop. Renaming the file or deleting weights
will not make it compatible. If multiple matching submodels exist, the helper
uses the first one in the file. Check that this is the model you intend to use.

## 3. Embed the capture and build

```sh
python3 tools/prepare_capture.py /path/to/your-capture.nam
make GCC_PATH=/absolute/path/to/arm-toolchain/bin
```

The helper replaces `models/riffsmith_clean_embedded.h` in your working copy
and saves the selected model as `models/custom-capture.nam`. The original capture
file is preserved. The filename of the header is historical; its contents now
hold your capture. It does not retain RiffSmith's weights. USB diagnostics label
this build as a user-provided capture.

The resulting firmware is:

```text
firmware/build-usb-public-clean/legio_riffsmith_clean_usb.bin
```

The output filename is also historical. Rename your finished binary to identify
your capture. This recipe keeps the same controls, chorus, swell and short block
size. Confirm the binary is at most 131,072 bytes before installing it. Do not
use a larger image with the installation steps below.

## 4. Install your build

Use the same firmware-update procedure as the supplied firmware. Keep monitoring
muted and the OUTPUT knob low. Connect USB. Hold BOOT, press and release RESET,
then release BOOT to enter the module's ROM DFU loader.

With `dfu-util` installed, list devices first:

```sh
dfu-util -l
```

Confirm the target is your Legio/STM32 bootloader, USB ID `0483:df11`, with
internal flash on alternate setting 0. If there is more than one matching device,
disconnect the others before proceeding. Keep your known-working release binary
available for recovery. Write only the internal application image:

```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D firmware/build-usb-public-clean/legio_riffsmith_clean_usb.bin
```

A USB disconnect at launch is not proof that the application is running. If it
stays in the loader, press RESET once. Confirm the normal bypass LEDs return
before unmuting at low volume. Keep the left chorus-depth CV jack unplugged at startup
for its zero calibration. Never change option bytes, flash protection or QSPI
storage as part of this recipe.

## 5. Check the result and keep the right licenses

Start quietly, engage, and test clean notes, strong chords, chorus and swell.
Listen for unexpected volume or clipping. If you use the USB diagnostic reader,
check for faults and missed deadlines during sustained playing. Matching the
supported layout is not a substitute for testing your particular capture.
If the new build fails, enter BOOT/RESET again and reinstall the original release
binary using the same procedure.

For personal use, follow the capture's own license. If you share your modified
firmware, you are also sharing its embedded capture: obtain redistribution rights
and include that author's required copyright/license notices. A free download is
not automatically a redistribution grant. Keep the release's third-party
licenses, notices and covered Eigen source; update the model credit and describe
your changes. The included RiffSmith license does not grant rights to a different
capture.

## Suggested video wording

“The download is ready to use with RiffSmith Clean. If you want to build it with
your own capture, I've included the source and a step-by-step guide. It supports
a specific small A2-Lite model format, so run the compatibility check first—not
every NAM file will work. If you share your modified firmware, make sure your
capture's license allows that too.”

## Verification scope

The helper was checked with the bundled licensed Clean and Distorted captures,
including container extraction, exact embedded float weights and rejection of
unsupported inputs. The custom-capture source recipe was built locally with the
pinned dependencies. This is not a clean-machine setup test, and a custom-capture
build has not been flashed to the user's module as part of this guide.
