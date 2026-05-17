#include "programmer.h"
#include "hwinit.h"

namespace {
constexpr uint8_t kHostMac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
constexpr uint8_t kLocalCast[6] = { 0x00, 0xB0, 0x52, 0x00, 0x00, 0x01 };
constexpr uint16_t kEtherTypeHomePlug = 0x88E1;
constexpr uint16_t kVsSwVer = 0xA000;
constexpr uint16_t kVsRsDev = 0xA01C;
constexpr uint16_t kVsHostAction = 0xA060;
constexpr uint16_t kVsWriteExecuteApplet = 0xA098;
constexpr uint16_t kVsModuleOperation = 0xA0B0;
constexpr uint16_t kMmtypeReq = 0x0000;
constexpr uint16_t kMmtypeCnf = 0x0001;
constexpr uint16_t kMmtypeInd = 0x0002;
constexpr uint16_t kMmtypeRsp = 0x0003;
constexpr uint16_t kImageTypeFirmware = 0x0004;
constexpr uint16_t kImageTypeMemctl = 0x0007;
constexpr uint16_t kImageTypeManifest = 0x000E;
constexpr uint16_t kImageTypePib = 0x000F;
constexpr uint8_t kMmv = 0x00;
constexpr size_t kEthernetMinimum = 60;
constexpr size_t kFrameBufferSize = 1700;
constexpr size_t kModuleChunk = 1400;
constexpr uint32_t kCookie = 0x78563412u;
constexpr uint32_t kStartTimeoutMs = 60000u;
constexpr uint32_t kResponseTimeoutMs = 5000u;
constexpr uint32_t kModuleTimeoutMs = 90000u;
constexpr uint32_t kCommitFlags = 0x80000003u;
constexpr uint32_t kModuleFlagExecute = (1u << 0);
constexpr uint32_t kModuleFlagAbsolute = (1u << 1);
constexpr uint32_t kRequiredEntryAlignment = sizeof(uint32_t);
constexpr uint16_t kModuleOpStartSession = 0x0010;
constexpr uint16_t kModuleOpWriteModule = 0x0011;
constexpr uint16_t kModuleOpCloseSession = 0x0012;
constexpr uint16_t kModuleIdFirmware = 0x7001;
constexpr uint16_t kModuleIdParameters = 0x7002;
constexpr uint16_t kModuleIdSoftloader = 0x7003;

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

struct PACKED VsSwVerRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
};

struct PACKED VsSwVerConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint8_t mstatus;
   uint8_t mdevice_id;
   uint8_t mversion_length;
   char mversion[0xFF];
   uint16_t mplatform;
};

struct PACKED VsWriteExecuteRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint32_t client_session_id;
   uint32_t server_session_id;
   uint32_t flags;
   uint8_t allowed_mem_types[8];
   uint32_t total_length;
   uint32_t current_part_length;
   uint32_t current_part_offset;
   uint32_t start_addr;
   uint32_t checksum;
   uint8_t reserved2[8];
   uint8_t image[kModuleChunk];
};

struct PACKED VsWriteExecuteConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint32_t mstatus;
   uint32_t client_session_id;
   uint32_t server_session_id;
   uint32_t flags;
   uint8_t allowed_mem_types[8];
   uint32_t total_length;
   uint32_t current_part_length;
   uint32_t current_part_offset;
   uint32_t start_addr;
   uint32_t checksum;
   uint8_t reserved2[8];
   uint32_t current_part_absolute_addr;
   uint32_t absolute_start_addr;
};

struct PACKED ModuleSpec
{
   uint16_t module_id;
   uint16_t module_sub_id;
   uint32_t module_length;
   uint32_t module_checksum;
};

struct PACKED VsModuleOperationStartRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint32_t reserved1;
   uint8_t num_op_data;
   struct PACKED
   {
      uint16_t mod_op;
      uint16_t mod_op_data_len;
      uint32_t mod_op_reserved;
      uint32_t mod_op_session_id;
      uint8_t num_modules;
   } module_spec;
   ModuleSpec modules[2];
};

struct PACKED VsModuleOperationStartConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint16_t mstatus;
   uint16_t err_rec_code;
   uint32_t reserved1;
   uint8_t num_op_data;
   struct PACKED
   {
      uint16_t mod_op;
      uint16_t mod_op_data_len;
      uint32_t mod_op_reserved;
      uint32_t mod_op_session_id;
      uint8_t num_modules;
   } module_spec;
   struct PACKED
   {
      uint16_t mod_status;
      uint16_t err_rec_code;
   } mod_op_data[1];
};

struct PACKED VsModuleOperationWriteRequest
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

struct PACKED VsModuleOperationWriteConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint16_t mstatus;
   uint16_t err_rec_code;
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
};

struct PACKED VsModuleOperationCommitRequest
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
      uint32_t commit_code;
   } request;
   uint8_t reserved2[20];
};

struct PACKED VsModuleOperationCommitConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint16_t mstatus;
   uint16_t err_rec_code;
   uint32_t reserved1;
   uint8_t num_op_data;
   struct PACKED
   {
      uint16_t mod_op;
      uint16_t mod_op_data_len;
      uint32_t mod_op_reserved;
      uint32_t mod_op_session_id;
      uint32_t commit_code;
      uint8_t num_modules;
   } confirm;
   struct PACKED
   {
      uint16_t mod_status;
      uint16_t err_rec_code;
   } mod_op_data[1];
};

struct PACKED VsRsDevRequest
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
};

struct PACKED VsRsDevConfirm
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint8_t mstatus;
};

struct PACKED VsHostActionResponse
{
   EthernetHeader ethernet;
   QualcommHeader qualcomm;
   uint8_t mstatus;
};

struct ImageDescriptor
{
   NvmHeader2 header;
   size_t header_offset;
   size_t data_offset;
};

void debug_put_hex32(uint32_t value)
{
   static const char kHex[] = "0123456789ABCDEF";
   for (int shift = 28; shift >= 0; shift -= 4)
      debug_putc(kHex[(value >> shift) & 0xFu]);
}

void debug_put_u32_dec(uint32_t value)
{
   char digits[10];
   int count = 0;
   if (value == 0u)
   {
      debug_putc('0');
      return;
   }
   while (value != 0u && count < (int)sizeof(digits))
   {
      digits[count++] = (char)('0' + (value % 10u));
      value /= 10u;
   }
   while (count-- > 0)
      debug_putc(digits[count]);
}

void debug_log_image(const char* label, const ImageDescriptor& descriptor)
{
   debug_puts("[IMG] ");
   debug_puts(label);
   debug_puts(" type=0x");
   debug_put_hex32(le32_to_host(descriptor.header.image_type));
   debug_puts(" addr=0x");
   debug_put_hex32(le32_to_host(descriptor.header.image_address));
   debug_puts(" len=");
   debug_put_u32_dec(le32_to_host(descriptor.header.image_length));
   debug_puts(" entry=0x");
   debug_put_hex32(le32_to_host(descriptor.header.entry_point));
   debug_puts(" csum=0x");
   debug_put_hex32(le32_to_host(descriptor.header.image_checksum));
   debug_puts("\r\n");
}

struct MmeSession
{
   const EthernetTransport* transport;
   uint8_t peer[6];
   uint8_t frame[kFrameBufferSize];
   uint8_t response[kFrameBufferSize];
};

void build_ethernet_header(EthernetHeader& header, const uint8_t* destination, const uint8_t* source)
{
   mem_copy(header.destination, destination, sizeof(header.destination));
   mem_copy(header.source, source, sizeof(header.source));
   header.type = host_to_be16(kEtherTypeHomePlug);
}

void build_qualcomm_header(QualcommHeader& header, uint16_t mmtype)
{
   header.mmv = kMmv;
   header.mmtype = host_to_le16(mmtype);
   header.oui[0] = 0x00;
   header.oui[1] = 0xB0;
   header.oui[2] = 0x52;
}

bool send_frame(MmeSession& session, size_t length)
{
   if (length > sizeof(session.frame))
      return false;

   if (length < kEthernetMinimum)
   {
      mem_set(session.frame + length, 0, kEthernetMinimum - length);
      length = kEthernetMinimum;
   }

   return session.transport->send_frame(session.transport->context, session.frame, length);
}

bool parse_mme(const uint8_t* frame, size_t length, uint16_t expected_mmtype)
{
   if (length < sizeof(EthernetHeader) + sizeof(QualcommHeader))
   {
      debug_puts("[MME] drop short frame len=");
      debug_put_u32_dec((uint32_t)length);
      debug_puts("\r\n");
      return false;
   }

   const EthernetHeader* ethernet = (const EthernetHeader*)frame;
   const QualcommHeader* qualcomm = (const QualcommHeader*)(frame + sizeof(EthernetHeader));
   uint16_t ethernet_type;
   uint16_t mmtype;
   mem_copy(&ethernet_type, &ethernet->type, sizeof(ethernet_type));
   mem_copy(&mmtype, &qualcomm->mmtype, sizeof(mmtype));

   if (ethernet_type != host_to_be16(kEtherTypeHomePlug))
   {
      debug_puts("[MME] drop ethertype=0x");
      debug_put_hex32((uint32_t)bswap16(ethernet_type));
      debug_puts("\r\n");
      return false;
   }

   if (le16_to_host(mmtype) != expected_mmtype)
   {
      debug_puts("[MME] drop mmtype expected=0x");
      debug_put_hex32((uint32_t)expected_mmtype);
      debug_puts(" got=0x");
      debug_put_hex32((uint32_t)le16_to_host(mmtype));
      debug_puts(" mmv=0x");
      debug_put_hex32((uint32_t)qualcomm->mmv);
      debug_puts("\r\n");
      return false;
   }

   return true;
}

int handle_host_action_indication(MmeSession& session, const uint8_t* frame, size_t length)
{
   if (length < sizeof(EthernetHeader) + sizeof(QualcommHeader))
      return 0;

   const EthernetHeader* ethernet = (const EthernetHeader*)frame;
   if (ethernet->type != host_to_be16(kEtherTypeHomePlug))
      return 0;

   const QualcommHeader* qualcomm = (const QualcommHeader*)(frame + sizeof(EthernetHeader));
   uint16_t mmtype = 0u;
   mem_copy(&mmtype, &qualcomm->mmtype, sizeof(mmtype));

   if (qualcomm->mmv != kMmv || le16_to_host(mmtype) != (uint16_t)(kVsHostAction | kMmtypeInd))
      return 0;

   debug_puts("[MME] host action ind -> rsp\r\n");
   VsHostActionResponse* response = (VsHostActionResponse*)session.frame;
   mem_set(session.frame, 0, sizeof(VsHostActionResponse));
   build_ethernet_header(response->ethernet, ethernet->source, kHostMac);
   build_qualcomm_header(response->qualcomm, (uint16_t)(kVsHostAction | kMmtypeRsp));
   response->mstatus = 0u;
   if (!send_frame(session, sizeof(VsHostActionResponse)))
      return -1;

   return 1;
}

int receive_matching(MmeSession& session, uint16_t expected_mmtype, uint32_t timeout_ms)
{
   const uint32_t deadline = session.transport->millis(session.transport->context) + timeout_ms;

   while ((int32_t)(deadline - session.transport->millis(session.transport->context)) >= 0)
   {
      status_running_light_update();
      const uint32_t remaining = deadline - session.transport->millis(session.transport->context);
      const int length = session.transport->receive_frame(session.transport->context,
                                                          session.response,
                                                          sizeof(session.response),
                                                          remaining);
      if (length <= 0)
      {
         debug_puts(length < 0 ? "[MME] receive error\r\n" : "[MME] receive timeout\r\n");
         return length;
      }

      const int host_action_result = handle_host_action_indication(session, session.response, (size_t)length);
      if (host_action_result < 0)
         return -1;
      if (host_action_result > 0)
         continue;

      if (parse_mme(session.response, (size_t)length, expected_mmtype))
         return length;
   }

   debug_puts("[MME] wait timeout expected=0x");
   debug_put_hex32((uint32_t)expected_mmtype);
   debug_puts("\r\n");
   return 0;
}

bool validate_nvm_header(const NvmHeader2& header)
{
   if (le16_to_host(header.major_version) != 1u)
      return false;
   if (le16_to_host(header.minor_version) != 1u)
      return false;

   if (checksum32(&header, sizeof(header), 0u) == 0u)
      return true;

   if (checksum32(&header, sizeof(header) - sizeof(header.header_checksum), 0u) == 0u)
      return true;

   return false;
}

bool find_image(const EmbeddedImage& image, uint32_t image_type, ImageDescriptor& descriptor)
{
   size_t offset = 0;

   while (offset + sizeof(NvmHeader2) <= image.size)
   {
      mem_copy(&descriptor.header, image.data + offset, sizeof(NvmHeader2));
      if (!validate_nvm_header(descriptor.header))
         return false;

      descriptor.header_offset = offset;
      descriptor.data_offset = offset + sizeof(NvmHeader2);

      if (le32_to_host(descriptor.header.image_type) == image_type)
         return true;

      const uint32_t next = le32_to_host(descriptor.header.next_header);
      if (next == 0xFFFFFFFFu)
         break;
      if (next >= image.size)
         return false;
      offset = next;
   }

   return false;
}

bool compute_module_spec(const EmbeddedImage& image, ModuleSpec& spec, uint16_t module_id)
{
   if ((image.size & 3u) != 0u)
      return false;

   spec.module_id = host_to_le16(module_id);
   spec.module_sub_id = host_to_le16(0u);
   spec.module_length = host_to_le32((uint32_t)image.size);
   spec.module_checksum = checksum32(image.data, image.size, 0u);
   return true;
}

bool validate_image_payload(const EmbeddedImage& image, const ImageDescriptor& descriptor)
{
   const uint32_t image_length = le32_to_host(descriptor.header.image_length);
   if (descriptor.data_offset + image_length > image.size)
      return false;

   return checksum32(image.data + descriptor.data_offset, image_length, 0u) ==
          le32_to_host(descriptor.header.image_checksum);
}

bool wait_for_start(MmeSession& session, char* version, size_t version_capacity)
{
   const uint32_t deadline = session.transport->millis(session.transport->context) + kStartTimeoutMs;
   VsSwVerRequest* request = (VsSwVerRequest*)session.frame;

   while ((int32_t)(deadline - session.transport->millis(session.transport->context)) >= 0)
   {
      status_running_light_update();
      mem_set(session.frame, 0, sizeof(VsSwVerRequest));
      build_ethernet_header(request->ethernet, session.peer, kHostMac);
      build_qualcomm_header(request->qualcomm, (uint16_t)(kVsSwVer | kMmtypeReq));
      if (!send_frame(session, sizeof(VsSwVerRequest)))
         return false;

      const int length = receive_matching(session, (uint16_t)(kVsSwVer | kMmtypeCnf), 250u);
      if (length > 0)
      {
         const VsSwVerConfirm* confirm = (const VsSwVerConfirm*)session.response;
         const size_t copy_length = confirm->mversion_length < version_capacity - 1u ?
               confirm->mversion_length : version_capacity - 1u;
         mem_set(version, 0, version_capacity);
         mem_copy(version, confirm->mversion, copy_length);
         mem_copy(session.peer, confirm->ethernet.source, sizeof(session.peer));
         return confirm->mstatus == 0u;
      }

      session.transport->delay_ms(session.transport->context, 50u);
   }

   return false;
}

bool write_execute(MmeSession& session,
                   const uint8_t* data,
                   size_t length,
                   uint32_t start_offset,
                   uint32_t total_length,
                   uint32_t entry_point,
                   uint32_t image_checksum,
                   uint8_t allowed_mem_type,
                   bool execute_on_last)
{
   debug_puts("[FLASH] VS_WRITE_EXECUTE len=");
   debug_put_u32_dec((uint32_t)length);
   debug_puts(" total=");
   debug_put_u32_dec(total_length);
   debug_puts(" start=0x");
   debug_put_hex32(start_offset);
   debug_puts(" entry=0x");
   debug_put_hex32(entry_point);
   debug_puts(" mem=");
   debug_put_u32_dec(allowed_mem_type);
   debug_puts("\r\n");

   uint32_t offset = start_offset;
   size_t remaining = length;

   while (remaining > 0u)
   {
      status_running_light_update();
      VsWriteExecuteRequest* request = (VsWriteExecuteRequest*)session.frame;
      mem_set(session.frame, 0, sizeof(VsWriteExecuteRequest));
      build_ethernet_header(request->ethernet, session.peer, kHostMac);
      build_qualcomm_header(request->qualcomm, (uint16_t)(kVsWriteExecuteApplet | kMmtypeReq));

      size_t chunk = remaining > kModuleChunk ? kModuleChunk : remaining;
      uint32_t flags = kModuleFlagAbsolute;
      if (execute_on_last && chunk == remaining && (entry_point % kRequiredEntryAlignment == 0u))
         flags |= kModuleFlagExecute;

      request->client_session_id = host_to_le32(kCookie);
      request->server_session_id = host_to_le32(0u);
      request->flags = host_to_le32(flags);
      request->allowed_mem_types[0] = allowed_mem_type;
      request->total_length = host_to_le32(total_length);
      request->current_part_length = host_to_le32((uint32_t)chunk);
      request->current_part_offset = host_to_le32(offset);
      request->start_addr = host_to_le32(entry_point);
      request->checksum = host_to_le32(image_checksum);
      mem_copy(request->image, data + (offset - start_offset), chunk);

      if (!send_frame(session, sizeof(VsWriteExecuteRequest)))
         return false;

      const int response_length = receive_matching(session,
                                                   (uint16_t)(kVsWriteExecuteApplet | kMmtypeCnf),
                                                   kResponseTimeoutMs);
      if (response_length <= 0)
         return false;

      const VsWriteExecuteConfirm* confirm = (const VsWriteExecuteConfirm*)session.response;
      if (le32_to_host(confirm->client_session_id) != kCookie)
         return false;
      if (le32_to_host(confirm->mstatus) != 0u)
         return false;
      if (le32_to_host(confirm->current_part_length) != chunk)
         return false;
      if (le32_to_host(confirm->current_part_offset) != offset)
         return false;

      remaining -= chunk;
      offset += (uint32_t)chunk;
   }

   return true;
}

bool module_session(MmeSession& session, const ModuleSpec* modules, uint8_t module_count)
{
   debug_puts("[FLASH] MODULE_SESSION modules=");
   debug_put_u32_dec(module_count);
   debug_puts("\r\n");

   VsModuleOperationStartRequest* request = (VsModuleOperationStartRequest*)session.frame;
   mem_set(session.frame, 0, sizeof(VsModuleOperationStartRequest));
   build_ethernet_header(request->ethernet, session.peer, kHostMac);
   build_qualcomm_header(request->qualcomm, (uint16_t)(kVsModuleOperation | kMmtypeReq));
   request->num_op_data = 1u;
   request->module_spec.mod_op = host_to_le16(kModuleOpStartSession);
   request->module_spec.mod_op_data_len = host_to_le16((uint16_t)(sizeof(request->module_spec) +
         module_count * sizeof(ModuleSpec)));
   request->module_spec.mod_op_session_id = host_to_le32(kCookie);
   request->module_spec.num_modules = module_count;
   for (uint8_t index = 0; index < module_count; index++)
      request->modules[index] = modules[index];

   const size_t length = sizeof(EthernetHeader) + sizeof(QualcommHeader) + sizeof(uint32_t) + 1u +
         sizeof(request->module_spec) + module_count * sizeof(ModuleSpec);
   if (!send_frame(session, length))
      return false;

   const int response_length = receive_matching(session,
                                                (uint16_t)(kVsModuleOperation | kMmtypeCnf),
                                                kResponseTimeoutMs);
   if (response_length <= 0)
      return false;

   const VsModuleOperationStartConfirm* confirm = (const VsModuleOperationStartConfirm*)session.response;
   return le16_to_host(confirm->mstatus) == 0u &&
          le16_to_host(confirm->module_spec.mod_op) == kModuleOpStartSession &&
          le32_to_host(confirm->module_spec.mod_op_session_id) == kCookie;
}

bool module_write(MmeSession& session, const EmbeddedImage& image, uint8_t module_index, const ModuleSpec& spec)
{
   debug_puts("[FLASH] MODULE_WRITE idx=");
   debug_put_u32_dec(module_index);
   debug_puts(" id=0x");
   debug_put_hex32(le16_to_host(spec.module_id));
   debug_puts(" len=");
   debug_put_u32_dec((uint32_t)image.size);
   debug_puts(" csum=0x");
   debug_put_hex32(spec.module_checksum);
   debug_puts("\r\n");

   size_t remaining = image.size;
   uint32_t offset = 0u;

   while (remaining > 0u)
   {
      status_running_light_update();
      VsModuleOperationWriteRequest* request = (VsModuleOperationWriteRequest*)session.frame;
      mem_set(session.frame, 0, sizeof(VsModuleOperationWriteRequest));
      build_ethernet_header(request->ethernet, session.peer, kHostMac);
      build_qualcomm_header(request->qualcomm, (uint16_t)(kVsModuleOperation | kMmtypeReq));
      request->num_op_data = 1u;
      request->module_spec.mod_op = host_to_le16(kModuleOpWriteModule);
      request->module_spec.mod_op_data_len = host_to_le16((uint16_t)(sizeof(request->module_spec) + kModuleChunk));
      request->module_spec.mod_op_session_id = host_to_le32(kCookie);
      request->module_spec.module_idx = module_index;
      request->module_spec.module_id = spec.module_id;
      request->module_spec.module_sub_id = spec.module_sub_id;
      const size_t chunk = remaining > kModuleChunk ? kModuleChunk : remaining;
      request->module_spec.module_length = host_to_le16((uint16_t)chunk);
      request->module_spec.module_offset = host_to_le32(offset);
      mem_copy(request->module_data, image.data + offset, chunk);

      if (!send_frame(session, sizeof(VsModuleOperationWriteRequest)))
         return false;

      const int response_length = receive_matching(session,
                                                   (uint16_t)(kVsModuleOperation | kMmtypeCnf),
                                                   kModuleTimeoutMs);
      if (response_length <= 0)
         return false;

      const VsModuleOperationWriteConfirm* confirm = (const VsModuleOperationWriteConfirm*)session.response;
      if (le16_to_host(confirm->mstatus) != 0u)
         return false;
      if (le16_to_host(confirm->module_spec.mod_op) != kModuleOpWriteModule)
         return false;
      if (le32_to_host(confirm->module_spec.mod_op_session_id) != kCookie)
         return false;
      if (le16_to_host(confirm->module_spec.module_length) != chunk)
         return false;
      if (le32_to_host(confirm->module_spec.module_offset) != offset)
         return false;

      remaining -= chunk;
      offset += (uint32_t)chunk;
   }

   return true;
}

bool module_commit(MmeSession& session)
{
   debug_puts("[FLASH] MODULE_COMMIT\r\n");

   VsModuleOperationCommitRequest* request = (VsModuleOperationCommitRequest*)session.frame;
   mem_set(session.frame, 0, sizeof(VsModuleOperationCommitRequest));
   build_ethernet_header(request->ethernet, session.peer, kHostMac);
   build_qualcomm_header(request->qualcomm, (uint16_t)(kVsModuleOperation | kMmtypeReq));
   request->num_op_data = 1u;
   request->request.mod_op = host_to_le16(kModuleOpCloseSession);
   request->request.mod_op_data_len = host_to_le16((uint16_t)(sizeof(request->request) + sizeof(request->reserved2)));
   request->request.mod_op_session_id = host_to_le32(kCookie);
   request->request.commit_code = host_to_le32(kCommitFlags);

   if (!send_frame(session, sizeof(VsModuleOperationCommitRequest)))
      return false;

   const int response_length = receive_matching(session,
                                                (uint16_t)(kVsModuleOperation | kMmtypeCnf),
                                                kModuleTimeoutMs);
   if (response_length <= 0)
      return false;

   const VsModuleOperationCommitConfirm* confirm = (const VsModuleOperationCommitConfirm*)session.response;
   return le16_to_host(confirm->mstatus) == 0u &&
          le16_to_host(confirm->confirm.mod_op) == kModuleOpCloseSession &&
          le32_to_host(confirm->confirm.mod_op_session_id) == kCookie;
}

bool reset_device(MmeSession& session)
{
   VsRsDevRequest* request = (VsRsDevRequest*)session.frame;
   mem_set(session.frame, 0, sizeof(VsRsDevRequest));
   build_ethernet_header(request->ethernet, session.peer, kHostMac);
   build_qualcomm_header(request->qualcomm, (uint16_t)(kVsRsDev | kMmtypeReq));
   if (!send_frame(session, sizeof(VsRsDevRequest)))
      return false;

   const int response_length = receive_matching(session,
                                                (uint16_t)(kVsRsDev | kMmtypeCnf),
                                                kResponseTimeoutMs);
   if (response_length <= 0)
      return false;

   const VsRsDevConfirm* confirm = (const VsRsDevConfirm*)session.response;
   return confirm->mstatus == 0u;
}

bool flash_softloader(MmeSession& session, const EmbeddedImage& image)
{
   debug_puts("[FLASH] softloader module start\r\n");
   ModuleSpec spec;
   if (!compute_module_spec(image, spec, kModuleIdSoftloader))
      return false;
   if (!module_session(session, &spec, 1u))
      return false;
   if (!module_write(session, image, 0u, spec))
      return false;
   return module_commit(session);
}

bool flash_firmware_and_pib(MmeSession& session, const EmbeddedImage& firmware, const EmbeddedImage& pib)
{
   debug_puts("[FLASH] firmware+pib module start\r\n");
   ModuleSpec specs[2];
   if (!compute_module_spec(pib, specs[0], kModuleIdParameters))
      return false;
   if (!compute_module_spec(firmware, specs[1], kModuleIdFirmware))
      return false;
   if (!module_session(session, specs, 2u))
      return false;
   if (!module_write(session, pib, 0u, specs[0]))
      return false;
   if (!module_write(session, firmware, 1u, specs[1]))
      return false;
   return module_commit(session);
}
} // namespace

ProgrammerResult run_programmer(const EmbeddedImages& images, const EthernetTransport& transport)
{
   status_running_light_update();
   debug_puts("[PROGRAMMER] start\r\n");

   if (!transport.send_frame || !transport.receive_frame || !transport.delay_ms || !transport.millis)
      return PROGRAMMER_TRANSPORT_ERROR;

   ImageDescriptor memctl;
   ImageDescriptor runtime;
   ImageDescriptor pib;
   ImageDescriptor pib_manifest;
   const uint8_t* pib_upload_data = images.pib.data;
   uint32_t pib_upload_length = (uint32_t)images.pib.size;
   uint32_t pib_upload_start = 0u;
   uint32_t pib_upload_total = (uint32_t)images.pib.size;
   uint32_t pib_upload_entry = 0u;
   uint32_t pib_upload_checksum = checksum32(images.pib.data, images.pib.size, 0u);

   if (!find_image(images.firmware, kImageTypeMemctl, memctl) ||
       !find_image(images.firmware, kImageTypeFirmware, runtime))
      return PROGRAMMER_INVALID_IMAGES;

   debug_log_image("memctl", memctl);
   debug_log_image("runtime", runtime);

   if (!validate_image_payload(images.firmware, memctl) ||
       !validate_image_payload(images.firmware, runtime))
      return PROGRAMMER_INVALID_IMAGES;

   if (find_image(images.pib, kImageTypePib, pib) &&
       find_image(images.pib, kImageTypeManifest, pib_manifest) &&
       validate_image_payload(images.pib, pib) &&
       validate_image_payload(images.pib, pib_manifest))
   {
      debug_log_image("pib", pib);
      debug_log_image("pib_manifest", pib_manifest);
      const uint32_t pib_length = le32_to_host(pib.header.image_length);
      const uint32_t pib_transfer_length = (uint32_t)(sizeof(NvmHeader2) +
            le32_to_host(pib_manifest.header.image_length) + sizeof(NvmHeader2) + pib_length);

      if (pib_transfer_length > images.pib.size)
         return PROGRAMMER_INVALID_IMAGES;

      pib_upload_data = images.pib.data;
      pib_upload_length = pib_transfer_length;
      pib_upload_start = le32_to_host(pib.header.image_address);
      pib_upload_total = pib_length;
      pib_upload_entry = le32_to_host(pib.header.entry_point);
      pib_upload_checksum = le32_to_host(pib.header.image_checksum);
      debug_puts("[IMG] pib source=nvm-wrapped\r\n");
   }
   else if (images.pib.size == 0u || images.pib.size > 0xFFFFFFFFu)
   {
      return PROGRAMMER_INVALID_IMAGES;
   }
   else
   {
      debug_puts("[IMG] pib source=raw len=");
      debug_put_u32_dec((uint32_t)images.pib.size);
      debug_puts(" csum=0x");
      debug_put_hex32(pib_upload_checksum);
      debug_puts("\r\n");
   }

   const uint32_t memctl_length = le32_to_host(memctl.header.image_length);
   const uint32_t runtime_length = le32_to_host(runtime.header.image_length);

   if (memctl.data_offset + memctl_length > images.firmware.size ||
       runtime.data_offset + runtime_length > images.firmware.size ||
       pib_upload_length > images.pib.size)
      return PROGRAMMER_INVALID_IMAGES;

   MmeSession session;
   session.transport = &transport;
   mem_copy(session.peer, kLocalCast, sizeof(session.peer));

   char version[32];
   if (!wait_for_start(session, version, sizeof(version)))
      return PROGRAMMER_TIMEOUT;
   if (!cstr_equal(version, "BootLoader"))
      return PROGRAMMER_PROTOCOL_ERROR;
   debug_puts("[PROGRAMMER] bootloader ready\r\n");

   if (!write_execute(session,
                      images.firmware.data + memctl.data_offset,
                      memctl_length,
                      le32_to_host(memctl.header.image_address),
                      memctl_length,
                      le32_to_host(memctl.header.entry_point),
                      le32_to_host(memctl.header.image_checksum),
                      1u,
                      true))
      return PROGRAMMER_PROTOCOL_ERROR;

   if (!write_execute(session,
                       pib_upload_data,
                       pib_upload_length,
                       pib_upload_start,
                       pib_upload_total,
                       pib_upload_entry,
                       pib_upload_checksum,
                       0u,
                       false))
      return PROGRAMMER_PROTOCOL_ERROR;

   if (!write_execute(session,
                      images.firmware.data + runtime.data_offset,
                      runtime_length,
                      le32_to_host(runtime.header.image_address),
                      runtime_length,
                      le32_to_host(runtime.header.entry_point),
                      le32_to_host(runtime.header.image_checksum),
                      1u,
                      true))
      return PROGRAMMER_PROTOCOL_ERROR;

   if (!wait_for_start(session, version, sizeof(version)))
      return PROGRAMMER_TIMEOUT;
   debug_puts("[PROGRAMMER] runtime ready, flashing modules\r\n");
   if (!flash_softloader(session, images.softloader))
      return PROGRAMMER_PROTOCOL_ERROR;
   if (!flash_firmware_and_pib(session, images.firmware, images.pib))
      return PROGRAMMER_PROTOCOL_ERROR;
   if (!reset_device(session))
      return PROGRAMMER_PROTOCOL_ERROR;
   if (!wait_for_start(session, version, sizeof(version)))
      return PROGRAMMER_TIMEOUT;
   debug_puts("[PROGRAMMER] done\r\n");

   return PROGRAMMER_OK;
}
