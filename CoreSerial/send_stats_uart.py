#!/usr/bin/env python3

"""
send_stats_uart.py

Send cyberdeck stats to M5Stack Core1 over a direct UART link
using the Pi's GPIO UART and the Core's Grove GPIOs (G26 TX, G36 RX).

Same protocol as send_stats.py (key=value;... newline-terminated).

Wiring (3.3 V logic only):

  Pi GPIO14 (TXD0, pin 8)  -> Core G36 (RX)
  Pi GPIO15 (RXD0, pin 10) -> Core G26 (TX)
  Pi GND                   -> Core G (GND on Grove)

On the Pi, enable the serial port (without login shell) and then run:

  python send_stats_uart.py --port /dev/serial0
"""

import argparse
import time

import serial  # type: ignore

from send_stats import collect_stats


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Send system stats to M5Stack Core1 over UART (Pi GPIO UART).",
    )
    parser.add_argument(
        "--port",
        "-p",
        default="/dev/serial0",
        help="UART serial device (default: /dev/serial0 for Pi GPIO14/15)",
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

    print(f"[CoreSerial UART] Opening {args.port} @ {args.baud} baud...")
    ser = serial.Serial(args.port, args.baud, timeout=1)

    try:
        while True:
            line = collect_stats()
            encoded = (line + "\n").encode("utf-8", errors="ignore")
            ser.write(encoded)
            ser.flush()
            print(f"[CoreSerial UART] Sent: {line}")
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n[CoreSerial UART] Stopping sender...")
    finally:
        ser.close()


if __name__ == "__main__":
    main()

