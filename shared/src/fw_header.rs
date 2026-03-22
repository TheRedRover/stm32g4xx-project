/// Magic number identifying valid firmware: "G4FW" = 0x47344657
pub const FW_MAGIC_NUMBER: u32 = 0x4734_4657;

pub const VER_MAJOR_SHIFT: u32 = 24;
pub const VER_MINOR_SHIFT: u32 = 16;
pub const VER_PATCH_MASK: u32 = 0xFFFF;
pub const VER_BYTE_MASK: u32 = 0xFF;

/// Firmware header structure, exactly 128 bytes.
/// Binary-compatible with the C `fw_header_t` struct.
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct FwHeader {
    pub magic_number: u32,
    pub fw_size: u32,
    pub version: u32,
    pub fw_crc: u32,
    pub timestamp: u32,
    pub git_hash: [u8; 8],
    pub reserved: [u8; 96],
    pub header_crc: u32,
}

const _: () = assert!(core::mem::size_of::<FwHeader>() == 128);

/// Number of bytes covered by the header CRC (everything except header_crc itself)
pub const HEADER_CRC_LEN: usize = core::mem::size_of::<FwHeader>() - core::mem::size_of::<u32>();

/// Pack a semantic version into a single u32.
/// Format: `(major << 24) | (minor << 16) | patch`
pub const fn pack_version(major: u8, minor: u8, patch: u16) -> u32 {
    ((major as u32) << VER_MAJOR_SHIFT)
        | ((minor as u32) << VER_MINOR_SHIFT)
        | (patch as u32 & VER_PATCH_MASK)
}

/// Unpack a version u32 into (major, minor, patch).
pub const fn unpack_version(version: u32) -> (u8, u8, u16) {
    let major = ((version >> VER_MAJOR_SHIFT) & VER_BYTE_MASK) as u8;
    let minor = ((version >> VER_MINOR_SHIFT) & VER_BYTE_MASK) as u8;
    let patch = (version & VER_PATCH_MASK) as u16;
    (major, minor, patch)
}

impl FwHeader {
    /// Read a firmware header from a flash address.
    ///
    /// # Safety
    /// `addr` must point to a valid, readable memory region of at least 128 bytes.
    pub unsafe fn from_addr(addr: u32) -> Self {
        unsafe { core::ptr::read_volatile(addr as *const Self) }
    }

    /// Check if the magic number is valid.
    pub fn is_valid_magic(&self) -> bool {
        self.magic_number == FW_MAGIC_NUMBER
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn header_size_is_128() {
        assert_eq!(core::mem::size_of::<FwHeader>(), 128);
    }

    #[test]
    fn header_crc_len_is_124() {
        assert_eq!(HEADER_CRC_LEN, 124);
    }

    #[test]
    fn version_roundtrip() {
        let packed = pack_version(1, 4, 12);
        assert_eq!(packed, 0x0104_000C);

        let (major, minor, patch) = unpack_version(packed);
        assert_eq!(major, 1);
        assert_eq!(minor, 4);
        assert_eq!(patch, 12);
    }

    #[test]
    fn version_edge_cases() {
        assert_eq!(pack_version(0, 0, 0), 0x0000_0000);
        assert_eq!(pack_version(255, 255, 65535), 0xFFFF_FFFF);

        let (major, minor, patch) = unpack_version(0xFFFF_FFFF);
        assert_eq!(major, 255);
        assert_eq!(minor, 255);
        assert_eq!(patch, 65535);
    }

    #[test]
    fn magic_number_value() {
        // "G4FW" in ASCII: G=0x47, 4=0x34, F=0x46, W=0x57
        assert_eq!(FW_MAGIC_NUMBER, 0x4734_4657);
    }
}
