#pragma once
#include <stdint.h>

/* NVM image header v1.1 (Qualcomm Atheros, 96 bytes, little-endian, packed) */
typedef struct __attribute__((packed)) {
    uint16_t MajorVersion;      /* must be 1                              */
    uint16_t MinorVersion;      /* must be 1                              */
    uint32_t ExecuteMask;
    uint32_t ImageNvmAddress;
    uint32_t ImageAddress;      /* SDRAM target address                   */
    uint32_t ImageLength;       /* image byte count (not including header)*/
    uint32_t ImageChecksum;     /* CRC32 of image data                    */
    uint32_t EntryPoint;        /* execution entry point (0xFFFFFFFF=none)*/
    uint32_t NextHeader;        /* offset of next header (0xFFFFFFFF=end) */
    uint32_t PrevHeader;
    uint32_t ImageType;
    uint16_t ModuleID;
    uint16_t ModuleSubID;
    uint16_t AppletEntryVersion;
    uint16_t Reserved0;
    uint32_t Reserved[11]; /* Padding to reach 96-byte total:
                            * 2+2+4+4+4+4+4+4+4+4+4+2+2+2+2 = 48 bytes used,
                            * 11×4 = 44 reserved + 4 HeaderChecksum = 96 */
    uint32_t HeaderChecksum;    /* CRC32 of this header (result == 0)     */
} nvm_header2_t;

/* Compile-time size assertion (checked at runtime in tests) */
/* sizeof(nvm_header2_t) == 96 */

#define NVM_NO_NEXT_HEADER  0xFFFFFFFFu

/* ImageType constants */
#define NVM_IMAGE_GENERIC       0x0000u
#define NVM_IMAGE_MEMCTL        0x0007u  /* Memory controller applet      */
#define NVM_IMAGE_FIRMWARE      0x0004u  /* Main runtime firmware         */
#define NVM_IMAGE_SOFTLOADER    0x000Bu  /* Softloader                    */
#define NVM_IMAGE_FLASHLAYOUT   0x000Cu
#define NVM_IMAGE_MANIFEST      0x000Eu  /* Manifest                      */
#define NVM_IMAGE_PIB           0x000Fu  /* Parameter Information Block   */
