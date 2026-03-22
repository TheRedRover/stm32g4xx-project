use embassy_stm32::flash::{self, Flash};
use embassy_stm32::Peri;

use shared::memory_map::*;

/// Flash operations for firmware copy-update.
pub struct FlashOps<'d> {
    flash: Flash<'d, flash::Blocking>,
}

impl<'d> FlashOps<'d> {
    pub fn new(peripheral: Peri<'d, embassy_stm32::peripherals::FLASH>) -> Self {
        Self {
            flash: Flash::new_blocking(peripheral),
        }
    }

    /// Copy firmware from Slot 2 to Slot 1 (header + firmware data).
    ///
    /// Erases the destination, then copies `fw_size` bytes of firmware plus the header.
    pub fn copy_slot2_to_slot1(&mut self, fw_size: u32) -> Result<(), flash::Error> {
        let total_size = FW_HDR_SIZE + fw_size;

        // Offsets from FLASH_BASE (embassy flash API uses offsets, not absolute addresses)
        let src_offset = FW_2_HDR_ADDR - FLASH_BASE_ADDR;
        let dst_offset = FW_1_HDR_ADDR - FLASH_BASE_ADDR;

        // Erase destination (Slot 1 header + firmware area)
        self.flash.blocking_erase(dst_offset, dst_offset + total_size)?;

        // Copy data in chunks (read from source, write to destination)
        // Embassy flash doesn't have a direct memory-to-flash copy,
        // so we read from flash into a buffer and write it back.
        let mut buf = [0u8; 256];
        let mut offset = 0u32;
        while offset < total_size {
            let chunk_size = core::cmp::min(256, (total_size - offset) as usize);
            self.flash
                .blocking_read(src_offset + offset, &mut buf[..chunk_size])?;
            self.flash
                .blocking_write(dst_offset + offset, &buf[..chunk_size])?;
            offset += chunk_size as u32;
        }

        Ok(())
    }
}
