# A toy OS designed around minimal POSIX compliance

This kernel + libc project is to prove that I can run userspace processes without relying on a preexisting kernel. 

## Installing
Various dependencies are required for compiling and running the OS. On a debian system:
- install homebrew [from this link](https://brew.sh/)
- install rust/cargo [from this link](https://doc.rust-lang.org/cargo/getting-started/installation.html)
- run the installation script: `./install_script_debian.sh`
- compile the OS: `./build_all.sh`
- run the OS: `./run.sh`

font from https://github.com/dhepper/font8x8