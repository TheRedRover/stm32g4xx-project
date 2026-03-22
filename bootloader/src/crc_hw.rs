use embassy_stm32::crc::{Config, Crc, InputReverseConfig, PolySize};
use embassy_stm32::Peri;

/// Hardware CRC32 wrapper.
///
/// Configured to produce standard CRC-32/ISO-HDLC output matching `zlib.crc32()`.
/// The STM32 hardware CRC peripheral with:
/// - Default polynomial (0x04C11DB7)
/// - Init value 0xFFFFFFFF
/// - Byte-level input bit reversal
/// - Output bit reversal
/// produces CRC-32 with reflected I/O. After XOR with 0xFFFFFFFF, this matches
/// the standard CRC-32.
pub struct HwCrc<'d> {
    inner: Crc<'d>,
}

impl<'d> HwCrc<'d> {
    pub fn new(peripheral: Peri<'d, embassy_stm32::peripherals::CRC>) -> Self {
        let config = Config::new(
            InputReverseConfig::Byte, // Byte-level input reversal
            true,                     // Output reversal
            PolySize::Width32,        // CRC-32
            0xFFFF_FFFF,              // Init value
            0x04C1_1DB7,              // Standard CRC-32 polynomial
        )
        .unwrap();

        Self {
            inner: Crc::new(peripheral, config),
        }
    }

    /// Calculate CRC32 over a byte slice.
    /// Returns standard CRC-32 value (after XOR with 0xFFFFFFFF).
    pub fn calculate(&mut self, data: &[u8]) -> u32 {
        self.inner.reset();
        let raw = self.inner.feed_bytes(data);
        raw ^ 0xFFFF_FFFF
    }
}
