#!/usr/bin/env python3

"""
send_stats_i2c.py

Send cyberdeck stats to M5Stack Core1 over I2C (Pi as master, Core as slave).

Same protocol as send_stats.py (key=value;... newline-terminated). Core listens
as I2C slave at 0x42 on SDA=21, SCL=22. Enable I2C on the Pi (raspi-config) and
wire Pi SDA/SCL to Core SDA (G21) / SCL (G22); share GND.

  pip install smbus2
  python send_stats_i2c.py --bus 1
"""

import argparse
import time

from smbus2 import SMBus, i2c_msg  # type: ignore

# Reuse stats collection from serial script
from send_stats import collect_stats  # noqa: I001

# Core I2C slave address (must match firmware)
CORE_I2C_ADDR = 0x42
# Raw I2C payload size per write. Keep under 32 bytes for compatibility.
# We prepend a single 0x00 byte per transaction (firmware ignores first byte).
CHUNK_SIZE = 30


def send_line_i2c(bus: SMBus, line: str) -> None:
    """Send one newline-terminated line to Core using plain I2C writes."""
    payload = (line + "\n").encode("utf-8", errors="ignore")
    offset = 0
    while offset < len(payload):
        chunk = payload[offset : offset + CHUNK_SIZE]
        # Use raw I2C write (not SMBus "block write") for better ESP32 slave compatibility.
        msg = i2c_msg.write(CORE_I2C_ADDR, b"\x00" + bytes(chunk))
        bus.i2c_rdwr(msg)
        offset += len(chunk)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Send system stats to M5Stack Core1 over I2C.",
    )
    parser.add_argument(
        "--bus",
        "-b",
        type=int,
        default=1,
        help="I2C bus number (default: 1 for Pi GPIO 2/3)",
    )
    parser.add_argument(
        "--interval",
        "-i",
        type=float,
        default=1.0,
        help="Update interval in seconds (default: 1.0)",
    )
    args = parser.parse_args()

    print(f"[CoreSerial I2C] Opening bus {args.bus}, slave 0x{CORE_I2C_ADDR:02x}...")
    with SMBus(args.bus) as bus:
        try:
            while True:
                line = collect_stats()
                send_line_i2c(bus, line)
                print(f"[CoreSerial I2C] Sent: {line}")
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\n[CoreSerial I2C] Stopping...")


if __name__ == "__main__":
    main()
