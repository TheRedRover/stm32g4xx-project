#ifndef __MEMORY_MAP_H
#define __MEMORY_MAP_H

#ifdef __cplusplus
 extern "C" {
#endif


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
#define FW_HEADER_SIZE           (1 * _U(1024))
#define SLOT_TOTAL_SIZE          (240 * _U(1024))

/* Slot 1 */
#define FW_1_HDR_ADDR       (BL_START_ADDR + BL_SIZE)
#define FW_1_ADDR           (FW_1_HDR_ADDR + FW_HEADER_SIZE)
#define FW_1_SIZE           (SLOT_TOTAL_SIZE - FW_HEADER_SIZE)

/* Slot 2 */
#define FW_2_HDR_ADDR       (FW_1_HDR_ADDR + SLOT_TOTAL_SIZE)
#define FW_2_ADDR           (FW_2_HDR_ADDR + FW_HEADER_SIZE)
#define FW_2_SIZE           (SLOT_TOTAL_SIZE - FW_HEADER_SIZE)


#ifdef __cplusplus
}
#endif

#endif /* __MEMORY_MAP_H */