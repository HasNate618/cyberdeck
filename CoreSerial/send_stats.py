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
import getpass
import socket
import time
import urllib.request
from typing import Optional, Tuple

import psutil  # type: ignore
import serial  # type: ignore


_prev_net_bytes: Optional[Tuple[int, int]] = None
_prev_net_time: Optional[float] = None

_last_public_ip: str = ""
_last_public_ip_ts: float = 0.0
_PUBLIC_IP_REFRESH_SECONDS = 300.0


def _get_local_ip() -> str:
    """Best-effort local IP discovery that prefers real interfaces over loopback."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # We never actually send packets; this just forces a route lookup
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "0.0.0.0"


def _maybe_refresh_public_ip(now_epoch: float) -> str:
    """Fetch public IP occasionally, cache between calls."""
    global _last_public_ip, _last_public_ip_ts

    if _last_public_ip and (now_epoch - _last_public_ip_ts) < _PUBLIC_IP_REFRESH_SECONDS:
        return _last_public_ip

    try:
        with urllib.request.urlopen("https://api.ipify.org", timeout=2.0) as resp:
            _last_public_ip = resp.read().decode("utf-8", errors="ignore").strip()
            _last_public_ip_ts = now_epoch
    except Exception:
        if not _last_public_ip:
            _last_public_ip = "n/a"

    return _last_public_ip


def _get_cpu_temp_c() -> float:
    """Return CPU temperature in Celsius if available, else 0.0."""
    if not hasattr(psutil, "sensors_temperatures"):
        return 0.0
    try:
        temps = psutil.sensors_temperatures()
    except Exception:
        return 0.0
    if not temps:
        return 0.0

    # Prefer some common sensor names, fall back to first available
    for key in ("coretemp", "cpu-thermal", "k10temp"):
        entries = temps.get(key)
        if entries:
            return float(entries[0].current)

    first_key = next(iter(temps))
    entries = temps.get(first_key) or []
    if entries:
        return float(entries[0].current)
    return 0.0


def _get_net_speeds_mbps(now_epoch: float) -> Tuple[float, float]:
    """Return (up_mbps, down_mbps) based on deltas from last call."""
    global _prev_net_bytes, _prev_net_time

    try:
        counters = psutil.net_io_counters()
    except Exception:
        return 0.0, 0.0

    if _prev_net_bytes is None or _prev_net_time is None:
        _prev_net_bytes = (counters.bytes_sent, counters.bytes_recv)
        _prev_net_time = now_epoch
        return 0.0, 0.0

    dt = now_epoch - _prev_net_time
    if dt <= 0:
        return 0.0, 0.0

    sent_delta = counters.bytes_sent - _prev_net_bytes[0]
    recv_delta = counters.bytes_recv - _prev_net_bytes[1]

    _prev_net_bytes = (counters.bytes_sent, counters.bytes_recv)
    _prev_net_time = now_epoch

    up_mbps = max(0.0, (sent_delta * 8.0) / dt / 1_000_000.0)
    down_mbps = max(0.0, (recv_delta * 8.0) / dt / 1_000_000.0)
    return up_mbps, down_mbps


def collect_stats() -> str:
    """Collect a single snapshot of system stats and return encoded line."""
    now_epoch = time.time()
    now_str = _dt.datetime.fromtimestamp(now_epoch).strftime("%Y-%m-%d %H:%M:%S")
    user = getpass.getuser()
    hostname = socket.gethostname()
    cpu_pct = psutil.cpu_percent(interval=None)

    vm = psutil.virtual_memory()
    ram_used_mb = vm.used // (1024 * 1024)
    ram_total_mb = vm.total // (1024 * 1024)
    ram_percent = vm.percent

    local_ip = _get_local_ip()
    public_ip = _maybe_refresh_public_ip(now_epoch)
    cpu_temp_c = _get_cpu_temp_c()
    up_mbps, down_mbps = _get_net_speeds_mbps(now_epoch)

    # Send temp and usages as integers (no decimals) for display as "57C, 15%"
    parts = [
        f"time={now_str}",
        f"user={user}",
        f"hostname={hostname}",
        f"cpu={int(round(cpu_pct))}",
        f"ram_used_mb={ram_used_mb}",
        f"ram_total_mb={ram_total_mb}",
        f"ram_percent={int(round(ram_percent))}",
        f"local_ip={local_ip}",
        f"public_ip={public_ip}",
        f"cpu_temp_c={int(round(cpu_temp_c))}",
        f"net_up_mbps={up_mbps:.2f}",
        f"net_down_mbps={down_mbps:.2f}",
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

