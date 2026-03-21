#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

#include "fw_header.h"

typedef struct {
    uint32_t stack_pointer;  // Address + 0
    uint32_t reset_handler;  // Address + 4
    uint32_t nmi_handler;    // Address + 8
} vector_table_t;

/**
  * @brief  Jumps to the application code located at the specified address.
  * @param[in]  app_start_addr: The starting address of the firmware code.
  * @retval None
  */
void Boot_JumpToApplication(uint32_t app_start_addr);

/**
  * @brief  Chooses the active partition for booting.
  * @param[in]  fw1_header: Pointer to the firmware header of the first partition.
  * @param[in]  fw2_header: Pointer to the firmware header of the second partition.
  * @retval The starting address of the chosen firmware partition. Returns 0 if no valid partition is found.
  */
uint32_t Boot_ChoosePartition(const fw_header_t *fw1_header, const fw_header_t *fw2_header);

/**
  * @brief  Reads the firmware header from the specified address.
  * @param[in]  header_addr: The address of the firmware header.
  * @param[out]  header: Pointer to the firmware header structure.
  * @retval None
  */
void Boot_ReadFwHeader(uint32_t header_addr, fw_header_t *header);

/**
  * @brief  Validates the firmware by calculating its CRC and comparing it to the expected value in the header.
  * @param[in]  header: Pointer to the firmware header structure.
  * @param[in]  fw_data_addr: The address of the firmware data.
  * @param[in]  fw_data_length: The length of the firmware data.
  * @retval 1 if the CRC is valid, 0 otherwise.
  */
uint8_t Boot_ValidateFirmware(const fw_header_t *header, uint32_t fw_data_addr, uint32_t fw_data_length);

/**
  * @brief  Validates the firmware header by checking its magic number and CRC.
  * @param[in]  header: Pointer to the firmware header structure.
  * @param[in]  header_addr: The address of the firmware header.
  * @param[in]  header_length: The length of the firmware header.
  * @retval 1 if the header is valid, 0 otherwise.
  */
uint8_t Boot_ValidateHeader(const fw_header_t *header, uint32_t header_addr, uint32_t header_length);



#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */