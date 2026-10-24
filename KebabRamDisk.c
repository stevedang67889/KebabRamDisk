#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/RamDisk.h>

//
// The raw payload FAT image embedded in FVMAIN_COMPACT by kebab.fdf.
//
STATIC EFI_GUID  mPayloadGuid = { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x60 } };

EFI_STATUS
EFIAPI
KebabRamDiskEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_HANDLE                      *Handles;
  UINTN                           Count;
  UINTN                           Index;
  EFI_FIRMWARE_VOLUME2_PROTOCOL   *Fv;
  EFI_RAM_DISK_PROTOCOL           *RamDisk;
  EFI_FV_FILETYPE                 Type;
  EFI_FV_FILE_ATTRIBUTES          Attributes;
  UINT32                          AuthenticationStatus;
  UINTN                           Size;
  VOID                            *Buffer;

  Size   = 0;
  Buffer = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    Print (L"[KebabRamDisk] no firmware volumes: %r\n", Status);
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < Count && Buffer == NULL; Index++) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv))) {
      continue;
    }

    Status = Fv->ReadFile (Fv, &mPayloadGuid, NULL, &Size, &Type, &Attributes, &AuthenticationStatus);
    if (EFI_ERROR (Status) || (Size == 0)) {
      continue;
    }

    Buffer = AllocatePool (Size);
    if (Buffer == NULL) {
      break;
    }

    Status = Fv->ReadFile (Fv, &mPayloadGuid, Buffer, &Size, &Type, &Attributes, &AuthenticationStatus);
    if (EFI_ERROR (Status)) {
      FreePool (Buffer);
      Buffer = NULL;
    }
  }

  FreePool (Handles);

  if (Buffer == NULL) {
    Print (L"[KebabRamDisk] payload file not found\n");
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDisk);
  if (EFI_ERROR (Status)) {
    Print (L"[KebabRamDisk] RamDisk protocol missing: %r\n", Status);
    return EFI_SUCCESS;
  }

  Status = RamDisk->Register (
                       (UINT64)(UINTN)Buffer,
                       (UINT64)Size,
                       &gEfiVirtualDiskGuid,
                       NULL,
                       NULL
                       );
  Print (L"[KebabRamDisk] %u bytes registered: %r\n", (UINT64)Size, Status);
  return EFI_SUCCESS;
}
