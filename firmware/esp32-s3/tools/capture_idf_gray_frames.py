#!/usr/bin/env python3
import argparse
import base64
import binascii
import re
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

import serial


BEGIN_RE = re.compile(
    r"FRAME_BASE64_BEGIN index=(\d+) width=(\d+) height=(\d+) len=(\d+) crc32=0x([0-9A-Fa-f]+)"
)
END_RE = re.compile(r"FRAME_BASE64_END index=(\d+)")


def png_chunk(kind, payload):
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)


def write_gray_png(path, width, height, pixels):
    raw_rows = bytearray()
    stride = width
    for y in range(height):
        raw_rows.append(0)
        start = y * stride
        raw_rows.extend(pixels[start:start + stride])

    data = bytearray(b"\x89PNG\r\n\x1a\n")
    data.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)))
    data.extend(png_chunk(b"IDAT", zlib.compress(bytes(raw_rows), 9)))
    data.extend(png_chunk(b"IEND", b""))
    path.write_bytes(data)


def parse_stream(port, baud, timeout_s, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    frames = []
    current = None
    start = time.time()

    with serial.Serial(port, baud, timeout=0.2) as ser:
        while time.time() - start < timeout_s and len(frames) < 3:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", "replace").strip()
            print(line)

            begin = BEGIN_RE.search(line)
            if begin:
                current = {
                    "index": int(begin.group(1)),
                    "width": int(begin.group(2)),
                    "height": int(begin.group(3)),
                    "length": int(begin.group(4)),
                    "device_crc": int(begin.group(5), 16),
                    "b64": [],
                }
                continue

            end = END_RE.search(line)
            if end and current is not None:
                end_index = int(end.group(1))
                if end_index != current["index"]:
                    raise RuntimeError(f"Base64 end index mismatch: got {end_index}, expected {current['index']}")
                payload = base64.b64decode("".join(current["b64"]), validate=True)
                host_crc = binascii.crc32(payload) & 0xFFFFFFFF
                if len(payload) != current["length"]:
                    raise RuntimeError(f"Frame {current['index']} length mismatch: got {len(payload)}, expected {current['length']}")
                if host_crc != current["device_crc"]:
                    raise RuntimeError(
                        f"Frame {current['index']} CRC mismatch: host=0x{host_crc:08X}, device=0x{current['device_crc']:08X}"
                    )

                stem = f"idf_gray_frame_{current['index']:02d}"
                pgm_path = out_dir / f"{stem}.pgm"
                png_path = out_dir / f"{stem}.png"
                pgm_path.write_bytes(
                    f"P5\n{current['width']} {current['height']}\n255\n".encode("ascii") + payload
                )
                write_gray_png(png_path, current["width"], current["height"], payload)
                frames.append({**current, "host_crc": host_crc, "pgm": pgm_path, "png": png_path})
                print(
                    f"[MAC] frame={current['index']} crc_match=YES crc32=0x{host_crc:08X} "
                    f"pgm={pgm_path} png={png_path}"
                )
                current = None
                continue

            if current is not None and line and not line.startswith("["):
                current["b64"].append(line)

    if len(frames) != 3:
        raise RuntimeError(f"Expected 3 frames, captured {len(frames)}")
    return frames


def reset_target(port, baud):
    with serial.Serial(port, baud, timeout=0.1) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.2)
        ser.rts = False
        time.sleep(0.2)


def verify_pngs(frames):
    for frame in frames:
        png = frame["png"]
        result = subprocess.run(
            ["sips", "-g", "pixelWidth", "-g", "pixelHeight", str(png)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=True,
        )
        print(result.stdout.strip())
        if f"pixelWidth: {frame['width']}" not in result.stdout:
            raise RuntimeError(f"{png} width verification failed")
        if f"pixelHeight: {frame['height']}" not in result.stdout:
            raise RuntimeError(f"{png} height verification failed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem21401")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=45.0)
    parser.add_argument("--out-dir", default="captures/idf_gray_stack4096")
    parser.add_argument("--reset", action="store_true", help="toggle RTS before capture")
    args = parser.parse_args()

    if args.reset:
        reset_target(args.port, args.baud)
    frames = parse_stream(args.port, args.baud, args.timeout, Path(args.out_dir).resolve())
    verify_pngs(frames)
    print("[MAC] captured_frames=3 png_verify=OK")
    for frame in frames:
        print(
            f"[MAC] summary frame={frame['index']} width={frame['width']} height={frame['height']} "
            f"len={frame['length']} crc32=0x{frame['host_crc']:08X} png={frame['png']}"
        )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[MAC] ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
