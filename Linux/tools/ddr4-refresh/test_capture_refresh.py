"""Offline tests: no sysfs file or hardware is opened."""

import struct
import unittest
from unittest.mock import patch

import capture_refresh as tool


class RefreshDiagnosticTests(unittest.TestCase):
    def test_bios_triplet_matches_known_default(self):
        self.assertEqual(tool.rfc_fields(0x21060138), (312, 192, 132))
        self.assertEqual(tool.rfc_fields(0xFFFFFFFF), (2047, 2047, 1023))

    def test_selection_matches_existing_software_without_relabelling_ranks(self):
        default = tool.DEFAULT_RFC
        self.assertEqual(tool.heuristic_selection(default, default), 0)
        self.assertEqual(tool.heuristic_selection(default, 0x12345678), 1)
        self.assertEqual(tool.heuristic_selection(0x12345678, default), 0)
        self.assertEqual(tool.heuristic_selection(0, default), 0)

    def test_only_fixed_addresses_and_three_passes(self):
        addresses = []

        def reader(address):
            addresses.append(address)
            return address

        samples = tool.capture(reader)
        self.assertEqual(len(samples), 3)
        self.assertEqual(len(addresses), 72)
        self.assertEqual(set(addresses), tool.ADDRESSES)
        self.assertEqual(samples[0], samples[1])

    def test_one_word_submission_only(self):
        with patch.object(tool.os, "open", return_value=42) as opened, \
             patch.object(tool.os, "write", return_value=4) as written, \
             patch.object(tool.os, "lseek") as seek, \
             patch.object(tool.os, "read", return_value=b"\x38\x01\x06\x21") as read, \
             patch.object(tool.os, "close") as close:
            self.assertEqual(tool.read_register(0x150264), tool.DEFAULT_RFC)
            opened.assert_called_once_with(tool.BASE / "smn", tool.os.O_RDWR | tool.os.O_CLOEXEC)
            written.assert_called_once_with(42, struct.pack("<I", 0x150264))
            seek.assert_called_once_with(42, 0, tool.os.SEEK_SET)
            read.assert_called_once_with(42, 4)
            close.assert_called_once_with(42)

    def test_unlisted_address_never_opens_sysfs(self):
        with patch.object(tool.os, "open") as opened:
            with self.assertRaises(ValueError):
                tool.read_register(0xDEADBEEF)
            opened.assert_not_called()

    def test_short_write_is_not_retried_and_reply_not_read(self):
        with patch.object(tool.os, "open", return_value=42), \
             patch.object(tool.os, "write", return_value=2) as written, \
             patch.object(tool.os, "read") as read, \
             patch.object(tool.os, "close") as close:
            with self.assertRaises(OSError):
                tool.read_register(0x50260)
            self.assertEqual(written.call_count, 1)
            read.assert_not_called()
            close.assert_called_once_with(42)

    def test_short_read_closes_file(self):
        with patch.object(tool.os, "open", return_value=42), \
             patch.object(tool.os, "write", return_value=4), \
             patch.object(tool.os, "lseek"), \
             patch.object(tool.os, "read", return_value=b"\x00"), \
             patch.object(tool.os, "close") as close:
            with self.assertRaises(OSError):
                tool.read_register(0x50260)
            close.assert_called_once_with(42)

    def test_stable_channels_are_reported_separately(self):
        values = {a: 0 for a in tool.ADDRESSES}
        values[0x50260] = tool.DEFAULT_RFC
        values[0x50264] = 630 | (468 << 11) | (288 << 22)
        values[0x150260] = tool.DEFAULT_RFC
        values[0x150264] = tool.DEFAULT_RFC
        lines = []
        self.assertTrue(tool.report([values.copy() for _ in range(3)], lines.append))
        text = "\n".join(lines)
        self.assertIn("tRFC=312, tRFC2=192, tRFC4=132", text)
        self.assertIn("tRFC=630, tRFC2=468, tRFC4=288", text)
        self.assertIn("UMC0", text)
        self.assertIn("UMC1", text)

    def test_changed_channel_is_not_decoded_as_stable(self):
        samples = [{a: 0 for a in tool.ADDRESSES} for _ in range(3)]
        samples[1][0x50260] = tool.DEFAULT_RFC
        lines = []
        self.assertFalse(tool.report(samples, lines.append))
        self.assertIn("CHANGED", "\n".join(lines))
        self.assertEqual(sum("heuristic would select" in line for line in lines), 1)

    def test_driver_guard_precedes_any_hardware_capture(self):
        with patch.object(tool.Path, "read_text", side_effect=["0.1.7\n", "12\n"]):
            self.assertEqual(tool.check_driver(), ("0.1.7", "12"))
        for version, codename in (("0.1.8", "12"), ("0.1.7", "20")):
            with patch.object(tool.Path, "read_text", side_effect=[version, codename]):
                with self.assertRaises(RuntimeError):
                    tool.check_driver()


if __name__ == "__main__":
    unittest.main()
