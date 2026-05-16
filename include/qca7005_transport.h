#ifndef STM32_QCA7KPROGRAMMER_QCA7005_TRANSPORT_H
#define STM32_QCA7KPROGRAMMER_QCA7005_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "transport.h"

struct Qca7005Transport
{
   uint8_t tx_buffer[2048];
   uint8_t rx_buffer[2048];
};

void qca7005_setup(Qca7005Transport* transport);
uint16_t qca7005_read_signature(Qca7005Transport* transport);
EthernetTransport qca7005_make_transport(Qca7005Transport* transport);

#endif
