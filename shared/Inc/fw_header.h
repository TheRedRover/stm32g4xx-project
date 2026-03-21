#ifndef __FW_HEADER_H
#define __FW_HEADER_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "memory_map.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define FW_MAGIC_NUMBER 0x47344657
/* Defines for packing/unpacking */
#define VER_MAJOR_SHIFT  24
#define VER_MINOR_SHIFT  16
#define VER_PATCH_MASK   0xFFFF
#define VER_BYTE_MASK    0xFF

/* Packing macro: converts 1, 4, 12 -> 0x0104000C */
#define PACK_VERSION(maj, min, patch) \
    (((uint32_t)(maj) << VER_MAJOR_SHIFT) | \
     ((uint32_t)(min) << VER_MINOR_SHIFT) | \
     ((uint32_t)(patch) & VER_PATCH_MASK))

typedef struct __attribute__((packed)) {
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

/**
 * @brief Locates the firmware header in Flash memory.
 * @return Pointer to the fw_header_t structure.
 */
fw_header_t* Header_GetCurrentFwHeader(void);

/**
 * @brief Identifies and prints firmware metadata to the console.
 */
void Header_PrintMetadata(const fw_header_t *header);

void Header_PrintVersion(uint32_t version);

#ifdef __cplusplus
}
#endif

#endif /* __FW_HEADER_H */