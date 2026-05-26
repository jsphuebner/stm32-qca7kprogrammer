/*
 * programmer.cpp — Qualcomm QCA7K NVM programming logic.
 *
 * Implements the full plcboot protocol:
 *   1. Wait for BootLoader (VS_SW_VER)
 *   2. Upload MemCtl applet  (VS_WRITE_AND_EXECUTE_APPLET)
 *   3. Upload PIB            (VS_WRITE_AND_EXECUTE_APPLET, no execute)
 *   4. Upload Firmware       (VS_WRITE_AND_EXECUTE_APPLET, execute)
 *   5. Wait for runtime      (VS_SW_VER, non-BootLoader)
 *   6. Flash softloader      (VS_MODULE_OPERATION)
 *   7. Flash firmware + PIB  (VS_MODULE_OPERATION)
 *
 * Compilable both for STM32 firmware (-DSTM32F1) and native unit tests
 * (-DUNIT_TEST).
 */

#include "programmer.h"
#include "qca_spi.h"
#include "debug.h"

#ifndef UNIT_TEST
#include "hwinit.h"
#else
#include <stdint.h>
extern uint32_t millis(void);
#endif

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Protocol constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define ETH_HLEN        14u
#define QCA_HDR_LEN      6u
#define MME_HDR_LEN     (ETH_HLEN + QCA_HDR_LEN)   /* 20 bytes */

#define ETH_TYPE_MME    0x88E1u
#define MME_MMV         0x00u
#define MME_OUI_0       0x00u
#define MME_OUI_1       0xB0u
#define MME_OUI_2       0x52u

/* MME base codes */
#define VS_SW_VER                    0xA000u
#define VS_HOST_ACTION               0xA060u
#define VS_WRITE_AND_EXECUTE_APPLET  0xA098u
#define VS_MODULE_OPERATION          0xA0B0u

/* MMTYPE suffixes */
#define MMTYPE_REQ  0x0000u
#define MMTYPE_CNF  0x0001u
#define MMTYPE_IND  0x0002u
#define MMTYPE_RSP  0x0003u

/* VS_WRITE_AND_EXECUTE_APPLET flags */
#define PLC_MODULE_EXECUTE   (1u << 0)
#define PLC_MODULE_ABSOLUTE  (1u << 1)

/* Chunk size per VS_WRITE_AND_EXECUTE_APPLET / VS_MODULE_OPERATION packet.
 * 1400 bytes leaves room for the 68-byte MME header within a 1468-byte frame,
 * safely below the 1500-byte Ethernet MTU. (open-plc-utils PLC_MODULE_SIZE) */
#define PLC_MODULE_SIZE     1400u

/* VS_MODULE_OPERATION opcodes */
#define MOD_OP_START_SESSION  0x0010u
#define MOD_OP_WRITE_MODULE   0x0011u
#define MOD_OP_CLOSE_SESSION  0x0012u

/* Module IDs */
#define MODULE_ID_FW          0x7001u
#define MODULE_ID_PIB         0x7002u
#define MODULE_ID_SOFTLOADER  0x7003u

/* CLOSE_SESSION commit codes.
 * PLC_COMMIT_FACTPIB (bit 31) is only valid when closing a PIB module
 * session; sending it for non-PIB modules causes error 0x30.
 * PLC_COMMIT_NORESET prevents an intermediate device reset so that the
 * next module session can follow immediately without re-negotiation. */
#define PLC_COMMIT_FORCE      (1u << 0)
#define PLC_COMMIT_NORESET    (1u << 1)
#define PLC_COMMIT_FACTPIB    (1u << 31)
/* Softloader: no FACTPIB, keep NORESET so chip stays up for firmware+PIB */
#define COMMIT_CODE_SOFTLOADER (PLC_COMMIT_FORCE | PLC_COMMIT_NORESET)
/* Firmware+PIB: just FORCE — allow chip to reset and boot new firmware */
#define COMMIT_CODE_FW_PIB     (PLC_COMMIT_FORCE)

/* Session cookie */
#define SESSION_ID  0x78563412u

static const uint8_t k_dst_mac[6] = {0x00, 0xB0, 0x52, 0x00, 0x00, 0x01};
static const uint8_t k_src_mac[6] = {0x00, 0xB0, 0x52, 0x00, 0x00, 0x00};

/* Frame buffers — static to avoid large stack usage on Cortex-M3 */
static uint8_t s_send[1520];
static uint8_t s_recv[1520];

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t rd16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0]          | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)  | ((uint32_t)p[3] << 24);
}
static inline void wr16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}
static inline void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* ── CRC32 (IEEE 802.3 polynomial 0xEDB88320) ───────────────────────────── */
uint32_t compute_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u) crc = (crc >> 1) ^ 0xEDB88320u;
            else          crc >>= 1;
        }
    }
    return ~crc;
}

/* ── Module checksum: one's complement of XOR of all 32-bit LE words,
 *    matching open-plc-utils checksum32(data, len, 0) / fdchecksum32() ─── */
static uint32_t compute_module_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t chk = 0;
    while (len >= 4u) {
        chk ^= rd32le(data);
        data += 4;
        len  -= 4u;
    }
    if (len > 0u) {
        uint8_t tmp[4] = {0, 0, 0, 0};
        for (uint32_t i = 0; i < len; i++) tmp[i] = data[i];
        chk ^= rd32le(tmp);
    }
    return ~chk;
}

/* ── NVM traversal ──────────────────────────────────────────────────────── */
bool nvm_find_image(const uint8_t *file, uint32_t file_size,
                    uint32_t image_type,
                    const nvm_header2_t **hdr_out,
                    const uint8_t **data_out)
{
    if (hdr_out)  *hdr_out  = NULL;
    if (data_out) *data_out = NULL;

    uint32_t offset = 0;
    while (offset + (uint32_t)sizeof(nvm_header2_t) <= file_size) {
        const nvm_header2_t *hdr =
            reinterpret_cast<const nvm_header2_t *>(file + offset);

        if (hdr->MajorVersion != 1 || hdr->MinorVersion != 1)
            break;

        if (hdr->ImageType == image_type) {
            if (hdr_out)  *hdr_out  = hdr;
            if (data_out) *data_out = file + offset + (uint32_t)sizeof(nvm_header2_t);
            return true;
        }

        if (hdr->NextHeader == NVM_NO_NEXT_HEADER) break;
        offset = hdr->NextHeader;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame building / parsing helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Build 20-byte Ethernet + Qualcomm MME header into buf, return 20 */
static uint16_t build_mme_header(uint8_t *buf, uint16_t mmtype)
{
    memcpy(buf,     k_dst_mac, 6);
    memcpy(buf + 6, k_src_mac, 6);
    buf[12] = 0x88; buf[13] = 0xE1;       /* EtherType BE           */
    buf[14] = MME_MMV;
    buf[15] = (uint8_t)(mmtype & 0xFF);   /* MMTYPE LE              */
    buf[16] = (uint8_t)(mmtype >> 8);
    buf[17] = MME_OUI_0;
    buf[18] = MME_OUI_1;
    buf[19] = MME_OUI_2;
    return (uint16_t)MME_HDR_LEN;
}

/* Extract MMTYPE from a received frame; returns 0 on malformed frame */
static uint16_t frame_mmtype(const uint8_t *frame, uint16_t len)
{
    if (len < MME_HDR_LEN) return 0;
    if (frame[12] != 0x88 || frame[13] != 0xE1) return 0;
    if (frame[17] != MME_OUI_0 || frame[18] != MME_OUI_1 ||
        frame[19] != MME_OUI_2) return 0;
    return rd16le(frame + 15);
}

/* Send the frame in s_send of length len, padding to 60-byte minimum */
static int send_frame(uint16_t len)
{
    if (len < 60u) {
        memset(s_send + len, 0, 60u - len);
        len = 60;
    }
    return qca_send_frame(s_send, len);
}

/* Poll for a received frame, with up to timeout_ms wait */
static int recv_frame_timeout(uint32_t timeout_ms)
{
    uint16_t eth_len = 0;
    uint32_t start = millis();
    do {
        int r = qca_recv_frame(s_recv, &eth_len, (uint16_t)sizeof(s_recv));
        if (r == 1) return (int)eth_len;
        if (r < 0)  return r;
    } while ((millis() - start) < timeout_ms);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VS_SW_VER
 * ═══════════════════════════════════════════════════════════════════════════ */

static void send_sw_ver_req(void)
{
    uint16_t pos = build_mme_header(s_send, VS_SW_VER | MMTYPE_REQ);
    send_frame(pos);
}

/* Returns true and sets *is_bootloader if frame is a valid VS_SW_VER.CNF */
static bool parse_sw_ver_cnf(const uint8_t *frame, uint16_t len,
                              bool *is_bootloader)
{
    if (len < (uint16_t)(MME_HDR_LEN + 3u)) return false;
    if (frame[MME_HDR_LEN] != 0) return false; /* MSTATUS != success */
    uint8_t ver_len = frame[MME_HDR_LEN + 2u];
    if (len < (uint16_t)(MME_HDR_LEN + 3u + ver_len)) return false;
    const char *ver = reinterpret_cast<const char *>(frame + MME_HDR_LEN + 3u);
    *is_bootloader = (ver_len >= 10u &&
                      memcmp(ver, "BootLoader", 10) == 0);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VS_HOST_ACTION
 * ═══════════════════════════════════════════════════════════════════════════ */

static void send_host_action_rsp(void)
{
    uint16_t pos = build_mme_header(s_send, VS_HOST_ACTION | MMTYPE_RSP);
    s_send[pos++] = 0x00; /* MSTATUS = acknowledged */
    send_frame(pos);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * programmer_wait_for_bootloader
 * ═══════════════════════════════════════════════════════════════════════════ */

bool programmer_wait_for_bootloader(uint32_t timeout_ms)
{
    uint32_t overall_start = millis();
    do {
        send_sw_ver_req();

        /* Allow up to 500 ms for a response to arrive */
        uint32_t resp_start = millis();
        do {
            int rlen = recv_frame_timeout(10);
            if (rlen <= 0) continue;

            uint16_t mmtype = frame_mmtype(s_recv, (uint16_t)rlen);

            if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
                send_host_action_rsp();
            } else if (mmtype == (VS_SW_VER | MMTYPE_CNF)) {
                bool bl = false;
                if (parse_sw_ver_cnf(s_recv, (uint16_t)rlen, &bl) && bl) {
                    debug_printf("BootLoader detected\n");
                    return true;
                }
            }
        } while ((millis() - resp_start) < 500u);

    } while ((millis() - overall_start) < timeout_ms);

    debug_printf("Timeout waiting for BootLoader\n");
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Wait for runtime (VS_SW_VER with non-BootLoader version string)
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool programmer_wait_for_runtime(uint32_t timeout_ms)
{
    uint32_t overall_start = millis();
    do {
        send_sw_ver_req();

        uint32_t resp_start = millis();
        do {
            int rlen = recv_frame_timeout(10);
            if (rlen <= 0) continue;

            uint16_t mmtype = frame_mmtype(s_recv, (uint16_t)rlen);
            if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
                send_host_action_rsp();
            } else if (mmtype == (VS_SW_VER | MMTYPE_CNF)) {
                bool bl = false;
                if (parse_sw_ver_cnf(s_recv, (uint16_t)rlen, &bl) && !bl) {
                    debug_printf("Runtime firmware detected\n");
                    return true;
                }
            }
        } while ((millis() - resp_start) < 500u);

    } while ((millis() - overall_start) < timeout_ms);

    debug_printf("Timeout waiting for runtime\n");
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VS_WRITE_AND_EXECUTE_APPLET
 * ═══════════════════════════════════════════════════════════════════════════ */

bool programmer_write_execute(const uint8_t *image_data,
                               const nvm_header2_t *hdr,
                               bool execute, bool is_pib)
{
    const uint32_t total_len       = hdr->ImageLength;
    const uint32_t entry           = hdr->EntryPoint;
    const uint32_t image_addr      = hdr->ImageAddress;
    const uint32_t image_checksum  = hdr->ImageChecksum;

    uint32_t byte_offset = 0;
    while (byte_offset < total_len) {
        uint32_t chunk = total_len - byte_offset;
        if (chunk > PLC_MODULE_SIZE) chunk = PLC_MODULE_SIZE;

        bool     is_last = (byte_offset + chunk >= total_len);
        uint32_t flags   = PLC_MODULE_ABSOLUTE;
        if (is_last && execute && entry != NVM_NO_NEXT_HEADER)
            flags |= PLC_MODULE_EXECUTE;

        uint32_t curr_offset = image_addr + byte_offset;

        /* ── Build request ──────────────────────────────────────────── */
        uint16_t pos = build_mme_header(s_send,
                                        VS_WRITE_AND_EXECUTE_APPLET | MMTYPE_REQ);

        wr32le(s_send + pos, SESSION_ID);       pos += 4; /* CLIENT_SESSION_ID */
        wr32le(s_send + pos, 0);                pos += 4; /* SERVER_SESSION_ID */
        wr32le(s_send + pos, flags);            pos += 4; /* FLAGS             */

        /* ALLOWED_MEM_TYPES[8]: [0]=1 → SDRAM, [0]=0 → PIB area */
        memset(s_send + pos, 0, 8);
        if (!is_pib) s_send[pos] = 1;
        pos += 8;

        wr32le(s_send + pos, total_len);        pos += 4; /* TOTAL_LENGTH      */
        wr32le(s_send + pos, chunk);            pos += 4; /* CURR_PART_LENGTH  */
        wr32le(s_send + pos, curr_offset);      pos += 4; /* CURR_PART_OFFSET  */
        wr32le(s_send + pos, entry);            pos += 4; /* START_ADDR        */
        wr32le(s_send + pos, image_checksum);   pos += 4; /* CHECKSUM          */
        memset(s_send + pos, 0, 8);             pos += 8; /* RESERVED2         */

        memcpy(s_send + pos, image_data + byte_offset, chunk);
        pos = (uint16_t)(pos + chunk);

        if (qca_send_frame(s_send, pos) != 0) return false;

        /* ── Wait for confirm ───────────────────────────────────────── */
        bool     got_cnf = false;
        uint32_t t       = millis();
        while ((millis() - t) < 2000u) {
            uint16_t rlen = 0;
            if (qca_recv_frame(s_recv, &rlen, (uint16_t)sizeof(s_recv)) != 1)
                continue;

            uint16_t mmtype = frame_mmtype(s_recv, rlen);
            if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
                send_host_action_rsp();
                continue;
            }
            if (mmtype == (VS_WRITE_AND_EXECUTE_APPLET | MMTYPE_CNF)) {
                if (rlen < (uint16_t)(MME_HDR_LEN + 36u)) return false;
                uint32_t mstatus = rd32le(s_recv + MME_HDR_LEN);
                if (mstatus != 0) {
                    debug_printf("W&E CNF error %x\n", (unsigned)mstatus);
                    return false;
                }
                /* CURR_PART_LENGTH at payload offset 28, CURR_PART_OFFSET at 32 */
                uint32_t cnf_len = rd32le(s_recv + MME_HDR_LEN + 28u);
                uint32_t cnf_off = rd32le(s_recv + MME_HDR_LEN + 32u);
                if (cnf_len != chunk || cnf_off != curr_offset) {
                    debug_printf("W&E CNF mismatch\n");
                    return false;
                }
                got_cnf = true;
                break;
            }
        }
        if (!got_cnf) {
            debug_printf("W&E timeout offset=%x\n", (unsigned)byte_offset);
            return false;
        }

        byte_offset += chunk;
    }
    return true;
}

bool programmer_find_and_upload(const uint8_t *nvm_file, uint32_t file_size,
                                 uint32_t image_type,
                                 bool execute, bool is_pib)
{
    const nvm_header2_t *hdr  = NULL;
    const uint8_t       *data = NULL;

    if (!nvm_find_image(nvm_file, file_size, image_type, &hdr, &data)) {
        debug_printf("Image type %x not found\n", (unsigned)image_type);
        return false;
    }
    debug_printf("Uploading image type %x len=%u\n",
                 (unsigned)image_type, (unsigned)hdr->ImageLength);
    return programmer_write_execute(data, hdr, execute, is_pib);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VS_MODULE_OPERATION helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint16_t module_id;
    uint16_t module_sub_id;
    uint32_t module_length;
    uint32_t module_chksum;
} module_spec_t;

static bool mod_start_session(uint32_t session_id,
                               const module_spec_t *modules, uint8_t num_modules)
{
    /* MOD_OP_DATA_LEN uses INCLUSIVE convention (counts from MOD_OP itself),
     * matching open-plc-utils: MOD_OP(2)+MOD_OP_DATA_LEN(2)+MOD_OP_RSVD(4)+
     * SESSION_ID(4)+NUM_MODULES(1)+N×12 = 13 + N×12 */
    uint16_t mod_op_data_len = (uint16_t)(13u + (uint16_t)num_modules * 12u);

    uint32_t overall_start = millis();
    do {
        /* (Re)build and send the request on every attempt */
        uint16_t pos = build_mme_header(s_send, VS_MODULE_OPERATION | MMTYPE_REQ);

        memset(s_send + pos, 0, 4);  pos += 4; /* RESERVED1        */
        s_send[pos++] = 1;                      /* NUM_OP_DATA      */
        wr16le(s_send + pos, MOD_OP_START_SESSION); pos += 2;
        wr16le(s_send + pos, mod_op_data_len);  pos += 2;
        memset(s_send + pos, 0, 4);  pos += 4; /* MOD_OP_RSVD      */
        wr32le(s_send + pos, session_id);       pos += 4;
        s_send[pos++] = num_modules;

        for (uint8_t i = 0; i < num_modules; i++) {
            wr16le(s_send + pos, modules[i].module_id);       pos += 2;
            wr16le(s_send + pos, modules[i].module_sub_id);   pos += 2;
            wr32le(s_send + pos, modules[i].module_length);   pos += 4;
            wr32le(s_send + pos, modules[i].module_chksum);   pos += 4;
        }

        if (send_frame(pos) != 0) return false;

        uint32_t resp_start = millis();
        do {
            uint16_t rlen = 0;
            if (qca_recv_frame(s_recv, &rlen, (uint16_t)sizeof(s_recv)) != 1)
                continue;
            uint16_t mmtype = frame_mmtype(s_recv, rlen);
            if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
                send_host_action_rsp();
                break; /* break inner loop: outer loop will resend the request */
            }
            if (mmtype == (VS_MODULE_OPERATION | MMTYPE_CNF)) {
                if (rlen < (uint16_t)(MME_HDR_LEN + 2u)) return false;
                uint16_t status = rd16le(s_recv + MME_HDR_LEN);
                if (status != 0) {
                    debug_printf("START_SESSION err %x\n", (unsigned)status);
                    return false;
                }
                return true;
            }
        } while ((millis() - resp_start) < 500u);

    } while ((millis() - overall_start) < 5000u);

    debug_printf("START_SESSION timeout\n");
    return false;
}

static bool mod_close_session(uint32_t session_id, uint32_t commit_code)
{
    /* MOD_OP_DATA_LEN uses INCLUSIVE convention (counts from MOD_OP itself),
     * matching open-plc-utils: MOD_OP(2)+MOD_OP_DATA_LEN(2)+MOD_OP_RSVD(4)+
     * SESSION_ID(4)+COMMIT_CODE(4)+RSVD(20) = 36 */
    uint16_t pos = build_mme_header(s_send, VS_MODULE_OPERATION | MMTYPE_REQ);

    memset(s_send + pos, 0, 4);  pos += 4; /* RESERVED         */
    s_send[pos++] = 1;                      /* NUM_OP_DATA      */
    wr16le(s_send + pos, MOD_OP_CLOSE_SESSION); pos += 2;
    wr16le(s_send + pos, 36);               pos += 2; /* MOD_OP_DATA_LEN */
    memset(s_send + pos, 0, 4);  pos += 4; /* MOD_OP_RSVD      */
    wr32le(s_send + pos, session_id);       pos += 4;
    wr32le(s_send + pos, commit_code);      pos += 4;
    memset(s_send + pos, 0, 20); pos += 20; /* RSVD[20]        */

    if (send_frame(pos) != 0) return false;

    uint32_t t = millis();
    while ((millis() - t) < 2000u) {
        uint16_t rlen = 0;
        if (qca_recv_frame(s_recv, &rlen, (uint16_t)sizeof(s_recv)) != 1)
            continue;
        uint16_t mmtype = frame_mmtype(s_recv, rlen);
        if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
            send_host_action_rsp();
            continue;
        }
        if (mmtype == (VS_MODULE_OPERATION | MMTYPE_CNF)) {
            if (rlen < (uint16_t)(MME_HDR_LEN + 2u)) return false;
            uint16_t status = rd16le(s_recv + MME_HDR_LEN);
            if (status != 0) {
                debug_printf("CLOSE_SESSION err %x\n", (unsigned)status);
                return false;
            }
            return true;
        }
    }
    debug_printf("CLOSE_SESSION timeout\n");
    return false;
}

bool programmer_flash_module(const uint8_t *file_data, uint32_t file_size,
                              uint16_t module_id, uint32_t session_id,
                              uint8_t module_idx,
                              uint8_t num_modules_in_session)
{
    (void)num_modules_in_session;

    uint32_t byte_offset = 0;
    while (byte_offset < file_size) {
        uint32_t chunk = file_size - byte_offset;
        if (chunk > PLC_MODULE_SIZE) chunk = PLC_MODULE_SIZE;

        /* MOD_OP_DATA_LEN uses INCLUSIVE convention (counts from MOD_OP itself).
         * open-plc-utils always uses sizeof(MODULE_SPEC)+sizeof(MODULE_DATA) =
         * MOD_OP(2)+MOD_OP_DATA_LEN(2)+MOD_OP_RSVD(4)+SESSION_ID(4)+
         * MODULE_IDX(1)+MODULE_ID(2)+SUB_ID(2)+LENGTH(2)+OFFSET(4)+
         * DATA(1400) = 23 + PLC_MODULE_SIZE, even for the last partial chunk.
         * MODULE_LENGTH tells the device how many bytes in DATA are valid. */
        uint16_t mod_op_data_len = (uint16_t)(23u + PLC_MODULE_SIZE);

        uint16_t pos = build_mme_header(s_send, VS_MODULE_OPERATION | MMTYPE_REQ);

        memset(s_send + pos, 0, 4);  pos += 4;  /* RESERVED            */
        s_send[pos++] = 1;                        /* NUM_OP_DATA         */
        wr16le(s_send + pos, MOD_OP_WRITE_MODULE); pos += 2;
        wr16le(s_send + pos, mod_op_data_len);     pos += 2;
        memset(s_send + pos, 0, 4);  pos += 4;  /* MOD_OP_RSVD         */
        wr32le(s_send + pos, session_id);          pos += 4;
        s_send[pos++] = module_idx;               /* MODULE_IDX          */
        wr16le(s_send + pos, module_id);           pos += 2;
        wr16le(s_send + pos, 0);                   pos += 2; /* SUB_ID  */
        wr16le(s_send + pos, (uint16_t)chunk);     pos += 2; /* LENGTH  */
        wr32le(s_send + pos, byte_offset);         pos += 4; /* OFFSET  */

        memcpy(s_send + pos, file_data + byte_offset, chunk);
        if (chunk < PLC_MODULE_SIZE) {
            memset(s_send + pos + chunk, 0, PLC_MODULE_SIZE - chunk);
        }
        pos = (uint16_t)(pos + PLC_MODULE_SIZE);

        if (qca_send_frame(s_send, pos) != 0) return false;

        bool     got_cnf = false;
        uint32_t t       = millis();
        while ((millis() - t) < 2000u) {
            uint16_t rlen = 0;
            if (qca_recv_frame(s_recv, &rlen, (uint16_t)sizeof(s_recv)) != 1)
                continue;
            uint16_t mmtype = frame_mmtype(s_recv, rlen);
            if (mmtype == (VS_HOST_ACTION | MMTYPE_IND)) {
                send_host_action_rsp();
                continue;
            }
            if (mmtype == (VS_MODULE_OPERATION | MMTYPE_CNF)) {
                if (rlen < (uint16_t)(MME_HDR_LEN + 2u)) return false;
                uint16_t status = rd16le(s_recv + MME_HDR_LEN);
                if (status != 0) {
                    debug_printf("WRITE_MODULE err %x\n", (unsigned)status);
                    return false;
                }
                got_cnf = true;
                break;
            }
        }
        if (!got_cnf) {
            debug_printf("WRITE_MODULE timeout off=%u\n", (unsigned)byte_offset);
            return false;
        }

        byte_offset += chunk;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * programmer_run — full plcboot sequence
 * ═══════════════════════════════════════════════════════════════════════════ */

bool programmer_run(const embedded_images_t *images)
{
    /* ── 1. Wait for BootLoader ─────────────────────────────────────────── */
    debug_printf("Step 1: Wait for BootLoader\n");
    if (!programmer_wait_for_bootloader(10000u)) return false;

    /* ── 2. InitDevice: upload MemCtl applet ────────────────────────────── */
    debug_printf("Step 2: Upload MemCtl\n");
    if (!programmer_find_and_upload(images->firmware.data, images->firmware.size,
                                    NVM_IMAGE_MEMCTL, true, false)) return false;

    /* ── 3. BootParameters: upload PIB ─────────────────────────────────── */
    debug_printf("Step 3: Upload PIB\n");
    if (!programmer_find_and_upload(images->pib.data, images->pib.size,
                                    NVM_IMAGE_PIB, false, true)) return false;

    /* ── 4. BootFirmware: upload main firmware ──────────────────────────── */
    debug_printf("Step 4: Upload Firmware\n");
    if (!programmer_find_and_upload(images->firmware.data, images->firmware.size,
                                    NVM_IMAGE_FIRMWARE, true, false)) return false;

    /* ── 5. Wait for runtime ────────────────────────────────────────────── */
    debug_printf("Step 5: Wait for runtime\n");
    if (!programmer_wait_for_runtime(30000u)) return false;

    /* ── 6. Flash softloader via VS_MODULE_OPERATION ────────────────────── */
    debug_printf("Step 6: Flash softloader\n");
    {
        module_spec_t spec;
        spec.module_id     = MODULE_ID_SOFTLOADER;
        spec.module_sub_id = 0;
        spec.module_length = images->softloader.size;
        spec.module_chksum = compute_module_checksum(images->softloader.data,
                                           images->softloader.size);

        if (!mod_start_session(SESSION_ID, &spec, 1)) return false;
        if (!programmer_flash_module(images->softloader.data,
                                     images->softloader.size,
                                     MODULE_ID_SOFTLOADER,
                                     SESSION_ID, 0, 1)) return false;
        if (!mod_close_session(SESSION_ID, COMMIT_CODE_SOFTLOADER)) return false;
    }

    /* ── 7. Flash firmware + PIB ─────────────────────────────────────────── */
    debug_printf("Step 7: Flash firmware + PIB\n");
    {
        module_spec_t specs[2];
        /* Index 0 = PIB, Index 1 = firmware (per open-plc-utils FlashDevice2) */
        specs[0].module_id     = MODULE_ID_PIB;
        specs[0].module_sub_id = 0;
        specs[0].module_length = images->pib.size;
        specs[0].module_chksum = compute_module_checksum(images->pib.data, images->pib.size);

        specs[1].module_id     = MODULE_ID_FW;
        specs[1].module_sub_id = 0;
        specs[1].module_length = images->firmware.size;
        specs[1].module_chksum = compute_module_checksum(images->firmware.data,
                                               images->firmware.size);

        if (!mod_start_session(SESSION_ID, specs, 2)) return false;
        if (!programmer_flash_module(images->pib.data, images->pib.size,
                                     MODULE_ID_PIB,
                                     SESSION_ID, 0, 2)) return false;
        if (!programmer_flash_module(images->firmware.data, images->firmware.size,
                                     MODULE_ID_FW,
                                     SESSION_ID, 1, 2)) return false;
        if (!mod_close_session(SESSION_ID, COMMIT_CODE_FW_PIB)) return false;
    }

    /* ── Final: QCA resets and boots from NVM into runtime ─────────────── */
    debug_printf("Done. Waiting for device reboot from NVM...\n");
    programmer_wait_for_runtime(30000u); /* best-effort, ignore result */

    return true;
}
