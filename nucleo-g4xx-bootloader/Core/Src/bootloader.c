
#include "bootloader.h"

#include "stm32g4xx_hal.h"

#include "memory_map.h"

#include <stdint.h>
#include <stdio.h>

typedef void (*pFunction)(void);

__attribute__((naked, noinline)) static void do_jump(uint32_t sp, uint32_t pc)
{
  __asm volatile("msr msp, %0    \n"
                 "dsb            \n"
                 "isb            \n"
                 "bx  %1         \n"
                 :
                 : "r"(sp), "r"(pc)
                 : "memory");
}

void Boot_JumpToApplication(uint32_t start_addr)
{
  volatile vector_table_t *app_vectors = (vector_table_t *) start_addr;

  if ((app_vectors->stack_pointer & 0xFF000000U) != 0x20000000U)
  {
#ifdef DEBUG
    printf("Invalid stack pointer in vector table: %08lX\n", app_vectors->stack_pointer);
#endif
    return;
  }

  if ((app_vectors->reset_handler & 0xFF000000U) != 0x08000000U)
  {
#ifdef DEBUG
    printf("Invalid reset handler in vector table: %08lX\n", app_vectors->reset_handler);
#endif
    return;
  }

  __disable_irq();

  HAL_RCC_DeInit();
  HAL_DeInit();

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL  = 0;

  for (uint32_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  __DSB();
  __ISB();

  SCB->VTOR = start_addr;
  __DSB();
  __ISB();

  do_jump(app_vectors->stack_pointer, app_vectors->reset_handler);
}

void Boot_ReadFwHeader(uint32_t header_addr, fw_header_t *header)
{
  *header = *((fw_header_t *) header_addr);
}

uint8_t Boot_ValidateFirmware(const fw_header_t *header,
                              uint32_t           fw_data_addr,
                              uint32_t           fw_data_length)
{
  if (header->magic_number != FW_MAGIC_NUMBER || header->fw_size == 0)
  {
    return 0;
  }

  if (header->magic_number != FW_MAGIC_NUMBER || fw_data_length == 0)
    return 0;

  CRC_HandleTypeDef hcrc            = {0};
  hcrc.Instance                     = CRC;
  hcrc.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_BYTE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
  hcrc.InputDataFormat              = CRC_INPUTDATA_FORMAT_BYTES;

  if (HAL_CRC_Init(&hcrc) != HAL_OK)
    return 0;

  uint32_t calculated = HAL_CRC_Calculate(&hcrc, (uint32_t *) fw_data_addr, fw_data_length);

  calculated ^= 0xFFFFFFFF;
#ifdef DEBUG
  printf("CRC Calc: %08lX | Exp: %08lX | Addr: %08lX\n", calculated, header->fw_crc, fw_data_addr);
#endif
  HAL_CRC_DeInit(&hcrc);
  return (calculated == header->fw_crc);
}

uint8_t Boot_ValidateHeader(const fw_header_t *header, uint32_t header_addr)
{
  if (header->magic_number != FW_MAGIC_NUMBER)
  {
    return 0;
  }

  CRC_HandleTypeDef hcrc            = {0};
  hcrc.Instance                     = CRC;
  hcrc.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_BYTE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
  hcrc.InputDataFormat              = CRC_INPUTDATA_FORMAT_BYTES;

  if (HAL_CRC_Init(&hcrc) != HAL_OK)
    return 0;

  uint32_t calculated =
      HAL_CRC_Calculate(&hcrc, (uint32_t *) header_addr, sizeof(fw_header_t) - sizeof(uint32_t));
  calculated ^= 0xFFFFFFFF;

  HAL_CRC_DeInit(&hcrc);
#ifdef DEBUG
  printf("Calculated Header CRC vs Expected Header CRC [%08lX] [%08lX]\n",
         calculated,
         header->header_crc);
#endif
  // Compare with expected CRC in the header
  return (calculated == header->header_crc);
}

HAL_StatusTypeDef Boot_PerformCopyUpdate(uint32_t src_addr, uint32_t dst_addr, uint32_t size)
{
  HAL_StatusTypeDef      status     = HAL_OK;
  uint32_t               page_error = 0;
  FLASH_EraseInitTypeDef erase_init;

  HAL_FLASH_Unlock();

  // Calculate how many pages to erase based on size
  uint32_t num_pages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks     = FLASH_BANK_1;
  erase_init.Page      = (dst_addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  erase_init.NbPages   = num_pages;

#ifdef DEBUG
  printf("Erasing partition: %ld pages...\r\n", num_pages);
#endif
  status = HAL_FLASHEx_Erase(&erase_init, &page_error);
  if (status != HAL_OK)
  {
    HAL_FLASH_Lock();
    return status;
  }

#ifdef DEBUG
  printf("Copying data...\r\n");
#endif
  for (uint32_t i = 0; i < size; i += 8)
  {
    uint64_t data = *(volatile uint64_t *) (src_addr + i);
    status        = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dst_addr + i, data);

    if (status != HAL_OK)
    {
#ifdef DEBUG
      printf("Error programming flash at address: %08lX\n", dst_addr + i);
#endif
      break;
    }
  }

  HAL_FLASH_Lock();
  return status;
}

uint8_t Boot_CheckForFirmwareUpdate(void)
{
  // This function can be implemented to check for a specific condition (e.g., a GPIO pin state, a
  // command received over UART, etc.) For demonstration purposes, we'll just return 0 (no update)
  return 0;
}