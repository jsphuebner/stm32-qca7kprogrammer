#include "embedded_images.h"

extern "C" {
extern const unsigned char g_softloader_nvm[];
extern const unsigned int g_softloader_nvm_len;
extern const unsigned char g_firmware_nvm[];
extern const unsigned int g_firmware_nvm_len;
extern const unsigned char g_evse_pib[];
extern const unsigned int g_evse_pib_len;
}

const EmbeddedImages g_embedded_images = {
   { "softloader.nvm", g_softloader_nvm, g_softloader_nvm_len },
   { "firmware.nvm", g_firmware_nvm, g_firmware_nvm_len },
   { "evse.pib", g_evse_pib, g_evse_pib_len },
};
