#include <assert.h>
#include <stdio.h>
#include <deque>
#include <vector>
#include "common.h"
#include "programmer.h"

namespace {
constexpr uint16_t kVsSwVer = 0xA000;
constexpr uint16_t kVsRsDev = 0xA01C;
constexpr uint16_t kVsHostAction = 0xA060;
constexpr uint16_t kVsWriteExecuteApplet = 0xA098;
constexpr uint16_t kVsModuleOperation = 0xA0B0;
constexpr uint16_t kMmtypeReq = 0x0000;
constexpr uint16_t kMmtypeCnf = 0x0001;
constexpr uint16_t kMmtypeInd = 0x0002;
constexpr uint16_t kMmtypeRsp = 0x0003;
constexpr uint16_t kModuleOpStartSession = 0x0010;
constexpr uint16_t kModuleOpWriteModule = 0x0011;
constexpr uint16_t kModuleOpCloseSession = 0x0012;
constexpr size_t kModuleChunk = 1400;
constexpr uint8_t kDeviceMac[6] = { 0x00, 0xB0, 0x52, 0x12, 0x34, 0x56 };
constexpr uint8_t kHostMac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

struct PACKED EthernetHeader
{
   uint8_t destination[6];
   uint8_t source[6];
   uint16_t type;
};

struct PACKED QualcommHeader
{
   uint8_t mmv;
   uint16_t mmtype;
   uint8_t oui[3];
};

struct PACKED VsWriteExecuteRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint32_t client_session_id;
   uint32_t server_session_id;
   uint32_t write_flags;
   uint8_t allowed_mem_types[8];
   uint32_t total_length;
   uint32_t current_part_length;
   uint32_t current_part_offset;
   uint32_t start_addr;
   uint32_t checksum;
   uint8_t reserved2[8];
   uint8_t image[kModuleChunk];
};

struct PACKED VsModuleWriteRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint32_t reserved;
   uint8_t num_op_data;
   struct PACKED
   {
      uint16_t mod_op;
      uint16_t mod_op_data_len;
      uint32_t mod_op_reserved;
      uint32_t mod_op_session_id;
      uint8_t module_idx;
      uint16_t module_id;
      uint16_t module_sub_id;
      uint16_t module_length;
      uint32_t module_offset;
   } module_spec;
   uint8_t module_data[kModuleChunk];
};

struct FakeTransport
{
   std::deque<std::vector<uint8_t>> responses;
   std::vector<uint16_t> sequence;
   uint32_t now_ms = 0;
   int sw_ver_count = 0;
   bool inject_host_action = false;

   static void push_ethernet(std::vector<uint8_t>& frame, uint16_t mmtype)
   {
      frame.resize(60, 0);
      EthernetHeader* ethernet = reinterpret_cast<EthernetHeader*>(frame.data());
      QualcommHeader* qualcomm = reinterpret_cast<QualcommHeader*>(frame.data() + sizeof(EthernetHeader));
      mem_copy(ethernet->destination, kHostMac, sizeof(kHostMac));
      mem_copy(ethernet->source, kDeviceMac, sizeof(kDeviceMac));
      ethernet->type = host_to_be16(0x88E1u);
      qualcomm->mmv = 0;
      qualcomm->mmtype = host_to_le16(mmtype);
      qualcomm->oui[0] = 0;
      qualcomm->oui[1] = 0xB0;
      qualcomm->oui[2] = 0x52;
   }

   void enqueue_sw_ver(const char* version)
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsSwVer | kMmtypeCnf));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 3 + cstr_length(version) + 2, 0);
      uint8_t* payload = frame.data() + sizeof(EthernetHeader) + sizeof(QualcommHeader);
      payload[0] = 0;
      payload[1] = 3;
      payload[2] = (uint8_t)cstr_length(version);
      mem_copy(payload + 3, version, cstr_length(version));
      responses.push_back(frame);
   }

   void enqueue_write_execute_confirm(const VsWriteExecuteRequest& request)
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsWriteExecuteApplet | kMmtypeCnf));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 52, 0);
      uint8_t* payload = frame.data() + sizeof(EthernetHeader) + sizeof(QualcommHeader);
      mem_copy(payload + 4, &request.client_session_id, sizeof(uint32_t));
      mem_copy(payload + 24, &request.total_length, sizeof(uint32_t));
      mem_copy(payload + 28, &request.current_part_length, sizeof(uint32_t));
      mem_copy(payload + 32, &request.current_part_offset, sizeof(uint32_t));
      responses.push_back(frame);
   }

   void enqueue_module_start_confirm(const uint8_t* frame_data)
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsModuleOperation | kMmtypeCnf));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 21, 0);
      uint8_t* payload = frame.data() + sizeof(EthernetHeader) + sizeof(QualcommHeader);
      const uint8_t* request = frame_data + sizeof(EthernetHeader) + sizeof(QualcommHeader) + 5;
      payload[8] = 1;
      payload[9] = request[0];
      payload[10] = request[1];
      mem_copy(payload + 17, request + 8, sizeof(uint32_t));
      responses.push_back(frame);
   }

   void enqueue_module_write_confirm(const VsModuleWriteRequest& request)
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsModuleOperation | kMmtypeCnf));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 32, 0);
      uint8_t* payload = frame.data() + sizeof(EthernetHeader) + sizeof(QualcommHeader);
      payload[8] = 1;
      payload[9] = request.module_spec.mod_op & 0xFFu;
      payload[10] = request.module_spec.mod_op >> 8;
      mem_copy(payload + 17, &request.module_spec.mod_op_session_id, sizeof(uint32_t));
      payload[21] = request.module_spec.module_idx;
      mem_copy(payload + 22, &request.module_spec.module_id, sizeof(uint16_t));
      mem_copy(payload + 24, &request.module_spec.module_sub_id, sizeof(uint16_t));
      mem_copy(payload + 26, &request.module_spec.module_length, sizeof(uint16_t));
      mem_copy(payload + 28, &request.module_spec.module_offset, sizeof(uint32_t));
      responses.push_back(frame);
   }

   void enqueue_module_commit_confirm(const uint8_t* frame_data)
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsModuleOperation | kMmtypeCnf));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 25, 0);
      uint8_t* payload = frame.data() + sizeof(EthernetHeader) + sizeof(QualcommHeader);
      const uint8_t* request = frame_data + sizeof(EthernetHeader) + sizeof(QualcommHeader) + 5;
      payload[8] = 1;
      payload[9] = request[0];
      payload[10] = request[1];
      mem_copy(payload + 17, request + 8, sizeof(uint32_t));
      responses.push_back(frame);
   }

   void enqueue_reset_confirm()
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsRsDev | kMmtypeCnf));
      responses.push_back(frame);
   }

   void enqueue_host_action_ind()
   {
      std::vector<uint8_t> frame;
      push_ethernet(frame, (uint16_t)(kVsHostAction | kMmtypeInd));
      frame.resize(sizeof(EthernetHeader) + sizeof(QualcommHeader) + 1u, 0u);
      responses.push_back(frame);
   }

   static bool send_frame(void* context, const uint8_t* frame, size_t)
   {
      FakeTransport* self = static_cast<FakeTransport*>(context);
      const QualcommHeader* qualcomm = reinterpret_cast<const QualcommHeader*>(frame + sizeof(EthernetHeader));
      const uint16_t mmtype = le16_to_host(qualcomm->mmtype);
      self->sequence.push_back(mmtype);

      switch (mmtype)
      {
      case (kVsSwVer | kMmtypeReq):
         if (self->inject_host_action)
            self->enqueue_host_action_ind();
         self->enqueue_sw_ver(self->sw_ver_count++ == 0 ? "BootLoader" : "Runtime");
         break;
      case (kVsWriteExecuteApplet | kMmtypeReq):
         if (self->inject_host_action)
            self->enqueue_host_action_ind();
         self->enqueue_write_execute_confirm(*reinterpret_cast<const VsWriteExecuteRequest*>(frame));
         break;
      case (kVsModuleOperation | kMmtypeReq):
      {
         if (self->inject_host_action)
            self->enqueue_host_action_ind();
         const uint8_t* payload = frame + sizeof(EthernetHeader) + sizeof(QualcommHeader) + 5;
         const uint16_t mod_op = (uint16_t)(payload[0] | (payload[1] << 8));
         self->sequence.push_back((uint16_t)(0xF000u | mod_op));
         if (mod_op == kModuleOpStartSession)
            self->enqueue_module_start_confirm(frame);
         else if (mod_op == kModuleOpWriteModule)
            self->enqueue_module_write_confirm(*reinterpret_cast<const VsModuleWriteRequest*>(frame));
         else if (mod_op == kModuleOpCloseSession)
            self->enqueue_module_commit_confirm(frame);
         break;
      }
      case (kVsRsDev | kMmtypeReq):
         if (self->inject_host_action)
            self->enqueue_host_action_ind();
         self->enqueue_reset_confirm();
         break;
      case (kVsHostAction | kMmtypeRsp):
         break;
      default:
         assert(false && "unexpected request");
      }

      return true;
   }

   static int receive_frame(void* context, uint8_t* frame, size_t capacity, uint32_t timeout_ms)
   {
      FakeTransport* self = static_cast<FakeTransport*>(context);
      self->now_ms += timeout_ms;
      if (self->responses.empty())
         return 0;
      std::vector<uint8_t> next = self->responses.front();
      self->responses.pop_front();
      assert(next.size() <= capacity);
      mem_copy(frame, next.data(), next.size());
      return (int)next.size();
   }

   static void delay_ms(void* context, uint32_t delay)
   {
      static_cast<FakeTransport*>(context)->now_ms += delay;
   }

   static uint32_t millis(void* context)
   {
      return static_cast<FakeTransport*>(context)->now_ms;
   }
};

std::vector<uint8_t> make_header(uint32_t image_type, uint32_t image_address, uint32_t entry_point,
                                 const std::vector<uint8_t>& payload, uint32_t next_header)
{
   NvmHeader2 header = {};
   header.major_version = host_to_le16(1u);
   header.minor_version = host_to_le16(1u);
   header.image_address = host_to_le32(image_address);
   header.image_length = host_to_le32((uint32_t)payload.size());
   header.image_checksum = host_to_le32(checksum32(payload.data(), payload.size(), 0u));
   header.entry_point = host_to_le32(entry_point);
   header.next_header = host_to_le32(next_header);
   header.prev_header = host_to_le32(0xFFFFFFFFu);
   header.image_type = host_to_le32(image_type);
   header.header_checksum = 0u;
   header.header_checksum = checksum32(&header, sizeof(header), 0u);

   std::vector<uint8_t> bytes(sizeof(header));
   mem_copy(bytes.data(), &header, sizeof(header));
   return bytes;
}

std::vector<uint8_t> make_legacy_header(uint32_t image_type, uint32_t image_address, uint32_t entry_point,
                                        const std::vector<uint8_t>& payload, uint32_t next_header,
                                        uint32_t legacy_header_checksum)
{
   NvmHeader2 header = {};
   header.major_version = host_to_le16(1u);
   header.minor_version = host_to_le16(1u);
   header.image_address = host_to_le32(image_address);
   header.image_length = host_to_le32((uint32_t)payload.size());
   header.image_checksum = host_to_le32(checksum32(payload.data(), payload.size(), 0u));
   header.entry_point = host_to_le32(entry_point);
   header.next_header = host_to_le32(next_header);
   header.prev_header = host_to_le32(0xFFFFFFFFu);
   header.image_type = host_to_le32(image_type);
   header.header_checksum = host_to_le32(legacy_header_checksum);

   const uint32_t checksum = checksum32(&header, sizeof(header) - sizeof(header.header_checksum), 0u);
   const uint32_t current_xor = ~checksum;
   const uint32_t adjust = current_xor ^ 0xFFFFFFFFu;
   header.reserved11 = host_to_le32(adjust);
   assert(checksum32(&header, sizeof(header) - sizeof(header.header_checksum), 0u) == 0u);

   std::vector<uint8_t> bytes(sizeof(header));
   mem_copy(bytes.data(), &header, sizeof(header));
   return bytes;
}

std::vector<uint8_t> append(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs)
{
   std::vector<uint8_t> out = lhs;
   out.insert(out.end(), rhs.begin(), rhs.end());
   return out;
}

void test_programmer_flow()
{
   const std::vector<uint8_t> softloader_payload = { 0x10, 0x20, 0x30, 0x40 };
   const std::vector<uint8_t> memctl_payload = { 0x01, 0x02, 0x03, 0x04 };
   const std::vector<uint8_t> firmware_payload = { 0x11, 0x22, 0x33, 0x44 };
   const std::vector<uint8_t> manifest_payload = { 0xAA, 0xBB, 0xCC, 0xDD };
   const std::vector<uint8_t> pib_payload = { 0x55, 0x66, 0x77, 0x88 };

   std::vector<uint8_t> softloader = append(make_header(0x000Bu, 0x1000u, 0x1000u, softloader_payload, 0xFFFFFFFFu), softloader_payload);

   const uint32_t firmware_second_header = (uint32_t)(sizeof(NvmHeader2) + memctl_payload.size());
   std::vector<uint8_t> firmware = append(make_header(0x0007u, 0x2000u, 0x2000u, memctl_payload, firmware_second_header), memctl_payload);
   firmware = append(firmware, make_header(0x0004u, 0x3000u, 0x3000u, firmware_payload, 0xFFFFFFFFu));
   firmware = append(firmware, firmware_payload);

   const uint32_t pib_second_header = (uint32_t)(sizeof(NvmHeader2) + manifest_payload.size());
   std::vector<uint8_t> pib = append(make_header(0x000Eu, 0x4000u, 0x4000u, manifest_payload, pib_second_header), manifest_payload);
   pib = append(pib, make_header(0x000Fu, 0x5000u, 0x5000u, pib_payload, 0xFFFFFFFFu));
   pib = append(pib, pib_payload);

   EmbeddedImages images = {
      { "softloader.nvm", softloader.data(), softloader.size() },
      { "firmware.nvm", firmware.data(), firmware.size() },
      { "evse.pib", pib.data(), pib.size() }
   };

   FakeTransport fake;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };

   const ProgrammerResult result = run_programmer(images, transport);
   if (result != PROGRAMMER_OK)
   {
      printf("result=%d\n", result);
      for (size_t index = 0; index < fake.sequence.size(); index++)
         printf("%04X ", fake.sequence[index]);
      printf("\n");
   }
   assert(result == PROGRAMMER_OK);

   const std::vector<uint16_t> expected = {
      0xA000, 0xA098, 0xA098, 0xA098, 0xA000,
      0xA0B0, 0xF010, 0xA0B0, 0xF011, 0xA0B0, 0xF012,
      0xA0B0, 0xF010, 0xA0B0, 0xF011, 0xA0B0, 0xF011, 0xA0B0, 0xF012,
      0xA01C, 0xA000
   };
   assert(fake.sequence == expected);
}

void test_invalid_image_detection()
{
   const uint8_t invalid[4] = { 0, 1, 2, 3 };
   EmbeddedImages images = {
      { "softloader.nvm", invalid, sizeof(invalid) },
      { "firmware.nvm", invalid, sizeof(invalid) },
      { "evse.pib", invalid, sizeof(invalid) }
   };
   FakeTransport fake;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };
   assert(run_programmer(images, transport) == PROGRAMMER_INVALID_IMAGES);
}

void test_raw_pib_detection()
{
   const std::vector<uint8_t> payload = { 0x01, 0x02, 0x03, 0x04 };
   std::vector<uint8_t> softloader = append(make_header(0x000Bu, 0x1000u, 0x1000u, payload, 0xFFFFFFFFu), payload);

   const uint32_t firmware_second_header = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> firmware = append(make_header(0x0007u, 0x2000u, 0x2000u, payload, firmware_second_header), payload);
   firmware = append(firmware, make_header(0x0004u, 0x3000u, 0x3000u, payload, 0xFFFFFFFFu));
   firmware = append(firmware, payload);

   const std::vector<uint8_t> pib = { 0x11, 0x22, 0x33, 0x44 };

   EmbeddedImages images = {
      { "softloader.nvm", softloader.data(), softloader.size() },
      { "firmware.nvm", firmware.data(), firmware.size() },
      { "evse.pib", pib.data(), pib.size() }
   };

   FakeTransport fake;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };

   assert(run_programmer(images, transport) == PROGRAMMER_OK);
}

void test_corrupt_payload_detection()
{
   const std::vector<uint8_t> payload = { 0x01, 0x02, 0x03, 0x04 };
   std::vector<uint8_t> softloader = append(make_header(0x000Bu, 0x1000u, 0x1000u, payload, 0xFFFFFFFFu), payload);

   const uint32_t firmware_second_header = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> firmware = append(make_header(0x0007u, 0x2000u, 0x2000u, payload, firmware_second_header), payload);
   firmware = append(firmware, make_header(0x0004u, 0x3000u, 0x3000u, payload, 0xFFFFFFFFu));
   firmware = append(firmware, payload);

   const uint32_t pib_second_header = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> pib = append(make_header(0x000Eu, 0x4000u, 0x4000u, payload, pib_second_header), payload);
   pib = append(pib, make_header(0x000Fu, 0x5000u, 0x5000u, payload, 0xFFFFFFFFu));
   pib = append(pib, payload);

   firmware[sizeof(NvmHeader2)] ^= 0xFFu;

   EmbeddedImages images = {
      { "softloader.nvm", softloader.data(), softloader.size() },
      { "firmware.nvm", firmware.data(), firmware.size() },
      { "evse.pib", pib.data(), pib.size() }
   };
   FakeTransport fake;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };
   assert(run_programmer(images, transport) == PROGRAMMER_INVALID_IMAGES);
}

void test_firmware_lookup_with_nonzero_header_checksums()
{
   const std::vector<uint8_t> payload = { 0x01, 0x02, 0x03, 0x04 };
   std::vector<uint8_t> softloader = append(make_header(0x000Bu, 0x1000u, 0x1000u, payload, 0xFFFFFFFFu), payload);

   const uint32_t second = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   const uint32_t third = second + (uint32_t)(sizeof(NvmHeader2) + payload.size());

   std::vector<uint8_t> firmware = append(make_header(0x000Eu, 0x1800u, 0x1800u, payload, second), payload);
   firmware = append(firmware, make_legacy_header(0x0007u, 0x2000u, 0x2000u, payload, third, 0xEA00001Eu));
   firmware = append(firmware, payload);
   firmware = append(firmware, make_legacy_header(0x0004u, 0x3000u, 0x3000u, payload, 0xFFFFFFFFu, 0x00010001u));
   firmware = append(firmware, payload);

   const uint32_t pib_second = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> pib = append(make_header(0x000Eu, 0x4000u, 0x4000u, payload, pib_second), payload);
   pib = append(pib, make_header(0x000Fu, 0x5000u, 0x5000u, payload, 0xFFFFFFFFu));
   pib = append(pib, payload);

   EmbeddedImages images = {
      { "softloader.nvm", softloader.data(), softloader.size() },
      { "firmware.nvm", firmware.data(), firmware.size() },
      { "evse.pib", pib.data(), pib.size() }
   };

   FakeTransport fake;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };
   const ProgrammerResult result = run_programmer(images, transport);
   assert(result == PROGRAMMER_OK);
}

void test_host_action_indication_is_acknowledged()
{
   const std::vector<uint8_t> payload = { 0x01, 0x02, 0x03, 0x04 };
   std::vector<uint8_t> softloader = append(make_header(0x000Bu, 0x1000u, 0x1000u, payload, 0xFFFFFFFFu), payload);

   const uint32_t firmware_second_header = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> firmware = append(make_header(0x0007u, 0x2000u, 0x2000u, payload, firmware_second_header), payload);
   firmware = append(firmware, make_header(0x0004u, 0x3000u, 0x3000u, payload, 0xFFFFFFFFu));
   firmware = append(firmware, payload);

   const uint32_t pib_second_header = (uint32_t)(sizeof(NvmHeader2) + payload.size());
   std::vector<uint8_t> pib = append(make_header(0x000Eu, 0x4000u, 0x4000u, payload, pib_second_header), payload);
   pib = append(pib, make_header(0x000Fu, 0x5000u, 0x5000u, payload, 0xFFFFFFFFu));
   pib = append(pib, payload);

   EmbeddedImages images = {
      { "softloader.nvm", softloader.data(), softloader.size() },
      { "firmware.nvm", firmware.data(), firmware.size() },
      { "evse.pib", pib.data(), pib.size() }
   };

   FakeTransport fake;
   fake.inject_host_action = true;
   EthernetTransport transport = { &fake, FakeTransport::send_frame, FakeTransport::receive_frame,
                                   FakeTransport::delay_ms, FakeTransport::millis };
   assert(run_programmer(images, transport) == PROGRAMMER_OK);

   bool saw_rsp = false;
   for (size_t index = 0; index < fake.sequence.size(); index++)
   {
      if (fake.sequence[index] == (uint16_t)(kVsHostAction | kMmtypeRsp))
      {
         saw_rsp = true;
         break;
      }
   }
   assert(saw_rsp);
}
} // namespace

int main()
{
   test_programmer_flow();
   test_invalid_image_detection();
   test_raw_pib_detection();
   test_corrupt_payload_detection();
   test_firmware_lookup_with_nonzero_header_checksums();
   test_host_action_indication_is_acknowledged();
   return 0;
}
