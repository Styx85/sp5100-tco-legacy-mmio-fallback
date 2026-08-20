# sp5100-tco legacy MMIO relocation fallback

Experimental Linux `sp5100_tco` watchdog driver patch adding a reversible
MMIO relocation mechanism for older AMD SP5100/SB7x0 southbridges.

## Background

On some legacy AMD southbridges the firmware-provided watchdog MMIO address
is `0xfec000f0`.

On the tested system this address lies inside the kernel resource assigned to
the IOAPIC:

```text
fec00000-ffffffff : Reserved
  fec00000-fec003ff : IOAPIC 0
```

The current Linux `sp5100_tco` driver therefore fails to reserve the watchdog
region:

```text
sp5100-tco: Failed to reserve MMIO or alternate MMIO region
sp5100-tco: probe with driver sp5100-tco failed with error -16
```

Linux previously contained a relocation mechanism for this class of conflict.

It was introduced for Linux 3.8 by commit 740fbddf5c3f
("watchdog: sp5100_tco: Add SB8x0 chipset support").

The mechanism was deliberately removed shortly afterwards by commit
18e4321276fc ("watchdog: sp5100_tco: Remove code that may cause a boot
failure") after an SB700 system failed to load BIOS after running a kernel
containing the relocation path until power was completely removed.

The exact root cause of that historical failure is not documented. One
relevant difference is that the old relocation path reprogrammed the watchdog
base but did not restore the firmware-programmed PM register state when the
driver was removed.

This repository therefore implements relocation as a reversible operation:
the original PM control and base registers are saved before reprogramming and
restored on probe failure or device removal.

## Tested hardware

Tested on:

* Gigabyte GA-78LMT-USB3
* Board revision: 4.1
* BIOS: Award F4, 2012-10-19
* AMD SBx00 SMBus controller
* PCI ID: `1002:4385`
* PCI revision: `0x3c`
* Arch Linux
* Hardware-tested kernel: `7.1.8-arch1-3`

The `sp5100_tco.c` driver in current upstream Linux mainline was verified to
be identical to the Linux 7.1.8 version used for the hardware tests.

## Original watchdog configuration

The firmware programs the watchdog MMIO base to:

```text
0xfec000f0
```

The AMD PM registers contain:

```text
PM 0x6c = 0xf0
PM 0x6d = 0x00
PM 0x6e = 0xc0
PM 0x6f = 0xfe
```

which is little-endian `0xfec000f0`.

This address overlaps the IOAPIC kernel resource.

## Relocation

The experimental fallback dynamically locates a free, naturally aligned
8-byte child resource inside the existing firmware-reserved MMIO parent.

On the tested system the first available range is:

```text
0xfec00400
```

The patched driver registers this range as a child resource of the existing
firmware-reserved MMIO window:

```text
fec00000-ffffffff : Reserved
  fec00000-fec003ff : IOAPIC 0
  fec00400-fec00407 : SP5100 TCO
```

The AMD PM registers are reprogrammed to:

```text
PM 0x6c = 0x00
PM 0x6d = 0x04
PM 0x6e = 0xc0
PM 0x6f = 0xfe
```

which is little-endian `0xfec00400` on the tested system.

## Result

With the patched driver:

```text
sp5100-tco: Relocated legacy SP5100 watchdog MMIO from 0xfec000f0 to 0xfec00400
sp5100-tco: Using relocated 0xfec00400 for watchdog MMIO address
sp5100-tco: initialized. heartbeat=60 sec (nowayout=0)
```

The watchdog device is registered successfully:

```text
/dev/watchdog
/dev/watchdog0
```

Sysfs reports:

```text
identity   : SP5100 TCO timer
timeout    : 60
nowayout   : 0
state      : inactive
status     : 0x0
bootstatus : 0
```

## Repository contents

```text
sp5100_tco.c
    Patched watchdog driver.

sp5100_tco.h
    Corresponding driver header.

patches/
    Patch against Linux v7.1.8.

read-amd-pm.c
    Diagnostic helper for reading AMD PM registers.

Makefile
    External kernel module build file.
```

## Build

On Arch Linux:

```bash
sudo pacman -S --needed linux-headers base-devel
```

Build the module:

```bash
make
```

Verify:

```bash
modinfo ./sp5100_tco.ko
```

The module `vermagic` must match the running kernel.

## Experimental loading

Remove the in-tree driver if loaded:

```bash
sudo modprobe -r sp5100_tco
```

Load the patched module:

```bash
sudo insmod ./sp5100_tco.ko
```

Inspect the result:

```bash
sudo dmesg | grep -i -E 'sp5100|watchdog|MMIO'
ls -l /dev/watchdog*
```

Inspect the MMIO resource:

```bash
sudo grep -i -A10 '^fec00000' /proc/iomem
```

## PM register diagnostic helper

Build:

```bash
gcc -O2 -Wall -Wextra -o read-amd-pm read-amd-pm.c
```

Run:

```bash
sudo ./read-amd-pm
```

## Current status

Verified:

* firmware watchdog base is `0xfec000f0`
* this address conflicts with the IOAPIC resource
* the current driver fails with `-EBUSY`
* the fallback path is entered
* the watchdog can be relocated to a dynamically selected free 8-byte MMIO window
* PM registers `0x6c..0x6f` are reprogrammed
* the resource is represented correctly in `/proc/iomem`
* `sp5100_tco` initializes successfully
* `/dev/watchdog0` is registered
* the original PM control and base registers are restored on module removal
* the relocated resource disappears from `/proc/iomem` on module removal
* the final upstream patch passes `checkpatch.pl --strict` with 0 errors,
  0 warnings and 0 checks

A deliberate hardware watchdog timeout/reset has not yet been tested.

## Limitations

This is currently an experimental and hardware-specific implementation.

The fallback address is selected dynamically from free child space inside
the existing firmware-reserved MMIO parent. No board-specific replacement
address is hard-coded.

Do not use this code on unrelated hardware without understanding the AMD
southbridge watchdog and MMIO resource layout involved.

## License

The source files retain their upstream Linux SPDX license declarations.

`sp5100_tco.c` uses `GPL-2.0-or-later`, while `sp5100_tco.h` uses `GPL-2.0`.

The repository contains the GNU General Public License version 2 text in
`LICENSE`.
