#pragma once
#include <stdint.h>
#include "nvm.h"
#include "embedded_images.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CRC32 (IEEE 802.3 polynomial 0xEDB88320) */
uint32_t compute_crc32(const uint8_t *data, uint32_t len);

/* Find an image of the given type in an NVM file.
 * Sets *hdr_out and *data_out (pointer to image data, just after the header).
 * Returns true if found. */
bool nvm_find_image(const uint8_t *file, uint32_t file_size,
                    uint32_t image_type,
                    const nvm_header2_t **hdr_out,
                    const uint8_t **data_out);

/* Wait for QCA bootloader to announce itself via VS_SW_VER.
 * Also acks any VS_HOST_ACTION.IND received during polling.
 * Returns true if BootLoader is detected within timeout_ms. */
bool programmer_wait_for_bootloader(uint32_t timeout_ms);

/* Upload one NVM image via VS_WRITE_AND_EXECUTE_APPLET.
 * execute: set PLC_MODULE_EXECUTE on last chunk when EntryPoint is valid.
 * is_pib:  use ALLOWED_MEM_TYPES[0]=0 (PIB memory area). */
bool programmer_write_execute(const uint8_t *image_data,
                               const nvm_header2_t *hdr,
                               bool execute, bool is_pib);

/* Find image_type in nvm_file and upload it via VS_WRITE_AND_EXECUTE_APPLET. */
bool programmer_find_and_upload(const uint8_t *nvm_file, uint32_t file_size,
                                 uint32_t image_type,
                                 bool execute, bool is_pib);

/* Write all chunks of file_data via VS_MODULE_OPERATION WRITE_MODULE. */
bool programmer_flash_module(const uint8_t *file_data, uint32_t file_size,
                              uint16_t module_id, uint32_t session_id,
                              uint8_t module_idx,
                              uint8_t num_modules_in_session);

/* Run the full plcboot sequence using the embedded images. */
bool programmer_run(const embedded_images_t *images);

#ifdef __cplusplus
}
#endif
