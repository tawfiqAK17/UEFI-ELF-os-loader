#include "efidef.h"
#include "efierr.h"
#include "elf_parser.h"
#include <efi.h>
#include <efilib.h>
#include <elf.h>

EFI_STATUS load_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                  UINT64 p_memsz, VOID *kernel_pages,
                  EFI_PHYSICAL_ADDRESS *out_ph_addr);
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

  InitializeLib(ImageHandle, SystemTable);

  EFI_STATUS status;

  EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
  uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
                    &gEfiLoadedImageProtocolGuid, (VOID **)&loaded_image);

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *file_system;
  uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle,
                    &gEfiSimpleFileSystemProtocolGuid, (VOID **)&file_system);

  EFI_FILE_PROTOCOL *root;
  uefi_call_wrapper(file_system->OpenVolume, 2, file_system, &root);

  EFI_FILE_PROTOCOL *kernel_file;
  status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"kernel.bin",
                             EFI_FILE_MODE_READ, 0);

  UINTN buffer_size = 0;
  uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &gEfiFileInfoGuid,
                    &buffer_size, NULL);

  EFI_FILE_INFO *kernel_file_info;
  status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, buffer_size,
                             (VOID **)&kernel_file_info);

  status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file,
                             &gEfiFileInfoGuid, &buffer_size, kernel_file_info);

  UINT64 kernel_file_size = kernel_file_info->FileSize;

  // TODO:need to be freed
  EFI_PHYSICAL_ADDRESS kernel_pages;

  uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                    EFI_SIZE_TO_PAGES(kernel_file_size), &kernel_pages);

  uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &kernel_file_size,
                    (VOID *)kernel_pages);

  unsigned char elf_class = class((void *)kernel_pages);
  Elf64_Off elf_ph_off = ph_off((void *)kernel_pages);
  Elf64_Half elf_ph_ent_size = ph_ent_size((void *)kernel_pages);
  Elf64_Half elf_ph_number = ph_number((void *)kernel_pages);

  EFI_PHYSICAL_ADDRESS current_ph = kernel_pages + elf_ph_off;

  for (int i = 0; i < elf_ph_number; i++) {
    EFI_PHYSICAL_ADDRESS ph_addr;
    if (elf_class == ELFCLASS32) {
      Elf32_Phdr *ph = (Elf32_Phdr *)current_ph;
      load_segment(ph->p_type, ph->p_offset, ph->p_filesz, ph->p_memsz,
                   (VOID *)kernel_pages, &ph_addr);
    }

    if (elf_class == ELFCLASS64) {
      Elf64_Phdr *ph = (Elf64_Phdr *)current_ph;
      load_segment(ph->p_type, ph->p_offset, ph->p_filesz, ph->p_memsz,
                   (VOID *)kernel_pages, &ph_addr);
    }
    
    //TODO: add the invalide class case

  }

  return EFI_SUCCESS;
}

EFI_STATUS load_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                  UINT64 p_memsz, VOID *kernel_pages,
                  EFI_PHYSICAL_ADDRESS *out_ph_addr) {
  if (p_type != PT_LOAD)
    return EFI_SUCCESS;

  if (p_memsz == 0)
    return EFI_SUCCESS;

  EFI_PHYSICAL_ADDRESS ph_addr;

  uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                    EFI_SIZE_TO_PAGES(p_memsz), &ph_addr);

  CopyMem((VOID *)ph_addr, (VOID *)((UINT8 *)kernel_pages + p_offset),
          p_filesz);

  if (p_memsz > p_filesz) {
    uefi_call_wrapper(BS->SetMem, 3, (VOID *)(ph_addr + p_filesz),
                      p_memsz - p_filesz, 0);
  }
  *out_ph_addr = ph_addr;
  return EFI_SUCCESS;
}
