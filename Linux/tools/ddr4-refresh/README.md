# DDR4 refresh capture

An optional, one-shot diagnostic for Vermeer (codename 12), using the audited
ryzen_smu 0.1.7 sysfs protocol. This is not part of the application, its installer,
DKMS or its refresh loop. Other targets stop before opening the SMN interface;
this guard limits the diagnostic's scope, not the application's compatibility.
There are no motherboard-specific addresses or corrections.

## Why capture both registers?

On the test machine, the BIOS shows tRFC/tRFC2/tRFC4 = 312/192/132 while
TuxTimings shows 630/468 for the first two. The BIOS triplet decodes exactly from
0x21060138, the pattern both TuxTimings and ZenStates-Core treat as a default.
Their current rule selects RFC1 only if RFC0 equals that default and the two
registers differ; otherwise it selects RFC0. This is a heuristic, not evidence
that the register belongs to the active rank. The discrepancy alone does not
prove either the BIOS display or the software reading correct.

The tool captures both registers on UMC0 and UMC1 (offsets 0 and 0x100000), plus
chip-select configuration, ratio, refresh-mode, primary-timing, REFI and UMC
status registers. Three consecutive passes show whether the captured values
are repeatable. Only stable channels are decoded. No scan outside this fixed
allowlist is possible through the tool. A stable read is not proof of validity.
UMC indices and chip-select bits are not labelled as motherboard DIMM slots.
RFC4 is decoded separately from DDR5 RFCsb; FGR is reported as a raw code.
No active refresh mode or per-DIMM assignment is inferred.

## Read protocol and limitations

The audited [driver drv.c](https://github.com/amkillam/ryzen_smu/blob/main/drv.c)
(Git blob 9dd747186dd618a0dd3df8fe59ac8dc7c7fbee9c) dispatches by payload length:

- Exactly 4 bytes (one little-endian address) request a register **read**.
- 8 bytes (address and value) write the target register. This tool never submits
  that format, combines requests or retries partial writes.

The low-level SMN read selects an address through PCI config space, as the
existing application does. It does not request a write to the selected SMN
register. No SMU command, WMI call, MSR access or BIOS setting change is added.

The sysfs result cache is global, not per-open-file. Close TuxTimings, OCCT,
ryzen_monitor and any other program that reads SMN before capture. An advisory
lock cannot exclude non-cooperating programs; the script does not claim exclusive
access. The driver also does not propagate every low-level read failure to the
syscall return value. Short I/O is rejected, but successful I/O cannot certify
that the cached value came from a successful hardware read.

Decode/selection references:

- TuxTimings Linux/src/dram.c, upstream ffd12bf plus refresh patch 0001.
- [ZenStates-Core v1.93 Ddr4Timings.cs](https://github.com/irusanov/ZenStates-Core/blob/869b7e8/Hardware/DRAM/Ddr4Timings.cs).
- [ZenStates-Core v1.93 MemoryConfig.cs](https://github.com/irusanov/ZenStates-Core/blob/869b7e8/Hardware/DRAM/MemoryConfig.cs)
  uses channel offsets i << 20 and DDR4 chip-select enable bits at 0x50000+4*CS.

## Use

Hardware-free tests (no root needed):

```sh
python3 -B -m unittest discover -s Linux/tools/ddr4-refresh -v
```

After closing the programs listed above, capture once from the repository root:

```sh
set -o pipefail
sudo python3 -B Linux/tools/ddr4-refresh/capture_refresh.py |
    tee ddr4-refresh-report.txt
```

No application rebuild, package installation, module reload or compiler is needed.
Keep the report, including all raw values and any CHANGED markers. If the target
guard rejects a different driver version, inspect that driver's source before
expanding the guard; do not remove it to force a read.
