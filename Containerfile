FROM debian:trixie

RUN apt update

# Step 1: Install apt dependencies
RUN apt install xorriso git curl build-essential nasm -y

# Step 2: Install bare-metal gcc
RUN curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh | bash
ENV PATH="$PATH:/home/linuxbrew/.linuxbrew/bin"
RUN brew install x86_64-elf-gcc

# Step 3: Install rust
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="$PATH:/root/.cargo/bin"
RUN rustup target add x86_64-unknown-none

WORKDIR /home/root

# Step 4: Compile limine
RUN git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1 my_os_3/limine
RUN make -C my_os_3/limine

# Step 5: Compile StuOS
CMD ["./build_all.sh"]

#now install qemu-system-x86_64 and run the binary!