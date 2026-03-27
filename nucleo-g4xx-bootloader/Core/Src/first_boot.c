#include "first_boot.h"
#include "bootloader.h"
#include "helpers.h"
#include "memory_map.h"

#include "stm32g4xx_hal.h"

#include "debug_print.h"

// TODO: implement lock logic to prevent accidental erasure of the bootloader
// bank, and trigger
FB_STATUS FB_RunFirstBootProcess()
{
  const STATUS bl_twin_valid   = Boot_ValidateBLTwin();
  const STATUS bl_twin_locked  = FB_CheckAnotherBLLocked();
  STATUS       replication_res = FAILURE;

  DBG_PRINT("Checking second bootloader twin...\r\n");
  DBG_PRINT("Twin valid: %s\n", bl_twin_valid == STAT_OK ? "valid" : "invalid");
  DBG_PRINT("Twin locked: %s\n",
            bl_twin_locked == STAT_OK ? "locked" : "unlocked");

  if (bl_twin_valid != STAT_OK && bl_twin_locked != STAT_OK)
  {
    replication_res = FB_ReplicateBL();
  }
  else if (bl_twin_valid != STAT_OK && bl_twin_locked == STAT_OK)
  {
    DBG_PRINT("The bootloader twin is locked and invalid.\r\n");
    return FB_TWIN_BL_LOCKED_INCORRECT;
  }

  // If the twin is not valid and replication also failed, return an error
  if (bl_twin_valid != STAT_OK && replication_res != STAT_OK)
  {
    DBG_PRINT("The replication of the bootloader twin failed.\r\n");
    return FB_BL_REPLICATION_FAILED;
  }
#ifdef LOCK_BL
  if (FB_LockTwinsBLAndReset() != HAL_OK)
  {
    return FB_LOCK_FAILED;
  }
#endif

  DBG_PRINT("Second bootloader is ready.\r\n");

  return STAT_OK;
}

uint8_t FB_CheckAnotherBLLocked()
{
  FLASH_OBProgramInitTypeDef OBInit;

  uint8_t active_bank = GetActiveBank();
  if (active_bank == 1)
  {
    OBInit.WRPArea = OB_WRPAREA_BANK2_AREAA;
  }
  else
  {
    OBInit.WRPArea = OB_WRPAREA_BANK1_AREAA;
  }

  HAL_FLASHEx_OBGetConfig(&OBInit);

  if (OBInit.WRPStartOffset == 0x00 &&
      OBInit.WRPEndOffset >= BL_SIZE / FLASH_PAGE_SIZE - 1)
  {
    return STAT_OK;
  }
  return FAILURE;
}

uint8_t FB_ReplicateBL()
{

  DBG_PRINT("Replicating bootloader from [%08X] to [%08X]...\r\n",
            BL_1_START_ADDR,
            BL_2_START_ADDR);

  if (FlashCopyAreaBank2Bank(BL_1_START_ADDR, BL_2_START_ADDR, BL_SIZE) ==
      HAL_OK)
  {
    __DSB();
    __ISB();

    DBG_PRINT("Bootloader replication completed successfully.\r\n");

    return Boot_ValidateBLTwin();
  }
  return FAILURE;
}

HAL_StatusTypeDef FB_LockTwinsBLAndReset()
{

  DBG_PRINT("Locking second bootloader bank and resetting...\r\n");

  FLASH_OBProgramInitTypeDef OBInit;

  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();

  HAL_FLASHEx_OBGetConfig(&OBInit);

  OBInit.OptionType   = OPTIONBYTE_WRP;
  uint8_t active_bank = GetActiveBank();
  if (active_bank == 1)
  {
    OBInit.WRPArea = OB_WRPAREA_BANK2_AREAA;
  }
  else
  {
    OBInit.WRPArea = OB_WRPAREA_BANK1_AREAA;
  }

  OBInit.WRPStartOffset = 0x00;
  OBInit.WRPEndOffset   = BL_SIZE / FLASH_PAGE_SIZE - 1;

  if (HAL_FLASHEx_OBProgram(&OBInit) != HAL_OK)
  {
    return HAL_ERROR;
  }

  HAL_FLASH_OB_Launch();

  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
  return HAL_OK;
}
