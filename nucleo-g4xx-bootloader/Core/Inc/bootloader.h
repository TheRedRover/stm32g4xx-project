#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32g4xx_hal.h"

#include "constants.h"
#include "fw_header.h"

typedef struct
{
    uint32_t stack_pointer; // Address + 0
    uint32_t reset_handler; // Address + 4
    uint32_t nmi_handler;   // Address + 8
} vector_table_t;

typedef enum
{
    NONE   = 0,
    SLOT_1 = 1,
    SLOT_2 = 2,
} VALID_BANK;

/**
 * @brief  Jumps to the application code located at the specified address.
 * @param[in]  app_start_addr: The starting address of the firmware code.
 * @retval None
 */
void Boot_JumpToApplication(uint32_t app_start_addr);

/**
 * @brief  Checks the magic number of the firmware header.
 * @param[in]  header: Pointer to the firmware header structure.
 * @retval STAT_OK if the magic number is valid, non-zero otherwise.
 */
uint8_t Boot_CheckMN(const fw_header_t *header);

/**
 * @brief  Checks the firmware size in the header.
 * @param[in]  header: Pointer to the firmware header structure.
 * @retval STAT_OK if the firmware size is valid, non-zero otherwise.
 */
uint8_t Boot_CheckFwSize(const fw_header_t *header);

/**
 * @brief  Compares the versions of two firmware headers.
 * @param[in]  header1: Pointer to the first firmware header structure.
 * @param[in]  header2: Pointer to the second firmware header structure.
 * @retval -1 if the version in header1 is older than header2
 * @retval 0 if the versions are the same
 * @retval 1 if the version in header1 is newer than header2
 */
int8_t Boot_CmpVersions(const fw_header_t *header1, const fw_header_t *header2);

/**
 * @brief  Validates the CRC of the firmware data.
 * @param[in] header: Pointer to the firmware header structure containing the
 * expected CRC and firmware size.
 * @retval STAT_OK if the CRC is valid, non-zero otherwise.
 */
uint8_t Boot_ValidateFwCRC(const fw_header_t *header);

/**
 * @brief  Validates the firmware signature using the header information and the
 * firmware data.
 * @param[in]  header: Pointer to the firmware header structure.
 * @param[in]  slot_addr: The starting address of the firmware slot to validate.
 * @retval STAT_OK if the firmware signature is valid, non-zero otherwise.
 */
uint8_t Boot_ValidateSignature(const fw_header_t *header);

/**
 * @brief  Validates the header and firmware data

 *         The function performs the following checks:
 *         - Validates the CRC of the firmware data.
 *         - Validates the firmware signature.
 * @param[in]  header: Pointer to the firmware header structure.
 validate.
 * @retval STAT_OK if the firmware slot is valid, non-zero otherwise.
 */
uint8_t Boot_ValidateSlot(const fw_header_t *header);

/**
 * @brief Switch the active bank by modifying the option bytes and triggering a
 * system reset.
 * @return HAL_StatusTypeDef
 */
HAL_StatusTypeDef Boot_SwitchBanks(void);

/**
 * @brief Toggles the active bank and triggers a system reset.
 * @return HAL_StatusTypeDef
 */
HAL_StatusTypeDef Boot_ToggleBank();

/**
 * @brief Chooses the bank to boot based on the firmware headers.
 * @param[in]  fw1_header: Pointer to the firmware header structure of
 * Slot 1.
 * @param[in]  fw2_header: Pointer to the firmware header structure of
 * Slot 2.
 * @retval VALID_BANK enum indicating which slot is valid (SLOT_1, SLOT_2,
 * or NONE)
 */
VALID_BANK
Boot_ChooseBankToBoot(const fw_header_t *fw1_header,
                      const fw_header_t *fw2_header);

uint8_t Boot_ValidateBLTwin(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */