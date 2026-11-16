make clean

if [[ $1 == "-c" ]]; then
  rm kernel.o kernel.bin disk.img qemu.log  boot.log
  exit
fi

make LOG_LEVEL=_DEBUG

# Compile the kernel
# Compile freestanding, no PIC, no PIE, no unwind tables
gcc -c kernel.c -o kernel.o \
    -ffreestanding -fno-pic -fno-pie \
    -fno-stack-protector -mno-red-zone \
    -fno-asynchronous-unwind-tables -fno-unwind-tables

# link the kernel using a linker script
ld -T linker.ld -nostdlib -o kernel.bin kernel.o

# Build a FAT32 disk image
dd if=/dev/zero of=disk.img bs=1M count=64
mkfs.fat -F 32 disk.img
mmd -i disk.img ::/EFI
mmd -i disk.img ::/EFI/BOOT
mcopy -i disk.img BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
mcopy -i disk.img kernel.bin ::/kernel.bin

# boot the image using qemu 
# the OVMF_CODE.4m.fd and OVMF_VARS.4m.fd can be found in /usr/share/edk2/x64
# or can be downloaded online
qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=OVMF_VARS.4m.fd \
  -drive file=disk.img,format=raw \
  # stdout is written also to this file
  -serial file:boot.log\
  # log interrupts and cpu reset
  -d int,cpu_reset -D qemu.log \
  # export the port :1234 for gdb to connect
  -s\
  -m 512 \
  -net none
