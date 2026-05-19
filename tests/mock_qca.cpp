/*
 * tests/mock_qca.cpp — Mock QCA7K SPI layer for unit tests.
 *
 * Simulates the QCA7K SPI hardware by:
 *   - Returning a VS_SW_VER.CNF with "BootLoader" on VS_SW_VER.REQ
 *   - Returning successful CNFs for VS_WRITE_AND_EXECUTE_APPLET.REQ
 *   - Returning successful CNFs for VS_MODULE_OPERATION.REQ
 */

#include "qca_spi.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* ── Mock state ─────────────────────────────────────────────────────────── */
static bool    s_runtime_mode        = false;
static bool    s_autoswitch_on_exec  = false; /* switch to runtime on W&E+EXECUTE */
static uint8_t s_pending_rx[1520];
static uint16_t s_pending_rx_len     = 0;
static uint32_t s_millis             = 0;

/* Version strings */
static const char k_bootloader_version[] = "BootLoader";
static const char k_runtime_version[]    = "QCA7420-0.1.2.3";

/* millis() stub */
uint32_t millis(void) { return s_millis; }

void mock_reset(void)
{
    s_runtime_mode       = false;
    s_autoswitch_on_exec = false;
    s_pending_rx_len     = 0;
    s_millis             = 0;
}

void mock_set_runtime_mode(bool on)
{
    s_runtime_mode = on;
}

/* When enabled, the mock switches to runtime mode the first time it
 * receives a VS_WRITE_AND_EXECUTE_APPLET.REQ with PLC_MODULE_EXECUTE set.
 * This simulates the firmware beginning to execute after upload. */
void mock_enable_autoswitch(void)
{
    s_autoswitch_on_exec = true;
}

/* ── Protocol constants (mirrors programmer.cpp) ────────────────────────── */
#define ETH_HLEN        14u
#define QCA_HDR_LEN      6u
#define MME_HDR_LEN     (ETH_HLEN + QCA_HDR_LEN)

static const uint8_t k_dst_mac[6] = {0x00, 0xB0, 0x52, 0x00, 0x00, 0x01};
static const uint8_t k_src_mac[6] = {0x00, 0xB0, 0x52, 0x00, 0x00, 0x00};

#define VS_SW_VER                    0xA000u
#define VS_HOST_ACTION               0xA060u
#define VS_WRITE_AND_EXECUTE_APPLET  0xA098u
#define VS_MODULE_OPERATION          0xA0B0u
#define MMTYPE_REQ  0x0000u
#define MMTYPE_CNF  0x0001u

#define PLC_MODULE_EXECUTE  (1u << 0)  /* FLAGS bit in W&E request payload */

static inline uint16_t rd16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline void wr16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
}
static inline void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);        p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);  p[3] = (uint8_t)(v >> 24);
}

/* Build a response frame in s_pending_rx */
static void build_response_header(uint8_t *buf, uint16_t mmtype)
{
    /* Swap src/dst so the response appears to come from the QCA device */
    memcpy(buf,     k_src_mac, 6);  /* ODA = host (reversed perspective) */
    memcpy(buf + 6, k_dst_mac, 6);  /* OSA = device */
    buf[12] = 0x88; buf[13] = 0xE1;
    buf[14] = 0x00;                  /* MMV */
    wr16le(buf + 15, mmtype);        /* MMTYPE LE */
    buf[17] = 0x00; buf[18] = 0xB0; buf[19] = 0x52; /* OUI */
}

static void queue_sw_ver_cnf(void)
{
    const char *ver = s_runtime_mode ? k_runtime_version : k_bootloader_version;
    uint8_t ver_len = (uint8_t)strlen(ver);
    uint8_t *buf    = s_pending_rx;

    build_response_header(buf, VS_SW_VER | MMTYPE_CNF);
    uint16_t pos = (uint16_t)MME_HDR_LEN;
    buf[pos++] = 0x00;                /* MSTATUS = success */
    buf[pos++] = 0x20;                /* MDEVICEID         */
    buf[pos++] = ver_len;             /* MVERLENGTH        */
    memcpy(buf + pos, ver, ver_len);  pos = (uint16_t)(pos + ver_len);

    /* Pad to 60 bytes minimum */
    if (pos < 60) { memset(buf + pos, 0, 60 - pos); pos = 60; }
    s_pending_rx_len = pos;
}

static void queue_write_execute_cnf(const uint8_t *req, uint16_t req_len)
{
    if (req_len < (uint16_t)(MME_HDR_LEN + 36u)) return;

    /* Auto-switch to runtime if the EXECUTE flag is set in FLAGS */
    if (s_autoswitch_on_exec) {
        /* FLAGS is at payload offset 8 (after CLIENT_SESSION_ID+SERVER_SESSION_ID) */
        uint32_t flags = (uint32_t)req[MME_HDR_LEN + 8]
                       | ((uint32_t)req[MME_HDR_LEN + 9]  << 8)
                       | ((uint32_t)req[MME_HDR_LEN + 10] << 16)
                       | ((uint32_t)req[MME_HDR_LEN + 11] << 24);
        if (flags & PLC_MODULE_EXECUTE)
            s_runtime_mode = true;
    }

    uint8_t *buf = s_pending_rx;
    build_response_header(buf, VS_WRITE_AND_EXECUTE_APPLET | MMTYPE_CNF);
    uint16_t pos = (uint16_t)MME_HDR_LEN;

    wr32le(buf + pos, 0); pos += 4;  /* MSTATUS = 0 */
    /* Copy CLIENT_SESSION_ID .. RESERVED2 (32 bytes) from request payload */
    const uint8_t *rp = req + MME_HDR_LEN;
    memcpy(buf + pos, rp, 32); pos = (uint16_t)(pos + 32u);
    wr32le(buf + pos, 0); pos += 4;  /* CURR_PART_ABSOLUTE_ADDR */
    wr32le(buf + pos, 0); pos += 4;  /* ABSOLUTE_START_ADDR */

    /* pos is now MME_HDR_LEN + 4 + 32 + 4 + 4 = 64, always > 60 */
    s_pending_rx_len = pos;
}

static void queue_module_op_cnf(void)
{
    uint8_t *buf = s_pending_rx;
    build_response_header(buf, VS_MODULE_OPERATION | MMTYPE_CNF);
    uint16_t pos = (uint16_t)MME_HDR_LEN;
    wr16le(buf + pos, 0); pos += 2;  /* MSTATUS = 0 */
    wr16le(buf + pos, 0); pos += 2;  /* ERR_REC_CODE */
    if (pos < 60) { memset(buf + pos, 0, 60 - pos); pos = 60; }
    s_pending_rx_len = pos;
}

/* ── QCA SPI stubs ──────────────────────────────────────────────────────── */

void qca_spi_init(void) {}

void qca_write_reg(uint16_t /*reg*/, uint16_t /*val*/) {}

uint16_t qca_read_reg(uint16_t reg)
{
    if (reg == SPI_REG_SIGNATURE) return QCASPI_GOOD_SIGNATURE;
    return 0;
}

int qca_send_frame(const uint8_t *eth_frame, uint16_t eth_len)
{
    if (eth_len < (uint16_t)MME_HDR_LEN) return 0;

    /* Identify MMTYPE */
    if (eth_frame[12] != 0x88 || eth_frame[13] != 0xE1) return 0;
    uint16_t mmtype = rd16le(eth_frame + 15);

    switch (mmtype) {
    case VS_SW_VER | MMTYPE_REQ:
        queue_sw_ver_cnf();
        break;
    case VS_WRITE_AND_EXECUTE_APPLET | MMTYPE_REQ:
        queue_write_execute_cnf(eth_frame, eth_len);
        break;
    case VS_MODULE_OPERATION | MMTYPE_REQ:
        queue_module_op_cnf();
        break;
    default:
        break;
    }
    return 0;
}

int qca_recv_frame(uint8_t *eth_frame, uint16_t *eth_len_out, uint16_t maxlen)
{
    if (s_pending_rx_len == 0) return 0;
    uint16_t len = s_pending_rx_len;
    if (len > maxlen) return -1;
    memcpy(eth_frame, s_pending_rx, len);
    *eth_len_out     = len;
    s_pending_rx_len = 0;
    return 1;
}

int qca_wait_ready(uint32_t /*timeout_ms*/) { return 1; }

void qca_reset(void) {}
