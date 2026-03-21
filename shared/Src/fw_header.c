#include "fw_header.h"

#include "stm32g4xx_hal.h"

#include <stdio.h>
#include <stdint.h>

/**
 * @brief Identifies and prints firmware metadata to the console.
 */
fw_header_t* Header_GetCurrentFwHeader(void) {
    uint32_t fw_start_address = SCB->VTOR;
    uint32_t header_address = fw_start_address - FW_HDR_SIZE;

    return (fw_header_t*)header_address;
}

#include <inttypes.h> // Add this include
#include "fw_header.h"
#include <stdio.h>

#include "fw_header.h"
#include <stdio.h>

void Header_PrintMetadata(const fw_header_t *header) {
    if (header->magic_number == FW_MAGIC_NUMBER) {
        printf("\r\n--- Firmware Self-Identification ---\r\n");
        
        /* Cast to (unsigned long) to match %lX and %lu specifiers */
        printf("Header Location: 0x%08lX\r\n", (unsigned long)(uintptr_t)header);
        printf("Magic Number:    0x%08lX (Match)\r\n", (unsigned long)header->magic_number);
        printf("Version:         %lu\r\n", (unsigned long)header->version);
        
        printf("Git Commit:      %.8s\r\n", header->git_hash);
        printf("------------------------------------\r\n");
    } else {
        printf("Error: Header not found! (Found Magic: 0x%08lX at 0x%08lX)\r\n", 
                (unsigned long)header->magic_number, (unsigned long)(uintptr_t)header);
    }
}