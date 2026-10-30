#include "boot_loader.h"
#include "efibind.h"
#include "efidef.h"
#include "efierr.h"
#include "elf_parser.h"

EFI_STATUS load_and_map_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                                UINT64 p_memsz, VOID *kernel_pages,
                                EFI_VIRTUAL_ADDRESS virtual_addr,
                                EFI_PHYSICAL_ADDRESS pml4);

EFI_STATUS allocate_empty_page(EFI_PHYSICAL_ADDRESS *page_addr);

EFI_STATUS insert_pt_entry(EFI_PHYSICAL_ADDRESS pml4,
                           EFI_PHYSICAL_ADDRESS physical_addr,
                           EFI_VIRTUAL_ADDRESS virtual_addr, UINT16 flags);

EFI_STATUS create_pdpt(UINT16 pdpt_idx, UINT16 pd_idx, UINT16 pt_idx,
                       EFI_VIRTUAL_ADDRESS vr_addr,
                       EFI_PHYSICAL_ADDRESS *out_pdpt_ph_addr, UINT16 flags);
EFI_STATUS create_pd(UINT16 pd_idx, UINT16 pt_idx, EFI_VIRTUAL_ADDRESS vr_addr,
                     EFI_PHYSICAL_ADDRESS *out_pd_ph_addr, UINT16 flags);

EFI_STATUS create_pt(UINT16 pt_idx, EFI_VIRTUAL_ADDRESS vr_addr,
                     EFI_PHYSICAL_ADDRESS *out_pt_ph_addr, UINT16 flags);
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

  EFI_PHYSICAL_ADDRESS pml4;
  allocate_empty_page(&pml4);

  unsigned char elf_class = class((void *)kernel_pages);
  Elf64_Off elf_ph_off = ph_off((void *)kernel_pages);
  Elf64_Half elf_ph_ent_size = ph_ent_size((void *)kernel_pages);
  Elf64_Half elf_ph_number = ph_number((void *)kernel_pages);

  EFI_PHYSICAL_ADDRESS current_ph = kernel_pages + elf_ph_off;

  for (int i = 0; i < elf_ph_number; i++) {
    EFI_PHYSICAL_ADDRESS ph_addr;
    if (elf_class == ELFCLASS32) {
      Elf32_Phdr *ph = (Elf32_Phdr *)current_ph;
      load_and_map_segment(ph->p_type, ph->p_offset, ph->p_filesz, ph->p_memsz,
                           (VOID *)kernel_pages, ph->p_vaddr, pml4);
    }

    if (elf_class == ELFCLASS64) {
      Elf64_Phdr *ph = (Elf64_Phdr *)current_ph;
      load_and_map_segment(ph->p_type, ph->p_offset, ph->p_filesz, ph->p_memsz,
                           (VOID *)kernel_pages, ph->p_vaddr, pml4);
    }

    // TODO: add the invalide class case
  }
  Print(L"Done??");
  return EFI_SUCCESS;
}

// TODO: all the allocations should use this function
EFI_STATUS allocate_empty_page(EFI_PHYSICAL_ADDRESS *page_addr) {

  uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1,
                    page_addr);
  uefi_call_wrapper(BS->SetMem, 3, page_addr, EFI_PAGE_SIZE, 0);

  return EFI_SUCCESS;
}

EFI_STATUS load_and_map_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                                UINT64 p_memsz, VOID *kernel_pages,
                                EFI_VIRTUAL_ADDRESS virtual_addr,
                                EFI_PHYSICAL_ADDRESS pml4) {
  if (p_type != PT_LOAD)
    return EFI_SUCCESS;

  if (p_memsz == 0)
    return EFI_SUCCESS;

  UINT64 offset = virtual_addr & EFI_PAGE_MASK;
  UINTN pages = EFI_SIZE_TO_PAGES(p_memsz + offset);

  EFI_PHYSICAL_ADDRESS ph_addr;

  uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                    pages, &ph_addr);

  uefi_call_wrapper(BS->SetMem, 3, (VOID *)(ph_addr), pages * EFI_PAGE_SIZE, 0);

  uefi_call_wrapper(BS->CopyMem, 3, (VOID *)(ph_addr + offset),
                    (VOID *)((UINT8 *)kernel_pages + p_offset), p_filesz);

  EFI_VIRTUAL_ADDRESS first_page_vert_addr = virtual_addr & ~EFI_PAGE_MASK;

  for (UINTN i = 0; i < pages; i++) {
    insert_pt_entry(pml4, ph_addr + (i * EFI_PAGE_SIZE),
                    first_page_vert_addr + (i * EFI_PAGE_SIZE), 0);
  }
  return EFI_SUCCESS;
}

EFI_STATUS insert_pt_entry(EFI_PHYSICAL_ADDRESS pml4,
                           EFI_PHYSICAL_ADDRESS physical_addr,
                           EFI_VIRTUAL_ADDRESS virtual_addr, UINT16 flags) {
  UINT16 pml4_idx = (virtual_addr >> PML4_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pdpt_idx = (virtual_addr >> PDPT_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pd_idx = (virtual_addr >> PD_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pt_idx = (virtual_addr >> PT_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 offset = virtual_addr & EFI_PAGE_MASK;

  EFI_PHYSICAL_ADDRESS *pml4_ph_addr = ((EFI_PHYSICAL_ADDRESS *)pml4);
  if (pml4_ph_addr[pml4_idx] & P) {
    EFI_PHYSICAL_ADDRESS *pdpt_ph_addr =
        ((EFI_PHYSICAL_ADDRESS *)pml4_ph_addr[pml4_idx]);
    if (pdpt_ph_addr[pdpt_idx] & P) {
      EFI_PHYSICAL_ADDRESS *pd_ph_addr =
          ((EFI_PHYSICAL_ADDRESS *)pdpt_ph_addr[pdpt_idx]);
      if (pd_ph_addr[pd_idx] & P) {
        EFI_PHYSICAL_ADDRESS *pt_ph_addr =
            ((EFI_PHYSICAL_ADDRESS *)pd_ph_addr[pd_idx]);
        if (pt_ph_addr[pt_idx] & P) {
          // TODO: page already mapped
        } else {
          EFI_PHYSICAL_ADDRESS pt_ph_addr;
          create_pt(pt_idx, virtual_addr, &pt_ph_addr, flags);
          pd_ph_addr[pd_idx] = pt_ph_addr;
        }
      } else {
        EFI_PHYSICAL_ADDRESS pd_ph_addr;
        create_pd(pd_idx, pt_idx, virtual_addr, &pd_ph_addr, flags);
        pdpt_ph_addr[pdpt_idx] = pd_ph_addr;
      }
    } else {
      EFI_PHYSICAL_ADDRESS pdpt_ph_addr;
      create_pdpt(pdpt_idx, pd_idx, pt_idx, virtual_addr, &pdpt_ph_addr, flags);
      pml4_ph_addr[pml4_idx] = pdpt_ph_addr;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS create_pdpt(UINT16 pdpt_idx, UINT16 pd_idx, UINT16 pt_idx,
                       EFI_VIRTUAL_ADDRESS vr_addr,
                       EFI_PHYSICAL_ADDRESS *out_pdpt_ph_addr, UINT16 flags) {

  allocate_empty_page(out_pdpt_ph_addr);
  EFI_PHYSICAL_ADDRESS pd_ph_addr;
  create_pd(pd_idx, pt_idx, vr_addr, &pd_ph_addr, flags);

  out_pdpt_ph_addr[pdpt_idx] = (pd_ph_addr << 12) | flags;
  return EFI_SUCCESS;
}
EFI_STATUS create_pd(UINT16 pd_idx, UINT16 pt_idx, EFI_VIRTUAL_ADDRESS vr_addr,
                     EFI_PHYSICAL_ADDRESS *out_pd_ph_addr, UINT16 flags) {

  allocate_empty_page(out_pd_ph_addr);

  EFI_PHYSICAL_ADDRESS pt_ph_addr;

  create_pt(pt_idx, vr_addr, &pt_ph_addr, flags);

  out_pd_ph_addr[pd_idx] = (pt_ph_addr << 12) | flags;
  return EFI_SUCCESS;
}

EFI_STATUS create_pt(UINT16 pt_idx, EFI_VIRTUAL_ADDRESS vr_addr,
                     EFI_PHYSICAL_ADDRESS *out_pt_ph_addr, UINT16 flags) {
  // allocate the table page
  allocate_empty_page(out_pt_ph_addr);

  // allocate the page that maps to the virtual addr
  EFI_PHYSICAL_ADDRESS physical_addr;
  allocate_empty_page(&physical_addr);

  // add the flags to the page physical addr and write it to the PT
  out_pt_ph_addr[pt_idx] = (physical_addr << 12) | flags;
  return EFI_SUCCESS;
}
