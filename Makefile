.PHONY: all firmware test get-deps check-cross-tools clean

# ── Cross-compiler toolchain ───────────────────────────────────────────────
CROSS    := arm-none-eabi-
CXX      := $(CROSS)g++
OBJCOPY  := $(CROSS)objcopy

# ── Native compiler (for tests) ───────────────────────────────────────────
CXX_NATIVE := g++

# ── Firmware compiler flags ───────────────────────────────────────────────
CXXFLAGS_FW := -mcpu=cortex-m3 -mthumb -Og -ggdb -fno-exceptions -fno-rtti
CXXFLAGS_FW += -DSTM32F1
CXXFLAGS_FW += -Ilibopencm3/include -Iinclude
CXXFLAGS_FW += -std=c++11 -Wall -Wextra
CXXFLAGS_FW += -ffunction-sections -fdata-sections

# ── Linker flags ──────────────────────────────────────────────────────────
LDFLAGS_FW  := -mcpu=cortex-m3 -mthumb -nostartfiles -T stm32f103.ld
LDFLAGS_FW  += -Wl,--gc-sections
FW_LIBS     := -Llibopencm3/lib -lopencm3_stm32f1 -lgcc

# ── Firmware sources ──────────────────────────────────────────────────────
FW_SRCS := src/hwinit.cpp \
           src/qca_spi.cpp \
           src/programmer.cpp \
           src/debug.cpp \
           src/embedded_images.cpp \
           src/main.cpp

FW_OBJS  := $(FW_SRCS:.cpp=.fw.o)
ASM_OBJ  := src/embedded_data.fw.o

# ── Test sources ──────────────────────────────────────────────────────────
TEST_SRCS := tests/test_programmer.cpp \
             tests/mock_qca.cpp \
             src/programmer.cpp \
             src/debug.cpp

TEST_CXXFLAGS := -DUNIT_TEST -Iinclude -std=c++11 -Wall -Wextra -g

# ══════════════════════════════════════════════════════════════════════════
# Targets
# ══════════════════════════════════════════════════════════════════════════

all: firmware

# ── Firmware build ────────────────────────────────────────────────────────
firmware: check-cross-tools get-deps $(FW_OBJS) $(ASM_OBJ)
	$(CXX) $(LDFLAGS_FW) -o firmware.elf $(FW_OBJS) $(ASM_OBJ) $(FW_LIBS)
	$(OBJCOPY) -O binary firmware.elf firmware.bin
	@echo "firmware.bin built ($$(wc -c < firmware.bin) bytes)"

%.fw.o: %.cpp
	$(CXX) $(CXXFLAGS_FW) -c $< -o $@

$(ASM_OBJ): src/embedded_images.s
	$(CXX) $(CXXFLAGS_FW) -x assembler-with-cpp -c $< -o $@

# ── Unit tests (native build, no cross-compiler required) ─────────────────
test: $(TEST_SRCS)
	$(CXX_NATIVE) $(TEST_CXXFLAGS) -o tests/run_tests $(TEST_SRCS)
	cd $(CURDIR) && ./tests/run_tests

# ── Fetch and build libopencm3 submodule ──────────────────────────────────
get-deps:
	@if [ ! -f libopencm3/Makefile ]; then \
	    git submodule update --init --recursive; \
	fi
	$(MAKE) -C libopencm3 TARGETS=stm32/f1

# ── Verify cross-compiler is installed ───────────────────────────────────
check-cross-tools:
	@which $(CROSS)g++ > /dev/null 2>&1 || \
	    (echo "ERROR: $(CROSS)g++ not found. Install gcc-arm-none-eabi." && exit 1)

# ── Clean ─────────────────────────────────────────────────────────────────
clean:
	rm -f src/*.fw.o firmware.elf firmware.bin tests/run_tests
