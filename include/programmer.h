#ifndef STM32_QCA7KPROGRAMMER_PROGRAMMER_H
#define STM32_QCA7KPROGRAMMER_PROGRAMMER_H

#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "embedded_images.h"
#include "transport.h"

enum ProgrammerResult
{
   PROGRAMMER_OK = 0,
   PROGRAMMER_INVALID_IMAGES,
   PROGRAMMER_TRANSPORT_ERROR,
   PROGRAMMER_PROTOCOL_ERROR,
   PROGRAMMER_TIMEOUT
};

struct PACKED NvmHeader2
{
   uint16_t major_version;
   uint16_t minor_version;
   uint32_t execute_mask;
   uint32_t image_nvm_address;
   uint32_t image_address;
   uint32_t image_length;
   uint32_t image_checksum;
   uint32_t entry_point;
   uint32_t next_header;
   uint32_t prev_header;
   uint32_t image_type;
   uint16_t module_id;
   uint16_t module_sub_id;
   uint16_t applet_entry_version;
   uint16_t reserved0;
   uint32_t reserved1;
   uint32_t reserved2;
   uint32_t reserved3;
   uint32_t reserved4;
   uint32_t reserved5;
   uint32_t reserved6;
   uint32_t reserved7;
   uint32_t reserved8;
   uint32_t reserved9;
   uint32_t reserved10;
   uint32_t reserved11;
   uint32_t header_checksum;
};

ProgrammerResult run_programmer(const EmbeddedImages& images, const EthernetTransport& transport);

#endif
