#![no_std]
#![no_main]
#![feature(never_type)]

use defmt_rtt as _;
use panic_halt as _;

use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_stm32::rcc::{self, Pll, PllPreDiv, PllMul, PllRDiv, PllSource};

use shared::fw_header::{FwHeader, HEADER_CRC_LEN};
use shared::memory_map::*;

mod crc_hw;
mod error;
mod flash;
mod jump;

fn make_config() -> embassy_stm32::Config {
    let mut config = embassy_stm32::Config::default();

    // HSI (16 MHz) -> PLL: M=/4 -> 4MHz, N=*85 -> 340MHz VCO, R=/2 -> 170MHz SYSCLK
    config.rcc.hsi = true;
    config.rcc.sys = rcc::Sysclk::PLL1_R;
    config.rcc.boost = true;
    config.rcc.pll = Some(Pll {
        source: PllSource::HSI,
        prediv: PllPreDiv::DIV4,
        mul: PllMul::MUL85,
        divp: None,
        divq: None,
        divr: Some(PllRDiv::DIV2),
    });

    config
}

/// Validate the firmware header CRC.
fn validate_header(crc: &mut crc_hw::HwCrc<'_>, header: &FwHeader, header_addr: u32) -> bool {
    if !header.is_valid_magic() {
        return false;
    }

    // CRC over first 124 bytes of the header (everything except header_crc)
    let header_bytes = unsafe {
        core::slice::from_raw_parts(header_addr as *const u8, HEADER_CRC_LEN)
    };
    let calculated = crc.calculate(header_bytes);
    let expected = { header.header_crc }; // Copy from packed struct
    calculated == expected
}

/// Validate the firmware data CRC.
fn validate_firmware(
    crc: &mut crc_hw::HwCrc<'_>,
    header: &FwHeader,
    fw_addr: u32,
    max_fw_size: u32,
) -> bool {
    if !header.is_valid_magic() {
        return false;
    }

    let fw_size = { header.fw_size }; // Copy from packed struct
    if fw_size == 0 || fw_size > max_fw_size {
        return false;
    }

    let fw_bytes = unsafe { core::slice::from_raw_parts(fw_addr as *const u8, fw_size as usize) };
    let calculated = crc.calculate(fw_bytes);
    let expected = { header.fw_crc }; // Copy from packed struct

    defmt::debug!(
        "CRC calc={:#010X} exp={:#010X} addr={:#010X}",
        calculated,
        expected,
        fw_addr
    );

    calculated == expected
}

/// Check for firmware update condition (stub: always returns false).
fn check_for_update() -> bool {
    false
}

#[cortex_m_rt::entry]
fn main() -> ! {
    let mut p = embassy_stm32::init(make_config());

    // GPIO PA5 as push-pull output (LED for error indication)
    let mut led = Output::new(p.PA5, Level::Low, Speed::Low);

    // Initialize hardware CRC
    let mut crc = crc_hw::HwCrc::new(p.CRC);

    defmt::info!("Bootloader started");

    loop {
        let fw1_header = unsafe { FwHeader::from_addr(FW_1_HDR_ADDR) };

        // Check for firmware update
        if check_for_update() {
            let fw2_header = unsafe { FwHeader::from_addr(FW_2_HDR_ADDR) };
            if validate_header(&mut crc, &fw2_header, FW_2_HDR_ADDR)
                && validate_firmware(&mut crc, &fw2_header, FW_2_ADDR, FW_2_SIZE)
            {
                defmt::info!("Valid update in Slot 2, copying to Slot 1...");
                let fw2_size = { fw2_header.fw_size };
                let mut flash_ops = flash::FlashOps::new(p.FLASH.reborrow());
                match flash_ops.copy_slot2_to_slot1(fw2_size) {
                    Ok(()) => continue,
                    Err(e) => defmt::error!("Flash copy failed: {}", e),
                }
            }
        }

        // Validate Slot 1 and jump
        if validate_header(&mut crc, &fw1_header, FW_1_HDR_ADDR)
            && validate_firmware(&mut crc, &fw1_header, FW_1_ADDR, FW_1_SIZE)
        {
            defmt::info!("Valid firmware, jumping to {:#010X}", FW_1_ADDR);
            unsafe {
                let Err(()) = jump::jump_to_application(FW_1_ADDR);
            }
            defmt::error!("Invalid firmware vectors!");
        }

        // No valid firmware found
        defmt::error!("No valid firmware!");
        error::blink_error_pattern(&mut led);
    }
}
