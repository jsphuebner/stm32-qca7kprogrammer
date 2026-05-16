#ifndef STM32_QCA7KPROGRAMMER_COMMON_H
#define STM32_QCA7KPROGRAMMER_COMMON_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

static inline uint16_t bswap16(uint16_t value)
{
   return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint32_t bswap32(uint32_t value)
{
   return ((value & 0x000000FFu) << 24) |
          ((value & 0x0000FF00u) << 8) |
          ((value & 0x00FF0000u) >> 8) |
          ((value & 0xFF000000u) >> 24);
}

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
static inline uint16_t host_to_le16(uint16_t value) { return bswap16(value); }
static inline uint32_t host_to_le32(uint32_t value) { return bswap32(value); }
static inline uint16_t le16_to_host(uint16_t value) { return bswap16(value); }
static inline uint32_t le32_to_host(uint32_t value) { return bswap32(value); }
static inline uint16_t host_to_be16(uint16_t value) { return value; }
#else
static inline uint16_t host_to_le16(uint16_t value) { return value; }
static inline uint32_t host_to_le32(uint32_t value) { return value; }
static inline uint16_t le16_to_host(uint16_t value) { return value; }
static inline uint32_t le32_to_host(uint32_t value) { return value; }
static inline uint16_t host_to_be16(uint16_t value) { return bswap16(value); }
#endif

static inline void mem_copy(void* dst, const void* src, size_t length)
{
   uint8_t* out = (uint8_t*)dst;
   const uint8_t* in = (const uint8_t*)src;

   for (size_t index = 0; index < length; index++)
      out[index] = in[index];
}

static inline void mem_set(void* dst, uint8_t value, size_t length)
{
   uint8_t* out = (uint8_t*)dst;

   for (size_t index = 0; index < length; index++)
      out[index] = value;
}

static inline int mem_compare(const void* lhs, const void* rhs, size_t length)
{
   const uint8_t* left = (const uint8_t*)lhs;
   const uint8_t* right = (const uint8_t*)rhs;

   for (size_t index = 0; index < length; index++)
   {
      if (left[index] != right[index])
         return left[index] < right[index] ? -1 : 1;
   }

   return 0;
}

static inline size_t cstr_length(const char* text)
{
   size_t length = 0;
   while (text[length] != '\0')
      length++;
   return length;
}

static inline int cstr_equal(const char* lhs, const char* rhs)
{
   size_t index = 0;
   while (lhs[index] != '\0' || rhs[index] != '\0')
   {
      if (lhs[index] != rhs[index])
         return 0;
      index++;
   }
   return 1;
}

static inline uint32_t checksum32(const void* memory, size_t extent, uint32_t checksum)
{
   const uint8_t* bytes = (const uint8_t*)memory;

   while (extent >= sizeof(checksum))
   {
      uint32_t word;
      mem_copy(&word, bytes, sizeof(word));
      checksum ^= word;
      bytes += sizeof(word);
      extent -= sizeof(word);
   }

   return ~checksum;
}

#endif
