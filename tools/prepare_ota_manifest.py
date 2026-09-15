#!/usr/bin/env python3
import argparse
import json
import shutil
import sys
import zlib
from pathlib import Path


DEFAULT_OWNER_REPO = "kenhkythuat/wbee_mobifone"
DEFAULT_BRANCH = "feature/ota_mobifone"
DEFAULT_APP_ADDR = 0x08008000
DEFAULT_MAX_SIZE = 224 * 1024


def parse_int(value):
    return int(value, 0)


def read_hex_data(path, app_addr, max_size):
    upper_addr = 0
    total_size = 0
    crc = 0
    min_addr = None
    max_addr = None

    with path.open("r", encoding="ascii") as hex_file:
        for line_no, raw_line in enumerate(hex_file, 1):
            line = raw_line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError(f"Line {line_no}: invalid Intel HEX record")

            try:
                byte_count = int(line[1:3], 16)
                address = int(line[3:7], 16)
                record_type = int(line[7:9], 16)
                data = bytes.fromhex(line[9:9 + byte_count * 2])
                checksum = int(line[9 + byte_count * 2:11 + byte_count * 2], 16)
            except ValueError as exc:
                raise ValueError(f"Line {line_no}: malformed Intel HEX record") from exc

            checksum_sum = byte_count + (address >> 8) + (address & 0xFF) + record_type
            checksum_sum += sum(data) + checksum
            if (checksum_sum & 0xFF) != 0:
                raise ValueError(f"Line {line_no}: checksum mismatch")

            if record_type == 0x00:
                absolute_addr = upper_addr + address
                record_end = absolute_addr + byte_count
                app_end = app_addr + max_size

                if absolute_addr < app_addr or record_end > app_end:
                    raise ValueError(
                        f"Line {line_no}: data address 0x{absolute_addr:08X}..0x{record_end - 1:08X} "
                        f"is outside OTA app range 0x{app_addr:08X}..0x{app_end - 1:08X}"
                    )

                crc = zlib.crc32(data, crc)
                total_size += byte_count
                min_addr = absolute_addr if min_addr is None else min(min_addr, absolute_addr)
                max_addr = record_end if max_addr is None else max(max_addr, record_end)

            elif record_type == 0x01:
                break
            elif record_type == 0x04:
                if byte_count != 2:
                    raise ValueError(f"Line {line_no}: invalid extended linear address record")
                upper_addr = int.from_bytes(data, "big") << 16
            elif record_type in (0x02, 0x03, 0x05):
                continue
            else:
                raise ValueError(f"Line {line_no}: unsupported record type {record_type:02X}")

    if total_size == 0:
        raise ValueError("No firmware data found in OTA app range")

    return {
        "size": total_size,
        "crc32": crc & 0xFFFFFFFF,
        "min_addr": min_addr,
        "max_addr": max_addr,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Copy OTA Intel HEX and update ota/manifest.json with size/crc32."
    )
    parser.add_argument("--hex", required=True, type=Path, help="Built OTA .hex file")
    parser.add_argument("--version", required=True, help="Firmware version, for example 2.2")
    parser.add_argument("--branch", default=DEFAULT_BRANCH, help="GitHub branch name")
    parser.add_argument("--repo", default=DEFAULT_OWNER_REPO, help="GitHub owner/repo")
    parser.add_argument("--app-addr", default=DEFAULT_APP_ADDR, type=parse_int)
    parser.add_argument("--max-size", default=DEFAULT_MAX_SIZE, type=parse_int)
    parser.add_argument("--ota-dir", default=Path("ota"), type=Path)
    args = parser.parse_args()

    hex_path = args.hex
    if not hex_path.exists():
        print(f"HEX file not found: {hex_path}", file=sys.stderr)
        return 1

    try:
        info = read_hex_data(hex_path, args.app_addr, args.max_size)
    except ValueError as exc:
        print(f"OTA HEX invalid: {exc}", file=sys.stderr)
        return 1

    args.ota_dir.mkdir(parents=True, exist_ok=True)
    output_name = f"wbee_v{args.version}.hex"
    output_path = args.ota_dir / output_name
    shutil.copyfile(hex_path, output_path)

    manifest = {
        "version": args.version,
        "app_addr": f"0x{args.app_addr:08X}",
        "size": info["size"],
        "crc32": f"0x{info['crc32']:08X}",
        "hex_url": (
            f"https://raw.githubusercontent.com/{args.repo}/"
            f"{args.branch}/ota/{output_name}"
        ),
    }

    manifest_path = args.ota_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")

    print(f"Copied: {hex_path} -> {output_path}")
    print(f"Updated: {manifest_path}")
    print(f"Address range: 0x{info['min_addr']:08X}..0x{info['max_addr'] - 1:08X}")
    print(f"Size: {info['size']}")
    print(f"CRC32: 0x{info['crc32']:08X}")
    print(f"URL: {manifest['hex_url']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
