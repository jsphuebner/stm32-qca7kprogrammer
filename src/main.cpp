#include "embedded_images.h"
#include "programmer.h"
#include "qca7005_transport.h"

#ifndef HOST_BUILD
extern "C" int main(void)
{
   Qca7005Transport transport;
   qca7005_setup(&transport);

   if (qca7005_read_signature(&transport) == 0xAA55u)
      (void)run_programmer(g_embedded_images, qca7005_make_transport(&transport));

   while (1)
      ;

   return 0;
}
#endif
