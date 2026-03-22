/// Software CRC32 implementation (CRC-32/ISO-HDLC).
///
/// Matches the output of:
/// - Python `zlib.crc32(data)`
/// - STM32 hardware CRC with byte input inversion + output inversion + XOR 0xFFFFFFFF
///
/// This is used for host-side unit tests. On-target, use the hardware CRC peripheral.
pub fn crc32(data: &[u8]) -> u32 {
    let mut crc: u32 = 0xFFFF_FFFF;
    for &byte in data {
        crc ^= byte as u32;
        for _ in 0..8 {
            if crc & 1 != 0 {
                crc = (crc >> 1) ^ 0xEDB8_8320; // Reflected polynomial
            } else {
                crc >>= 1;
            }
        }
    }
    crc ^ 0xFFFF_FFFF
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc32_empty() {
        assert_eq!(crc32(&[]), 0x0000_0000);
    }

    #[test]
    fn crc32_check_value() {
        // Standard CRC-32 check value for "123456789"
        let data = b"123456789";
        assert_eq!(crc32(data), 0xCBF4_3926);
    }

    #[test]
    fn crc32_matches_crc_crate() {
        // Verify our implementation matches the `crc` crate
        let crc_algo = crc::Crc::<u32>::new(&crc::CRC_32_ISO_HDLC);

        let test_vectors: &[&[u8]] = &[
            b"",
            b"Hello World!",
            b"123456789",
            &[0u8; 128],
            &[0xFF; 256],
        ];

        for data in test_vectors {
            assert_eq!(
                crc32(data),
                crc_algo.checksum(data),
                "CRC mismatch for data of len {}",
                data.len()
            );
        }
    }
}
