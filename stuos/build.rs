// build.rs

use bindgen::MacroTypeVariation;

fn main() {
    //uapi conversion
    let rust_from_c = ["syscalls", "errno"];

    for x in rust_from_c {
        
        bindgen::Builder::default()
            .use_core()
            .header(format!("../abi/uapi/{}.h", x))
            .default_macro_constant_type(MacroTypeVariation::Signed)
            .generate()
            .expect("Unable to generate bindings")
            .write_to_file(format!("src/rs_uapi/{}.rs", x))
            .expect("Couldn't write bindings");
    }

    bindgen::Builder::default()
        .use_core()
        .header("src/memory.h")
        .clang_arg("-I../abi/")
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file("src/memory.rs")
        .expect("Couldn't write bindings");
}