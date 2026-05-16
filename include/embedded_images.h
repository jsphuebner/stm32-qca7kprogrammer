#ifndef STM32_QCA7KPROGRAMMER_EMBEDDED_IMAGES_H
#define STM32_QCA7KPROGRAMMER_EMBEDDED_IMAGES_H

#include <stddef.h>
#include <stdint.h>

struct EmbeddedImage
{
   const char* name;
   const uint8_t* data;
   size_t size;
};

struct EmbeddedImages
{
   EmbeddedImage softloader;
   EmbeddedImage firmware;
   EmbeddedImage pib;
};

extern const EmbeddedImages g_embedded_images;

#endif
