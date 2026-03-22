use cortex_m::peripheral::SCB;

/// Vector table layout at the start of firmware.
#[repr(C)]
struct VectorTable {
    stack_pointer: u32,
    reset_handler: u32,
}

// STM32G4 RCC register addresses
const RCC_BASE: u32 = 0x4002_1000;
const RCC_CR: *mut u32 = RCC_BASE as *mut u32;                    // offset 0x00
const RCC_CFGR: *mut u32 = (RCC_BASE + 0x08) as *mut u32;        // offset 0x08

/// Jump to the firmware application at the given flash address.
///
/// Returns `Err(())` if the vector table contains invalid SP or PC values,
/// allowing the caller to fall through to an error handler.
///
/// # Safety
/// `app_start_addr` must point to a valid Cortex-M vector table in flash.
pub unsafe fn jump_to_application(app_start_addr: u32) -> Result<!, ()> {
    let vt = unsafe { &*(app_start_addr as *const VectorTable) };

    // Validate stack pointer is in SRAM range
    if (vt.stack_pointer & 0xFF00_0000) != 0x2000_0000 {
        defmt::error!("Invalid SP: {:#010X}", vt.stack_pointer);
        return Err(());
    }

    // Validate reset handler is in FLASH range
    if (vt.reset_handler & 0xFF00_0000) != 0x0800_0000 {
        defmt::error!("Invalid reset handler: {:#010X}", vt.reset_handler);
        return Err(());
    }

    let sp = vt.stack_pointer;
    let pc = vt.reset_handler;

    unsafe {
        // Disable all interrupts
        cortex_m::interrupt::disable();

        // Reset RCC to default state via direct register access
        // Enable HSI (bit 8 = HSION)
        let cr = core::ptr::read_volatile(RCC_CR);
        core::ptr::write_volatile(RCC_CR, cr | (1 << 8));
        // Wait for HSI ready (bit 10 = HSIRDY)
        while core::ptr::read_volatile(RCC_CR) & (1 << 10) == 0 {}

        // Switch system clock to HSI (SW[1:0] = 0b01 for HSI)
        let cfgr = core::ptr::read_volatile(RCC_CFGR);
        core::ptr::write_volatile(RCC_CFGR, (cfgr & !0b11) | 0b01);
        // Wait for HSI as system clock (SWS[1:0] = 0b01)
        while (core::ptr::read_volatile(RCC_CFGR) >> 2) & 0b11 != 0b01 {}

        // Disable PLL (bit 24 = PLLON) and HSE (bit 16 = HSEON)
        let cr = core::ptr::read_volatile(RCC_CR);
        core::ptr::write_volatile(RCC_CR, cr & !((1 << 24) | (1 << 16)));

        // Disable SysTick via direct register access
        // SysTick base: 0xE000_E010
        const SYST_CSR: *mut u32 = 0xE000_E010 as *mut u32;
        const SYST_RVR: *mut u32 = 0xE000_E014 as *mut u32;
        const SYST_CVR: *mut u32 = 0xE000_E018 as *mut u32;
        core::ptr::write_volatile(SYST_CSR, 0);
        core::ptr::write_volatile(SYST_RVR, 0);
        core::ptr::write_volatile(SYST_CVR, 0);

        // Clear all NVIC interrupt enable and pending registers (8 registers = 256 interrupts)
        // NVIC_ICER base: 0xE000_E180, NVIC_ICPR base: 0xE000_E280
        for i in 0..8u32 {
            let icer = (0xE000_E180 + i * 4) as *mut u32;
            let icpr = (0xE000_E280 + i * 4) as *mut u32;
            core::ptr::write_volatile(icer, 0xFFFF_FFFF);
            core::ptr::write_volatile(icpr, 0xFFFF_FFFF);
        }

        cortex_m::asm::dsb();
        cortex_m::asm::isb();

        // Set VTOR to application vector table
        (*SCB::PTR).vtor.write(app_start_addr);

        cortex_m::asm::dsb();
        cortex_m::asm::isb();

        // Jump: set MSP and branch to reset handler
        core::arch::asm!(
            "msr msp, {sp}",
            "dsb",
            "isb",
            "bx {pc}",
            sp = in(reg) sp,
            pc = in(reg) pc,
            options(noreturn)
        );
    }
}
