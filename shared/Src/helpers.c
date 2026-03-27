#include "helpers.h"
#include <string.h>

#include "debug_print.h"
#include <stdint.h>
#include <string.h>

static inline uint32_t min_u32(uint32_t a, uint32_t b)
{
  return (a < b) ? a : b;
}

static uint32_t FlashGetBank(uint32_t addr)
{
  return (addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_1 : FLASH_BANK_2;
}

static uint32_t FlashGetPage(uint32_t addr)
{
  if (addr < (FLASH_BASE + FLASH_BANK_SIZE))
  {
    return (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  }
  else
  {
    return (addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
  }
}

HAL_StatusTypeDef FlashCopyAreaBank2Bank(uint32_t src_addr,
                                         uint32_t dest_addr,
                                         uint32_t len)
{
  HAL_StatusTypeDef      status;
  FLASH_EraseInitTypeDef erase      = {0};
  uint32_t               page_error = 0;
  uint32_t               first_page, last_page, nb_pages;
  uint64_t               data64;
  uint32_t               remaining;

  if (len == 0U)
  {
    return HAL_OK;
  }

  /* STM32G4 flash programming is 8-byte aligned */
  DBG_PRINT("Copying flash area: src=[%08" PRIX32 "], dest=[%08" PRIX32
         "], len=[%" PRIu32 "] bytes\r\n",
         (uint32_t) src_addr,
         (uint32_t) dest_addr,
         (uint32_t) len);
  if (((src_addr & 0x7U) != 0U) || ((dest_addr & 0x7U) != 0U))
  {
    return HAL_ERROR;
  }

  DBG_PRINT("  Flash copy: src=[%08" PRIX32 "], dest=[%08" PRIX32
            "], len=[%" PRIu32 "] bytes\r\n",
            (uint32_t) src_addr,
            (uint32_t) dest_addr,
            (uint32_t) len);
  
  /* Basic overflow checks */
  if ((src_addr + len) < src_addr || (dest_addr + len) < dest_addr)
  {
    return HAL_ERROR;
  }

  /* Erase destination pages first */
  first_page = FlashGetPage(dest_addr);
  last_page  = FlashGetPage(dest_addr + len - 1U);
  nb_pages   = (last_page - first_page) + 1U;

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks     = FlashGetBank(dest_addr);
  erase.Page      = first_page;
  erase.NbPages   = nb_pages;

  DBG_PRINT("  Erasing flash: bank=%" PRIu32 ", first_page=%" PRIu32
         ", nb_pages=%" PRIu32 "\r\n",
         (uint32_t) erase.Banks,
         (uint32_t) erase.Page,
         (uint32_t) erase.NbPages);

  status = HAL_FLASHEx_Erase(&erase, &page_error);
  if (status != HAL_OK)
  {
    HAL_FLASH_Lock();
    return status;
  }

  /* Program copied data in double-words */
  DBG_PRINT("  Programming flash: src=[%08" PRIX32 "], dest=[%08" PRIX32
            "], len=[%" PRIu32 "] bytes\r\n",
            (uint32_t) src_addr,
            (uint32_t) dest_addr,
            (uint32_t) len);

  remaining = len;
  while (remaining >= 8U)
  {
    memcpy(&data64, (const void *) src_addr, sizeof(data64));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest_addr, data64);
    if (status != HAL_OK)
    {
      HAL_FLASH_Lock();
      return status;
    }

    src_addr += 8U;
    dest_addr += 8U;
    remaining -= 8U;
  }

  DBG_PRINT("  Remaining bytes to copy: %" PRIu32 "\r\n", (uint32_t) remaining);

  /* Tail bytes: pad with erased value 0xFF */
  if (remaining > 0U)
  {
    uint8_t tail[8];

    memset(tail, 0xFF, sizeof(tail));
    memcpy(tail, (const void *) src_addr, remaining);
    memcpy(&data64, tail, sizeof(data64));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest_addr, data64);
    if (status != HAL_OK)
    {

      DBG_PRINT("  Flash programming error: src=[%08" PRIX32 "], dest=[%08" PRIX32
             "], len=[%" PRIu32 "] "
             "bytes\r\n",
             (uint32_t) src_addr,
             (uint32_t) dest_addr,
             (uint32_t) len);

      HAL_FLASH_Lock();
      return status;
    }
  }

  DBG_PRINT("Flash copy completed successfully.\r\n");

  HAL_FLASH_Lock();
  return HAL_OK;
}

uint8_t GetActiveBank()
{
  volatile uint32_t remap = READ_BIT(SYSCFG->MEMRMP, 0x1 << 8);
  return remap == 0 ? FLASH_BANK_1 : FLASH_BANK_2;
}