#include "qca7005_transport.h"
#include "common.h"
#include "hwinit.h"

#ifndef HOST_BUILD
namespace {
constexpr uintptr_t GPIOA_BASE = 0x40010800u;
constexpr uintptr_t SPI1_BASE = 0x40013000u;
constexpr uintptr_t GPIOA_BSRR = GPIOA_BASE + 0x10u;
constexpr uintptr_t SPI1_SR = SPI1_BASE + 0x08u;
constexpr uintptr_t SPI1_DR = SPI1_BASE + 0x0Cu;
constexpr uint8_t kSpiReadSignature = 0xDAu;
constexpr uint8_t kSpiWriteBfrSize = 0x41u;
constexpr uint8_t kSpiReadRxAvailable = 0xC3u;
constexpr size_t kQcaFrameOverhead = 12u;

inline volatile uint32_t& reg32(uintptr_t address)
{
   return *(volatile uint32_t*)address;
}

void chip_select(bool asserted)
{
   if (asserted)
      reg32(GPIOA_BSRR) = (1u << (4 + 16));
   else
      reg32(GPIOA_BSRR) = (1u << 4);
}

uint8_t spi_transfer(uint8_t value)
{
   while ((reg32(SPI1_SR) & (1u << 1)) == 0u)
      ;
   reg32(SPI1_DR) = value;
   while ((reg32(SPI1_SR) & (1u << 0)) == 0u)
      ;
   return (uint8_t)reg32(SPI1_DR);
}

void spi_exchange(uint8_t* rx, const uint8_t* tx, size_t length)
{
   chip_select(true);
   for (size_t index = 0; index < length; index++)
      rx[index] = spi_transfer(tx[index]);
   while ((reg32(SPI1_SR) & (1u << 7)) != 0u)
      ;
   chip_select(false);
}

uint16_t read_internal_register(Qca7005Transport* transport, uint8_t command)
{
   transport->tx_buffer[0] = command;
   transport->tx_buffer[1] = 0x00u;
   transport->tx_buffer[2] = 0x00u;
   transport->tx_buffer[3] = 0x00u;
   spi_exchange(transport->rx_buffer, transport->tx_buffer, 4u);
   return (uint16_t)((transport->rx_buffer[2] << 8) | transport->rx_buffer[3]);
}

void write_bfr_size(Qca7005Transport* transport, uint16_t size)
{
   transport->tx_buffer[0] = kSpiWriteBfrSize;
   transport->tx_buffer[1] = 0x00u;
   transport->tx_buffer[2] = (uint8_t)(size >> 8);
   transport->tx_buffer[3] = (uint8_t)size;
   spi_exchange(transport->rx_buffer, transport->tx_buffer, 4u);
}

bool qca_send_frame_impl(void* context, const uint8_t* frame, size_t length)
{
   Qca7005Transport* transport = (Qca7005Transport*)context;
   if (length + kQcaFrameOverhead > sizeof(transport->tx_buffer))
      return false;

   write_bfr_size(transport, (uint16_t)(length + 10u));
   transport->tx_buffer[0] = 0x00u;
   transport->tx_buffer[1] = 0x00u;
   transport->tx_buffer[2] = 0xAAu;
   transport->tx_buffer[3] = 0xAAu;
   transport->tx_buffer[4] = 0xAAu;
   transport->tx_buffer[5] = 0xAAu;
   transport->tx_buffer[6] = (uint8_t)length;
   transport->tx_buffer[7] = (uint8_t)(length >> 8);
   transport->tx_buffer[8] = 0x00u;
   transport->tx_buffer[9] = 0x00u;
   mem_copy(&transport->tx_buffer[10], frame, length);
   transport->tx_buffer[10 + length] = 0x55u;
   transport->tx_buffer[11 + length] = 0x55u;
   spi_exchange(transport->rx_buffer, transport->tx_buffer, length + kQcaFrameOverhead);
   return true;
}

int qca_receive_frame_impl(void* context, uint8_t* frame, size_t capacity, uint32_t timeout_ms)
{
   Qca7005Transport* transport = (Qca7005Transport*)context;
   const uint32_t deadline = millis() + timeout_ms;

   while ((int32_t)(deadline - millis()) >= 0)
   {
      uint16_t available = read_internal_register(transport, kSpiReadRxAvailable);
      if (available > 0u)
      {
         if (available + 2u > sizeof(transport->tx_buffer))
            available = (uint16_t)(sizeof(transport->tx_buffer) - 2u);

         write_bfr_size(transport, available);
         transport->tx_buffer[0] = 0x80u;
         transport->tx_buffer[1] = 0x00u;
         mem_set(&transport->tx_buffer[2], 0x00u, available);
         spi_exchange(transport->rx_buffer, transport->tx_buffer, available + 2u);

         for (uint16_t index = 0; index < available; index++)
            transport->rx_buffer[index] = transport->rx_buffer[index + 2u];

         size_t offset = 0u;
         while (offset + 12u <= available)
         {
            const uint16_t outer_length = (uint16_t)((transport->rx_buffer[offset + 2u] << 8) |
                                                     transport->rx_buffer[offset + 3u]);
            uint16_t inner_length = (uint16_t)((transport->rx_buffer[offset + 9u] << 8) |
                                               transport->rx_buffer[offset + 8u]);
            if (outer_length < 10u || inner_length + 10u != outer_length)
               break;
            if (offset + outer_length + 4u > available)
               break;
            if (transport->rx_buffer[offset + 4u] != 0xAAu ||
                transport->rx_buffer[offset + 5u] != 0xAAu ||
                transport->rx_buffer[offset + 6u] != 0xAAu ||
                transport->rx_buffer[offset + 7u] != 0xAAu)
               break;
            if (inner_length > capacity)
               inner_length = (uint16_t)capacity;
            mem_copy(frame, &transport->rx_buffer[offset + 12u], inner_length);
            return inner_length;
         }
      }
      delay_ms(1u);
   }

   return 0;
}

void delay_ms_impl(void*, uint32_t timeout_ms)
{
   delay_ms(timeout_ms);
}

uint32_t millis_impl(void*)
{
   return millis();
}
} // namespace

void qca7005_setup(Qca7005Transport*)
{
   clock_setup();
   gpio_setup();
   spi_setup();
   debug_uart_setup();
   systick_setup(8000000u);
}

uint16_t qca7005_read_signature(Qca7005Transport* transport)
{
   return read_internal_register(transport, kSpiReadSignature);
}

EthernetTransport qca7005_make_transport(Qca7005Transport* transport)
{
   EthernetTransport iface = { transport, qca_send_frame_impl, qca_receive_frame_impl, delay_ms_impl, millis_impl };
   return iface;
}
#else
void qca7005_setup(Qca7005Transport*) {}
uint16_t qca7005_read_signature(Qca7005Transport*) { return 0u; }
static bool qca_send_frame_impl(void*, const uint8_t*, size_t) { return false; }
static int qca_receive_frame_impl(void*, uint8_t*, size_t, uint32_t) { return 0; }
static void delay_ms_impl(void*, uint32_t) {}
static uint32_t millis_impl(void*) { return 0u; }
EthernetTransport qca7005_make_transport(Qca7005Transport* transport)
{
   EthernetTransport iface = { transport, qca_send_frame_impl, qca_receive_frame_impl, delay_ms_impl, millis_impl };
   return iface;
}
#endif
