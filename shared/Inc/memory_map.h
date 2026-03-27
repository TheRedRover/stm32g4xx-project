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
#define _U(x) x##U
#endif

#define FLASH_BASE_ADDR _U(0x08000000)
#define BANK_SIZE (256 * _U(1024))
#define PAGE_SIZE _U(2048)

//
// SIZES
//

#define BL_SIZE (16 * PAGE_SIZE)          // 32 KB
#define STORAGE_PAGE_SIZE (4 * PAGE_SIZE) // 8 KB
#define FW_HDR_SIZE _U(128)               // 128 Bytes

#define SLOT_TOTAL_SIZE (BANK_SIZE - BL_SIZE - STORAGE_PAGE_SIZE) // 216 KB
#define FW_SIZE (SLOT_TOTAL_SIZE - FW_HDR_SIZE)

//
// ADDRESSES
//

// BANK 1
#define BANK1_START_ADDR (FLASH_BASE_ADDR)
#define BL_1_START_ADDR (BANK1_START_ADDR)
#define FW_1_HDR_ADDR (BL_1_START_ADDR + BL_SIZE)
#define FW_1_ADDR (FW_1_HDR_ADDR + FW_HDR_SIZE)
#define STORAGE_PAGE1_ADDR (FLASH_BASE_ADDR + BANK_SIZE - STORAGE_PAGE_SIZE)

// BANK 2
#define BANK2_START_ADDR (FLASH_BASE_ADDR + BANK_SIZE)
#define BL_2_START_ADDR (BANK2_START_ADDR)
#define FW_2_HDR_ADDR (BL_2_START_ADDR + BL_SIZE)
#define FW_2_ADDR (FW_2_HDR_ADDR + FW_HDR_SIZE)
#define STORAGE_PAGE2_ADDR (BANK2_START_ADDR + BANK_SIZE - STORAGE_PAGE_SIZE)

#ifdef __cplusplus
}
#endif

#endif /* __MEMORY_MAP_H */