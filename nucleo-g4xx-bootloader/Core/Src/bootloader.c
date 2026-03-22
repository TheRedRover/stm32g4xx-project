
#include "bootloader.h"

#include "stm32g4xx_hal.h"

#include "debug_print.h"
#include "helpers.h"
#include "memory_map.h"
#include "public_key.h"

#include <tinycrypt/sha256.h>
#include <uECC.h>

#include <stdint.h>
#include <string.h>

#define HASH_CHUNK_SIZE 256
#define uECC_PLATFORM uECC_arm_thumb2

typedef void (*pFunction)(void);

__attribute__((naked, noinline)) static void do_jump(uint32_t sp, uint32_t pc)
{
  __asm__ volatile("msr msp, r0    \n" // sp is in r0
                   "dsb            \n"
                   "isb            \n"
                   "bx  r1         \n" // pc is in r1
                   :
                   :
                   : "memory");
}

void Boot_JumpToApplication(uint32_t start_addr)
{
  volatile vector_table_t *app_vectors = (vector_table_t *) start_addr;

  if ((app_vectors->stack_pointer & 0xFF000000U) != 0x20000000U)
  {

    DBG_PRINT("Invalid stack pointer in vector table: %08" PRIX32 "\n",
              app_vectors->stack_pointer);

    return;
  }

  if ((app_vectors->reset_handler & 0xFF000000U) != 0x08000000U)
  {

    DBG_PRINT("Invalid reset handler in vector table: %08" PRIX32 "\n",
              app_vectors->reset_handler);

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

uint8_t Boot_CheckMN(const fw_header_t *header)
{
  return (header->magic_number == FW_MAGIC_NUMBER) ? STAT_OK : FAILURE;
}

uint8_t Boot_CheckFwSize(const fw_header_t *header)
{
  return (header->fw_size > 0 && header->fw_size <= FW_SIZE) ? STAT_OK
                                                             : FAILURE;
}

int8_t Boot_CmpVersions(const fw_header_t *header1, const fw_header_t *header2)
{
  if (header1->version > header2->version)
  {
    return 1; // header1 is newer
  }
  else if (header1->version < header2->version)
  {
    return -1; // header1 is older
  }

  return 0; // versions are the same
}

static uint32_t Internal_ComputeCRC(const uint32_t *data, uint32_t length)
{
  uint32_t          result = 0;
  CRC_HandleTypeDef hcrc   = {0};

  hcrc.Instance                     = CRC;
  hcrc.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_BYTE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
  hcrc.InputDataFormat              = CRC_INPUTDATA_FORMAT_BYTES;

  if (HAL_CRC_Init(&hcrc) == HAL_OK)
  {
    result = HAL_CRC_Calculate(&hcrc, data, length) ^ 0xFFFFFFFF;
    HAL_CRC_DeInit(&hcrc);
  }

  return result;
}

uint8_t Boot_ValidateFwCRC(const fw_header_t *header)
{
  if (header->crc == 0 || header->fw_size == 0)
  {
    return FAILURE;
  }

  const uint32_t *start_pos =
      (const uint32_t *) ((const uint8_t *) header +
                          offsetof(fw_header_t, magic_number));
  const uint32_t total_len =
      (sizeof(fw_header_t) - offsetof(fw_header_t, magic_number)) +
      header->fw_size;

  uint32_t calculated = Internal_ComputeCRC(start_pos, total_len);

  DBG_PRINT("CRC Calc: %08" PRIX32 " | Exp: %08" PRIX32 " | Len: %" PRIu32 "\n",
            calculated,
            header->crc,
            total_len);
  return (calculated == header->crc) ? STAT_OK : FAILURE;
}

/**
 * @brief  This function is compute sha256 hash of the firmware for signature
 * verification.
 * @param  header: pointer to the firmware header
 */
static uint8_t calculate_sha256(const fw_header_t *header,
                                uint8_t digest[TC_SHA256_DIGEST_SIZE])
{

  DBG_PRINT("Calculating SHA256...\r\n");

  struct tc_sha256_state_struct ctx;
  if (!tc_sha256_init(&ctx))
  {

    DBG_PRINT("SHA256 initialization failed.\r\n");

    return FAILURE;
  }

  const uint8_t *ptr = (const uint8_t *) header + offsetof(fw_header_t, crc);
  uint32_t       remaining =
      (sizeof(fw_header_t) - offsetof(fw_header_t, crc)) + header->fw_size;

  while (remaining > 0)
  {
    uint32_t chunk =
        (remaining > HASH_CHUNK_SIZE) ? HASH_CHUNK_SIZE : remaining;

    if (!tc_sha256_update(&ctx, ptr, chunk))
    {

      DBG_PRINT("SHA256 update failed at chunk starting at offset %" PRIu32
                "\n",
                (uint32_t) (ptr - (const uint8_t *) header));

      return FAILURE;
    }

    ptr += chunk;
    remaining -= chunk;
  }

  DBG_PRINT("SHA256 calculation completed.\r\n");

  return tc_sha256_final(digest, &ctx) ? STAT_OK : FAILURE;
}

uint8_t Boot_ValidateSignature(const fw_header_t *header)
{

  DBG_PRINT("Validating firmware signature...\r\n");

  uint8_t hash[TC_SHA256_DIGEST_SIZE];
  if (calculate_sha256(header, hash) != STAT_OK)
  {

    DBG_PRINT("SHA256 calculation failed.\r\n");

    return FAILURE;
  }

  const struct uECC_Curve_t *curve = uECC_secp256r1();

  DBG_PRINT("Verifying signature with uECC...\r\n");

  return uECC_verify(g_public_key, hash, sizeof(hash), header->signature, curve)
             ? STAT_OK
             : FAILURE;
}

uint8_t Boot_ValidateSlot(const fw_header_t *header)
{

  DBG_PRINT("Validating firmware slot at header address: %08" PRIX32 "\n",
            (uint32_t) header);

  return (Boot_CheckMN(header) == STAT_OK &&
          Boot_CheckFwSize(header) == STAT_OK &&
          Boot_ValidateFwCRC(header) == STAT_OK &&
          Boot_ValidateSignature(header) == STAT_OK)
             ? STAT_OK
             : FAILURE;
}

HAL_StatusTypeDef Boot_ToggleBank()
{

  DBG_PRINT("=========================\r\n");
  DBG_PRINT("|Toggling active bank...|\r\n");
  DBG_PRINT("=========================\r\n");

  FLASH_OBProgramInitTypeDef OBInit;
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
  HAL_FLASH_OB_Unlock();
  HAL_FLASHEx_OBGetConfig(&OBInit);

  OBInit.OptionType = OPTIONBYTE_USER;
  OBInit.USERType   = OB_USER_BFB2;

  if (((OBInit.USERConfig) & (OB_BFB2_ENABLE)) == OB_BFB2_ENABLE)
  {
    OBInit.USERConfig = OB_BFB2_DISABLE;
  }
  else
  {
    OBInit.USERConfig = OB_BFB2_ENABLE;
  }

  do
  {
    if (HAL_FLASHEx_OBProgram(&OBInit) != HAL_OK)
    {
      break;
    }
    if (HAL_FLASH_OB_Launch() != HAL_OK)
    {
      break;
    }

  } while (0);

  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();

  // we should never reach this point as the system will reset before, but just
  // in case, report success
  return HAL_OK;
}

VALID_BANK Boot_ChooseBankToBoot(const fw_header_t *fw1_header,
                                 const fw_header_t *fw2_header)
{

  DBG_PRINT("Boot_ChooseBankToBoot\r\n");

  uint8_t b1_layout = (Boot_CheckMN(fw1_header) == STAT_OK &&
                       Boot_CheckFwSize(fw1_header) == STAT_OK);
  uint8_t b2_layout = (Boot_CheckMN(fw2_header) == STAT_OK &&
                       Boot_CheckFwSize(fw2_header) == STAT_OK);

  uint8_t slot2_is_newer = (b1_layout && b2_layout) &&
                           (Boot_CmpVersions(fw2_header, fw1_header) > 0);

  if (slot2_is_newer)
  {

    DBG_PRINT("Slot 2 has a newer version than Slot 1.\r\n");

    if (Boot_ValidateSlot(fw2_header) == STAT_OK)
    {
      return SLOT_2; // Bank switch required
    }
    else
    {
      // Optional: mark slot as invalid to prevent next checks
      // e.g. remove magic number or set fw_size to 0
    }
  }

  DBG_PRINT(
      "Slot 2 is not newer than Slot 1, or one of the slots has an invalid "
      "layout.\r\n");

  if (Boot_ValidateSlot(fw1_header) == STAT_OK)
  {
    return SLOT_1;
  }
  else if (!slot2_is_newer && b2_layout)
  {

    DBG_PRINT("Slot 1 is not valid, but Slot 2 has a valid layout. Checking "
              "signature...\r\n");

    // Optional: mark slot as invalid to prevent next checks
    // e.g. remove magic number or set fw_size to 0
    if (Boot_ValidateSlot(fw2_header) == STAT_OK)
    {
      return SLOT_2;
    }
    else
    {
      // Optional: mark slot as invalid to prevent next checks
      // e.g. remove magic number or set fw_size to 0
    }
  }

  return NONE; // No valid firmware found
}

uint8_t Boot_ValidateBLTwin()
{

  DBG_PRINT("Validating bootloader twin...\r\n");

  const uint32_t current_crc =
      Internal_ComputeCRC((const uint32_t *) BANK1_START_ADDR, BL_SIZE);
  const uint32_t twin_crc =
      Internal_ComputeCRC((const uint32_t *) BANK2_START_ADDR, BL_SIZE);

  DBG_PRINT("Bootloader twin CRCs: current=%08" PRIX32 ", twin=%08" PRIX32 "\n",
            current_crc,
            twin_crc);

  return (current_crc == twin_crc) && (current_crc != 0) ? STAT_OK : FAILURE;
}
