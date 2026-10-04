// build.rs

fn main() {
    //uapi conversion
    let rust_from_c = ["syscalls"];

    for x in rust_from_c {
        
        bindgen::Builder::default()
            .use_core()
            .header(format!("../abi/uapi/{}.h", x))
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