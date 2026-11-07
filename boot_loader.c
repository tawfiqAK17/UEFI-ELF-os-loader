#include "boot_loader.h"
#include "efibind.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "elf_parser.h"
#include <sys/types.h>

EFI_STATUS load_and_map_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                                UINT64 p_memsz, VOID *kernel_pages,
                                EFI_VIRTUAL_ADDRESS virtual_addr,
                                EFI_PHYSICAL_ADDRESS pml4);

EFI_STATUS allocate_empty_page(EFI_PHYSICAL_ADDRESS *page_addr);

EFI_STATUS insert_pt_entry(EFI_PHYSICAL_ADDRESS pml4,
                           EFI_PHYSICAL_ADDRESS physical_addr,
                           EFI_VIRTUAL_ADDRESS virtual_addr, UINT16 flags);

EFI_STATUS create_pdpt(UINT16 pdpt_idx, UINT16 pd_idx, UINT16 pt_idx,
                       EFI_PHYSICAL_ADDRESS physical_addr,
                       EFI_PHYSICAL_ADDRESS *out_pdpt_ph_addr, UINT16 flags);
EFI_STATUS create_pd(UINT16 pd_idx, UINT16 pt_idx,
                     EFI_PHYSICAL_ADDRESS physical_addr,
                     EFI_PHYSICAL_ADDRESS *out_pd_ph_addr, UINT16 flags);

EFI_STATUS create_pt(UINT16 pt_idx, EFI_PHYSICAL_ADDRESS physical_addr,
                     EFI_PHYSICAL_ADDRESS *out_pt_ph_addr, UINT16 flags);
extern uint Check_long_mode_supported();
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

  LOG_INFO(L"initializing efi libe");
  InitializeLib(ImageHandle, SystemTable);

  EFI_STATUS status;

  LOG_INFO(L"getting loaded image protocol from the image handle");
  EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
  status =
      uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
                        &gEfiLoadedImageProtocolGuid, (VOID **)&loaded_image);
  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to locate the loaded image protocole, status code: %u",
              status);
  }

  LOG_INFO(L"getting simple file system protocol from the loaded image device "
           L"handle");
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *file_system;
  status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle,
                             &gEfiSimpleFileSystemProtocolGuid,
                             (VOID **)&file_system);
  if (EFI_ERROR(status)) {
    LOG_ERROR(
        L"failed to locate the simple file system protocol, status code: %u",
        status);
  }

  LOG_INFO(L"getting file protocol for the root directory");
  EFI_FILE_PROTOCOL *root;
  status = uefi_call_wrapper(file_system->OpenVolume, 2, file_system, &root);
  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to locate the file protocol for the root directory, "
              L"status code: %u",
              status);
  }

  LOG_INFO(L"getting the file protocol for the kernel");
  EFI_FILE_PROTOCOL *kernel_file;
  status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"kernel.bin",
                             EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to locate the file protocol for the kernel, "
              L"status code: %u",
              status);
  }

  LOG_INFO(L"getting the kernel info buffer size");
  UINTN buffer_size = 0;
  status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file,
                             &gEfiFileInfoGuid, &buffer_size, NULL);

  LOG_DEBUG(L"kernel file info size is: %dB", buffer_size);
  LOG_INFO(L"allocating pool for the kernel file info");
  EFI_FILE_INFO *kernel_file_info;
  status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, buffer_size,
                             (VOID **)&kernel_file_info);

  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to allocate pool for kernel file info, "
              L"status code: %u",
              status);
  }
  LOG_INFO(L"loading the kernel file info");
  status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file,
                             &gEfiFileInfoGuid, &buffer_size, kernel_file_info);

  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to load kernel file info, "
              L"status code: %u",
              status);
  }

  UINT64 kernel_file_size = kernel_file_info->FileSize;
  LOG_DEBUG(L"kernel file size is: %dB", kernel_file_size);

  // TODO:need to be freed
  EFI_PHYSICAL_ADDRESS kernel_pages;

  LOG_INFO(L"allocating pages for the kernel file");
  status =
      uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                        EFI_SIZE_TO_PAGES(kernel_file_size), &kernel_pages);

  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to allocate pages for kernel file, "
              L"status code: %u",
              status);
  }
  LOG_INFO(L"loading the kernel file");
  status = uefi_call_wrapper(kernel_file->Read, 3, kernel_file,
                             &kernel_file_size, (VOID *)kernel_pages);
  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to load kernel file, "
              L"status code: %u",
              status);
  }
  LOG_DEBUG(L"kernel file was loaded at address: 0x%016llx", kernel_pages);

  EFI_PHYSICAL_ADDRESS pml4;
  LOG_INFO(L"alocating empty page for the pml4 table");
  status = allocate_empty_page(&pml4);

  if (EFI_ERROR(status)) {
    LOG_ERROR(L"failed to allocate page for pml4 table, "
              L"status code: %u",
              status);
  }
  LOG_DEBUG(L"pml4 table is at address: 0x%016llx", pml4);

  unsigned char elf_class = class((void *)kernel_pages);
  LOG_DEBUG(L"kernel elf class: %u (0: ELFCLASSNONE, 1: ELFCLASS32, 2: "
            L"ELFCLASS64)",
            elf_class)

  Elf64_Off elf_ph_off = ph_off((void *)kernel_pages);
  LOG_DEBUG(L"kernel program header table offset: %llu", elf_ph_off);

  Elf64_Half elf_ph_ent_size = ph_ent_size((void *)kernel_pages);
  LOG_DEBUG(L"kernel program header table entry size: %u", elf_ph_ent_size);

  Elf64_Half elf_ph_number = ph_number((void *)kernel_pages);
  LOG_DEBUG(L"kernel program header table enties number: %u", elf_ph_number);

  EFI_PHYSICAL_ADDRESS phdr_ph_addr = kernel_pages + elf_ph_off;
  LOG_DEBUG(L"program header table physical address: 0x%016llx", phdr_ph_addr);
  if (elf_class == ELFCLASS32) {
    Elf32_Phdr *ph = (Elf32_Phdr *)phdr_ph_addr;
    for (int i = 0; i < elf_ph_number; i++) {
      LOG_INFO(L"loading segment number %d/%d", i + 1, elf_ph_number);
      status = load_and_map_segment(ph->p_type, ph->p_offset, ph->p_filesz,
                                    ph->p_memsz, (VOID *)kernel_pages,
                                    ph->p_vaddr, pml4);
      if (EFI_ERROR(status)) {
        return status;
      }
      ph = ph + elf_ph_ent_size;
    }
  }

  if (elf_class == ELFCLASS64) {
    Elf64_Phdr *ph = (Elf64_Phdr *)phdr_ph_addr;
    for (int i = 0; i < elf_ph_number; i++) {
      LOG_INFO(L"loading segment number %d/%d", i + 1, elf_ph_number);
      status = load_and_map_segment(ph[i].p_type, ph[i].p_offset,
                                    ph[i].p_filesz, ph[i].p_memsz,
                                    (VOID *)kernel_pages, ph[i].p_vaddr, pml4);
      if (EFI_ERROR(status)) {
        return status;
      }
    }

    // TODO: add the invalide class case
  }

  uint long_mode_ret = Check_long_mode_supported();
  if (long_mode_ret == 1) {
    LOG_ERROR(L"the cpu does not support cpuid instruction");
  }
  if (long_mode_ret == 2) {
    LOG_ERROR(L"the cpu does not support long mode addressing");
  }

  EFI_PHYSICAL_ADDRESS trampoline_addr;
  insert_pt_entry(pml4, trampoline_addr, trampoline_addr, R_W | P);
  VOID (*trampoline)(EFI_PHYSICAL_ADDRESS pml4,
                     EFI_VIRTUAL_ADDRESS kernel_entry) =
      (VOID *)trampoline_addr;

Print(L"calling the tranpoline...");
  trampoline(pml4, entry((VOID *)kernel_file));
Print(L"tranpoline was called");

  uefi_call_wrapper(BS->Stall, 1, 100000000);
  return EFI_SUCCESS;
}

// TODO: all the allocations should use this function
EFI_STATUS allocate_empty_page(EFI_PHYSICAL_ADDRESS *page_addr) {

  EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
                                        EfiLoaderData, 1, page_addr);
  if (EFI_ERROR(status)) {
    return status;
  }

  status = uefi_call_wrapper(BS->SetMem, 3, *page_addr, EFI_PAGE_SIZE, 0);
  if (EFI_ERROR(status)) {
    return status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS load_and_map_segment(UINT64 p_type, UINT64 p_offset, UINT64 p_filesz,
                                UINT64 p_memsz, VOID *kernel_pages,
                                EFI_VIRTUAL_ADDRESS virtual_addr,
                                EFI_PHYSICAL_ADDRESS pml4) {

  LOG_DEBUG(L"p_type: 0x%llx, p_offset: %llu, p_filesz: %llu, p_memsz: %llu, "
            L"p_vaddr: 0x%016llx",
            p_type, p_offset, p_filesz, p_memsz, virtual_addr);
  if (p_type != PT_LOAD)
    return EFI_SUCCESS;

  if (p_memsz == 0)
    return EFI_SUCCESS;

  UINT64 offset = virtual_addr & EFI_PAGE_MASK;
  LOG_DEBUG(L"offset: 0x%016llx", offset);
  UINTN pages = EFI_SIZE_TO_PAGES(p_memsz + offset);
  LOG_DEBUG(L"number of pages: %d", pages);

  EFI_PHYSICAL_ADDRESS ph_addr;

  EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
                                        EfiLoaderData, pages, &ph_addr);
  if (EFI_ERROR(status)) {
    return status;
  }

  LOG_DEBUG(L"pages allocated at: 0x%016llx", ph_addr);

  status = uefi_call_wrapper(BS->SetMem, 3, (VOID *)(ph_addr),
                             pages * EFI_PAGE_SIZE, 0);
  if (EFI_ERROR(status)) {
    return status;
  }

  LOG_INFO(L"copying the segment to the allocated physical pages");
  status =
      uefi_call_wrapper(BS->CopyMem, 3, (VOID *)(ph_addr + offset),
                        (VOID *)((UINT8 *)kernel_pages + p_offset), p_filesz);
  if (EFI_ERROR(status)) {
    return status;
  }

  EFI_VIRTUAL_ADDRESS first_page_vert_addr = virtual_addr & ~EFI_PAGE_MASK;
  LOG_DEBUG(L"pages starting vertual address : 0x%016llx",
            first_page_vert_addr);

  for (UINTN i = 0; i < pages; i++) {
    LOG_DEBUG(L"mapping page number %d/%d", i + 1, pages);
    status =
        insert_pt_entry(pml4, ph_addr + (i * EFI_PAGE_SIZE),
                        first_page_vert_addr + (i * EFI_PAGE_SIZE), P | R_W);
    if (EFI_ERROR(status)) {
      return status;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS insert_pt_entry(EFI_PHYSICAL_ADDRESS pml4,
                           EFI_PHYSICAL_ADDRESS physical_addr,
                           EFI_VIRTUAL_ADDRESS virtual_addr, UINT16 flags) {
  EFI_STATUS status;
  UINT16 pml4_idx = (virtual_addr >> PML4_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pdpt_idx = (virtual_addr >> PDPT_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pd_idx = (virtual_addr >> PD_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 pt_idx = (virtual_addr >> PT_SHIFT) & PAGE_TABLE_IDX_MASK;
  UINT16 offset = virtual_addr & EFI_PAGE_MASK;
  LOG_DEBUG(L"pml4_idx: %u, pdpt_idx: %u, pd_idx: %u, pt_idx: %u", pml4_idx,
            pdpt_idx, pd_idx, pt_idx);

  EFI_PHYSICAL_ADDRESS *pml4_ph_addr = ((EFI_PHYSICAL_ADDRESS *)pml4);
  if (pml4_ph_addr[pml4_idx] & P) {
    LOG_DEBUG(L"pml4 entry already exist: 0x%016llx", pml4_ph_addr);
    EFI_PHYSICAL_ADDRESS *pdpt_ph_addr =
        ((EFI_PHYSICAL_ADDRESS *)pml4_ph_addr[pml4_idx]);
    if (pdpt_ph_addr[pdpt_idx] & P) {
      LOG_DEBUG(L"pdpt entry already exist: 0x%016llx", pdpt_ph_addr);
      EFI_PHYSICAL_ADDRESS *pd_ph_addr =
          ((EFI_PHYSICAL_ADDRESS *)pdpt_ph_addr[pdpt_idx]);
      if (pd_ph_addr[pd_idx] & P) {
        LOG_DEBUG(L"pd entry already exist: 0x%016llx", pd_ph_addr);
        EFI_PHYSICAL_ADDRESS *pt_ph_addr =
            ((EFI_PHYSICAL_ADDRESS *)pd_ph_addr[pd_idx]);
        if (pt_ph_addr[pt_idx] & P) {
          // TODO: page already mapped
        } else {
          EFI_PHYSICAL_ADDRESS pt_ph_addr;
          status = create_pt(pt_idx, physical_addr, &pt_ph_addr, flags);
          if (EFI_ERROR(status)) {
            return status;
          }
          pd_ph_addr[pd_idx] = pt_ph_addr;
          LOG_DEBUG(L"pt entry created: 0x%016llx", pt_ph_addr);
        }
      } else {
        EFI_PHYSICAL_ADDRESS pd_ph_addr;
        status = create_pd(pd_idx, pt_idx, physical_addr, &pd_ph_addr, flags);
        if (EFI_ERROR(status)) {
          return status;
        }
        pdpt_ph_addr[pdpt_idx] = pd_ph_addr;
        LOG_DEBUG(L"pd entry created: 0x%016llx", pd_ph_addr);
      }
    } else {
      EFI_PHYSICAL_ADDRESS pdpt_ph_addr;
      status = create_pdpt(pdpt_idx, pd_idx, pt_idx, physical_addr,
                           &pdpt_ph_addr, flags);
      if (EFI_ERROR(status)) {
        return status;
      }
      pml4_ph_addr[pml4_idx] = pdpt_ph_addr;
      LOG_DEBUG(L"pdpt entry created: 0x%016llx", pdpt_ph_addr);
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS create_pdpt(UINT16 pdpt_idx, UINT16 pd_idx, UINT16 pt_idx,
                       EFI_PHYSICAL_ADDRESS physical_addr,
                       EFI_PHYSICAL_ADDRESS *out_pdpt_ph_addr, UINT16 flags) {

  EFI_STATUS status;
  status = allocate_empty_page(out_pdpt_ph_addr);
  if (EFI_ERROR(status)) {
    return status;
  }
  EFI_PHYSICAL_ADDRESS pd_ph_addr;
  status = create_pd(pd_idx, pt_idx, physical_addr, &pd_ph_addr, flags);
  if (EFI_ERROR(status)) {
    return status;
  }
  out_pdpt_ph_addr[pdpt_idx] = (pd_ph_addr << 12) | flags;
  return EFI_SUCCESS;
}
EFI_STATUS create_pd(UINT16 pd_idx, UINT16 pt_idx,
                     EFI_PHYSICAL_ADDRESS physical_addr,
                     EFI_PHYSICAL_ADDRESS *out_pd_ph_addr, UINT16 flags) {

  EFI_STATUS status;
  status = allocate_empty_page(out_pd_ph_addr);
  if (EFI_ERROR(status)) {
    return status;
  }
  EFI_PHYSICAL_ADDRESS pt_ph_addr;

  status = create_pt(pt_idx, physical_addr, &pt_ph_addr, flags);
  if (EFI_ERROR(status)) {
    return status;
  }
  out_pd_ph_addr[pd_idx] = (pt_ph_addr << 12) | flags;
  return EFI_SUCCESS;
}

EFI_STATUS create_pt(UINT16 pt_idx, EFI_PHYSICAL_ADDRESS physical_addr,
                     EFI_PHYSICAL_ADDRESS *out_pt_ph_addr, UINT16 flags) {
  EFI_STATUS status;
  // allocate the table page
  status = allocate_empty_page(out_pt_ph_addr);
  if (EFI_ERROR(status)) {
    return status;
  }

  // add the flags to the page physical addr and write it to the PT
  out_pt_ph_addr[pt_idx] = (physical_addr << 12) | flags;
  return EFI_SUCCESS;
}

EFI_STATUS activate_long_mode_paging(EFI_PHYSICAL_ADDRESS pml4_addr) {
  return EFI_SUCCESS;
}
