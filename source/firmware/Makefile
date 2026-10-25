TARGET = legio_nam_lite
LIBDAISY_DIR = ../vendor/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
GCC_PATH ?= ../tools/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin
CORE = ../host-research/NeuralAmpModelerCore-current
EIGEN = ../vendor/nam-pedal/NeuralAmpModelerCore/Dependencies/eigen
LEGIO_THREE_MODELS ?= 1
LEGIO_DIAGNOSTICS ?= 0
LEGIO_PROBE ?= 0
LEGIO_FIXED_MODEL ?= 0
LEGIO_USB_TELEMETRY ?= 0
LEGIO_ENCODER_ONLY ?= 0
LEGIO_BLOCK_FRAMES ?= 48
LEGIO_MIDS_SWELL ?= 0
LEGIO_PRESETS ?= 0
LEGIO_CHORUS ?= 0
LEGIO_CHORUS_CLOCK ?= 0
LEGIO_PUBLIC_CAPTURE ?= 1
CPP_SOURCES = main.cpp heap.cpp diagnostics.cpp usb_telemetry.cpp ../src/lite_processor.cpp ../src/a2_lite_factory.cpp generated/nam_dsp.cpp
C_INCLUDES = -I../src -I$(CORE) -I$(CORE)/NAM -I$(CORE)/Dependencies/nlohmann -I$(EIGEN)
C_INCLUDES += -I$(LIBDAISY_DIR)/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
C_DEFS = -DNAM_SAMPLE_FLOAT -DNAM_ENABLE_A2_FAST -DNAM_DEFAULT_MAX_BUFFER_SIZE=$(LEGIO_BLOCK_FRAMES) -DLEGIO_THREE_MODELS=$(LEGIO_THREE_MODELS) -DLEGIO_DIAGNOSTICS=$(LEGIO_DIAGNOSTICS) -DLEGIO_PROBE=$(LEGIO_PROBE) -DEIGEN_DONT_VECTORIZE -DNDEBUG
C_DEFS += -DLEGIO_FIXED_MODEL=$(LEGIO_FIXED_MODEL) -DLEGIO_USB_TELEMETRY=$(LEGIO_USB_TELEMETRY) -DLEGIO_ENCODER_ONLY=$(LEGIO_ENCODER_ONLY)
C_DEFS += -DLEGIO_BLOCK_FRAMES=$(LEGIO_BLOCK_FRAMES)
C_DEFS += -DLEGIO_MIDS_SWELL=$(LEGIO_MIDS_SWELL)
C_DEFS += -DLEGIO_PRESETS=$(LEGIO_PRESETS)
C_DEFS += -DLEGIO_CHORUS=$(LEGIO_CHORUS)
C_DEFS += -DLEGIO_CHORUS_CLOCK=$(LEGIO_CHORUS_CLOCK)
C_DEFS += -DLEGIO_PUBLIC_CAPTURE=$(LEGIO_PUBLIC_CAPTURE) -DEIGEN_MPL2_ONLY
CPP_STANDARD = -std=gnu++17
OPT = -Os
APP_TYPE = BOOT_NONE
include $(SYSTEM_FILES_DIR)/Makefile
# Upstream NAM reports construction errors with exceptions. Keep that behavior;
# no model construction or exception path is used in ordinary audio processing.
CPPFLAGS = $(CFLAGS) -fasm -finline -finline-functions-called-once -fno-move-loop-invariants -fno-rtti -Wno-register -fexceptions -flto
LDFLAGS += -flto
ifeq ($(LEGIO_DIAGNOSTICS),1)
LDFLAGS += -Wl,--wrap=HAL_DMA_IRQHandler
endif
# Inference needs throughput; keep board/setup code small. No fast-math: finite
# checks and the original floating-point behavior remain valid.
$(BUILD_DIR)/a2_lite_factory.o: CPPFLAGS += -O3

.PHONY: prepare
prepare:
	python3 ../tools/prepare_embedded_dsp.py

# Deliberately disable all inherited device-write recipes in this build.
program program-dfu program-boot debug:
	$(error Use the documented DFU installation procedure in CUSTOM_CAPTURE.md)
