#ifndef __FIRST_BOOT_H
#define __FIRST_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

#include "bootloader.h"
#include "constants.h"

typedef enum
{
    FB_STAT_OK = 0,
    FB_TWIN_BL_LOCKED_INCORRECT,
    FB_BL_REPLICATION_FAILED,
    FB_LOCK_FAILED,
} FB_STATUS;

/**
 * @brief  Runs the first boot process, which includes checking the presence
 * of the second bootloader, replicating the bootloader if necessary, and
 * locking the second bootloader bank.
 * @retval FB_STATUS indicating the result of the first boot process.
 */
FB_STATUS FB_RunFirstBootProcess();

/**
 * @brief  Checks the presence of the second bootloader by checking the write
 * protection status of the second BL area. If the second bank is
 * write-protected, it indicates that the second bootloader is present and
 * valid.
 * @retval STAT_OK if the second bootloader is present and valid, FAILURE
 * otherwise.
 */
uint8_t FB_CheckAnotherBLLocked();

/**
 * @brief  Replicates the bootloader from the first bank to the second bank.
 * @retval STAT_OK on success, FAILURE on failure.
 */
uint8_t FB_ReplicateBL();

/**
 * @brief  Locks the second bootloader bank to prevent accidental erasure or
 * overwriting, and triggers a system reset to apply the changes.
 * @retval HAL status (HAL_OK on success)
 */
HAL_StatusTypeDef FB_LockTwinsBLAndReset();

#ifdef __cplusplus
}
#endif
#endif /* __FIRST_BOOT_H */
