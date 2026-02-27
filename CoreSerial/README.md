## CoreSerial – Cyberdeck Status Display

This component lets your Raspberry Pi / Linux host stream system stats over USB serial to the M5Stack Core1 (M5GO Core), which renders them in a retro cyberdeck style UI.

### Layout

- `CoreSerial/m5core1_serial/` – PlatformIO firmware project for the M5Stack Core1
  - `platformio.ini` – board + library configuration
  - `src/main.cpp` – firmware that reads serial and draws the UI
- `CoreSerial/send_stats.py` – Python script that runs on the Pi / Linux host and pushes stats over serial

### High‑level protocol

The host sends one line of text every ~500–1000 ms. Each line is a set of `key=value` pairs separated by `;`, ending with a newline:

time=2026-02-27 13:45:12;hostname=cyberdeck;cpu=12.3;ram_used_mb=1024;ram_total_mb=3950;ram_percent=25.9;load_1=0.21;load_5=0.17;load_15=0.11

The Core1:

- Reads a full line from the serial port
- Parses into fields
- Updates the on‑screen widgets (header, CPU bar, RAM bar, load, etc.)

Both sides default to **115200 baud**.

### Flashing the Core1 firmware (PlatformIO)

1. Plug the M5Stack Core1 (M5GO Core) into your Linux machine via USB.
2. Identify the serial port (for the QinHeng adapter this is usually `/dev/ttyUSB0`):

   ```bash
   ls /dev/ttyUSB*
   ```

3. Build and upload the firmware:

   ```bash
   cd CoreSerial/m5core1_serial
   pio run --target upload --upload-port /dev/ttyUSB0
   ```

4. To watch debug output:

   ```bash
   pio device monitor -b 115200 -p /dev/ttyUSB0
   ```

### Running the host stats sender

1. Install Python dependencies on the Pi / Linux host (uses `pyserial` and `psutil`):

   ```bash
   cd CoreSerial
   python -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   ```

2. Run the sender, pointing it at the USB serial device that maps to the Core1:

   ```bash
   python send_stats.py --port /dev/ttyUSB0
   ```

3. You should see the Core1 screen update every second with:

- Current time and hostname
- CPU usage + animated bar
- RAM usage + bar
- Load averages

### Notes

- The USB VID/PID you mentioned (`1a86:55d4 QinHeng Electronics USB Single Serial`) is typical of CH340 serial adapters. On Linux this is normally exposed as `/dev/ttyUSB0`, but adjust the `--port` argument as needed.
- The firmware is intentionally self‑contained and does not use WiFi; it just listens on the USB serial connected to your cyberdeck.

