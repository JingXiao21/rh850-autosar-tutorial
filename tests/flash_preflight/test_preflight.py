"""仅在临时目录创建合成器件/HEX/ELF；这些数据绝不能用于 ECU。

覆盖格式、地址回绕、保护区、整扇区附带擦除、ELF 映射和证据绑定。
"""
import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

from tools import flash_preflight as h


def record(kind, address=0, data=b""):
    body = bytes([len(data), address >> 8, address & 255, kind]) + data
    return ":" + (body + bytes([-sum(body) & 255])).hex().upper()


def ihex(chunks, entry=None):
    lines = []
    for start, data in chunks:
        for offset in range(0, len(data), 16):
            address = start + offset
            lines.append(record(4, data=(address >> 16).to_bytes(2, "big")))
            lines.append(record(0, address & 0xFFFF, data[offset:offset+16]))
    if entry is not None:
        lines.append(record(5, data=entry.to_bytes(4, "big")))
    return ("\n".join(lines + [record(1)]) + "\n").encode("ascii")


def elf(segments, entry=0x2000, machine=87, flags=0):
    # segments: (VMA, LMA, bytes, memory_size, permission)
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    hdr = ident + struct.pack("<HHIIIIIHHHHHH", 2, machine, 1, entry, 52, 0, flags, 52, 32, len(segments), 0, 0, 0)
    table, payload, offset = bytearray(), bytearray(), 0x400
    for vma, lma, data, memory, permissions in segments:
        table.extend(struct.pack("<IIIIIIII", 1, offset, vma, lma, len(data), memory, permissions, 1))
        payload.extend(data)
        offset += len(data)
    return hdr + table + bytes(0x400-len(hdr)-len(table)) + payload


def rng(lo, hi):
    return {"start": hex(lo), "end": hex(hi)}


class Fixture:
    def __init__(self, root):
        self.root = root
        self.data = bytes(range(64))
        self.hex = root / "synthetic.hex"
        self.elf = root / "synthetic.elf"
        self.profile_path = root / "profile.json"
        self.plan_path = root / "plan.json"
        self.hex.write_bytes(ihex([(0x2000, self.data)], 0x2000))
        self.elf.write_bytes(elf([(0x2000, 0x2000, self.data, 64, 5)]))
        artifacts = []
        for role in sorted(h.REQUIRED_ROLES):
            path = root / f"{role}.txt"
            path.write_text(f"SYNTHETIC SELFTEST, NO ECU. Evidence role: {role}\n", encoding="utf-8")
            artifacts.append({"role": role, "path": path.name, "sha256": h.digest(path.read_bytes())})
        (root / "layout.txt").write_text("Synthetic geometry, only for unit tests.", encoding="utf-8")
        self.profile = {
            "schema_version": 1, "profile_status": "company_verified",
            "profile_id": "SYNTHETIC_SELFTEST_NOT_FOR_HARDWARE",
            "device": {"part_number": "FAKE_NOT_A_REAL_MCU", "silicon_revision": "SIM", "board_id": "NO_BOARD"},
            "tool": {"name": "SIMULATED_TOOL_DO_NOT_FLASH", "version": "TEST", "cpu_selection": "FAKE", "algorithm": "NONE_REAL"},
            "layout_evidence": {"path": "layout.txt", "sha256": h.digest((root / "layout.txt").read_bytes())},
            "algorithm_sha256": next(a["sha256"] for a in artifacts if a["role"] == "flash_algorithm"),
            "flash_banks": [{"name": "synthetic_flash", **rng(0, 0x3FFF), "erase_block_size": 256, "program_unit": 16}],
            "ram_ranges": [rng(0xFE000000, 0xFE000FFF)],
            "allowed_program_ranges": [rng(0x2000, 0x2FFF)],
            "allowed_erase_ranges": [rng(0x2000, 0x2FFF)],
            "allowed_blank_ranges": [rng(0x2000, 0x2FFF)],
            "protected_ranges": [rng(0, 0x1FFF), rng(0xFFF00000, 0xFFFFFFFF)],
            "entry_ranges": [rng(0x2000, 0x200F)],
            "required_image_ranges": [rng(0x2000, 0x200F)],
            "elf_machine": 87, "elf_flags": 0, "hex_padding": "none",
        }
        self.plan = {
            "schema_version": 1, "device": copy.deepcopy(self.profile["device"]),
            "tool": copy.deepcopy(self.profile["tool"]), "image_sha256": "", "elf_sha256": "", "profile_sha256": "",
            "operations": ["erase", "program", "verify"], "erase_mode": "explicit_sectors",
            "erase_ranges": [rng(0x2000, 0x20FF)], "program_ranges": [rng(0x2000, 0x203F)],
            "blank_after_erase_ranges": [rng(0x2040, 0x20FF)], "run_after_programming": False,
            "review": dict.fromkeys(h.REVIEW_KEYS, True), "artifacts": artifacts,
        }
        self.save()

    def save(self, rebind=True):
        self.profile_path.write_text(json.dumps(self.profile), encoding="utf-8")
        if rebind:
            for role, path in [("image", self.hex), ("elf", self.elf), ("profile", self.profile_path)]:
                self.plan[role + "_sha256"] = h.digest(path.read_bytes())
        self.plan_path.write_text(json.dumps(self.plan), encoding="utf-8")

    def audit(self):
        return h.audit(self.hex, self.profile_path, self.plan_path, self.elf)


class HexTests(unittest.TestCase):
    def bad(self, raw, code):
        with self.assertRaises(h.Rejected) as caught:
            h.parse_hex(raw)
        self.assertEqual(caught.exception.code, code)

    def test_known_literal(self):
        image = h.parse_hex(b":0400000001020304F2\n:00000001FF\n")
        self.assertEqual(image.chunks, [(0, bytes([1, 2, 3, 4]))])

    def test_extended_linear(self):
        image = h.parse_hex(ihex([(0xFF200000, b"abcd")]))
        self.assertEqual(image.ranges, [(0xFF200000, 0xFF200004)])

    def test_segment_then_linear_base_replaces_previous(self):
        raw = "\n".join([record(2, data=b"\x12\x34"), record(0, 2, b"a"),
                         record(4, data=b"\x00\x02"), record(0, 4, b"b"), record(1)]).encode()
        self.assertEqual(h.parse_hex(raw).chunks, [(0x12342, b"a"), (0x20004, b"b")])

    def test_start_segment(self):
        raw = "\n".join([record(0, 0x10, b"x"), record(3, data=b"\x01\x00\x00\x20"), record(1)]).encode()
        self.assertEqual(h.parse_hex(raw).entry, 0x1020)

    def test_bad_checksum(self):
        self.bad(b":0400000001020304F3\n:00000001FF\n", "HEX_CHECKSUM")

    def test_truncated_length(self):
        self.bad(b":040000000102F9\n", "HEX_LENGTH")

    def test_missing_eof(self):
        self.bad(record(0, 0, b"a").encode(), "HEX_NO_EOF")

    def test_after_eof(self):
        self.bad(ihex([(0, b"a")]) + record(0, 2, b"b").encode(), "HEX_AFTER_EOF")

    def test_duplicate_eof(self):
        self.bad(ihex([(0, b"a")]) + record(1).encode(), "HEX_AFTER_EOF")

    def test_identical_overlap_rejected(self):
        self.bad(ihex([(1, b"ab"), (1, b"ab")]), "IMAGE_OVERLAP")

    def test_conflicting_overlap_rejected(self):
        self.bad(ihex([(1, b"abc"), (2, b"xy")]), "IMAGE_OVERLAP")

    def test_window_crossing(self):
        raw = (record(0, 0xFFFF, b"ab") + "\n" + record(1)).encode()
        self.bad(raw, "HEX_WINDOW_CROSS")

    def test_last_32bit_address(self):
        self.assertEqual(h.parse_hex(ihex([(0xFFFFFFFF, b"x")])).ranges, [(0xFFFFFFFF, 1 << 32)])

    def test_unknown_record(self):
        self.bad((record(6) + "\n" + record(1)).encode(), "HEX_RECORD_TYPE")

    def test_malformed_extension(self):
        self.bad((record(4, 1, b"\x00\x01") + "\n").encode(), "HEX_BASE")

    def test_multiple_entries(self):
        self.bad(ihex([(0, b"a")], 0)[:-12] + (record(5, data=bytes(4))+"\n"+record(1)).encode(), "HEX_ENTRY")

    def test_empty_image(self):
        self.bad(b":00000001FF\n", "IMAGE_EMPTY")

    def test_bom_or_binary(self):
        self.bad(b"\xef\xbb\xbf:00000001FF", "HEX_ASCII")

    def test_embedded_space(self):
        self.bad(b":04 00000001020304F2\n", "HEX_SYNTAX")


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.f = Fixture(Path(self.temp.name))

    def expect(self, code, status="FAIL", save=True):
        if save:
            self.f.save()
        report = self.f.audit()
        self.assertEqual(report["status"], status, report)
        self.assertEqual(report["findings"][-1]["code"], code, report)

    def test_complete_static_pass(self):
        report = self.f.audit()
        self.assertEqual(report["status"], "PASS_STATIC", report)
        self.assertEqual(report["image_bytes"], 64)
        self.assertEqual(len(report["evidence"]), 9)
        self.assertTrue(report["limitations"])

    def test_hex_only_never_passes(self):
        report = h.audit(self.f.hex)
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["image_bytes"], 64)

    def test_template_blocked(self):
        self.f.profile["profile_status"] = "template"
        self.expect("PROFILE_UNVERIFIED", "BLOCKED")

    def test_unknown_schema_key(self):
        self.f.profile["allow_anything"] = True
        self.expect("SCHEMA")

    def test_bool_not_address(self):
        self.f.profile["allowed_program_ranges"][0]["start"] = True
        self.expect("SCHEMA")

    def test_negative_address(self):
        self.f.profile["flash_banks"][0]["start"] = -1
        self.expect("RANGE")

    def test_reversed_range(self):
        self.f.profile["ram_ranges"] = [rng(20, 10)]
        self.expect("RANGE")

    def test_profile_overlap_with_protected(self):
        self.f.profile["allowed_program_ranges"] = [rng(0, 0x2FFF)]
        self.expect("PROFILE_PROTECTED")

    def test_program_outside(self):
        self.f.hex.write_bytes(ihex([(0x3000, self.f.data)]))
        self.expect("WRITE_OUTSIDE_ALLOWLIST")

    def test_program_bootloader(self):
        self.f.hex.write_bytes(ihex([(0x100, self.f.data)]))
        self.expect("WRITE_OUTSIDE_ALLOWLIST")

    def test_program_security_address(self):
        self.f.hex.write_bytes(ihex([(0xFFF00000, self.f.data)]))
        self.expect("WRITE_OUTSIDE_ALLOWLIST")

    def test_partial_program_unit(self):
        self.f.hex.write_bytes(ihex([(0x2000, self.f.data[:17])]))
        self.expect("PROGRAM_UNIT")

    def test_required_bytes_missing(self):
        self.f.hex.write_bytes(ihex([(0x2010, self.f.data)]))
        self.expect("REQUIRED_BYTES_MISSING")

    def test_elf_mismatch_single_byte(self):
        data = bytes([255]) + self.f.data[1:]
        self.f.hex.write_bytes(ihex([(0x2000, data)]))
        self.expect("HEX_ELF_MISMATCH")

    def test_elf_machine_mismatch(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 5)], machine=40))
        self.expect("ELF_TARGET")

    def test_elf_flags_mismatch(self):
        self.f.profile["elf_flags"] = 1
        self.expect("ELF_TARGET")

    def test_wrong_ram_layout(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 5),
                                   (0xFD000000, 0, b"", 128, 6)]))
        self.expect("ELF_VMA")

    def test_valid_bss_is_not_programmed(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 5),
                                   (0xFE000000, 0, b"", 128, 6)]))
        self.f.save()
        self.assertEqual(self.f.audit()["status"], "PASS_STATIC")

    def test_ram_initialized_data_flash_lma(self):
        data = self.f.data
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, data[:32], 32, 5),
                                   (0xFE000000, 0x2020, data[32:], 64, 6)]))
        self.f.save()
        self.assertEqual(self.f.audit()["status"], "PASS_STATIC")

    def test_ram_lma_not_guessed(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 5),
                                   (0xFE000000, 0xFE000000, b"1234", 4, 6)]))
        self.expect("ELF_LMA")

    def test_overlapping_ram_segments_rejected(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 5),
                                   (0xFE000000, 0, b"", 128, 6),
                                   (0xFE000040, 0, b"", 128, 6)]))
        self.expect("ELF_VMA_OVERLAP")

    def test_zero_fill_flash_not_assumed(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 128, 5)]))
        self.expect("ELF_ZERO_FILL")

    def test_elf_file_size_exceeds_memory_size(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 32, 5)]))
        self.expect("ELF_SEGMENT")

    def test_elf_truncated(self):
        self.f.elf.write_bytes(self.f.elf.read_bytes()[:-1])
        self.expect("ELF_SEGMENT")

    def test_elf_wrong_class(self):
        raw = bytearray(self.f.elf.read_bytes()); raw[4] = 2
        self.f.elf.write_bytes(raw)
        self.expect("ELF_FORMAT")

    def test_elf_entry_not_executable(self):
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, self.f.data, 64, 4)]))
        self.expect("ELF_ENTRY")

    def test_hex_entry_differs(self):
        self.f.hex.write_bytes(ihex([(0x2000, self.f.data)], 0x2004))
        self.expect("ENTRY_MISMATCH")

    def test_elf_approved_ff_padding(self):
        data = self.f.data[:33]
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, data, len(data), 5)]))
        self.f.hex.write_bytes(ihex([(0x2000, data+b"\xFF"*15)]))
        self.f.profile["hex_padding"] = "ff_to_program_unit"
        self.f.plan["program_ranges"] = [rng(0x2000, 0x202F)]
        self.f.plan["blank_after_erase_ranges"] = [rng(0x2030, 0x20FF)]
        self.f.save()
        self.assertEqual(self.f.audit()["status"], "PASS_STATIC")

    def test_padding_cannot_fill_arbitrary_gap(self):
        self.f.profile["hex_padding"] = "ff_to_program_unit"
        self.f.hex.write_bytes(ihex([(0x2000, self.f.data+b"\xFF"*16)]))
        self.expect("HEX_ELF_MISMATCH")

    def test_exact_device_mismatch(self):
        self.f.plan["device"]["part_number"] = "ANOTHER_MCU"
        self.expect("DEVICE_MISMATCH")

    def test_board_mismatch(self):
        self.f.plan["device"]["board_id"] = "ANOTHER_BOARD"
        self.expect("DEVICE_MISMATCH")

    def test_algorithm_selection_mismatch(self):
        self.f.plan["tool"]["algorithm"] = "WRONG_ALGORITHM"
        self.expect("TOOL_MISMATCH")

    def test_algorithm_file_changed(self):
        self.f.profile["algorithm_sha256"] = "1" * 64
        self.expect("ALGORITHM_HASH")

    def test_image_hash_stale(self):
        self.f.plan["image_sha256"] = "1" * 64
        self.f.save(rebind=False)
        self.expect("INPUT_HASH", save=False)

    def test_profile_hash_stale(self):
        self.f.profile["profile_id"] = "UPDATED_COMPANY_LAYOUT"
        self.f.save(rebind=False)
        self.expect("INPUT_HASH", save=False)

    def test_script_modified_after_review(self):
        (self.f.root / "flash_script.txt").write_text("Changed script")
        self.expect("EVIDENCE_HASH")

    def test_missing_evidence_role(self):
        self.f.plan["artifacts"] = [a for a in self.f.plan["artifacts"] if a["role"] != "recovery"]
        self.expect("EVIDENCE_MISSING", "BLOCKED")

    def test_missing_evidence_file(self):
        (self.f.root / "recovery.txt").unlink()
        self.expect("FILE_MISSING", "BLOCKED")

    def test_review_not_complete(self):
        self.f.plan["review"]["recovery_path_verified"] = False
        self.expect("REVIEW_INCOMPLETE", "BLOCKED")

    def test_security_operation_rejected(self):
        self.f.plan["operations"].append("set_otp")
        self.expect("OPERATIONS")

    def test_mass_erase_rejected(self):
        self.f.plan["erase_mode"] = "mass"
        self.expect("ERASE_MODE")

    def test_auto_erase_not_silently_accepted(self):
        self.f.plan["erase_mode"] = "auto"
        self.expect("ERASE_MODE")

    def test_auto_run_rejected(self):
        self.f.plan["run_after_programming"] = True
        self.expect("AUTO_RUN")

    def test_missing_program_range(self):
        self.f.plan["program_ranges"] = [rng(0x2000, 0x201F)]
        self.expect("PROGRAM_PLAN")

    def test_erase_not_sector_aligned(self):
        self.f.plan["erase_ranges"] = [rng(0x2000, 0x203F)]
        self.expect("ERASE_ALIGNMENT")

    def test_erase_bootloader_even_hex_is_app_only(self):
        self.f.plan["erase_ranges"] = [rng(0x1000, 0x20FF)]
        self.expect("ERASE_OUTSIDE_ALLOWLIST")

    def test_unexplained_collateral_erasure(self):
        self.f.plan["blank_after_erase_ranges"] = []
        self.expect("ERASE_COLLATERAL")

    def test_blank_not_authorized_by_profile(self):
        self.f.profile["allowed_blank_ranges"] = []
        self.expect("BLANK_NOT_ALLOWED")

    def test_blank_overlaps_program(self):
        self.f.plan["blank_after_erase_ranges"] = [rng(0x2000, 0x20FF)]
        self.expect("BLANK_OVERLAP")

    def test_write_without_erase_rejected(self):
        self.f.plan["erase_ranges"] = [rng(0x2100, 0x21FF)]
        self.expect("PROGRAM_NOT_ERASED")

    def test_bad_geometry(self):
        self.f.profile["flash_banks"][0]["program_unit"] = 24
        self.expect("GEOMETRY")

    def test_nonuniform_sector_geometry(self):
        self.f.profile["flash_banks"] = [
            {"name": "small_sectors", **rng(0, 0x1FFF), "erase_block_size": 256, "program_unit": 16},
            {"name": "large_sectors", **rng(0x2000, 0x3FFF), "erase_block_size": 512, "program_unit": 16}]
        self.f.plan["erase_ranges"] = [rng(0x2000, 0x21FF)]
        self.f.plan["blank_after_erase_ranges"] = [rng(0x2040, 0x21FF)]
        self.f.save()
        self.assertEqual(self.f.audit()["status"], "PASS_STATIC")
        self.f.plan["erase_ranges"] = [rng(0x2000, 0x20FF)]
        self.expect("ERASE_ALIGNMENT")

    def test_subunit_gap_only_ff_padding(self):
        data = b"a"*17 + b"\xFF"*7 + b"b"*8
        self.f.profile["hex_padding"] = "ff_to_program_unit"
        self.f.hex.write_bytes(ihex([(0x2000, data)]))
        self.f.elf.write_bytes(elf([(0x2000, 0x2000, b"a"*17, 17, 5),
                                   (0x2018, 0x2018, b"b"*8, 8, 4)]))
        self.f.plan["program_ranges"] = [rng(0x2000, 0x201F)]
        self.f.plan["blank_after_erase_ranges"] = [rng(0x2020, 0x20FF)]
        self.f.save()
        self.assertEqual(self.f.audit()["status"], "PASS_STATIC")
        self.f.hex.write_bytes(ihex([(0x2000, b"a"*17 + b"\x00"*7 + b"b"*8)]))
        self.expect("HEX_ELF_MISMATCH")

    def test_repeated_json_key(self):
        self.f.profile_path.write_text('{"schema_version":1,"schema_version":2}')
        self.expect("JSON_DUPLICATE_KEY", save=False)

    def test_json_nan_rejected(self):
        self.f.profile_path.write_text('{"elf_flags":NaN}')
        self.expect("JSON_FORMAT", save=False)

    def test_cli_failure_still_writes_fresh_report(self):
        self.f.hex.write_bytes(b":0400000001020304F3\n:00000001FF\n")
        out = self.f.root / "failed_reports"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(h.main(["--hex", str(self.f.hex), "--out-dir", str(out)]), 1)
        report = json.loads(next(out.glob("*/report.json")).read_text(encoding="utf-8"))
        self.assertEqual(report["findings"][-1]["code"], "HEX_CHECKSUM")

    def test_cli_codes_and_separate_reports(self):
        out = self.f.root / "reports"
        args = ["--hex", str(self.f.hex), "--out-dir", str(out)]
        with redirect_stdout(io.StringIO()):
            self.assertEqual(h.main(args), 2)
            self.assertEqual(h.main(args + ["--elf", str(self.f.elf), "--profile", str(self.f.profile_path),
                                            "--plan", str(self.f.plan_path)]), 0)
        reports = list(out.glob("*/report.json"))
        self.assertEqual(len(reports), 2)
        self.assertEqual({json.loads(p.read_text(encoding="utf-8"))["status"] for p in reports},
                         {"BLOCKED", "PASS_STATIC"})
        self.assertEqual(self.f.hex.read_bytes(), ihex([(0x2000, self.f.data)], 0x2000))


if __name__ == "__main__":
    unittest.main()
