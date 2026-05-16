#ifndef STM32_QCA7KPROGRAMMER_TRANSPORT_H
#define STM32_QCA7KPROGRAMMER_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

struct EthernetTransport
{
   void* context;
   bool (*send_frame)(void* context, const uint8_t* frame, size_t length);
   int (*receive_frame)(void* context, uint8_t* frame, size_t capacity, uint32_t timeout_ms);
   void (*delay_ms)(void* context, uint32_t delay_ms);
   uint32_t (*millis)(void* context);
};

#endif
