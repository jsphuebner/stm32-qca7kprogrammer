#include "hwinit.h"
#include "programmer.h"
#include "embedded_images.h"
#include <stdint.h>

int main(void)
{
    hw_init();

    /* Show we have started */
    led_set(LED_ALIVE, true);

    const embedded_images_t *imgs = embedded_images();

    if (programmer_run(imgs)) {
        led_set(LED_STATEC, true);
    } else {
        led_set(LED_CONTACT, true);
    }

    /* Blink alive LED at 1 Hz */
    uint32_t last_toggle = millis();
    bool     alive_state = true;
    while (1) {
        uint32_t now = millis();
        if (now - last_toggle >= 500u) {
            alive_state = !alive_state;
            led_set(LED_ALIVE, alive_state);
            last_toggle = now;
        }
    }
    return 0;
}
