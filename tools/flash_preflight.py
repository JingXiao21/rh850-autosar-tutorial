"""离线 Flash 烧录前静态检查。只读取输入并写审计报告，绝不连接/擦写 ECU。

PASS_STATIC 只说明输入与给定公司策略一致，不能认证策略真实性或硬件可恢复性。
仅支持严格 Intel HEX + ELF32 little-endian ET_EXEC，未知格式一律不放行。
"""
from __future__ import annotations

import argparse
from bisect import bisect_right
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import uuid

VERSION = "1.0.0"
LIMIT = 1 << 32
MAX_FILE = 64 * 1024 * 1024
MAX_PAYLOAD = 8 * 1024 * 1024


class Rejected(ValueError):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def require(ok, code, message):
    if not ok:
        raise Rejected(code, message)


def number(value, where, maximum=LIMIT - 1):
    # bool 在 Python 中是 int 的子类，必须显式排除，避免 true 被当作地址 1。
    if type(value) is int:
        result = value
    elif isinstance(value, str) and re.fullmatch(r"0[xX][0-9a-fA-F]+", value):
        result = int(value, 16)
    else:
        raise Rejected("SCHEMA", f"{where}: 必须为整数或 0x 十六进制字符串")
    require(0 <= result <= maximum, "RANGE", f"{where}: 数值越界")
    return result


def fields(obj, required, where):
    require(isinstance(obj, dict), "SCHEMA", f"{where}: 必须为 object")
    require(set(obj) == set(required), "SCHEMA",
            f"{where}: missing={sorted(set(required)-set(obj))}, unknown={sorted(set(obj)-set(required))}")


def label(value, where):
    require(isinstance(value, str) and bool(value.strip()), "SCHEMA", f"{where}: 不能为空")
    require(not any(x in value.upper() for x in ["REQUIRED", "TODO", "TBD", "REPLACE_ME"]),
            "PLACEHOLDER", f"{where}: 仍是占位值")
    return value


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def sha(value, where):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value) is not None,
            "SCHEMA", f"{where}: SHA-256 必须是 64 位十六进制")
    return value.lower()


def read_file(path):
    require(path.is_file(), "FILE_MISSING", f"文件不存在: {path}")
    require(path.stat().st_size <= MAX_FILE, "SIZE_LIMIT", f"文件超过 {MAX_FILE} 字节限制: {path}")
    raw = path.read_bytes()
    require(len(raw) <= MAX_FILE, "SIZE_LIMIT", f"文件读取期间超过大小限制: {path}")
    return raw


def read_json(raw):
    def pairs(items):
        out = {}
        for key, value in items:
            require(key not in out, "JSON_DUPLICATE_KEY", f"JSON 重复 key: {key}")
            out[key] = value
        return out
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=pairs,
                          parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
    except (UnicodeError, ValueError, RecursionError) as error:
        if isinstance(error, Rejected):
            raise
        raise Rejected("JSON_FORMAT", f"无效 JSON: {error}") from error


# 内部区间统一使用 [start, end)；JSON 输入则使用两端包含的 start/end。
def span(obj, where):
    fields(obj, ["start", "end"], where)
    start, end = number(obj["start"], where), number(obj["end"], where)
    require(start <= end, "RANGE", f"{where}: start > end")
    return start, end + 1


def union(ranges):
    out = []
    for start, end in sorted(ranges):
        if out and start <= out[-1][1]:
            out[-1] = (out[-1][0], max(end, out[-1][1]))
        else:
            out.append((start, end))
    return out


def spans(value, where, empty=False):
    require(isinstance(value, list) and (empty or len(value) > 0), "SCHEMA", f"{where}: 区间列表为空或类型错误")
    out = sorted(span(item, f"{where}[{i}]") for i, item in enumerate(value))
    require(all(a[1] <= b[0] for a, b in zip(out, out[1:])), "RANGE_OVERLAP", f"{where}: 区间重叠")
    return out


def covered(start, end, ranges):
    return any(lo <= start and end <= hi for lo, hi in union(ranges))


def intersects(a, b):
    return a[0] < b[1] and b[0] < a[1]


def show_ranges(ranges):
    return [{"start": f"0x{lo:08X}", "end": f"0x{hi-1:08X}", "bytes": hi-lo}
            for lo, hi in ranges]


@dataclass
class Image:
    chunks: list[tuple[int, bytes]]
    entry: int | None = None

    @property
    def ranges(self):
        return [(start, start + len(data)) for start, data in self.chunks]

    @property
    def size(self):
        return sum(len(data) for _, data in self.chunks)


def image_from_chunks(chunks, entry=None):
    require(bool(chunks), "IMAGE_EMPTY", "镜像没有数据")
    require(sum(len(data) for _, data in chunks) <= MAX_PAYLOAD, "SIZE_LIMIT", "镜像有效负载超过 8 MiB")
    merged = []
    for start, data in sorted(chunks, key=lambda item: item[0]):
        require(start >= 0 and start + len(data) <= LIMIT, "ADDRESS_WRAP", "镜像地址超过 32 位空间")
        if merged:
            previous_end = merged[-1][0] + len(merged[-1][1])
            require(start >= previous_end, "IMAGE_OVERLAP", "镜像包含重叠/重复地址，即使字节相同也拒绝")
            if start == previous_end:
                merged[-1][1].extend(data)
                continue
        merged.append((start, bytearray(data)))
    return Image([(start, bytes(data)) for start, data in merged], entry)


def parse_hex(raw):
    try:
        text = raw.decode("ascii")
    except UnicodeError as error:
        raise Rejected("HEX_ASCII", "HEX 必须为 ASCII，不接受 BOM/二进制文件") from error
    base, entry, eof, total = 0, None, False, 0
    chunks = []
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        where = f"HEX line {line_number}"
        require(not eof, "HEX_AFTER_EOF", f"{where}: EOF 后还有记录")
        require(len(line) <= 521 and re.fullmatch(r":[0-9a-fA-F]+", line) is not None,
                "HEX_SYNTAX", f"{where}: 非法字符、空格或长度")
        require(len(line) % 2 == 1, "HEX_LENGTH", f"{where}: 半字节记录")
        record = bytes.fromhex(line[1:])
        require(len(record) >= 5 and len(record) == record[0] + 5, "HEX_LENGTH", f"{where}: 字节数不匹配")
        require(sum(record) % 256 == 0, "HEX_CHECKSUM", f"{where}: 校验和错误")
        count, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        data = record[4:-1]
        if kind == 0:
            require(count > 0, "HEX_EMPTY_RECORD", f"{where}: 不支持空 data 记录")
            require(address + count <= 0x10000, "HEX_WINDOW_CROSS", f"{where}: 单条记录跨越 64KiB 窗口，拒绝隐式回绕")
            require(base + address + count <= LIMIT, "ADDRESS_WRAP", f"{where}: 32 位地址回绕")
            total += count
            require(total <= MAX_PAYLOAD, "SIZE_LIMIT", "HEX 有效负载超过 8 MiB")
            chunks.append((base + address, data))
        elif kind == 1:
            require(count == 0 and address == 0, "HEX_EOF", f"{where}: 非法 EOF")
            eof = True
        elif kind in (2, 4):
            require(count == 2 and address == 0, "HEX_BASE", f"{where}: 非法扩展地址记录")
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5):
            require(count == 4 and address == 0 and entry is None, "HEX_ENTRY", f"{where}: 非法或重复入口记录")
            entry = ((int.from_bytes(data[:2], "big") << 4) + int.from_bytes(data[2:], "big")
                     if kind == 3 else int.from_bytes(data, "big"))
        else:
            raise Rejected("HEX_RECORD_TYPE", f"{where}: 未支持的 record type {kind}")
    require(eof, "HEX_NO_EOF", "HEX 缺少 EOF")
    return image_from_chunks(chunks, entry)


def evidence(item, root, report, role):
    fields(item, ["path", "sha256"], f"evidence.{role}")
    label(item["path"], f"{role}.path")
    path = (root / item["path"]).resolve()
    raw = read_file(path)
    actual = digest(raw)
    require(actual == sha(item["sha256"], role), "EVIDENCE_HASH", f"{role}: 文件哈希不匹配: {path}")
    require(len(raw) > 0, "EVIDENCE_EMPTY", f"{role}: 证据文件为空")
    report["evidence"].append({"role": role, "path": str(path), "sha256": actual, "bytes": len(raw)})
    return actual


def validate_device(obj):
    fields(obj, ["part_number", "silicon_revision", "board_id"], "device")
    for key, value in obj.items():
        label(value, f"device.{key}")


def validate_tool(obj):
    fields(obj, ["name", "version", "cpu_selection", "algorithm"], "tool")
    for key, value in obj.items():
        label(value, f"tool.{key}")


def validate_profile(p, root, report):
    fields(p, ["schema_version", "profile_status", "profile_id", "device", "tool", "layout_evidence",
               "algorithm_sha256", "flash_banks", "ram_ranges", "allowed_program_ranges",
               "allowed_erase_ranges", "allowed_blank_ranges", "protected_ranges", "entry_ranges",
               "required_image_ranges", "elf_machine", "elf_flags", "hex_padding"], "profile")
    require(type(p["schema_version"]) is int and p["schema_version"] == 1, "SCHEMA", "profile schema_version 不支持")
    require(p["profile_status"] == "company_verified", "PROFILE_UNVERIFIED", "模板/示例 profile 不可用于放行；需要公司真实布局及核查证据")
    label(p["profile_id"], "profile_id")
    validate_device(p["device"])
    validate_tool(p["tool"])
    evidence(p["layout_evidence"], root, report, "company_layout")
    sha(p["algorithm_sha256"], "algorithm_sha256")
    number(p["elf_machine"], "elf_machine", 65535)
    number(p["elf_flags"], "elf_flags")
    require(p["hex_padding"] in ("none", "ff_to_program_unit"), "SCHEMA", "hex_padding 只能是 none / ff_to_program_unit")
    require(isinstance(p["flash_banks"], list) and p["flash_banks"], "SCHEMA", "flash_banks 不能为空")
    banks, names = [], set()
    for bank in p["flash_banks"]:
        fields(bank, ["name", "start", "end", "erase_block_size", "program_unit"], "flash_bank")
        name = label(bank["name"], "bank.name")
        require(name not in names, "SCHEMA", "重复 bank name")
        names.add(name)
        lo, hi = span({k: bank[k] for k in ("start", "end")}, name)
        block, unit = number(bank["erase_block_size"], "erase_block_size"), number(bank["program_unit"], "program_unit")
        require(0 < unit <= block and block % unit == 0 and (hi-lo) % block == 0,
                "GEOMETRY", f"{name}: bank/erase block/program unit 几何关系错误")
        banks.append({"name": name, "lo": lo, "hi": hi, "block": block, "unit": unit})
    banks.sort(key=lambda b: b["lo"])
    require(all(a["hi"] <= b["lo"] for a, b in zip(banks, banks[1:])), "GEOMETRY", "Flash bank 重叠")
    ranges = {}
    for key in ["ram_ranges", "allowed_program_ranges", "allowed_erase_ranges", "allowed_blank_ranges",
                "protected_ranges", "entry_ranges", "required_image_ranges"]:
        ranges[key] = spans(p[key], key, empty=key == "allowed_blank_ranges")
    flash = [(b["lo"], b["hi"]) for b in banks]
    for key in ["allowed_program_ranges", "allowed_erase_ranges", "allowed_blank_ranges", "entry_ranges", "required_image_ranges"]:
        for lo, hi in ranges[key]:
            require(covered(lo, hi, flash), "PROFILE_RANGE", f"{key}: 不在已声明 Flash 内")
            require(not any(intersects((lo, hi), guard) for guard in ranges["protected_ranges"]),
                    "PROFILE_PROTECTED", f"{key}: 策略本身与保护区重叠")
    for lo, hi in ranges["allowed_blank_ranges"]:
        require(covered(lo, hi, ranges["allowed_erase_ranges"]), "PROFILE_RANGE", "允许留空区必须位于允许擦除区")
    require(not any(intersects(a, b) for a in flash for b in ranges["ram_ranges"]), "PROFILE_RANGE", "RAM 与 Flash 重叠")
    return banks, ranges


def bank_at(banks, address):
    return next((bank for bank in banks if bank["lo"] <= address < bank["hi"]), None)


def check_write_ranges(image, banks, ranges):
    for lo, hi in image.ranges:
        require(covered(lo, hi, ranges["allowed_program_ranges"]), "WRITE_OUTSIDE_ALLOWLIST",
                f"HEX 写入不在白名单内: 0x{lo:08X}..0x{hi-1:08X}")
        require(not any(intersects((lo, hi), guard) for guard in ranges["protected_ranges"]),
                "WRITE_PROTECTED", "HEX 与保护区相交")
        position = lo
        while position < hi:
            bank = bank_at(banks, position)
            require(bank is not None, "WRITE_NONFLASH", "写入未知存储空间")
            end = min(hi, bank["hi"])
            require((position-bank["lo"]) % bank["unit"] == 0 and (end-bank["lo"]) % bank["unit"] == 0,
                    "PROGRAM_UNIT", "HEX 没覆盖完整编程单元；不假定烧录工具如何补齐")
            position = end
    for lo, hi in ranges["required_image_ranges"]:
        require(covered(lo, hi, image.ranges), "REQUIRED_BYTES_MISSING", "入口/向量等必需镜像区不完整")


def parse_elf(raw, profile, banks, ranges):
    require(len(raw) >= 52 and raw[:4] == b"\x7fELF", "ELF_HEADER", "不是有效 ELF 文件")
    require(raw[4:7] == b"\x01\x01\x01", "ELF_FORMAT", "只支持 ELF32 / little-endian / v1")
    header = struct.unpack_from("<HHIIIIIHHHHHH", raw, 16)
    kind, machine, version, entry, phoff, _, flags, ehsize, phsize, phnum, _, _, _ = header
    require(kind == 2 and version == 1 and ehsize == 52, "ELF_FORMAT", "只支持 ELF32 ET_EXEC")
    require(machine == number(profile["elf_machine"], "elf_machine", 65535) and
            flags == number(profile["elf_flags"], "elf_flags"), "ELF_TARGET", "ELF machine/flags 与公司 profile 不匹配")
    require(0 < phnum <= 1024 and phsize == 32 and phoff >= 52 and phoff + phnum*phsize <= len(raw),
            "ELF_PROGRAM_HEADERS", "ELF program header 表错误/截断/不支持")
    chunks, segments, vmas, executable_entry, total = [], [], [], False, 0
    flash = [(b["lo"], b["hi"]) for b in banks]
    for index in range(phnum):
        typ, offset, vaddr, paddr, filesz, memsz, permissions, align = struct.unpack_from("<IIIIIIII", raw, phoff + index*32)
        if typ != 1:  # 只取 PT_LOAD；调试 section 不是要烧录的数据。
            continue
        require(filesz <= memsz and offset + filesz <= len(raw) and vaddr + memsz <= LIMIT and paddr + filesz <= LIMIT,
                "ELF_SEGMENT", f"PT_LOAD[{index}]: 大小/偏移/地址错误")
        require(align in (0, 1) or (align & (align-1) == 0 and vaddr % align == offset % align),
                "ELF_ALIGNMENT", f"PT_LOAD[{index}]: 对齐错误")
        if memsz:
            require(covered(vaddr, vaddr+memsz, flash) or covered(vaddr, vaddr+memsz, ranges["ram_ranges"]),
                    "ELF_VMA", f"PT_LOAD[{index}]: VMA/RAM 分配超出器件范围")
            require(not any(intersects((vaddr, vaddr+memsz), previous) for previous in vmas),
                    "ELF_VMA_OVERLAP", f"PT_LOAD[{index}]: 运行地址重叠；不支持 overlay/共享映射")
            vmas.append((vaddr, vaddr+memsz))
            require(memsz == filesz or covered(vaddr, vaddr+memsz, ranges["ram_ranges"]),
                    "ELF_ZERO_FILL", "文件数据之外的零初始化区必须在 RAM，不能猜测 Flash 的隐含零填充")
            segments.append({"vaddr": f"0x{vaddr:08X}", "paddr": f"0x{paddr:08X}", "filesz": filesz, "memsz": memsz})
        if not filesz:
            continue  # BSS 没有文件数据，不能把 ELF 尾部扩展区当成 Flash 内容。
        require(covered(paddr, paddr+filesz, flash), "ELF_LMA",
                f"PT_LOAD[{index}]: 文件数据 p_paddr 不在 Flash；不猜测供应商的 LMA 规则")
        total += filesz
        require(total <= MAX_PAYLOAD, "SIZE_LIMIT", "ELF load 数据超过 8 MiB")
        chunks.append((paddr, raw[offset:offset+filesz]))
        if permissions & 1 and vaddr <= entry < vaddr+filesz and vaddr == paddr:
            executable_entry = True
    require(executable_entry and covered(entry, entry+1, ranges["entry_ranges"]), "ELF_ENTRY",
            "ELF 入口不在允许范围或不是已加载的 Flash 可执行字节")
    image = image_from_chunks(chunks, entry)
    return image, segments


def padded_elf(image, banks):
    # 只允许补齐包含 ELF 数据的最小编程单元，不允许填充任意大地址空洞。
    areas = []
    for lo, hi in image.ranges:
        while lo < hi:
            bank = bank_at(banks, lo)
            require(bank is not None, "ELF_LMA", "ELF 地址不在 Flash bank")
            end = min(hi, bank["hi"])
            start_unit = bank["lo"] + ((lo-bank["lo"]) // bank["unit"]) * bank["unit"]
            end_unit = bank["lo"] + ((end-bank["lo"]+bank["unit"]-1) // bank["unit"]) * bank["unit"]
            areas.append((start_unit, end_unit))
            lo = end
    areas = union(areas)
    require(sum(hi-lo for lo, hi in areas) <= MAX_PAYLOAD, "SIZE_LIMIT", "编程单元补齐后的镜像超过限制")
    starts = [lo for lo, _ in areas]
    buffers = [bytearray(b"\xFF") * (hi-lo) for lo, hi in areas]
    for start, data in image.chunks:
        index = bisect_right(starts, start)-1
        offset = start - starts[index]
        buffers[index][offset:offset+len(data)] = data
    return Image([(lo, bytes(data)) for (lo, _), data in zip(areas, buffers)], image.entry)


REVIEW_KEYS = ["all_script_dependencies_listed", "no_configuration_security_otp_writes",
               "erase_plan_includes_implicit_erase", "no_application_run_after_programming",
               "recovery_path_verified", "elf_paddr_is_flash_lma_verified", "map_linker_layout_reviewed",
               "bootloader_integrity_requirements_verified"]
REQUIRED_ROLES = {"device_readback", "linker", "map", "flash_script", "flash_algorithm",
                  "tool_settings", "recovery", "review"}


def validate_plan(plan, profile, raw_hashes, root, report, image, banks, ranges):
    fields(plan, ["schema_version", "device", "tool", "image_sha256", "elf_sha256", "profile_sha256",
                  "operations", "erase_mode", "erase_ranges", "program_ranges", "blank_after_erase_ranges",
                  "run_after_programming", "review", "artifacts"], "plan")
    require(type(plan["schema_version"]) is int and plan["schema_version"] == 1, "SCHEMA", "plan schema_version 不支持")
    validate_device(plan["device"])
    validate_tool(plan["tool"])
    require(plan["device"] == profile["device"], "DEVICE_MISMATCH", "plan 与 profile 的完整器件/修订/板卡不一致")
    require(plan["tool"] == profile["tool"], "TOOL_MISMATCH", "烧录工具、版本、CPU 选择或算法不匹配")
    for name in ["image", "elf", "profile"]:
        require(sha(plan[f"{name}_sha256"], name) == raw_hashes[name], "INPUT_HASH", f"{name}: 计划没有绑定本次实际文件")
    require(plan["operations"] == ["erase", "program", "verify"], "OPERATIONS",
            "只允许 erase/program/verify，不支持额外配置、安全、OTP 或自定义操作")
    require(plan["erase_mode"] == "explicit_sectors", "ERASE_MODE", "必须展开实际扇区计划，不接受 mass/auto/unknown")
    require(plan["run_after_programming"] is False, "AUTO_RUN", "本检查配置不允许下载后自动运行应用")
    fields(plan["review"], REVIEW_KEYS, "review")
    require(all(plan["review"][key] is True for key in REVIEW_KEYS), "REVIEW_INCOMPLETE", "公司 agent 核查项尚未全部确认")
    report["agent_attestations"] = plan["review"]
    require(isinstance(plan["artifacts"], list) and plan["artifacts"], "SCHEMA", "artifacts 不能为空")
    roles = set()
    for item in plan["artifacts"]:
        fields(item, ["role", "path", "sha256"], "artifact")
        role = label(item["role"], "artifact.role")
        require(role in REQUIRED_ROLES or role == "script_dependency", "SCHEMA", f"未知 artifact role: {role}")
        require(role not in roles or role == "script_dependency", "SCHEMA", f"重复 artifact role: {role}")
        actual = evidence({k: item[k] for k in ["path", "sha256"]}, root, report, role)
        if role == "flash_algorithm":
            require(actual == sha(profile["algorithm_sha256"], "algorithm_sha256"), "ALGORITHM_HASH", "算法文件与 profile 固定版本不同")
        roles.add(role)
    require(REQUIRED_ROLES <= roles, "EVIDENCE_MISSING", f"缺少证据: {sorted(REQUIRED_ROLES-roles)}")
    erase = spans(plan["erase_ranges"], "erase_ranges")
    program = spans(plan["program_ranges"], "program_ranges")
    blank = spans(plan["blank_after_erase_ranges"], "blank_after_erase_ranges", empty=True)
    require(union(program) == image.ranges, "PROGRAM_PLAN", "声明的实际写入范围与 HEX 数据范围不同")
    for lo, hi in erase:
        bank = bank_at(banks, lo)
        require(bank is not None and hi <= bank["hi"] and (lo-bank["lo"]) % bank["block"] == 0 and
                (hi-bank["lo"]) % bank["block"] == 0, "ERASE_ALIGNMENT", "擦除范围须按真实 bank 扇区对齐，跨 bank 请拆开声明")
        require(covered(lo, hi, ranges["allowed_erase_ranges"]), "ERASE_OUTSIDE_ALLOWLIST", "擦除范围越过公司白名单")
        require(not any(intersects((lo, hi), guard) for guard in ranges["protected_ranges"]), "ERASE_PROTECTED", "擦除涉及保护区域")
    for lo, hi in program:
        require(covered(lo, hi, erase), "PROGRAM_NOT_ERASED", "写入范围未被声明擦除覆盖；不猜测旧 Flash 的 0/1 状态")
    for lo, hi in blank:
        require(covered(lo, hi, erase) and covered(lo, hi, ranges["allowed_blank_ranges"]),
                "BLANK_NOT_ALLOWED", "擦除后留空范围未经 profile 允许或不在擦除区")
        require(not any(intersects((lo, hi), written) for written in program), "BLANK_OVERLAP", "留空范围与写入数据重叠")
    require(union(program + blank) == union(erase), "ERASE_COLLATERAL",
            "擦除扇区有未被 HEX 恢复、也未明确允许留空的字节；可能破坏邻接数据")
    report["erase_ranges"] = show_ranges(erase)
    report["blank_after_erase_ranges"] = show_ranges(blank)


LIMITATIONS = [
    "PASS_STATIC 不是烧录授权或 ECU 安全认证；公司 profile、读回记录、人工/agent 核查内容是受信输入。",
    "哈希证明文件匹配，不证明文件内容正确。工具不连接芯片、不读取保护状态、不验证认证密钥。",
    "不解释或执行 CMM/脚本；脚本动态分支、外部依赖和隐式擦除须由公司 agent 展开并核实。",
    "ELF 检查仅支持 ELF32 LE ET_EXEC，使用经核实的 PT_LOAD.p_paddr 作 Flash LMA；MAP/linker 只校验哈希，其语义由 agent 核查。",
    "不分析机器码的 GPIO/供电/自编程行为，不证明启动、实时性、板级安全或恢复能力；报告不允许下载后自动运行。",
    "不验证公司 bootloader 的镜像 CRC/签名/安全启动/版本回退规则；这些要求由公司工具和 agent 核实。",
    "所有结论只绑定本次输入哈希；文件、工具、连接设置、板卡或保护状态变化后必须重新核查。",
]


def audit(hex_path, profile_path=None, plan_path=None, elf_path=None):
    report = {"schema_version": 1, "harness_version": VERSION,
              "harness_sha256": digest(Path(__file__).read_bytes()),
              "utc": datetime.now(timezone.utc).isoformat(), "status": "BLOCKED",
              "inputs": {}, "evidence": [], "findings": [], "machine_checks": [],
              "agent_attestations": {}, "limitations": LIMITATIONS}
    paths = {"image": hex_path, "profile": profile_path, "plan": plan_path, "elf": elf_path}
    raw = {}
    try:
        for role, path in paths.items():
            if path is None:
                report["findings"].append({"code": "MISSING_INPUT", "message": f"缺少 {role}，不能放行"})
                continue
            path = Path(path).resolve()
            raw[role] = read_file(path)
            report["inputs"][role] = {"path": str(path), "sha256": digest(raw[role]), "bytes": len(raw[role])}
        require("image" in raw, "HEX_MISSING", "必须提供 HEX")
        image = parse_hex(raw["image"])
        report["image_ranges"] = show_ranges(image.ranges)
        report["image_bytes"] = image.size
        report["hex_entry"] = None if image.entry is None else f"0x{image.entry:08X}"
        report["machine_checks"].append("HEX: ASCII / record length / checksum / EOF / addresses / non-overlap")
        if len(raw) != 4:
            return report
        profile, plan = read_json(raw["profile"]), read_json(raw["plan"])
        banks, ranges = validate_profile(profile, Path(profile_path).resolve().parent, report)
        report["device"] = profile["device"]
        report["profile_id"] = profile["profile_id"]
        check_write_ranges(image, banks, ranges)
        report["machine_checks"].append("HEX: write allowlist / protected regions / program-unit coverage / required bytes")
        elf_image, segments = parse_elf(raw["elf"], profile, banks, ranges)
        expected = padded_elf(elf_image, banks) if profile["hex_padding"] == "ff_to_program_unit" else elf_image
        require(image.chunks == expected.chunks, "HEX_ELF_MISMATCH", "HEX 与 ELF Flash load 字节不完全一致（含补齐规则）")
        require(image.entry is None or image.entry == elf_image.entry, "ENTRY_MISMATCH", "HEX start address 与 ELF 入口不一致")
        report["elf_entry"] = f"0x{elf_image.entry:08X}"
        report["elf_segments"] = segments
        report["machine_checks"].append("ELF: class / machine / flags / VMA / Flash LMA / executable entry / exact HEX bytes")
        hashes = {name: digest(raw[name]) for name in ["image", "profile", "elf"]}
        validate_plan(plan, profile, hashes, Path(plan_path).resolve().parent, report, image, banks, ranges)
        report["machine_checks"].append("Plan: device/tool/hash binding / evidence hashes / sectors / protected ranges / erase collateral / no auto-run declaration")
        report["status"] = "PASS_STATIC"
    except Rejected as error:
        report["status"] = "BLOCKED" if error.code in {"PROFILE_UNVERIFIED", "PLACEHOLDER", "EVIDENCE_MISSING", "REVIEW_INCOMPLETE", "FILE_MISSING"} else "FAIL"
        report["findings"].append({"code": error.code, "message": str(error)})
    except OSError as error:
        report["status"] = "ERROR"
        report["findings"].append({"code": "IO_ERROR", "message": str(error)})
    return report


def markdown(report):
    lines = ["# Flash 离线检查报告", "", f"**结果：{report['status']}**", "",
             "PASS_STATIC 仅表示静态一致性检查通过，不等于可以安全烧录/运行。", "",
             f"UTC: {report['utc']}；harness: {report['harness_version']}",
             f"Harness SHA-256: `{report['harness_sha256']}`", "", "## 输入指纹", "",
             "| 角色 | 文件 | SHA-256 |", "|---|---|---|"]
    for role, item in report["inputs"].items():
        path = item['path'].replace('|', '\\|').replace('\n', ' ')
        lines.append(f"| {role} | {path} | `{item['sha256']}` |")
    lines += ["", "## 检查结果", ""]
    lines += [f"- `{item['code']}`：{item['message']}" for item in report["findings"]] or ["- 无静态阻断项。"]
    lines += ["", "## 程序实际检查", ""] + [f"- {item}" for item in report["machine_checks"]]
    lines += ["", "## 公司 agent 声明（程序未证明其真实性）", ""]
    lines += [f"- {key}: {value}" for key, value in report["agent_attestations"].items()] or ["- 未完成。"]
    lines += ["", "## 限制", ""] + [f"- {item}" for item in report["limitations"]]
    lines += ["", "区间、ELF 段和证据文件指纹见同目录 report.json。", ""]
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hex", required=True, type=Path, dest="hex_path")
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--out-dir", type=Path, default=Path("artifacts/flash-preflight"))
    args = parser.parse_args(argv)
    report = audit(args.hex_path, args.profile, args.plan, args.elf)
    # 每次新建目录，不覆盖历史 PASS 报告，也不修改任何 HEX/ELF/证据输入。
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    output = args.out_dir.resolve() / run_id
    try:
        output.mkdir(parents=True, exist_ok=False)
        (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        (output / "report.md").write_text(markdown(report), encoding="utf-8")
    except OSError as error:
        print(f"ERROR: 无法保存报告: {error}", file=sys.stderr)
        return 3
    print(f"{report['status']}: {output / 'report.md'}")
    for item in report["findings"]:
        print(f"  {item['code']}: {item['message']}")
    return {"PASS_STATIC": 0, "FAIL": 1, "BLOCKED": 2, "ERROR": 3}[report["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
