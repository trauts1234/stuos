SFDISK = /sbin/sfdisk
PARTED = /sbin/parted
MKFAT = /sbin/mkfs.fat

LIMINE_FOLDER=./ports/limine
LIMINE_CONFIG=./stuos/src/limine.conf
PUT_IN_FILESYSTEM=./put_in_filesystem

OS_DIR = ./stuos
LIBC_OBJ_DIRS = ./custom_libc/.build

OUTPUT_DISK=./filesystem.img
OUTPUT_SYSROOT=./sysroot

all: $(OUTPUT_DISK)

#force since running the virtual machine can clobber the output disk
#creates MBR with a partition at 1MB offset
$(OUTPUT_DISK): $(OUTPUT_SYSROOT) $(LIMINE_FOLDER)/limine
	rm $@ -f
	fallocate -l 15M $@
	$(PARTED) -s $@ \
		mklabel msdos \
		mkpart primary fat16 1M 100% \
		set 1 boot on
	$(MKFAT) -F 16 -f 2 -S 512 --offset 2048 $@
	mcopy -Q -s -i $@@@1M ./$</* ::/
	$(LIMINE_FOLDER)/limine bios-install $@

# this represents "the whole of limine being built"
$(LIMINE_FOLDER)/limine:
	make -C $(LIMINE_FOLDER)

#TODO
$(OUTPUT_SYSROOT): $(LIMINE_FOLDER)/limine FORCE
#sysroot things
	mkdir -p $@/dev $@/usr/include $@/usr/lib $@/boot/limine $@/EFI/BOOT

#boot things
	cp $(LIMINE_CONFIG) $(LIMINE_FOLDER)/limine-bios.sys $(LIMINE_FOLDER)/limine-bios-cd.bin $(LIMINE_FOLDER)/limine-uefi-cd.bin $@/boot/limine/
	cp $(LIMINE_FOLDER)/BOOTX64.EFI $(LIMINE_FOLDER)/BOOTIA32.EFI $@/EFI/BOOT

#OS things
	make -C  $(OS_DIR)

#libc things
	make -C custom_libc/
	
	cp $(PUT_IN_FILESYSTEM)/* $@/
# 	make -C custom_libc/fuzzing

#use the compilation of tcc to get libtcc1.a
	make -C ports/tcc cross-x86_64 install
	cp ports/tcc/linux_to_stuos/lib/tcc/x86_64-libtcc1.a $@/usr/lib/libtcc1.a

FORCE: ;

clean:
	rm -rf $(OUTPUT_DISK) $(OUTPUT_SYSROOT)
	make -C  custom_libc/ clean
	make -C  custom_libc/fuzzing clean
	make -C  stuos/ clean