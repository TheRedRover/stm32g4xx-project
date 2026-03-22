use embassy_stm32::gpio::Output;

/// Blink an error pattern on the LED.
///
/// Pattern matches the C BOOTLOADER_ERROR: 3 rounds of increasing blinks
/// (1 blink, 2 blinks, 3 blinks) with pauses between groups.
/// This function never returns.
pub fn blink_error_pattern(led: &mut Output<'_>) -> ! {
    loop {
        for i in 0..3u8 {
            for _ in 0..=i {
                led.set_high();
                cortex_m::asm::delay(170_000_000 / 5); // ~200ms at 170MHz
                led.set_low();
                cortex_m::asm::delay(170_000_000 / 5); // ~200ms
            }
            cortex_m::asm::delay(170_000_000 * 4 / 5); // ~800ms pause
        }
    }
}
