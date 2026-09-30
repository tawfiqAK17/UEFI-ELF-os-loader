#include "elf.h"

unsigned char class(void *elf) {
  return ((Elf64_Ehdr *)(elf))->e_ident[EI_CLASS];
}

Elf64_Addr entry(void *elf) {
  if (class(elf) == ELFCLASS32) {
    return ((Elf32_Ehdr *)(elf))->e_entry;
  }
  return ((Elf64_Ehdr *)(elf))->e_entry;
}

Elf64_Off ph_off(void *elf) {
  if (class(elf) == ELFCLASS32) {
    return ((Elf32_Ehdr *)(elf))->e_phoff;
  }
  return ((Elf64_Ehdr *)(elf))->e_phoff;
}

Elf64_Half ph_ent_size(void *elf) {
  if (class(elf) == ELFCLASS32) {
    return ((Elf32_Ehdr *)(elf))->e_phentsize;
  }
  return ((Elf64_Ehdr *)(elf))->e_phentsize;
}

Elf64_Half ph_number(void *elf) {
  if (class(elf) == ELFCLASS32) {
    return ((Elf32_Ehdr *)(elf))->e_phnum;
  }
  return ((Elf64_Ehdr *)(elf))->e_phnum;
}
