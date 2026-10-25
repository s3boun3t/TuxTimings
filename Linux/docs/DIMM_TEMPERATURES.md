# DIMM temperature association

The selected DIMM has a separate Temperature row. `RAM sensors` remains a
machine-wide inventory: its enumeration order does not identify DIMM slots.

## Evidence required

The Linux backend reads only devices already exposed by sysfs. It does not
scan SMBus addresses, instantiate I2C devices, load additional modules, access
`/dev/i2c-*`, program EEPROMs, or invoke firmware methods for this feature.
EEPROM files are opened read-only through their existing kernel drivers.

A selected-DIMM association requires all of the following:

- A non-placeholder SMBIOS serial representing exactly four hex bytes and a
  nonempty part number. Peripheral whitespace, hex letter case and an optional
  `0x` prefix on the serial are normalized; byte order and numbering are not.
- That serial occurs once in the populated SMBIOS inventory and once in the SPD
  inventory. Duplicate serials count even if a duplicate SPD lacks a usable part
  number or a temperature sensor.
- The trimmed part numbers match exactly and the per-DIMM SMBIOS Type agrees
  with the DDR4/DDR5 SPD format and the bound EEPROM driver. CPU codename is not
  a substitute for the SMBIOS Type. Unknown and LPDDR formats are unassociated.
- The temperature sensor has the corresponding kernel device topology.

For DDR4, an ee1004 SPD at 0x50-0x57 and a JC42 sensor must share the same
canonical adapter path. The JC42 address is `0x18 | (spd_address & 7)`.
For DDR5, the SPD5118 EEPROM and its hwmon sensor share one canonical I2C device.
A bus number, hwmon index, UMC index, slot enumeration or nearby temperature does
not independently establish the physical slot.

The inventory is bounded at 32 SPD devices and 32 supported sensors. Overflow,
failed directory enumeration, unresolved driver links at I2C SPD addresses
0x50-0x57, or incomplete SMBIOS disables association rather than accepting apparent
uniqueness from a partial list. Missing or dangling driver links at other I2C
addresses do not block association; resolved non-SPD drivers are ignored.
SMBIOS command output is accepted only after successful stream reading and
command exit. A missing, unknown
or unusable SMBIOS Size also makes the inventory incomplete; only explicit
`No Module Installed` and `Not Installed` values identify empty slots. An
unreadable or unknown SPD identity also prevents claiming uniqueness. Readable sensors remain visible
in the global inventory even when association cannot be established.

On the documented B550-A configuration the SMBIOS serials are `00000000`.
Those serials provide no automatic A2/B2-to-SPD identity. The expected result is
`Unassociated`, while both independently identified JC42 readings remain visible.
No board-specific or manual mapping is substituted.

## Availability and refresh

Association and measurement are separate states. `Unassociated` means the
SMBIOS-to-SPD link is absent or ambiguous. Once that link is established,
`Unavailable` means the sensor is missing or its current sample failed. A genuine
0 degrees Celsius is valid. The historical 0-150 degrees plausibility range is
retained, with strict integer parsing of millidegrees. Failed samples immediately
clear the prior value.

Identities are cached for the process lifetime. The backend compares canonical
device identities and registration metadata on each refresh; discovery changes
invalidate the cache. EEPROM contents are not polled every second. Temperatures
are read each refresh. Selecting a different DIMM renders the last snapshot
immediately without triggering new hardware reads. The new row does not change
the UMC0 source of the existing DRAM timings.

## Identity layout references

The decoder contains only DDR4/DDR5 type, serial and part-number extraction.
It reads at most 512 or 1024 bytes respectively and rejects short or unsupported
images. No timing, capacity or voltage decoder is imported.

- [Linux ee1004, revision 8abc1c8badc11c859bb4ba43211611347e59c55c](https://code.googlesource.com/linux/torvalds/linux/+/8abc1c8badc11c859bb4ba43211611347e59c55c/drivers/misc/eeprom/ee1004.c):
  same-adapter SPD-to-JC42 address convention.
- [Linux SPD5118 documentation](https://docs.kernel.org/hwmon/spd5118.html):
  the EEPROM attribute belongs to the sensor's I2C device.
- [i2c-tools decode-dimms, blob 76823459da4693fe5a7ecf5b6e3aa092f280918a](https://kernel.googlesource.com/pub/scm/utils/i2c-tools/i2c-tools/+/refs/heads/master/eeprom/decode-dimms):
  DDR4 serial bytes 325-328 and 20-byte part number at 329.
- [coreboot DDR5 layout](https://github.com/coreboot/coreboot/blob/main/src/include/device/dram/ddr5.h):
  DDR5 serial bytes 517-520 and 30-byte part number at 521.

These are layout and driver-interface references. No implementation was copied
from those projects. There is no persistent mapping or hardware calibration.

## Offline verification

`Linux/tests/test_dimm_sensors.c` builds synthetic sysfs trees under `/tmp`.
It exercises identity decoding, multiple adapters, reordered modules/hwmon,
ambiguous and unusable serials, missing or short EEPROMs, unresolved and restored
SPD driver links, missing or duplicate matching sensors, cache invalidation, zero
and malformed samples, overflow, and directory I/O failures.
The SMBIOS parser and command-reader tests inject output and stdio failures to
cover failed commands, partial reads, uncertain sizes, explicit empty slots and
module overflow. Neither these C tests nor the GTK fixture harness read real
hardware or execute discovery commands.

GTK integration can be checked with `make -C Linux/tests check-ui` under a
working display. It uses synthetic backend data, verifies selection without
polling, and captures the real widgets without accessing hardware.
