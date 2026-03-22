/// Flash base address
pub const FLASH_BASE_ADDR: u32 = 0x0800_0000;

/// Total flash size: 512 KB
pub const TOTAL_FLASH_SIZE: u32 = 512 * 1024;

/// Bootloader start address (at flash base)
pub const BL_START_ADDR: u32 = FLASH_BASE_ADDR;

/// Bootloader size: 32 KB
pub const BL_SIZE: u32 = 32 * 1024;

/// Firmware header size: 1 KB
pub const FW_HDR_SIZE: u32 = 1 * 1024;

/// Total size per slot (header + firmware): 240 KB
pub const SLOT_TOTAL_SIZE: u32 = (TOTAL_FLASH_SIZE - BL_SIZE) / 2;

/// Flash page size: 2 KB
pub const FLASH_PAGE_SIZE: u32 = 2048;

// --- Slot 1 ---

/// Slot 1 header address
pub const FW_1_HDR_ADDR: u32 = BL_START_ADDR + BL_SIZE;

/// Slot 1 firmware address (after header)
pub const FW_1_ADDR: u32 = FW_1_HDR_ADDR + FW_HDR_SIZE;

/// Slot 1 firmware max size
pub const FW_1_SIZE: u32 = SLOT_TOTAL_SIZE - FW_HDR_SIZE;

// --- Slot 2 ---

/// Slot 2 header address
pub const FW_2_HDR_ADDR: u32 = FW_1_HDR_ADDR + SLOT_TOTAL_SIZE;

/// Slot 2 firmware address (after header)
pub const FW_2_ADDR: u32 = FW_2_HDR_ADDR + FW_HDR_SIZE;

/// Slot 2 firmware max size
pub const FW_2_SIZE: u32 = SLOT_TOTAL_SIZE - FW_HDR_SIZE;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn memory_map_addresses() {
        assert_eq!(FLASH_BASE_ADDR, 0x0800_0000);
        assert_eq!(TOTAL_FLASH_SIZE, 0x0008_0000); // 512K
        assert_eq!(BL_SIZE, 0x0000_8000); // 32K
        assert_eq!(FW_HDR_SIZE, 0x0000_0400); // 1K
        assert_eq!(SLOT_TOTAL_SIZE, 0x0003_C000); // 240K

        assert_eq!(FW_1_HDR_ADDR, 0x0800_8000);
        assert_eq!(FW_1_ADDR, 0x0800_8400);
        assert_eq!(FW_1_SIZE, 0x0003_BC00); // 239K

        assert_eq!(FW_2_HDR_ADDR, 0x0804_4000);
        assert_eq!(FW_2_ADDR, 0x0804_4400);
        assert_eq!(FW_2_SIZE, 0x0003_BC00); // 239K
    }

    #[test]
    fn partitions_fit_in_flash() {
        let end_of_slot2 = FW_2_HDR_ADDR + SLOT_TOTAL_SIZE;
        let flash_end = FLASH_BASE_ADDR + TOTAL_FLASH_SIZE;
        assert_eq!(end_of_slot2, flash_end);
    }
}
