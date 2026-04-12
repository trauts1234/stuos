# A toy OS designed around minimal POSIX compliance

This kernel + libc project is to prove that I can run userspace processes without relying on a preexisting kernel. 

## Installing on Debian and Ubuntu
Various dependencies are required for compiling and running the OS.
1. install the following dependencies from apt:
```bash
sudo apt install qemu xorriso git curl make
```
2. install x86_64-elf-gcc from source or via [homebrew](https://brew.sh/)
3. install rust/cargo [from this link](https://doc.rust-lang.org/cargo/getting-started/installation.html)
4. download and compile the limine bootloader:
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1 my_os_3/limine
make -C my_os_3/limine
```

5. compile the OS: 
```bash
./build_all.sh
```
6. run the OS:
```bash
./run.sh
```

## Installing via Docker/Podman
- Install Docker or Podman
- Install QEMU on the host system
- Make the image, for example `podman build -t stuos_compiler .`
- Start the container, for example `podman run -v .:/home/root stuos_compiler`