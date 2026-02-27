#!/usr/bin/env python3

"""
send_stats.py

Stream cyberdeck-style system stats over serial to an M5Stack Core1.

Protocol (one line per update, newline-terminated):

    time=2026-02-27 13:45:12;hostname=cyberdeck;cpu=12.3;ram_used_mb=1024;ram_total_mb=3950;ram_percent=25.9;load_1=0.21;load_5=0.17;load_15=0.11

The Core1 firmware parses key=value pairs separated by ';' and updates
its on-screen widgets accordingly.
"""

import argparse
import datetime as _dt
import socket
import time

import psutil  # type: ignore
import serial  # type: ignore


def collect_stats() -> str:
    """Collect a single snapshot of system stats and return encoded line."""
    now = _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    hostname = socket.gethostname()
    cpu_pct = psutil.cpu_percent(interval=None)

    vm = psutil.virtual_memory()
    ram_used_mb = vm.used // (1024 * 1024)
    ram_total_mb = vm.total // (1024 * 1024)
    ram_percent = vm.percent

    load1, load5, load15 = (0.0, 0.0, 0.0)
    if hasattr(psutil, "getloadavg"):
        try:
            load1, load5, load15 = psutil.getloadavg()
        except (OSError, AttributeError):
            pass

    # Construct the line
    parts = [
        f"time={now}",
        f"hostname={hostname}",
        f"cpu={cpu_pct:.1f}",
        f"ram_used_mb={ram_used_mb}",
        f"ram_total_mb={ram_total_mb}",
        f"ram_percent={ram_percent:.1f}",
        f"load_1={load1:.2f}",
        f"load_5={load5:.2f}",
        f"load_15={load15:.2f}",
    ]
    return ";".join(parts)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Send system stats over serial to an M5Stack Core1 cyberdeck display.",
    )
    parser.add_argument(
        "--port",
        "-p",
        required=True,
        help="Serial port for Core1 (e.g. /dev/ttyUSB0)",
    )
    parser.add_argument(
        "--baud",
        "-b",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)",
    )
    parser.add_argument(
        "--interval",
        "-i",
        type=float,
        default=1.0,
        help="Update interval in seconds (default: 1.0)",
    )

    args = parser.parse_args()

    print(f"[CoreSerial] Opening serial port {args.port} @ {args.baud} baud...")
    ser = serial.Serial(args.port, args.baud, timeout=1)

    # Give the Core1 a moment after opening the port (in case it auto-resets)
    time.sleep(2.0)

    try:
        while True:
            line = collect_stats()
            encoded = (line + "\n").encode("utf-8", errors="ignore")
            ser.write(encoded)
            ser.flush()
            print(f"[CoreSerial] Sent: {line}")
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n[CoreSerial] Stopping sender...")
    finally:
        ser.close()


if __name__ == "__main__":
    main()

