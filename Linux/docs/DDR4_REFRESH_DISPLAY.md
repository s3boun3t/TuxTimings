# DDR4 refresh display

The DDR4 UI shows the programmed tRFC, tRFC2, tRFC4 and configured Fine
Granularity Refresh (FGR) code. Codes 5 and 6 allow on-the-fly switching;
they do not reveal the instantaneous refresh mode. Codes 3, 4 and 7 remain
reserved and are displayed with their raw code. A failed FGR read is shown
as unavailable, not as normal mode.

Refresh availability checks cover userspace open, exact four-byte address
submission, flush, seek, exact four-byte reply and close. RFC availability
requires both register reads and a nonzero register selected by the existing
RFC0/RFC1 rule. Nanoseconds additionally require a nonzero DDR4 frequency
ratio; cycles remain visible if only the ratio is unavailable.

These checks report I/O success, not guaranteed hardware validity or
freshness. The ryzen_smu 0.1.7 driver keeps a shared result and can report a
successful syscall even when an underlying hardware read fails. This change
does not solve cross-process concurrency or change the driver protocol.
Only a four-byte address is submitted; no target-register value is written.

The decoder still reads UMC0 and uses the existing RFC selection heuristic.
Selecting a DIMM in the UI does not select a different timing register.
The recorded Vermeer case remains 630 / 468 / 288 cycles and
350 / 260 / 160 ns at DDR4-3600. No BIOS timing correction is inferred.

Memory-specific rows follow the decoder path: tRFC4/FGR for DDR4 and tRFCsb
for DDR5. This route describes the decoder used, not independent physical
memory detection or certification for LPDDR variants.
