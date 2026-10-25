# Build only the licensed public-capture configuration.
GCC_PATH ?= /path/to/arm-gnu-toolchain/bin
.PHONY: all
all:
	python3 tools/prepare_embedded_dsp.py
	$(MAKE) -C vendor/libDaisy GCC_PATH=$(GCC_PATH)
	$(MAKE) -C firmware GCC_PATH=$(GCC_PATH) LEGIO_THREE_MODELS=0 LEGIO_FIXED_MODEL=1 LEGIO_PUBLIC_CAPTURE=1 LEGIO_USB_TELEMETRY=1 LEGIO_DIAGNOSTICS=1 LEGIO_PROBE=1 LEGIO_BLOCK_FRAMES=24 LEGIO_MIDS_SWELL=1 LEGIO_PRESETS=0 LEGIO_CHORUS=1 LEGIO_CHORUS_CLOCK=1 BUILD_DIR=build-usb-public-clean TARGET=legio_riffsmith_clean_usb
