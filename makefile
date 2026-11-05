-include .env

CC       = gcc
ASM			 = nasm
LD       = ld
OBJCOPY  = objcopy

LOG_LEVEL?=_ERROR

INC_DIRS = -I$(GNUEFI_DIR)/inc \
           -I$(GNUEFI_DIR)/inc/x86_64 \
           -I$(GNUEFI_DIR)/inc/protocol \
					 -I$(ELF_DIR)

LIB_DIRS = -L$(GNUEFI_DIR)/x86_64/lib \
           -L$(GNUEFI_DIR)/x86_64/gnuefi

CRT0 = $(GNUEFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o
LDS  = $(GNUEFI_DIR)/gnuefi/elf_x86_64_efi.lds

CFLAGS  = $(INC_DIRS) -DLOG_LEVEL=$(LOG_LEVEL) -fpic -ffreestanding -fno-stack-protector -fno-stack-check \
          -fshort-wchar -mno-red-zone -maccumulate-outgoing-args
LDFLAGS = -shared -Bsymbolic $(LIB_DIRS) -T$(LDS)
LDLIBS  = -lgnuefi -lefi

SRC     = boot_loader.c
ASM_SRC =  enable_paging.asm
OBJ     = $(SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.asm=.o)
SO      = boot_loader.so
TARGET  = BOOTX64.EFI

all: $(TARGET)

$(ASM_OBJ): $(ASM_SRC)
	$(ASM) -f elf64 $(ASM_SRC) -o $(ASM_OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(SO): $(OBJ) $(ASM_OBJ)
	$(LD) $(LDFLAGS) $(CRT0) $(OBJ) $(ASM_OBJ) -o $@ $(LDLIBS)

$(TARGET): $(SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
	    -j .rela -j .reloc \
	    -Ielf64-x86-64 -Opei-x86-64 \
	    --subsystem=10 \
	    $< $@

clean:
	rm -f $(OBJ) $(ASM_OBJ) $(SO) $(TARGET)

.PHONY: all clean
