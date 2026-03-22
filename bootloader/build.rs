use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    // Memory map constants (must match shared/src/memory_map.rs)
    let bl_start: u32 = 0x0800_0000;
    let bl_size: u32 = 32 * 1024;

    let memory_x = format!(
        "MEMORY\n\
         {{\n\
           FLASH (rx)  : ORIGIN = 0x{bl_start:08X}, LENGTH = {bl_size}\n\
           RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K\n\
         }}\n"
    );

    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    fs::write(out.join("memory.x"), memory_x).unwrap();
    println!("cargo:rustc-link-search={}", out.display());
    println!("cargo:rerun-if-changed=build.rs");
}
