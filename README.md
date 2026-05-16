# stm32-qca7kprogrammer

Minimal STM32 firmware that talks to a QCA7005 over SPI and performs the same flash programming sequence as:

```sh
plcboot -F -S softloader.nvm -N firmware.nvm -P evse.pib
```

## What it does

On boot the firmware:

1. waits for the QCA7005 bootloader,
2. uploads the SDRAM init applet from `firmware.nvm`,
3. uploads `evse.pib` and the runtime image from `firmware.nvm` into SDRAM,
4. uses `VS_MODULE_OPERATION` to flash `softloader.nvm`, `evse.pib` and `firmware.nvm`,
5. resets the QCA7005 and waits for the final firmware to start.

The SPI framing follows the STM32/QCA7005 example used in `jsphuebner/ccs32clara`, while the HomePlug/MME sequencing follows `qca/open-plc-utils`.

## Embedded input files

The firmware links these three files into the STM32 image:

- `/home/runner/work/stm32-qca7kprogrammer/stm32-qca7kprogrammer/assets/softloader.nvm`
- `/home/runner/work/stm32-qca7kprogrammer/stm32-qca7kprogrammer/assets/firmware.nvm`
- `/home/runner/work/stm32-qca7kprogrammer/stm32-qca7kprogrammer/assets/evse.pib`

Placeholder files are committed so the project builds. Replace them with the real binaries before producing the final STM32 firmware image.

## Targets

- `make test` runs native protocol tests and syntax-checks the firmware sources in `HOST_BUILD` mode.
- `make firmware` builds the bare-metal STM32 image when `arm-none-eabi-*` tools are installed.

## Notes

- The firmware is written in a small standalone style similar to `stm32-CANBootloader`.
- The current hardware setup targets an STM32F103 with QCA7005 on SPI1 (`PA4` CS, `PA5` SCK, `PA6` MISO, `PA7` MOSI).
- The placeholder asset contents are not valid QCA images; flashing only works after replacing them with the real files.
