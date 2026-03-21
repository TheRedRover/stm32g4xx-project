#include "bootloader.h"

#include "memory_map.h"

#include "stm32g4xx_hal.h"

typedef void (*pFunction)(void);


void Boot_JumpToApplication(uint32_t start_addr)
{
    pFunction appMainFunction;
    volatile vector_table_t *app_vectors = (vector_table_t *)start_addr;

    // Sanity check
    if ((app_vectors->stack_pointer & 0xFF000000U) != 0x20000000U) {
        return;
    }

    if ((app_vectors->reset_handler & 0xFF000000U) != 0x08000000U) {
        return;
    }

    // Disable interrupts
    __disable_irq();

    // De-initialize peripherals and reset clock configuration
    HAL_DeInit();
    HAL_RCC_DeInit();

    // Disable SysTick timer and reset its registers
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // Disable all external interrupts and clear pending flags
    for (uint32_t i = 0; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    // Relocate Vector Table (VTOR) to the application start address
    SCB->VTOR = start_addr;

    // Final CPU State Preparation
    __set_CONTROL(0); // Switch to privileged mode using MSP
    __set_MSP(app_vectors->stack_pointer); // Set Main Stack Pointer
    __ISB(); // Instruction Synchronization Barrier

    // Jump to the application's Reset_Handler
    appMainFunction = (pFunction)(app_vectors->reset_handler);
    appMainFunction();
}

uint32_t Boot_ChoosePartition(const fw_header_t *fw1_header, const fw_header_t *fw2_header) {
    uint32_t fw1_valid = (fw1_header->magic_number == FW_HEADER_MAGIC_NUMBER) &&
                                 (fw1_header->fw_size > 0);
    uint32_t fw2_valid = (fw2_header->magic_number == FW_HEADER_MAGIC_NUMBER) &&
                                 (fw2_header->fw_size > 0);

    fw1_valid = fw1_valid && Boot_ValidateFirmware(fw1_header, FW_1_ADDR, fw1_header->fw_size);
    fw2_valid = fw2_valid && Boot_ValidateFirmware(fw2_header, FW_2_ADDR, fw2_header->fw_size);

    fw1_valid = fw1_valid && Boot_ValidateHeader(fw1_header, FW_1_HDR_ADDR, sizeof(fw_header_t));
    fw2_valid = fw2_valid && Boot_ValidateHeader(fw2_header, FW_2_HDR_ADDR, sizeof(fw_header_t));

    if (fw1_valid && fw2_valid) {
        // If both are valid, choose the one with the higher version
        return (fw1_header->version >= fw2_header->version) ? FW_1_ADDR : FW_2_ADDR;
    } else if (fw1_valid) {
        return FW_1_ADDR;
    } else if (fw2_valid) {
        return FW_2_ADDR;
    }

    // No valid firmware found
    return 0;
}

void Boot_ReadFwHeader(uint32_t header_addr, fw_header_t *header) {
    *header = *((fw_header_t *)header_addr);
}

uint8_t Boot_ValidateFirmware(const fw_header_t *header, uint32_t fw_data_addr, uint32_t fw_data_length) {
    if (header->magic_number != FW_HEADER_MAGIC_NUMBER || header->fw_size == 0) {
        return 0;
    }

    CRC_HandleTypeDef hcrc;

    HAL_CRC_Init(&hcrc);

    // Calculate CRC of the firmware data
    const uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)fw_data_addr, fw_data_length / 4);

    HAL_CRC_DeInit(&hcrc);
    // Compare with expected CRC in the header
    return (calculated_crc == header->fw_crc);
}

uint8_t Boot_ValidateHeader(const fw_header_t *header, uint32_t header_addr, uint32_t header_length) {
    if (header->magic_number != FW_HEADER_MAGIC_NUMBER) {
        return 0;
    }

    CRC_HandleTypeDef hcrc;
    HAL_CRC_Init(&hcrc);

    // Calculate CRC of the header (excluding the CRC field itself)
    const uint32_t header_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)header, sizeof(fw_header_t) - sizeof(uint32_t));

    HAL_CRC_DeInit(&hcrc);
    // Compare with expected CRC in the header
    return (header_crc == header->header_crc);
}