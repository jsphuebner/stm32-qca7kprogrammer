BINARY      = stm32_qca7kprogrammer
OUT_DIR     = build/firmware
HOST_DIR    = build/host
PREFIX      ?= arm-none-eabi
CC          = $(PREFIX)-gcc
CPP         = $(PREFIX)-g++
LD          = $(PREFIX)-g++
OBJCOPY     = $(PREFIX)-objcopy
SIZE        = $(PREFIX)-size
HOST_CXX    ?= g++
HOST_CC     ?= gcc
MKDIR_P     = mkdir -p

COMMON_INCLUDES = -Iinclude
FW_INCLUDES     = $(COMMON_INCLUDES) -Ilibopencm3/include
COMMON_WARNINGS = -Wall -Wextra -Werror -pedantic
FREESTANDING    = -ffreestanding -fno-builtin -fno-unwind-tables
FREESTANDING_CPP= -fno-exceptions -fno-rtti -fno-threadsafe-statics
FW_CFLAGS       = -Os $(COMMON_WARNINGS) $(FW_INCLUDES) $(FREESTANDING) -mcpu=cortex-m3 -mthumb -std=gnu11
FW_CPPFLAGS     = -Os $(COMMON_WARNINGS) $(FW_INCLUDES) $(FREESTANDING) $(FREESTANDING_CPP) -mcpu=cortex-m3 -mthumb -std=c++17
FW_LDFLAGS      = -nostdlib -Wl,--gc-sections,-Map,$(OUT_DIR)/$(BINARY).map -Tstm32_qca7kprogrammer.ld
HOST_FLAGS      = -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDES) -DHOST_BUILD -std=c++17
HOST_CFLAGS     = -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDES) -DHOST_BUILD -std=gnu11

FW_CPP_SRCS     = src/main.cpp src/programmer.cpp src/qca7005_transport.cpp src/hwinit.cpp src/embedded_images.cpp
FW_C_SRCS       = src/startup.c src/softloader_asset.c src/firmware_asset.c src/evse_asset.c
FW_CPP_OBJS     = $(patsubst src/%.cpp,$(OUT_DIR)/%.o,$(FW_CPP_SRCS))
FW_C_OBJS       = $(patsubst src/%.c,$(OUT_DIR)/%.o,$(FW_C_SRCS))
FW_OBJS         = $(FW_CPP_OBJS) $(FW_C_OBJS)
HOST_CPP_SRCS   = src/programmer.cpp src/qca7005_transport.cpp src/hwinit.cpp src/main.cpp src/embedded_images.cpp tests/programmer_tests.cpp
HOST_CPP_OBJS   = $(patsubst src/%.cpp,$(HOST_DIR)/src_%.o,$(filter src/%.cpp,$(HOST_CPP_SRCS))) \
                  $(HOST_DIR)/tests_programmer_tests.o
HOST_C_SRCS     = src/softloader_asset.c src/firmware_asset.c src/evse_asset.c
HOST_C_OBJS     = $(patsubst src/%.c,$(HOST_DIR)/src_%.o,$(HOST_C_SRCS))
HOST_OBJS       = $(HOST_CPP_OBJS) $(HOST_C_OBJS)

all: test

test: $(HOST_DIR)/programmer_tests
	$(HOST_DIR)/programmer_tests

firmware: check-cross-tools check-libopencm3 $(OUT_DIR)/$(BINARY).bin $(OUT_DIR)/$(BINARY).hex

check-cross-tools:
	@command -v $(CPP) >/dev/null 2>&1 || (echo "Missing $(CPP). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)
	@command -v $(CC) >/dev/null 2>&1 || (echo "Missing $(CC). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)
	@command -v $(OBJCOPY) >/dev/null 2>&1 || (echo "Missing $(OBJCOPY). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)

check-libopencm3:
	@test -d libopencm3/include || (echo "Missing libopencm3 submodule. Run: git submodule update --init --recursive" && exit 1)

$(OUT_DIR) $(HOST_DIR):
	$(MKDIR_P) $@

$(OUT_DIR)/%.o: src/%.cpp Makefile | $(OUT_DIR)
	$(CPP) $(FW_CPPFLAGS) -c $< -o $@

$(OUT_DIR)/%.o: src/%.c Makefile | $(OUT_DIR)
	$(CC) $(FW_CFLAGS) -c $< -o $@

$(OUT_DIR)/$(BINARY).elf: $(FW_OBJS) stm32_qca7kprogrammer.ld
	$(LD) $(FW_CPPFLAGS) $(FW_LDFLAGS) -o $@ $(FW_OBJS) -lgcc
	$(SIZE) $@

$(OUT_DIR)/$(BINARY).bin: $(OUT_DIR)/$(BINARY).elf
	$(OBJCOPY) -Obinary $< $@

$(OUT_DIR)/$(BINARY).hex: $(OUT_DIR)/$(BINARY).elf
	$(OBJCOPY) -Oihex $< $@

$(HOST_DIR)/src_%.o: src/%.cpp Makefile | $(HOST_DIR)
	$(HOST_CXX) $(HOST_FLAGS) -c $< -o $@

$(HOST_DIR)/tests_programmer_tests.o: tests/programmer_tests.cpp Makefile | $(HOST_DIR)
	$(HOST_CXX) $(HOST_FLAGS) -c $< -o $@

$(HOST_DIR)/src_%.o: src/%.c Makefile | $(HOST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_DIR)/programmer_tests: $(HOST_OBJS)
	$(HOST_CXX) -o $@ $(HOST_OBJS)

clean:
	rm -rf build

.PHONY: all clean firmware test check-cross-tools check-libopencm3
