#!/usr/bin/env python3
"""Small JPEG marker dumper for camera diagnostics."""

from __future__ import annotations

import argparse
from pathlib import Path


MARKER_NAMES = {
    0xC0: "SOF0",
    0xC1: "SOF1",
    0xC2: "SOF2",
    0xC3: "SOF3",
    0xC4: "DHT",
    0xC5: "SOF5",
    0xC6: "SOF6",
    0xC7: "SOF7",
    0xC8: "JPG",
    0xC9: "SOF9",
    0xCA: "SOF10",
    0xCB: "SOF11",
    0xCC: "DAC",
    0xCD: "SOF13",
    0xCE: "SOF14",
    0xCF: "SOF15",
    0xD8: "SOI",
    0xD9: "EOI",
    0xDA: "SOS",
    0xDB: "DQT",
    0xDC: "DNL",
    0xDD: "DRI",
    0xDE: "DHP",
    0xDF: "EXP",
    0xE0: "APP0",
    0xE1: "APP1",
    0xE2: "APP2",
    0xE3: "APP3",
    0xE4: "APP4",
    0xE5: "APP5",
    0xE6: "APP6",
    0xE7: "APP7",
    0xE8: "APP8",
    0xE9: "APP9",
    0xEA: "APP10",
    0xEB: "APP11",
    0xEC: "APP12",
    0xED: "APP13",
    0xEE: "APP14",
    0xEF: "APP15",
    0xFE: "COM",
}

NO_LENGTH_MARKERS = set(range(0xD0, 0xD8)) | {0x01, 0xD8, 0xD9}


def marker_name(marker: int) -> str:
    return MARKER_NAMES.get(marker, f"MARKER_{marker:02X}")


def next_marker(data: bytes, start: int) -> tuple[int, int] | None:
    pos = start
    while pos < len(data) - 1:
      if data[pos] == 0xFF:
          while pos < len(data) and data[pos] == 0xFF:
              pos += 1
          if pos >= len(data):
              return None
          marker = data[pos]
          if marker == 0x00:
              pos += 1
              continue
          return pos - 1, marker
      pos += 1
    return None


def parse(data: bytes) -> tuple[list[dict], list[str]]:
    markers: list[dict] = []
    warnings: list[str] = []

    found = next_marker(data, 0)
    if found is None:
        return markers, ["no JPEG markers found"]

    offset, marker = found
    pos = offset
    while True:
        if pos >= len(data):
            break
        if data[pos] != 0xFF:
            found = next_marker(data, pos)
            if found is None:
                break
            pos, marker = found
        else:
            found = next_marker(data, pos)
            if found is None:
                break
            pos, marker = found

        entry: dict = {
            "offset": pos,
            "marker": marker,
            "name": marker_name(marker),
            "length": None,
            "sof_width": None,
            "sof_height": None,
        }

        pos = pos + 2
        if marker in NO_LENGTH_MARKERS:
            markers.append(entry)
            if marker == 0xD9:
                break
            continue

        if pos + 2 > len(data):
            entry["error"] = "missing length bytes"
            markers.append(entry)
            break

        seg_len = int.from_bytes(data[pos : pos + 2], "big")
        entry["length"] = seg_len
        if seg_len < 2:
            entry["error"] = "invalid segment length"
            markers.append(entry)
            break

        payload_start = pos + 2
        payload_end = pos + seg_len
        if payload_end > len(data):
            entry["error"] = "segment exceeds file"
            markers.append(entry)
            break

        if 0xC0 <= marker <= 0xCF and marker not in {0xC4, 0xC8, 0xCC}:
            payload = data[payload_start:payload_end]
            if len(payload) >= 6:
                entry["sof_height"] = int.from_bytes(payload[1:3], "big")
                entry["sof_width"] = int.from_bytes(payload[3:5], "big")
            else:
                entry["error"] = "short SOF payload"

        markers.append(entry)
        pos = payload_end

        if marker == 0xDA:
            found = next_marker(data, pos)
            if found is None:
                warnings.append("SOS found but no following marker")
                break
            pos, marker = found
            continue

    return markers, warnings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--expect-width", type=int, default=160)
    parser.add_argument("--expect-height", type=int, default=120)
    args = parser.parse_args()

    exit_code = 0
    for path in args.files:
        data = path.read_bytes()
        markers, warnings = parse(data)
        names = [entry["name"] for entry in markers]
        sof_entries = [entry for entry in markers if entry["name"] in {"SOF0", "SOF2"}]
        has = {name: name in names for name in ("SOI", "DQT", "DHT", "SOS", "EOI")}
        has["SOF0_OR_SOF2"] = bool(sof_entries)
        missing = [name for name, ok in has.items() if not ok]
        print(f"file={path}")
        print(f"size={len(data)}")
        print("markers:")
        for entry in markers:
            extra = ""
            if entry["length"] is not None:
                extra += f" length={entry['length']}"
            if entry["sof_width"] is not None:
                extra += f" sof={entry['sof_width']}x{entry['sof_height']}"
            if "error" in entry:
                extra += f" error={entry['error']}"
            print(f"  0x{entry['offset']:06X} FF{entry['marker']:02X} {entry['name']}{extra}")
        for warning in warnings:
            print(f"warning={warning}")
        if sof_entries:
            first_sof = sof_entries[0]
            expected = (
                first_sof["sof_width"] == args.expect_width
                and first_sof["sof_height"] == args.expect_height
            )
            print(
                "sof_match="
                f"{'YES' if expected else 'NO'} "
                f"expected={args.expect_width}x{args.expect_height}"
            )
            if not expected:
                exit_code = 2
        else:
            print(f"sof_match=NO expected={args.expect_width}x{args.expect_height}")
            exit_code = 2
        print(f"missing={','.join(missing) if missing else 'none'}")
        if missing:
            exit_code = 2
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
