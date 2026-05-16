BINARY      = stm32_qca7kprogrammer
OUT_DIR     = build/firmware
HOST_DIR    = build/host
GENERATED   = generated
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
COMMON_WARNINGS = -Wall -Wextra -Werror -pedantic
FREESTANDING    = -ffreestanding -fno-builtin -fno-exceptions -fno-rtti -fno-unwind-tables -fno-threadsafe-statics
FW_CFLAGS       = -Os $(COMMON_WARNINGS) $(COMMON_INCLUDES) $(FREESTANDING) -mcpu=cortex-m3 -mthumb -std=gnu11
FW_CPPFLAGS     = -Os $(COMMON_WARNINGS) $(COMMON_INCLUDES) $(FREESTANDING) -mcpu=cortex-m3 -mthumb -std=c++17
FW_LDFLAGS      = -nostdlib -Wl,--gc-sections,-Map,$(OUT_DIR)/$(BINARY).map -Tstm32_qca7kprogrammer.ld
HOST_FLAGS      = -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDES) -DHOST_BUILD -std=c++17
HOST_CFLAGS     = -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDES) -DHOST_BUILD -std=gnu11

FW_CPP_SRCS     = src/main.cpp src/programmer.cpp src/qca7005_transport.cpp src/hwinit.cpp src/embedded_images.cpp
FW_C_SRCS       = src/startup.c $(GENERATED)/softloader_asset.c $(GENERATED)/firmware_asset.c $(GENERATED)/evse_asset.c
FW_CPP_OBJS     = $(patsubst src/%.cpp,$(OUT_DIR)/%.o,$(FW_CPP_SRCS))
FW_C_OBJS       = $(patsubst src/%.c,$(OUT_DIR)/%.o,$(filter src/%.c,$(FW_C_SRCS))) \
                  $(OUT_DIR)/softloader_asset.o $(OUT_DIR)/firmware_asset.o $(OUT_DIR)/evse_asset.o
FW_OBJS         = $(FW_CPP_OBJS) $(FW_C_OBJS)
HOST_CPP_SRCS   = src/programmer.cpp src/qca7005_transport.cpp src/hwinit.cpp src/main.cpp src/embedded_images.cpp tests/programmer_tests.cpp
HOST_CPP_OBJS   = $(patsubst src/%.cpp,$(HOST_DIR)/src_%.o,$(filter src/%.cpp,$(HOST_CPP_SRCS))) \
                  $(HOST_DIR)/tests_programmer_tests.o
HOST_C_OBJS     = $(HOST_DIR)/softloader_asset.o $(HOST_DIR)/firmware_asset.o $(HOST_DIR)/evse_asset.o
HOST_OBJS       = $(HOST_CPP_OBJS) $(HOST_C_OBJS)

all: test

test: $(HOST_DIR)/programmer_tests
	$(HOST_DIR)/programmer_tests

firmware: check-cross-tools $(OUT_DIR)/$(BINARY).bin $(OUT_DIR)/$(BINARY).hex

check-cross-tools:
	@command -v $(CPP) >/dev/null 2>&1 || (echo "Missing $(CPP). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)
	@command -v $(CC) >/dev/null 2>&1 || (echo "Missing $(CC). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)
	@command -v $(OBJCOPY) >/dev/null 2>&1 || (echo "Missing $(OBJCOPY). Install arm-none-eabi toolchain or set PREFIX=..." && exit 1)

$(GENERATED):
	$(MKDIR_P) $(GENERATED)

$(OUT_DIR) $(HOST_DIR):
	$(MKDIR_P) $@

$(GENERATED)/softloader_asset.c: assets/softloader.nvm | $(GENERATED)
	xxd -i -n g_softloader_nvm $< > $@

$(GENERATED)/firmware_asset.c: assets/firmware.nvm | $(GENERATED)
	xxd -i -n g_firmware_nvm $< > $@

$(GENERATED)/evse_asset.c: assets/evse.pib | $(GENERATED)
	xxd -i -n g_evse_pib $< > $@

$(OUT_DIR)/%.o: src/%.cpp Makefile | $(OUT_DIR)
	$(CPP) $(FW_CPPFLAGS) -c $< -o $@

$(OUT_DIR)/%.o: src/%.c Makefile | $(OUT_DIR)
	$(CC) $(FW_CFLAGS) -c $< -o $@

$(OUT_DIR)/softloader_asset.o: $(GENERATED)/softloader_asset.c Makefile | $(OUT_DIR)
	$(CC) $(FW_CFLAGS) -c $< -o $@

$(OUT_DIR)/firmware_asset.o: $(GENERATED)/firmware_asset.c Makefile | $(OUT_DIR)
	$(CC) $(FW_CFLAGS) -c $< -o $@

$(OUT_DIR)/evse_asset.o: $(GENERATED)/evse_asset.c Makefile | $(OUT_DIR)
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

$(HOST_DIR)/softloader_asset.o: $(GENERATED)/softloader_asset.c Makefile | $(HOST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_DIR)/firmware_asset.o: $(GENERATED)/firmware_asset.c Makefile | $(HOST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_DIR)/evse_asset.o: $(GENERATED)/evse_asset.c Makefile | $(HOST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_DIR)/programmer_tests: $(GENERATED)/softloader_asset.c $(GENERATED)/firmware_asset.c $(GENERATED)/evse_asset.c $(HOST_OBJS)
	$(HOST_CXX) -o $@ $(HOST_OBJS)

clean:
	rm -rf build generated

.PHONY: all clean firmware test check-cross-tools
