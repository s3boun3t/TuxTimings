#!/usr/bin/env python3
"""Capture fixed Vermeer UMC registers through ryzen_smu's read protocol."""

import os
import struct
import sys
from pathlib import Path


BASE = Path("/sys/kernel/ryzen_smu_drv")
CHANNELS = (0, 0x100000)
REGISTERS = (
    (0x50000, "CS0"), (0x50004, "CS1"),
    (0x50008, "CS2"), (0x5000C, "CS3"),
    (0x50100, "DRAM config"), (0x5012C, "Refresh mode"),
    (0x50200, "Ratio/flags"), (0x50204, "Primary timings"),
    (0x50230, "REFI"), (0x50260, "RFC0"), (0x50264, "RFC1"),
    (0x50DF0, "UMC status"),
)
ADDRESSES = frozenset(c | r for c in CHANNELS for r, _ in REGISTERS)
DEFAULT_RFC = 0x21060138


def check_driver():
    version = (BASE / "drv_version").read_text().strip()
    codename = (BASE / "codename").read_text().strip()
    if version != "0.1.7" or codename != "12":
        raise RuntimeError(
            f"Unsupported diagnostic target: driver={version}, codename={codename}; "
            "expected ryzen_smu 0.1.7 and Vermeer (12)"
        )
    return version, codename


def read_register(address):
    if address not in ADDRESSES:
        raise ValueError("Address is outside the fixed diagnostic allowlist")
    # drv.c smn_store(): exactly ONE u32 requests a read. TWO u32s write
    # the target register. Never combine requests or retry a short write.
    payload = struct.pack("<I", address)
    fd = os.open(BASE / "smn", os.O_RDWR | os.O_CLOEXEC)
    try:
        if os.write(fd, payload) != 4:
            raise OSError(f"Short address submission for 0x{address:08X}")
        os.lseek(fd, 0, os.SEEK_SET)
        value = os.read(fd, 4)
        if len(value) != 4:
            raise OSError(f"Short reply for 0x{address:08X}")
        return struct.unpack("<I", value)[0]
    finally:
        os.close(fd)


def rfc_fields(value):
    return value & 0x7FF, (value >> 11) & 0x7FF, (value >> 22) & 0x3FF


def heuristic_selection(rfc0, rfc1):
    # Reproduce the existing TuxTimings/ZenStates rule, without claiming
    # that this proves the active DIMM or rank.
    return 1 if rfc0 != rfc1 and rfc0 == DEFAULT_RFC else 0


def capture(reader):
    return [
        {c | r: reader(c | r) for c in CHANNELS for r, _ in REGISTERS}
        for _ in range(3)
    ]


def report(samples, emit=print):
    stable = True
    emit("Values are raw snapshots; successful syscalls do not prove hardware validity.")
    emit("ryzen_smu caches one global result: close all other SMN-reading programs.")
    emit("RFC decoding: bits [10:0], [21:11], [31:22]; units: memory clocks.")
    for channel, offset in enumerate(CHANNELS):
        emit(f"\nUMC{channel} offset=0x{offset:06X} (not a physical slot label)")
        channel_stable = True
        for reg, name in REGISTERS:
            values = [s[offset | reg] for s in samples]
            same = values.count(values[0]) == len(values)
            channel_stable &= same
            emit(f"0x{offset | reg:08X} {name:16} "
                 + " ".join(f"0x{v:08X}" for v in values)
                 + ("" if same else " CHANGED"))
        stable &= channel_stable
        if not channel_stable:
            emit("Channel changed between passes; do not infer a refresh selection.")
            continue
        s = samples[0]
        for index, reg in enumerate((0x50260, 0x50264)):
            raw = s[offset | reg]
            rfc, rfc2, rfc4 = rfc_fields(raw)
            suffix = " (known default pattern)" if raw == DEFAULT_RFC else ""
            emit(f"RFC{index}: tRFC={rfc}, tRFC2={rfc2}, tRFC4={rfc4}{suffix}")
        selected = heuristic_selection(s[offset | 0x50260], s[offset | 0x50264])
        emit(f"Existing software heuristic would select RFC{selected}; active rank not proven.")
        emit(f"FGR raw={(s[offset | 0x5012C] >> 16) & 7}")
    return stable


def main():
    try:
        version, codename = check_driver()
        print(f"DDR4 refresh diagnostic: driver={version}, codename={codename}", flush=True)
        stable = report(capture(read_register))
        if not stable:
            print("Unstable snapshot: retain raw output for diagnosis.", file=sys.stderr)
            return 1
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"Diagnostic stopped: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
