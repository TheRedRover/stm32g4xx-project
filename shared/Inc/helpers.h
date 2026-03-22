#ifndef __HELPERS_H
#define __HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "memory_map.h"
#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief Copies data from one Flash area to another, handling non-8-byte
 * aligned lengths.
 * @param src_addr  Source address in Flash.
 * @param dest_addr Destination address in Flash (MUST be page-aligned).
 * @param len   Number of bytes to copy.
 * @retval HAL_StatusTypeDef HAL status
 */
HAL_StatusTypeDef FlashCopyAreaBank2Bank(uint32_t src_addr,
                                         uint32_t dest_addr,
                                         uint32_t len);

uint8_t GetActiveBank();

#ifdef __cplusplus
}
#endif

#endif /* __HELPERS_H */