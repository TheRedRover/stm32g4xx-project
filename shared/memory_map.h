#ifdef __LINKER_SCRIPT__
  /* If included by the linker preprocessor, define plain numbers */
  #define _U(x) x
#else
  /* If included by C code, append the U suffix */
  #define _U(x) x ## U
#endif

/* Flash Base */
#define FLASH_BASE_ADDR           _U(0x08000000)

/* Bootloader */
#define BL_START_ADDR             FLASH_BASE_ADDR
#define BL_SIZE                   (32 * _U(1024))

/* Header & Partition Sizes */
#define APP_HEADER_SIZE           (1 * _U(1024))
#define SLOT_TOTAL_SIZE           (240 * _U(1024))

/* Slot 1 */
#define APP_1_HDR_ADDR            (BL_START_ADDR + BL_SIZE)
#define APP_1_CODE_ADDR           (APP_1_HDR_ADDR + APP_HEADER_SIZE)
#define APP_1_CODE_SIZE           (SLOT_TOTAL_SIZE - APP_HEADER_SIZE)

/* Slot 2 */
#define APP_2_HDR_ADDR            (APP_1_HDR_ADDR + SLOT_TOTAL_SIZE)
#define APP_2_CODE_ADDR           (APP_2_HDR_ADDR + APP_HEADER_SIZE)
#define APP_2_CODE_SIZE           (SLOT_TOTAL_SIZE - APP_HEADER_SIZE)