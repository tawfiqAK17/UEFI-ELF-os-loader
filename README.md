# UEFI ELF OS Loader

A minimal x86_64 UEFI bootloader that loads an ELF kernel, builds its own page tables, enables paging/long mode, exits boot services, and jumps to the kernel entry point.

## How it works

1. Opens `kernel.bin` from the boot volume.
2. Reads the ELF header and program headers (ELF32 and ELF64 supported).
3. Copies each `PT_LOAD` segment into freshly allocated pages.
4. Builds a PML4 → PDPT → PD → PT hierarchy mapping the kernel segments.
5. Copies a tiny trampoline (`mov cr3, rdi; jmp rsi`) into a mapped page.
6. Enables paging, gets the memory map, and calls `ExitBootServices`.
7. Jumps through the trampoline to the kernel entry.

## Project layout

| File | Purpose |
| --- | --- |
| `boot_loader.c` | UEFI application: file loading, ELF parsing, page table setup |
| `boot_loader.h` | Page table flags, shifts, and log macros |
| `elf_parser.h` | ELF header/program header helpers |
| `enable_paging.asm` | Long mode check and paging enable routines |
| `kernel.c` | Stub kernel (`cli; hlt`) |
| `linker.ld` | Links the kernel at `0x100000` |
| `makefile` | Builds `BOOTX64.EFI` |
| `run.sh` | Builds the kernel/disk image and boots it in QEMU |

## Requirements

- `gcc`, `ld`, `objcopy`, `nasm`
- [gnu-efi](https://github.com/vathpela/gnu-efi) (`GNUEFI_DIR`)
- ELF headers (`ELF_DIR`, usually `/usr/include`)
- `qemu-system-x86_64`, `mtools`, `dosfstools` (for `run.sh`)
- OVMF firmware: `OVMF_CODE.4m.fd` and `OVMF_VARS.4m.fd` in the project root

## Setup

```sh
cp .env.example .env
# edit .env
GNUEFI_DIR=/path/to/gnu-efi
ELF_DIR=/usr/include
```

## Build

```sh
make                    # BOOTX64.EFI
make LOG_LEVEL=_DEBUG   # verbose output
make clean
```

## Run

```sh
./run.sh        # build kernel + disk image, boot in QEMU
./run.sh -c     # clean generated files
```

`run.sh` creates `disk.img` with `kernel.bin` and `BOOTX64.EFI`, then boots QEMU with OVMF. Serial output goes to `boot.log`; QEMU interrupt/CPU-reset logs go to `qemu.log`. GDB can attach on `:1234`.


## ToDos
- [ ] share the memory map pages with the kernel.
- [ ] share the kernel memory info with the kernel.
