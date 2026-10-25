#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/RamDisk.h>

//
// The payload FAT image is a raw region of the firmware device, not an FFS file.
// Keeping it out of the firmware volume avoids both the 0xFFFFFF-byte limit of an
// FFS file and the failures seen with a very large volume; kebab.fdf declares
//
//   0x00300000|0x...      FILE = <payload image>
//
// so the image sits at a fixed offset inside the device and can be read without
// parsing anything.
//
#define KEBAB_FD_BASE        0x9FC00000UL
#define KEBAB_PAYLOAD_OFFSET 0x00300000UL

EFI_STATUS
EFIAPI
KebabRamDiskEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS             Status;
  EFI_RAM_DISK_PROTOCOL  *RamDisk;
  UINT8                  *Payload;
  UINT8                  *Buffer;
  UINT64                 Start;
  UINT64                 Sectors;
  UINT64                 Size;

  Payload = (UINT8 *)(KEBAB_FD_BASE + KEBAB_PAYLOAD_OFFSET);
  DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] entry, payload at %p\n", Payload));

  // Reject a missing or non-MBR image instead of registering garbage.
  if ((Payload[510] != 0x55) || (Payload[511] != 0xAA)) {
    DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] no MBR signature, nothing registered\n"));
    return EFI_SUCCESS;
  }

  Start   = *(UINT32 *)(Payload + 446 + 8);
  Sectors = *(UINT32 *)(Payload + 446 + 12);
  Size    = (Start + Sectors) * 512;
  DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] partition starts at %u, %u sectors, %u bytes\n", Start, Sectors, Size));

  if ((Size == 0) || (Size > (64 * 1024 * 1024))) {
    DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] implausible size, nothing registered\n"));
    return EFI_SUCCESS;
  }

  Buffer = AllocatePool ((UINTN)Size);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] out of memory\n"));
    return EFI_SUCCESS;
  }

  CopyMem (Buffer, Payload, (UINTN)Size);

  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDisk);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] RamDisk protocol missing: %r\n", Status));
    return EFI_SUCCESS;
  }

  Status = RamDisk->Register ((UINT64)(UINTN)Buffer, Size, &gEfiVirtualDiskGuid, NULL, NULL);
  DEBUG ((DEBUG_ERROR, L"[KebabRamDisk] %u bytes registered: %r\n", Size, Status));
  return EFI_SUCCESS;
}
