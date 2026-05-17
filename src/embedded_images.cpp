#include "embedded_images.h"

extern "C" {
extern const unsigned char g_softloader_nvm[];
extern const unsigned char g_softloader_nvm_end[];
extern const unsigned char g_firmware_nvm[];
extern const unsigned char g_firmware_nvm_end[];
extern const unsigned char g_evse_pib[];
extern const unsigned char g_evse_pib_end[];
}

const EmbeddedImages& embedded_images(void)
{
   static const EmbeddedImages images = {
      { "softloader.nvm", g_softloader_nvm, (size_t)(g_softloader_nvm_end - g_softloader_nvm) },
      { "firmware.nvm", g_firmware_nvm, (size_t)(g_firmware_nvm_end - g_firmware_nvm) },
      { "evse.pib", g_evse_pib, (size_t)(g_evse_pib_end - g_evse_pib) },
   };
   return images;
}
