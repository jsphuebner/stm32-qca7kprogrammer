#include "embedded_images.h"
#include "hwinit.h"
#include "programmer.h"
#include "qca7005_transport.h"

#ifndef HOST_BUILD
extern "C" int main(void)
{
   Qca7005Transport transport;
   qca7005_setup(&transport);

   ProgrammerResult result = PROGRAMMER_TRANSPORT_ERROR;
   if (qca7005_read_signature(&transport) == 0xAA55u)
      result = run_programmer(g_embedded_images, qca7005_make_transport(&transport));

   while (1)
   {
      if (result == PROGRAMMER_OK)
      {
         led_set_all(true);
         delay_ms(1000u);
         led_set_all(false);
         delay_ms(1000u);
         continue;
      }

      const uint32_t blink_count = (uint32_t)result + 1u;
      for (uint32_t blink = 0; blink < blink_count; blink++)
      {
         led_set_all(true);
         delay_ms(180u);
         led_set_all(false);
         delay_ms(220u);
      }
      delay_ms(1000u);
   }

   return 0;
}
#endif
