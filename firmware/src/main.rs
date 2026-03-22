#![no_std]
#![no_main]

use defmt_rtt as _;
use panic_probe as _;

use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_stm32::usart::{Config as UartConfig, UartTx};
use embassy_stm32::rcc::{self, Pll, PllPreDiv, PllMul, PllRDiv, PllSource};

use shared::fw_header::{FwHeader, unpack_version};
use shared::memory_map::FW_HDR_SIZE;

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

#[cortex_m_rt::entry]
fn main() -> ! {
    let p = embassy_stm32::init(make_config());

    // Re-enable interrupts — the bootloader disables them before jumping,
    // and embassy_stm32::init()'s critical section preserves that state.
    unsafe { cortex_m::interrupt::enable() };

    // GPIO PA5 as push-pull output (LED)
    let mut led = Output::new(p.PA5, Level::High, Speed::Low);

    // Use the NUCLEO-G474RE default ST-LINK virtual COM port wiring:
    // LPUART1 on PA2 (TX) / PA3 (RX). We only need TX here.
    let uart_config = UartConfig::default(); // 115200 8N1
    let mut tx = UartTx::new_blocking(p.LPUART1, p.PA2, uart_config).unwrap();

    defmt::info!("Firmware started!");
    let _ = tx.blocking_write(b"We are working!!!\r\n");

    // Read firmware header from flash
    let vtor = unsafe { (*cortex_m::peripheral::SCB::PTR).vtor.read() };
    let header_addr = vtor - FW_HDR_SIZE;
    let header = unsafe { FwHeader::from_addr(header_addr) };

    if header.is_valid_magic() {
        let version = { header.version };
        let fw_size = { header.fw_size };
        let (major, minor, patch) = unpack_version(version);
        defmt::info!(
            "FW v{}.{}.{}, size={} bytes",
            major, minor, patch,
            fw_size,
        );
    } else {
        let magic = { header.magic_number };
        defmt::warn!("Invalid firmware header magic: {:#010X}", magic);
    }

    // Main loop: blink LED + print Hello World
    loop {
        let _ = tx.blocking_write(b"Hello World!\r\n");
        led.toggle();
        cortex_m::asm::delay(170_000_000); // ~1s at 170MHz
    }
}
