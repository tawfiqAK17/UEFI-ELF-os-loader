-include .env

CC       = gcc
LD       = ld
OBJCOPY  = objcopy

INC_DIRS = -I$(GNUEFI_DIR)/inc \
           -I$(GNUEFI_DIR)/inc/x86_64 \
           -I$(GNUEFI_DIR)/inc/protocol \
					 -I$(ELF_DIR)

LIB_DIRS = -L$(GNUEFI_DIR)/x86_64/lib \
           -L$(GNUEFI_DIR)/x86_64/gnuefi

CRT0 = $(GNUEFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o
LDS  = $(GNUEFI_DIR)/gnuefi/elf_x86_64_efi.lds

CFLAGS  = $(INC_DIRS) -fpic -ffreestanding -fno-stack-protector -fno-stack-check \
          -fshort-wchar -mno-red-zone -maccumulate-outgoing-args
LDFLAGS = -shared -Bsymbolic $(LIB_DIRS) -T$(LDS)
LDLIBS  = -lgnuefi -lefi

SRC    = boot_loader.c
OBJ    = $(SRC:.c=.o)
SO     = boot_loader.so
TARGET = BOOTX64.EFI

all: $(TARGET)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(SO): $(OBJ)
	$(LD) $(LDFLAGS) $(CRT0) $(OBJ) -o $@ $(LDLIBS)

$(TARGET): $(SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
	    -j .rela -j .reloc \
	    -Ielf64-x86-64 -Opei-x86-64 \
	    --subsystem=10 \
	    $< $@

clean:
	rm -f $(OBJ) $(SO) $(TARGET)

.PHONY: all clean
