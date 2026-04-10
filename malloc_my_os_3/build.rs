use std::env;
use cbindgen::Language;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    cbindgen::Builder::new()
        .with_crate(manifest_dir)
        .with_language(Language::C)
        .with_no_includes()
        .with_include("uapi/stdint.h")
        .generate()
        .expect("Unable to generate C bindings")
        .write_to_file("rust_bindings.h");
}