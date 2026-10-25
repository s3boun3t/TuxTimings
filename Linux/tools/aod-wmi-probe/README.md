# Legacy AOD WMI diagnostic

This optional tool captures one firmware snapshot per device binding. It is not
part of the application's build, installer, DKMS modules, or sensor refresh loop.
It has no board-specific addresses, voltage corrections, or CPU-family fallback.

## Scope

Supported transport: legacy AMD_ACPI WMI GUID
`ABBC0F6A-8EA1-11D1-00A0-C90629100000`, instance 0, exactly one instance.
The newer AMD AOD GUID `95F20B61-3D44-41F8-87E1-03CBF94C4E8F` is not matched.
An existing driver already bound to a WMI device is not detached.

On binding, the tool calls QueryVersion (method 1), passing a zero 32-bit dummy
input. Only version 5 proceeds to GetObjectID (method 3). That method must return
a native ACPI package with 193 elements: count, 64 IDs, 64 properties, 64 names.
The count must be at most 64; active entries must have the expected types. The
getter must have both ID `0x00010002` and the exact name `Get memory voltages`.
Duplicate getter IDs, different names or malformed packages block the request.
The property field is type-checked but is not interpreted as an enable flag.

By default, this is the entire firmware interaction. The `report` sysfs file
only formats the cached snapshot, is root-readable, and never triggers a call.
Reloading the module or rebinding the device produces a new snapshot.

The load-time parameter `read_voltages=1` additionally permits **one** method 5
call after the checks above, with the fixed eight-byte input
`02 00 01 00 00 00 00 00`. There is no interface for arbitrary methods, commands
or arguments. Init, Get APCB Config, setters and command-buffer start/end are
never called. There is no retry and no direct SMN/MSR/physical-memory access.

The audited legacy AML dispatches that command to R102: it writes exchange
version 5 and the getter command mask into the shared mailbox, then triggers an
SMI. Thus this is a **firmware getter**, not passive memory access. It is intended
to retrieve data and does not request a BIOS setting change; the SMM handler's
implementation is outside the AML and cannot be verified by these offline tests.

The reply must be a 200-byte buffer, report status zero at offset 2, and contain
exchange version 5 at offset 0x0c. VDIO, VMTT and VMPP are decoded as packed
little-endian 16-bit values at 0x1b, 0x1d and 0x1f. They remain **raw**: the tool
does not identify them as measured voltages or assume units. In particular VMTT
is not VDDQ. Zero fields remain zero; they are not reported to TuxTimings as
usable sensors. Even a zero status does not prove freshness or sensor accuracy.
Unknown/extended layouts are deliberately rejected for this diagnostic.

## Build and first check

Run hardware-free protocol tests:

```sh
make -C Linux/tools/aod-wmi-probe check
```

Build against the intended kernel headers. For a kernel built with LLVM 21:

```sh
make -C "/lib/modules/$(uname -r)/build" \
    M="$PWD/Linux/tools/aod-wmi-probe" LLVM=-21 modules
```

For a GCC kernel, omit `LLVM=-21`. Tested here against Ubuntu Linux 6.8 headers
with GCC 13. The native per-device WMI evaluation API is retained for that kernel
baseline; it also preserves the returned ACPI package types for inspection. This
is diagnostic code, not a proposed upstream Linux WMI driver.

Sign the locally compiled module if the running kernel requires a trusted
signature. First load it **without** the voltage parameter:

```sh
sudo insmod ./Linux/tools/aod-wmi-probe/tuxtimings_aod_probe.ko
```

Read each report under the bound devices:

```sh
for device in /sys/bus/wmi/devices/ABBC0F6A-8EA1-11D1-00A0-C90629100000*; do
    [ -f "$device/report" ] || continue
    printf '\n%s\n' "$device"
    sudo cat "$device/report"
done
```

Expected gating results on a supported interface: `version_ret=0`, `version=5`,
`capabilities_ret=0`, `query_requested=0`, `query_attempted=0`, `query_ret=-61`
(`-ENODATA`, no voltage request performed). Other return codes are diagnostics,
not a reason to bypass validation. If no report exists, inspect the kernel log
and whether another driver owns the device; do not forcibly unbind it.

After validating that inventory, a separate manual diagnostic can use
`read_voltages=1` when reloading the module. The same checks remain mandatory.
The module can be removed with `sudo rmmod tuxtimings_aod_probe`; it is not
installed for future boots by these commands.

## Evidence and limitations

- The supplied legacy AMD AOD SSDT and decoded MOF declare QueryVersion,
  GetObjectID and RunCommand. They show the getter dispatch, packed field widths
  and response copying. Hardware observation before querying showed empty fields.
- ZenStates-Core v1.93, commit `869b7e8`, `Hardware/Aod/Aod.cs` discovers names and
  IDs through GetObjectID/GetObjectID2; `Hardware/WMI.cs` constructs eight-byte
  RunCommand input. Its existence does not establish units for this legacy reply.
- The October 2026 AMD AOD WMI patch series describes the status word at offset 2,
  with zero indicating success, but targets a different GUID. It is corroborating
  protocol evidence, not proof of support on this legacy firmware.
- Linux's per-device WMI API:
  <https://github.com/torvalds/linux/blob/master/include/linux/wmi.h> and
  <https://github.com/torvalds/linux/blob/master/drivers/platform/wmi/core.c>.

Tests execute the same collection/dispatch and decoding helpers as the module
with simulated ACPI objects and a recording transport. They cover malformed
types/counts, absent/ambiguous getters, unknown versions, short replies, firmware
errors, packed 16-bit decoding, the default no-request path, fixed request bytes,
and no retry after errors. They do not test firmware execution, SMM behavior,
units, actual sensor provenance, or accuracy. No second hardware platform has
been validated yet.
