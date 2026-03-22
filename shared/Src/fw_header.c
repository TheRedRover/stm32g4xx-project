#include "fw_header.h"

#include "stm32g4xx_hal.h"

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Identifies and prints firmware metadata to the console.
 */
fw_header_t *Header_GetCurrentFwHeader(void)
{
    uint32_t fw_start_address = SCB->VTOR;
    uint32_t header_address   = fw_start_address - FW_HDR_SIZE;

    return (fw_header_t *) header_address;
}

#include "fw_header.h"
#include <inttypes.h> // Add this include
#include <stdio.h>

#include "fw_header.h"
#include <stdio.h>

void Header_PrintMetadata(const fw_header_t *header)
{
    if (header->magic_number == FW_MAGIC_NUMBER)
    {
        printf("\r\n--- Firmware Self-Identification ---\r\n");

        /* Cast to (unsigned long) to match %lX and %lu specifiers */
        printf("Header Location: 0x%08lX\r\n", (unsigned long) (uintptr_t) header);
        printf("Magic Number:    0x%08lX (Match)\r\n", (unsigned long) header->magic_number);
        printf("Firmware Size:   %lu bytes\r\n", (unsigned long) header->fw_size);
        Header_PrintVersion(header->version);
        printf("Git Commit:      %.8s\r\n", header->git_hash);
        printf("------------------------------------\r\n");
    }
    else
    {
        printf("Error: Header not found! (Found Magic: 0x%08lX at 0x%08lX)\r\n",
               (unsigned long) header->magic_number, (unsigned long) (uintptr_t) header);
    }
}

void Header_PrintVersion(uint32_t version)
{
    uint8_t  major = (uint8_t) ((version >> VER_MAJOR_SHIFT) & VER_BYTE_MASK);
    uint8_t  minor = (uint8_t) ((version >> VER_MINOR_SHIFT) & VER_BYTE_MASK);
    uint16_t patch = (uint16_t) (version & VER_PATCH_MASK);

    printf("Firmware Version: %u.%u.%u\n", major, minor, patch);
}
