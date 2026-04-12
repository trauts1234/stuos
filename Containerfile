FROM debian:trixie

RUN apt update

# Step 1: Install apt dependencies
RUN apt install xorriso git curl make -y

# Step 2: Install gcc (brew must be referred to by its full path as it hasn't been added to $PATH)
RUN curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh | bash
RUN /home/linuxbrew/.linuxbrew/bin/brew install x86_64-elf-gcc

# Step 3: Install rust
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y

WORKDIR /home/root

RUN apt install gcc -y

# Step 4: Compile limine
RUN git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1 my_os_3/limine
RUN make -C my_os_3/limine

# Step 5: Compile StuOS
CMD ["./build_all.sh"]

#now install qemu-system-x86_64 and run the binary!