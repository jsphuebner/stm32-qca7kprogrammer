#pragma once
#include <stdint.h>

/* ── QCA7K SPI register definitions (from Linux qca_7k.h) ─────────────── */
#define QCA7K_SPI_READ      (1u << 15)
#define QCA7K_SPI_WRITE     (0u << 15)
#define QCA7K_SPI_INTERNAL  (1u << 14)
#define QCA7K_SPI_EXTERNAL  (0u << 14)

#define QCASPI_HW_BUF_LEN      0x0C5Bu
#define QCASPI_GOOD_SIGNATURE   0xAA55u
#define QCASPI_SLAVE_RESET_BIT  (1u << 6)

/* SPI internal registers */
#define SPI_REG_BFR_SIZE        0x0100u
#define SPI_REG_WRBUF_SPC_AVA   0x0200u
#define SPI_REG_RDBUF_BYTE_AVA  0x0300u
#define SPI_REG_SPI_CONFIG      0x0400u
#define SPI_REG_INTR_CAUSE      0x0C00u
#define SPI_REG_INTR_ENABLE     0x0D00u
#define SPI_REG_SIGNATURE       0x1A00u

/* SPI_REG_INTR_CAUSE / SPI_REG_INTR_ENABLE bit definitions (write-1-to-clear) */
#define SPI_INT_PKT_AVLBL       (1u << 2)  /* packet available in read buffer */

/* ── Public API ─────────────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

void     qca_spi_init(void);
void     qca_write_reg(uint16_t reg, uint16_t val);
uint16_t qca_read_reg(uint16_t reg);

/* Send raw Ethernet frame (eth_frame/eth_len = payload only, no SPI framing)
 * Returns 0 on success. */
int qca_send_frame(const uint8_t *eth_frame, uint16_t eth_len);

/* Receive one Ethernet frame into eth_frame (max maxlen bytes).
 * Returns 1 on success, 0 if no data available, -1 on error. */
int qca_recv_frame(uint8_t *eth_frame, uint16_t *eth_len_out, uint16_t maxlen);

/* Block until QCA signature register reads 0xAA55 or timeout_ms elapses.
 * Returns 1 on success, 0 on timeout. */
int qca_wait_ready(uint32_t timeout_ms);

/* Trigger a software reset of the QCA chip */
void qca_reset(void);

#ifdef __cplusplus
}
#endif
