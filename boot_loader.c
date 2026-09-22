#include "efidef.h"
#include <efi.h>
#include <efilib.h>

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

  EFI_PHYSICAL_ADDRESS kernel_pages;

  uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                    EFI_SIZE_TO_PAGES(kernel_file_size), &kernel_pages);

  uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &kernel_file_size,
                    (VOID *)kernel_pages);

  void (*kernel_entry)(void) = (void (*)(void))kernel_pages;

  return EFI_SUCCESS;
}
