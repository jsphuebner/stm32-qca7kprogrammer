/* Embedded binary images — included via .incbin into .rodata */

    .section .rodata
    .align 4

    .global _binary_softloader_nvm_start
    .type   _binary_softloader_nvm_start, %object
_binary_softloader_nvm_start:
    .incbin "softloader.nvm"
_binary_softloader_nvm_end:
    .global _binary_softloader_nvm_end
    .type   _binary_softloader_nvm_end, %object

    .align 4

    .global _binary_firmware_nvm_start
    .type   _binary_firmware_nvm_start, %object
_binary_firmware_nvm_start:
    .incbin "firmware.nvm"
_binary_firmware_nvm_end:
    .global _binary_firmware_nvm_end
    .type   _binary_firmware_nvm_end, %object

    .align 4

    .global _binary_evse_pib_start
    .type   _binary_evse_pib_start, %object
_binary_evse_pib_start:
    .incbin "evse.pib"
_binary_evse_pib_end:
    .global _binary_evse_pib_end
    .type   _binary_evse_pib_end, %object
