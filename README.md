````markdown
# sp5100-tco GA-78LMT-USB3 MMIO quirk

Linux `sp5100_tco` watchdog driver patch adding a narrowly scoped DMI quirk
for the Gigabyte GA-78LMT-USB3.

The board firmware programs the legacy SP5100 watchdog MMIO window at
`0xfec000f0`. This address lies inside the kernel resource assigned to the
IOAPIC, causing the unmodified driver to fail probing because it cannot reserve
the watchdog MMIO range.

The v3 patch leaves the firmware-programmed watchdog address unchanged and,
only on the affected board, permits the driver to map and use that MMIO window
without reserving it.

No watchdog MMIO relocation is performed.

## Background

On the tested Gigabyte GA-78LMT-USB3 the firmware programs the watchdog at:

```text
0xfec000f0
````

The corresponding kernel resource tree contains:

```text
fec00000-ffffffff : Reserved
  fec00000-fec003ff : IOAPIC 0
```

The watchdog range `0xfec000f0-0xfec000f7` therefore lies inside the IOAPIC
resource.

The unmodified Linux `sp5100_tco` driver tries to reserve the watchdog MMIO
window and fails:

```text
sp5100-tco: Failed to reserve MMIO or alternate MMIO region
sp5100-tco: probe with driver sp5100-tco failed with error -16
```

Linux previously contained a relocation mechanism for this class of conflict.

It was introduced for Linux 3.8 by commit `740fbddf5c3f`
(`watchdog: sp5100_tco: Add SB8x0 chipset support`).

The mechanism was deliberately removed shortly afterwards by commit
`18e4321276fc`
(`watchdog: sp5100_tco: Remove code that may cause a boot failure`)
after an SB700 system failed to load BIOS after running a kernel containing
the relocation path until power was completely removed.

Earlier versions of this patch attempted to restore that relocation approach.
After upstream review, this was dropped in favor of a narrowly scoped DMI
quirk for the affected system.

The v3 implementation leaves the firmware-programmed watchdog base unchanged.
On the affected GA-78LMT-USB3, the driver is allowed to use the existing
watchdog MMIO window without reserving it.

The exception is restricted to the legacy SP5100 register layout, the
GA-78LMT-USB3 DMI identity, and the firmware address `0xfec000f0`.

All other systems retain the existing resource reservation behavior.

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

The tested controller is handled by the legacy `sp5100` register layout.

## Firmware watchdog configuration

The firmware programs the watchdog MMIO base to:

```text
0xfec000f0
```

The AMD PM registers contain:

```text
PM 0x69 = 0x03
PM 0x6c = 0xf0
PM 0x6d = 0x00
PM 0x6e = 0xc0
PM 0x6f = 0xfe
```

Registers `0x6c..0x6f` encode the little-endian watchdog MMIO address:

```text
0xfec000f0
```

This address overlaps the IOAPIC kernel resource.

## DMI quirk

If the normal watchdog MMIO reservation fails on the affected
GA-78LMT-USB3, the driver uses the firmware-programmed `0xfec000f0` window
without reserving it.

The quirk is only used when all of the following match:

```text
System vendor   : Gigabyte Technology Co., Ltd.
Product name    : GA-78LMT-USB3
Register layout : legacy SP5100
MMIO address    : 0xfec000f0
```

No watchdog MMIO relocation is performed.

The patch does not:

* search for another MMIO range
* relocate the watchdog
* rewrite the watchdog base registers
* add a new watchdog child resource to `/proc/iomem`

All other systems retain the normal upstream resource reservation behavior.

## Result

With the patched v3 driver:

```text
sp5100_tco: SP5100/SB800 TCO WatchDog Timer Driver
sp5100-tco sp5100-tco: Using firmware watchdog MMIO 0xfec000f0 without reserving it
sp5100-tco sp5100-tco: Using 0xfec000f0 for watchdog MMIO address
sp5100-tco sp5100-tco: initialized. heartbeat=60 sec (nowayout=0)
```

The watchdog device is registered successfully:

```text
/dev/watchdog
/dev/watchdog0
```

The watchdog MMIO base remains unchanged after loading the driver:

```text
PM 0x6c = 0xf0
PM 0x6d = 0x00
PM 0x6e = 0xc0
PM 0x6f = 0xfe
```

`PM 0x69` changes from `0x03` to `0x06` as part of the existing driver's
normal watchdog enable/resolution setup. This is unrelated to the DMI quirk.

The resource tree remains unchanged while the driver is loaded:

```text
fec00000-ffffffff : Reserved
  fec00000-fec003ff : IOAPIC 0
  fed00000-fed003ff : HPET 0
```

In particular, no separate `SP5100 TCO` resource is inserted.

After unloading the module, the watchdog MMIO base remains `0xfec000f0`.

## Patch history

### v3

Current implementation:

* removes MMIO relocation entirely
* adds a DMI-scoped exception for the Gigabyte GA-78LMT-USB3
* requires the legacy SP5100 register layout
* requires the firmware watchdog address `0xfec000f0`
* uses the firmware MMIO window without reserving it
* retains existing behavior on all other systems
* hardware tested successfully
* upstream patch passes `checkpatch.pl --strict` with:

  * 0 errors
  * 0 warnings
  * 0 checks

### v2

The previously submitted v2 implemented reversible MMIO relocation.

It is retained for historical reference under:

```text
patches/archive/v2/
```

The corresponding repository state is also preserved by:

```text
tag:    v2-submitted
branch: archive/v2
```

## Repository contents

```text
sp5100_tco.c
    Patched watchdog driver.

sp5100_tco.h
    Corresponding driver header.

patches/
    Current upstream-style v3 patch submission.

patches/archive/v2/
    Archived upstream v2 relocation patch.

read-amd-pm.c
    Diagnostic helper for reading AMD PM registers.

Makefile
    External kernel module build file.

dkms.conf
    DKMS configuration for persistent installation.
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

## DKMS installation

For persistent use across kernel updates, the repository includes a
`dkms.conf`.

Install DKMS and the kernel headers:

```bash
sudo pacman -S --needed dkms linux-headers
```

Register the source tree with DKMS:

```bash
sudo dkms add "$PWD"
sudo dkms install sp5100-tco-legacy/0.1
```

Check the installation:

```bash
dkms status
modinfo -n sp5100_tco
```

The module should be loaded from a DKMS-managed path such as:

```text
/usr/lib/modules/<kernel>/updates/dkms/sp5100_tco.ko.zst
```

To load the patched driver automatically at boot:

```bash
echo sp5100_tco | sudo tee /etc/modules-load.d/sp5100_tco.conf
```

Arch Linux DKMS hooks rebuild and reinstall the module when a supported
kernel is updated.

## Upstream status

v3 is based on the current upstream `sp5100_tco` driver and implements the
maintainer-requested approach: a narrow DMI-specific exception instead of
generic watchdog MMIO relocation.

The patch is intended as a board-specific compatibility fix for the
GA-78LMT-USB3 firmware/resource conflict.

````

