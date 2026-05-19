#pragma once
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    uint32_t       size;
} embedded_image_t;

typedef struct {
    embedded_image_t softloader;
    embedded_image_t firmware;
    embedded_image_t pib;
} embedded_images_t;

/* Runtime accessor — avoids zero-size initialisation issues on bare-metal */
const embedded_images_t *embedded_images(void);
