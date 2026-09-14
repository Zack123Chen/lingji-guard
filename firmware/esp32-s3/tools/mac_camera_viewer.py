#!/usr/bin/env python3
import argparse
import io
import sys
import time
import tkinter as tk
from dataclasses import dataclass

import numpy as np
import serial
from PIL import Image, ImageTk


@dataclass
class Frame:
    width: int
    height: int
    pixel_format: int
    data: bytes


def read_until_frame_begin(
    port: serial.Serial,
    timeout_s: float,
    begin_marker: str,
    error_marker: str,
) -> tuple[int, int, int, int]:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        line = port.readline().decode("ascii", errors="ignore").strip()
        if not line:
            continue
        if line.startswith(error_marker):
            raise RuntimeError(line)
        if line.startswith(begin_marker):
            parts = line.split()
            if len(parts) != 5:
                raise RuntimeError(f"Bad frame header: {line}")
            return int(parts[1]), int(parts[2]), int(parts[3]), int(parts[4])
    raise TimeoutError(f"Timed out waiting for {begin_marker}")


def read_serial_frame(
    port: serial.Serial,
    timeout_s: float,
    command: bytes,
    begin_marker: str,
    error_marker: str,
    end_marker: str,
) -> Frame:
    port.write(command)
    port.flush()

    width, height, length, pixel_format = read_until_frame_begin(
        port,
        timeout_s,
        begin_marker,
        error_marker,
    )
    data = port.read(length)
    if len(data) != length:
        raise TimeoutError(f"Short frame: got {len(data)} of {length} bytes")

    # Consume the newline plus CAMFRAME_END line. Keep this lenient because binary
    # transport over a log-oriented serial stream may leave a leading newline.
    _ = port.readline()
    end = port.readline().decode("ascii", errors="ignore").strip()
    if end != end_marker:
      # Do not fail the frame after successfully reading the exact payload.
      print(f"warning: missing {end_marker}, got {end!r}", file=sys.stderr)

    return Frame(width=width, height=height, pixel_format=pixel_format, data=data)


def read_raw_frame(port: serial.Serial, timeout_s: float) -> Frame:
    frame = read_serial_frame(
        port,
        timeout_s,
        b"camdump\n",
        "CAMFRAME_BEGIN",
        "CAMFRAME_ERROR",
        "CAMFRAME_END",
    )
    if frame.pixel_format != 0:
        raise RuntimeError(f"Expected RGB565 format=0, got {frame.pixel_format}")
    return frame


def read_jpeg_frame(port: serial.Serial, timeout_s: float) -> Frame:
    return read_serial_frame(
        port,
        timeout_s,
        b"camjpeg\n",
        "CAMJPEG_BEGIN",
        "CAMJPEG_ERROR",
        "CAMJPEG_END",
    )


def rgb565_to_image(frame: Frame, scale: int, byte_order: str) -> Image.Image:
    raw = np.frombuffer(frame.data, dtype=np.uint8)
    if raw.size != frame.width * frame.height * 2:
        raise ValueError("RGB565 payload size does not match frame dimensions")

    pixels = raw.reshape((frame.height, frame.width, 2))
    if byte_order == "little":
        value = pixels[:, :, 0].astype(np.uint16) | (pixels[:, :, 1].astype(np.uint16) << 8)
    elif byte_order == "big":
        value = (pixels[:, :, 0].astype(np.uint16) << 8) | pixels[:, :, 1].astype(np.uint16)
    else:
        raise ValueError(f"Unsupported byte order: {byte_order}")

    red = ((value >> 11) & 0x1F).astype(np.uint8)
    green = ((value >> 5) & 0x3F).astype(np.uint8)
    blue = (value & 0x1F).astype(np.uint8)

    rgb = np.dstack((
        (red << 3) | (red >> 2),
        (green << 2) | (green >> 4),
        (blue << 3) | (blue >> 2),
    ))
    image = Image.fromarray(rgb, "RGB")
    if scale > 1:
        image = image.resize((frame.width * scale, frame.height * scale), Image.Resampling.NEAREST)
    return image


def jpeg_to_image(frame: Frame, scale: int) -> Image.Image:
    image = Image.open(io.BytesIO(frame.data)).convert("RGB")
    if scale > 1:
        image = image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
    return image


class CameraViewer:
    def __init__(
        self,
        port_name: str,
        baud: int,
        scale: int,
        interval_ms: int,
        timeout_s: float,
        mode: str,
        byte_order: str,
    ):
        self.serial = serial.Serial(port_name, baudrate=baud, timeout=1.0)
        self.scale = scale
        self.interval_ms = interval_ms
        self.timeout_s = timeout_s
        self.mode = mode
        self.byte_order = byte_order
        self.root = tk.Tk()
        self.root.title("CareGuard Camera Preview")
        self.label = tk.Label(self.root, bg="black")
        self.label.pack()
        self.status = tk.StringVar(value="Starting camera preview...")
        tk.Label(self.root, textvariable=self.status).pack(fill="x")
        self.photo = None
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        time.sleep(1.5)
        self.serial.reset_input_buffer()

    def close(self):
        try:
            self.serial.close()
        finally:
            self.root.destroy()

    def update(self):
        try:
            if self.mode == "jpeg":
                frame = read_jpeg_frame(self.serial, self.timeout_s)
                image = jpeg_to_image(frame, self.scale)
                format_label = "JPEG"
            else:
                frame = read_raw_frame(self.serial, self.timeout_s)
                image = rgb565_to_image(frame, self.scale, self.byte_order)
                format_label = f"RGB565/{self.byte_order}"
            self.photo = ImageTk.PhotoImage(image)
            self.label.configure(image=self.photo)
            self.status.set(
                f"{frame.width}x{frame.height} {format_label} len={len(frame.data)}  {time.strftime('%H:%M:%S')}"
            )
        except Exception as exc:
            self.status.set(f"Frame error: {exc}")
            print(f"Frame error: {exc}", file=sys.stderr)
        self.root.after(self.interval_ms, self.update)

    def run(self):
        self.root.after(200, self.update)
        self.root.mainloop()


def main() -> int:
    parser = argparse.ArgumentParser(description="Display ESP32-S3 OV2640 frames on macOS via USB serial.")
    parser.add_argument("--port", default="/dev/cu.usbmodem21401")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--scale", type=int, default=4)
    parser.add_argument("--interval-ms", type=int, default=100)
    parser.add_argument("--timeout-s", type=float, default=25.0)
    parser.add_argument("--mode", choices=("jpeg", "raw"), default="jpeg")
    parser.add_argument("--byte-order", choices=("big", "little"), default="big")
    args = parser.parse_args()

    viewer = CameraViewer(
        args.port,
        args.baud,
        args.scale,
        args.interval_ms,
        args.timeout_s,
        args.mode,
        args.byte_order,
    )
    viewer.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
