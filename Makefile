SFDISK = /sbin/sfdisk
PARTED = /sbin/parted
MKFAT = /sbin/mkfs.fat

LIMINE_FOLDER=./limine
LIMINE_CONFIG=./my_os_3/src/limine.conf
PUT_IN_FILESYSTEM=./put_in_filesystem

OS_DIR = ./my_os_3
HEADER_DIRS = ./abi/uapi ./custom_libc/include/*
LIBC_OBJ_DIRS = ./custom_libc/.build

OUTPUT_DISK=./filesystem.img
OUTPUT_SYSROOT=./sysroot

all: $(OUTPUT_DISK)

#force since running the virtual machine can clobber the output disk
#creates MBR with a partition at 1MB offset
$(OUTPUT_DISK): $(OUTPUT_SYSROOT)
	rm $@ -f
	fallocate -l 10M $@
	$(PARTED) -s $@ \
		mklabel msdos \
		mkpart primary fat16 1M 100% \
		set 1 boot on
	$(MKFAT) -F 16 -f 2 -S 512 --offset 2048 $@
	mcopy -Q -s -i $@@@1M ./$</* ::/
	$(LIMINE_FOLDER)/limine bios-install $@

$(OUTPUT_SYSROOT): FORCE
	rm -rf $@
#boot things
	mkdir -p $@/boot/limine $@/EFI/BOOT
	cp $(LIMINE_CONFIG) $(LIMINE_FOLDER)/limine-bios.sys $(LIMINE_FOLDER)/limine-bios-cd.bin $(LIMINE_FOLDER)/limine-uefi-cd.bin $@/boot/limine/
	cp $(LIMINE_FOLDER)/BOOTX64.EFI $(LIMINE_FOLDER)/BOOTIA32.EFI $@/EFI/BOOT

#OS things
	make -C  $(OS_DIR)
	cp $(OS_DIR)/.build/myos $@/boot

#sysroot things
	mkdir -p $@/dev $@/lib/tcc/include

#libc things
	make -C custom_libc/
	cp -r $(HEADER_DIRS) $@/lib/tcc/include/
	cp -r $(LIBC_OBJ_DIRS)/crt0.o $@/lib/tcc/crt0.o
	cp -r $(LIBC_OBJ_DIRS)/libc_stuos.a $@/lib/tcc/libc.a
	
	cp $(PUT_IN_FILESYSTEM)/* $@/
	make -C  custom_libc/fuzzing

FORCE: ;

clean:
	rm -rf $(OUTPUT_DISK) $(OUTPUT_SYSROOT)
	make -C  custom_libc/ clean
	make -C  custom_libc/fuzzing clean
	make -C  my_os_3/ clean