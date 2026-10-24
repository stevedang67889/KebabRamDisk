#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/RamDisk.h>

//
// The payload FAT image is embedded in FVMAIN_COMPACT as several raw FFS files.
// A single one cannot exceed 0xFFFFFF bytes (the FFS size field is 24 bits), so
// the image is split into 0xFFFF00-byte chunks that are concatenated here.
//
#define KEBAB_CHUNK_SIZE  0xFFFF00U
#define KEBAB_CHUNK_COUNT 4U

STATIC EFI_GUID  mPayloadGuids[KEBAB_CHUNK_COUNT] = {
  { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x60 } },
  { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x61 } },
  { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x62 } },
  { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x63 } },
};

//
// Read one embedded chunk into Destination. Returns the number of bytes copied.
//
STATIC
UINTN
ReadChunk (
  IN     EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv,
  IN     EFI_GUID                       *Guid,
  IN     VOID                           *Destination,
  IN     UINTN                          DestinationSize
  )
{
  EFI_STATUS              Status;
  EFI_FV_FILETYPE         Type;
  EFI_FV_FILE_ATTRIBUTES  Attributes;
  UINT32                  AuthenticationStatus;
  UINTN                   Size;

  Size   = 0;
  Status = Fv->ReadFile (Fv, Guid, NULL, &Size, &Type, &Attributes, &AuthenticationStatus);
  if (EFI_ERROR (Status) || (Size == 0) || (Size > DestinationSize)) {
    return 0;
  }

  Status = Fv->ReadFile (Fv, Guid, Destination, &Size, &Type, &Attributes, &AuthenticationStatus);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  return Size;
}

EFI_STATUS
EFIAPI
KebabRamDiskEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     *Handles;
  UINTN                          Count;
  UINTN                          Index;
  UINTN                          Chunk;
  EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv;
  EFI_RAM_DISK_PROTOCOL          *RamDisk;
  UINT8                          *Buffer;
  UINTN                          Total;
  UINTN                          Copied;

  Fv     = NULL;
  Buffer = NULL;
  Total  = 0;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    Print (L"[KebabRamDisk] no firmware volumes: %r\n", Status);
    return EFI_SUCCESS;
  }

  // Find the volume that holds the chunk files.
  for (Index = 0; Index < Count && Fv == NULL; Index++) {
    EFI_FV_FILETYPE                 Type;
    EFI_FV_FILE_ATTRIBUTES          Attributes;
    UINT32                          AuthenticationStatus;
    UINTN                           Size;
    EFI_FIRMWARE_VOLUME2_PROTOCOL   *Candidate;

    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Candidate))) {
      continue;
    }

    Size = 0;
    if (!EFI_ERROR (Candidate->ReadFile (Candidate, &mPayloadGuids[0], NULL, &Size, &Type, &Attributes, &AuthenticationStatus)) &&
        (Size != 0))
    {
      Fv = Candidate;
    }
  }

  FreePool (Handles);

  if (Fv == NULL) {
    Print (L"[KebabRamDisk] payload chunks not found\n");
    return EFI_SUCCESS;
  }

  Total  = KEBAB_CHUNK_SIZE * KEBAB_CHUNK_COUNT;
  Buffer = AllocatePool (Total);
  if (Buffer == NULL) {
    Print (L"[KebabRamDisk] out of memory for %u bytes\n", (UINT64)Total);
    return EFI_SUCCESS;
  }

  Copied = 0;
  for (Chunk = 0; Chunk < KEBAB_CHUNK_COUNT; Chunk++) {
    UINTN Size;

    Size = ReadChunk (Fv, &mPayloadGuids[Chunk], Buffer + Copied, Total - Copied);
    if (Size == 0) {
      break;
    }

    Copied += Size;
  }

  if (Copied == 0) {
    FreePool (Buffer);
    Print (L"[KebabRamDisk] reading the chunks failed\n");
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDisk);
  if (EFI_ERROR (Status)) {
    Print (L"[KebabRamDisk] RamDisk protocol missing: %r\n", Status);
    return EFI_SUCCESS;
  }

  Status = RamDisk->Register ((UINT64)(UINTN)Buffer, (UINT64)Copied, &gEfiVirtualDiskGuid, NULL, NULL);
  Print (L"[KebabRamDisk] %u bytes registered: %r\n", (UINT64)Copied, Status);
  return EFI_SUCCESS;
}
