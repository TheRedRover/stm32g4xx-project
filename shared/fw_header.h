#ifndef __FW_HEADER_H
#define __FW_HEADER_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <assert.h>

#define FW_HEADER_MAGIC_NUMBER 0x4734465748

typedef struct {
    uint32_t magic_number;
    uint32_t fw_size;
    uint32_t version;
    uint32_t fw_crc;
    uint32_t timestamp;
    char git_hash[8];
    char reserved[96]; // Reserved for future use
    uint32_t header_crc;  // should be the last field in the header, so that the CRC can be calculated over all the preceding fields

} fw_header_t;

_Static_assert(sizeof(fw_header_t) == 128, "Header size must be exactly 128 bytes");

#ifdef __cplusplus
}
#endif

#endif /* __FW_HEADER_H */