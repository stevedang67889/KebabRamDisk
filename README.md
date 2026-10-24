# KebabRamDisk

UEFI DXE driver for the OnePlus 8T (kebab, SM8250): registers the payload FAT
image that is embedded in the firmware volume as a RAM disk, so UniXNU can find
`\EFI\UniXNU\unixnu.toml` without a storage driver.

Part of the [UniXNU](https://github.com/UniXNU) ecosystem. Used by
[UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab) as
`drivers/KebabRamDisk`.

## Why

On the kebab port the vendor `UFSDxe` does not produce a block device (it
asserts with `Device Error` at `UFS.c:1004`, and with `EnableUfsIOC = 1` it
fails silently), so no `SimpleFileSystem` handle exists and UniXNU falls back to
its built-in menu. Until UFS works, the payload volume travels with the
firmware:

1. `kebab.fdf` packs the payload FAT image into `FVMAIN_COMPACT` as a raw
   `FILE FREEFORM` (uncompressed, memory mapped at the FD base).
2. This driver reads that file out of the firmware volume and registers the
   buffer through `EFI_RAM_DISK_PROTOCOL`.
3. `FatPkg` mounts it, and UniXNU scans it for its configuration.

The firmware volume grows by the size of the payload (64 MiB for kebab); the
boot partition is 96 MiB, so this fits with room to spare.

## Requirements

| Requirement | Why |
| --- | --- |
| `MdeModulePkg/Universal/Disk/RamDiskDxe` | provides `EFI_RAM_DISK_PROTOCOL` |
| `FatPkg/EnhancedFatDxe` | mounts the FAT volume |
| The payload file GUID in `KebabRamDisk.c` | must match the `FILE FREEFORM` GUID in the FDF |

## Integration

`kebab.fdf`:

```
  FILE FREEFORM = 7F3C2B1A-9D4E-4C21-8B7A-1E2D3C4B5A60 {
    SECTION RAW = <path to the payload FAT image>
  }
```

`kebab.dsc`:

```
  KebabRamDisk/KebabRamDisk.inf
```

The image must be an MBR disk with one FAT32 partition and 512-byte sectors,
because `RamDiskDxe` reports `BlockSize = 512` and `PartitionDxe` only creates a
partition child when it finds a partition table.

## License

BSD-2-Clause-Patent, matching UniXNU and Mu-Silicium.
