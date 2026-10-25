# KebabRamDisk

A UniXNU UEFI driver that publishes the firmware device's payload region as a
read-only block device on the OnePlus 8T (kebab, SM8250).

## Why a block device and not a RAM disk

The firmware image (`SILICIUM_UEFI.fd`) carries the UniXNU payload as a raw FDF
region at offset `0x00300000`. Three approaches were tried on the device:

| Approach | Result |
| --- | --- |
| Payload as an FFS file inside the FV | The FFS size field is 24-bit, so a 64 MiB payload is impossible; forcing it produced a CPU exception |
| `RamDiskDxe` + `EFI_RAM_DISK_PROTOCOL` | `Register` returned `EFI_INVALID_PARAMETER`: the protocol requires page-allocated memory, which this driver cannot obtain after DXE |
| This driver: `EFI_BLOCK_IO_PROTOCOL` over the FD region | Works. PartitionDxe and FatPkg mount the volume and UniXNU reads `unixnu.toml` from it |

## How it works

1. The FDF places the payload FAT image at `0x00300000` in the FD, and the FD is
   loaded at `0x9FC00000`, so the payload is reachable at `0x9FC00000 + 0x00300000`.
2. `KebabRamDiskEntryPoint` installs a `EFI_BLOCK_IO_PROTOCOL` on a new handle
   with a vendor device path.
3. `ReadBlocks`/`WriteBlocks` map LBAs straight onto that region (512-byte
   blocks, read-only, `WriteBlocks` returns `EFI_WRITE_PROTECTED`).

The device path matters: `PartitionDxe` only binds handles that carry one, and
without it the volume never appears even though the block I/O is installed.

## Build

The driver is built as part of the kebab Mu-Silicium platform:

```
[Components]
  kebabPkg/Driver/KebabRamDisk/KebabRamDisk.inf
```

Add `Platforms/OnePlus/kebabPkg/Driver/KebabRamDisk` to the tree and map the
package in `PACKAGES_PATH` (`kebabPkg/Driver/KebabRamDisk` must be the parent
directory of `KebabRamDisk.inf`).

## Evidence

With this driver the UniXNU boot menu lists the entry generated from
`board.toml` (`kebab first boot: source 4K XNU to console`) instead of the
built-in fallback menu, which proves the payload volume was found and parsed.

## License

BSD-2-Clause-Patent.