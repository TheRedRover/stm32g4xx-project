use embassy_stm32::flash::{self, Flash, MAX_ERASE_SIZE, WRITE_SIZE};
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
    /// Erases the destination (sector-aligned), then copies `fw_size` bytes of
    /// firmware plus the header. Write chunks are padded to the flash write size
    /// (8 bytes) with 0xFF.
    pub fn copy_slot2_to_slot1(&mut self, fw_size: u32) -> Result<(), flash::Error> {
        // Bounds check: fw_size must fit within a slot
        if fw_size > FW_2_SIZE {
            return Err(flash::Error::Size);
        }

        let total_size = FW_HDR_SIZE + fw_size;

        // Offsets from FLASH_BASE (embassy flash API uses offsets, not absolute addresses)
        let src_offset = FW_2_HDR_ADDR - FLASH_BASE_ADDR;
        let dst_offset = FW_1_HDR_ADDR - FLASH_BASE_ADDR;

        // Erase destination — round up to next sector boundary
        let erase_end = (dst_offset + total_size + MAX_ERASE_SIZE as u32 - 1)
            & !(MAX_ERASE_SIZE as u32 - 1);
        self.flash.blocking_erase(dst_offset, erase_end)?;

        // Copy data in chunks (read from source, write to destination).
        // Write chunks must be WRITE_SIZE-aligned; pad the last chunk with 0xFF (erased flash).
        let mut buf = [0xFFu8; 256];
        let mut offset = 0u32;
        while offset < total_size {
            let remaining = (total_size - offset) as usize;
            let chunk_size = core::cmp::min(256, remaining);
            let padded_size = (chunk_size + WRITE_SIZE - 1) & !(WRITE_SIZE - 1);

            // Fill buffer with 0xFF before reading so padding bytes are erased-flash-safe
            buf[chunk_size..padded_size].fill(0xFF);

            self.flash
                .blocking_read(src_offset + offset, &mut buf[..chunk_size])?;
            self.flash
                .blocking_write(dst_offset + offset, &buf[..padded_size])?;
            offset += padded_size as u32;
        }

        Ok(())
    }
}
