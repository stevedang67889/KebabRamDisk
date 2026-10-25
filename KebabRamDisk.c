#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

//
// The payload FAT image is a raw region of the firmware device at 0x300000.
// Publish it as a read-only block device so PartitionDxe/FatPkg mount it; a
// RAM disk through RamDiskDxe needs its memory allocated as pages and refused
// the pool buffer this driver can get.
//
#define KEBAB_FD_BASE        0x9FC00000UL
#define KEBAB_PAYLOAD_OFFSET 0x00300000UL
#define KEBAB_BLOCK_SIZE     512U

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH         Vendor;
  EFI_DEVICE_PATH_PROTOCOL   End;
} KEBAB_DEVICE_PATH;
#pragma pack()

STATIC KEBAB_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
    { 0x7F3C2B1A, 0x9D4E, 0x4C21, { 0x8B, 0x7A, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x75 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC EFI_BLOCK_IO_MEDIA     mMedia;
STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo;
STATIC UINT8                  *mPayload;
STATIC UINT64                 mSize;

STATIC
EFI_STATUS
EFIAPI
KebabReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
KebabReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  UINT64 Offset;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  Offset = MultU64x32 (Lba, KEBAB_BLOCK_SIZE);
  if ((MediaId != mMedia.MediaId) || (Offset + BufferSize > mSize)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mPayload + Offset, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
KebabWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
KebabFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
KebabRamDiskEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;
  UINT32      Start;
  UINT32      Sectors;

  mPayload = (UINT8 *)(KEBAB_FD_BASE + KEBAB_PAYLOAD_OFFSET);
  DEBUG ((DEBUG_ERROR, "[KebabRamDisk] entry, payload at %p\n", mPayload));

  if ((mPayload[510] != 0x55) || (mPayload[511] != 0xAA)) {
    DEBUG ((DEBUG_ERROR, "[KebabRamDisk] no MBR signature\n"));
    return EFI_SUCCESS;
  }

  CopyMem (&Start, mPayload + 446 + 8, sizeof (Start));
  CopyMem (&Sectors, mPayload + 446 + 12, sizeof (Sectors));
  mSize = MultU64x32 ((UINT64)Start + Sectors, KEBAB_BLOCK_SIZE);
  DEBUG ((DEBUG_ERROR, "[KebabRamDisk] MBR: start %u sectors %u size %lu\n", Start, Sectors, mSize));

  if ((mSize == 0) || (mSize > (128ULL * 1024 * 1024))) {
    DEBUG ((DEBUG_ERROR, "[KebabRamDisk] implausible size\n"));
    return EFI_SUCCESS;
  }

  ZeroMem (&mMedia, sizeof (mMedia));
  mMedia.MediaId          = 0;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = KEBAB_BLOCK_SIZE;
  mMedia.IoAlign          = 0;
  mMedia.LastBlock        = DivU64x32 (mSize, KEBAB_BLOCK_SIZE) - 1;

  mBlockIo.Revision    = EFI_BLOCK_IO_PROTOCOL_REVISION;
  mBlockIo.Media       = &mMedia;
  mBlockIo.Reset       = KebabReset;
  mBlockIo.ReadBlocks  = KebabReadBlocks;
  mBlockIo.WriteBlocks = KebabWriteBlocks;
  mBlockIo.FlushBlocks = KebabFlushBlocks;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                    &Handle,
                    &gEfiBlockIoProtocolGuid, &mBlockIo,
                    &gEfiDevicePathProtocolGuid, &mDevicePath,
                    NULL
                    );
  DEBUG ((DEBUG_ERROR, "[KebabRamDisk] block device installed: %r\n", Status));
  return EFI_SUCCESS;
}
