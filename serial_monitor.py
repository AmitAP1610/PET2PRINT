#!/usr/bin/env python3
"""
PET2PRINT Serial Telemetry & Commissioning Tool
Reads live telemetry stream from Arduino UNO R4 WiFi over USB serial.
Logs data and provides a real-time terminal dashboard.
"""

import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[NOTE] 'pyserial' not installed. Install via: pip install pyserial")
    serial = None

def list_serial_ports():
    if not serial:
        return []
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]

def main():
    print("=" * 60)
    print("   PET2PRINT - Serial Telemetry & Commissioning Utility")
    print("=" * 60)

    if not serial:
        print("[ERR] Please run: pip install pyserial to use this utility.")
        sys.exit(1)

    available = list_serial_ports()
    if not available:
        print("[WARN] No COM/serial ports detected. Ensure your Arduino UNO R4 is connected.")
        port = input("Enter port name manually (e.g., COM3 or /dev/ttyACM0): ").strip()
    else:
        print("Available serial ports:")
        for idx, p in enumerate(available):
            print(f"  [{idx + 1}] {p}")
        choice = input(f"Select port (1-{len(available)}) or press Enter for default [{available[0]}]: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(available):
            port = available[int(choice) - 1]
        else:
            port = available[0]

    baud = 115200
    print(f"[INFO] Connecting to {port} at {baud} baud...")

    try:
        ser = serial.Serial(port, baud, timeout=1.0)
        time.sleep(2.0)  # Wait for Arduino reboot
        print("[INFO] Connected successfully! Press Ctrl+C to stop.\n")
    except Exception as e:
        print(f"[ERR] Failed to open port {port}: {e}")
        sys.exit(1)

    try:
        while True:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                print(line)
    except KeyboardInterrupt:
        print("\n[INFO] Session terminated by user.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()

