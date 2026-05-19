#include "qca_spi.h"
#include "hwinit.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

/* ── CS helpers ────────────────────────────────────────────────────────── */
#define CS_LOW()  gpio_clear(GPIOA, GPIO4)
#define CS_HIGH() gpio_set(GPIOA, GPIO4)

/* ── Low-level SPI helpers ─────────────────────────────────────────────── */
static void spi_send_u16be(uint16_t val)
{
    spi_xfer(SPI1, (uint8_t)(val >> 8));
    spi_xfer(SPI1, (uint8_t)(val & 0xFF));
}

static uint16_t spi_recv_u16be(void)
{
    uint16_t hi = spi_xfer(SPI1, 0xFF);
    uint16_t lo = spi_xfer(SPI1, 0xFF);
    return (uint16_t)((hi << 8) | lo);
}

/* Receive buffer: sized for the maximum QCA7K hardware buffer */
static uint8_t recv_buf[QCASPI_HW_BUF_LEN + 16u];

/* ── Public API ─────────────────────────────────────────────────────────── */

void qca_spi_init(void)
{
    /* Hardware already configured in hw_init() */
}

void qca_write_reg(uint16_t reg, uint16_t val)
{
    CS_LOW();
    spi_send_u16be((uint16_t)(QCA7K_SPI_WRITE | QCA7K_SPI_INTERNAL | reg));
    spi_send_u16be(val);
    CS_HIGH();
}

uint16_t qca_read_reg(uint16_t reg)
{
    CS_LOW();
    spi_send_u16be((uint16_t)(QCA7K_SPI_READ | QCA7K_SPI_INTERNAL | reg));
    uint16_t val = spi_recv_u16be();
    CS_HIGH();
    return val;
}

int qca_send_frame(const uint8_t *eth_frame, uint16_t eth_len)
{
    /* framed = 4-byte AA preamble + 2-byte LE length + 2-byte reserved
     *         + eth_frame + 2-byte 0x55 trailer  =  eth_len + 10 */
    uint16_t framed_len = (uint16_t)(eth_len + 10u);

    qca_write_reg(SPI_REG_BFR_SIZE, framed_len);

    CS_LOW();
    spi_send_u16be(0x0000u); /* SPI_WRITE | SPI_EXTERNAL */

    spi_xfer(SPI1, 0xAA); spi_xfer(SPI1, 0xAA);
    spi_xfer(SPI1, 0xAA); spi_xfer(SPI1, 0xAA);

    spi_xfer(SPI1, (uint8_t)(eth_len & 0xFF));
    spi_xfer(SPI1, (uint8_t)(eth_len >> 8));

    spi_xfer(SPI1, 0x00); spi_xfer(SPI1, 0x00);

    for (uint16_t i = 0; i < eth_len; i++)
        spi_xfer(SPI1, eth_frame[i]);

    spi_xfer(SPI1, 0x55); spi_xfer(SPI1, 0x55);

    CS_HIGH();
    return 0;
}

int qca_recv_frame(uint8_t *eth_frame, uint16_t *eth_len_out, uint16_t maxlen)
{
    uint16_t available = qca_read_reg(SPI_REG_RDBUF_BYTE_AVA);
    if (available == 0) return 0;

    if (available > (uint16_t)sizeof(recv_buf))
        available = (uint16_t)sizeof(recv_buf);

    qca_write_reg(SPI_REG_BFR_SIZE, available);

    CS_LOW();
    spi_send_u16be(0x8000u); /* SPI_READ | SPI_EXTERNAL */
    for (uint16_t i = 0; i < available; i++)
        recv_buf[i] = (uint8_t)spi_xfer(SPI1, 0xFF);
    CS_HIGH();

    /* Minimum: 4 (HW_LEN) + 4 (AA) + 2 (len) + 2 (rsvd) + 2 (trailer) = 14 */
    if (available < 14u) return -1;

    if (recv_buf[4] != 0xAA || recv_buf[5] != 0xAA ||
        recv_buf[6] != 0xAA || recv_buf[7] != 0xAA) return -1;

    uint16_t frame_len = (uint16_t)(recv_buf[8] | ((uint16_t)recv_buf[9] << 8));
    if (frame_len > maxlen) return -1;
    if ((uint16_t)(12u + frame_len + 2u) > available) return -1;

    for (uint16_t i = 0; i < frame_len; i++)
        eth_frame[i] = recv_buf[12u + i];

    *eth_len_out = frame_len;
    return 1;
}

int qca_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = millis();
    do {
        if (qca_read_reg(SPI_REG_SIGNATURE) == QCASPI_GOOD_SIGNATURE)
            return 1;
    } while ((millis() - start) < timeout_ms);
    return 0;
}

void qca_reset(void)
{
    uint16_t cfg = qca_read_reg(SPI_REG_SPI_CONFIG);
    qca_write_reg(SPI_REG_SPI_CONFIG,
                  (uint16_t)(cfg | (uint16_t)QCASPI_SLAVE_RESET_BIT));
}
