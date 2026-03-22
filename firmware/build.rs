use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    // Memory map constants (must match shared/src/memory_map.rs)
    let flash_base: u32 = 0x0800_0000;
    let bl_size: u32 = 32 * 1024;
    let fw_hdr_size: u32 = 1 * 1024;
    let total_flash: u32 = 512 * 1024;
    let slot_total: u32 = (total_flash - bl_size) / 2;

    let fw_1_addr = flash_base + bl_size + fw_hdr_size;
    let fw_1_size = slot_total - fw_hdr_size;

    let memory_x = format!(
        "MEMORY\n\
         {{\n\
           FLASH (rx)  : ORIGIN = 0x{fw_1_addr:08X}, LENGTH = {fw_1_size}\n\
           RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K\n\
         }}\n"
    );

    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    fs::write(out.join("memory.x"), memory_x).unwrap();
    println!("cargo:rustc-link-search={}", out.display());
    println!("cargo:rerun-if-changed=build.rs");
}
