#!/usr/bin/env python3
"""
Voltas AC IR Code Recorder
Reads captured IR signals from Arduino serial and saves them with labels.

Usage:
    python3 record_codes.py

Workflow:
    1. Flash the voltas_ir_capture sketch onto the Arduino
    2. Wire up the VS1838B receiver
    3. Run this script
    4. In the Xiaomi app, set the AC to a specific state
    5. Press the button in the Xiaomi app while pointing at the receiver
    6. Type a label when prompted (e.g. "cool_24c_fan_auto")
    7. Repeat for all states
    8. Press Ctrl+C when done
"""
import json
import os
import re
import sys
import time

import serial
import serial.tools.list_ports

BAUD_RATE = 115200
OUTPUT_FILE = "voltas_183v_dzu2_codes.json"
TIMING_TOLERANCE = 0.15  # 15% tolerance for timing comparison


def find_arduino():
    """Find the Arduino serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "ACM" in port.device or "Arduino" in (port.description or ""):
            return port.device
        if "USB" in port.device and "tty" in port.device:
            return port.device
    return None


def parse_signal(raw_lines):
    """Parse the captured IR signal from serial output."""
    result = {
        "protocol": "UNKNOWN",
        "hash": None,
        "raw_timing": [],
        "pronto": None,
        "c_array": None,
        "address": None,
        "command": None,
    }

    raw_formatted_values = []

    for line in raw_lines:
        line = line.strip()

        # Protocol line
        if line.startswith("Protocol="):
            match = re.search(r"Protocol=(\S+)", line)
            if match:
                result["protocol"] = match.group(1)
            match = re.search(r"Hash=0x([0-9A-Fa-f]+)", line)
            if match:
                result["hash"] = match.group(1)

        # Address/Command from recognized protocols
        if line.startswith("Address="):
            match = re.search(r"Address=0x([0-9A-Fa-f]+)", line)
            if match:
                result["address"] = match.group(1)
        if line.startswith("Command="):
            match = re.search(r"Command=0x([0-9A-Fa-f]+)", line)
            if match:
                result["command"] = match.group(1)

        # C array line (primary source)
        if "uint16_t rawIRTimings" in line:
            match = re.search(r"\{(.+?)\}", line)
            if match:
                values_str = match.group(1)
                values = [int(v.strip()) for v in values_str.split(",") if v.strip()]
                result["raw_timing"] = values
                result["c_array"] = line.strip()

        # Formatted raw timing lines (fallback source)
        # Lines look like: "+ 550,-1600 + 550,- 500" or "+4450,-4450"
        if re.search(r'[+-]\s*\d+', line) and "rawIRTimings" not in line:
            pairs = re.findall(r'[+-]\s*(\d+)', line)
            for val in pairs:
                raw_formatted_values.append(int(val))

        # Pronto line
        if "prontoData" in line:
            match = re.search(r'"(.+?)"', line)
            if match:
                result["pronto"] = match.group(1).strip()

    # Fallback: use formatted raw values if C array wasn't found
    if not result["raw_timing"] and raw_formatted_values:
        result["raw_timing"] = raw_formatted_values

    return result


def decode_hex_bytes(timings):
    """Decode raw timings into hex bytes (first frame only)."""
    start = 2 if len(timings) > 4 and timings[1] > 3000 else 0
    byte_val = 0
    bit_count = 0
    hex_bytes = []
    for i in range(start, len(timings) - 1, 2):
        space = timings[i + 1] if i + 1 < len(timings) else 0
        byte_val = (byte_val << 1) | (1 if space > 1000 else 0)
        bit_count += 1
        if bit_count == 8:
            hex_bytes.append(byte_val)
            byte_val = 0
            bit_count = 0
        if space > 4000 and i > start + 4:
            break
    return hex_bytes


def timings_match(timing_a, timing_b, tolerance=TIMING_TOLERANCE):
    """Check if two raw timing arrays represent the same signal.

    Compares decoded hex bytes rather than raw microsecond values,
    since the same button press produces slightly different timings
    each time but identical data bits.
    """
    bytes_a = decode_hex_bytes(timing_a)
    bytes_b = decode_hex_bytes(timing_b)
    return bytes_a == bytes_b and len(bytes_a) > 0


def find_duplicate_signal(new_timing, codes):
    """Check if a signal already exists in the recorded codes.

    Returns the label of the matching code, or None.
    """
    for label, entry in codes.items():
        existing = entry.get("raw_timing", [])
        if existing and timings_match(new_timing, existing):
            return label
    return None


def main():  # noqa: C901
    """Main recording loop: read serial, capture signals, save to JSON."""
    # Find Arduino
    port = find_arduino()
    if not port:
        print("No Arduino found. Available ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device} - {p.description}")
        port = input("Enter port manually: ").strip()

    print(f"Using port: {port}")

    # Load existing codes if any
    codes = {}
    if os.path.exists(OUTPUT_FILE):
        with open(OUTPUT_FILE, encoding="utf-8") as f:
            codes = json.load(f)
        print(f"Loaded {len(codes)} existing codes from {OUTPUT_FILE}")

    # Open serial
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error opening {port}: {e}")
        sys.exit(1)

    time.sleep(2)  # Wait for Arduino reset
    ser.reset_input_buffer()

    print()
    print("=" * 60)
    print("  VOLTAS 183V DZU2 IR CODE RECORDER")
    print("=" * 60)
    print()
    print("Instructions:")
    print("  1. Point Xiaomi phone at VS1838B receiver")
    print("  2. Set desired state in the Xiaomi app")
    print("  3. Press the button to send IR signal")
    print("  4. Type a label when prompted")
    print("  5. Repeat for all states")
    print("  6. Press Ctrl+C when done")
    print()
    print("Suggested labels:")
    print("  power_off")
    print("  cool_24c_fan_auto, cool_24c_fan_low, cool_24c_fan_med, cool_24c_fan_high")
    print("  cool_22c_fan_auto, cool_26c_fan_auto, cool_18c_fan_auto")
    print("  heat_24c_fan_auto (if supported)")
    print("  dry_mode, fan_mode")
    print("  swing_on, swing_off")
    print("  turbo_on, turbo_off")
    print("  sleep_on, sleep_off")
    print("  temp_up, temp_down")
    print()
    print("Waiting for IR signals...")
    print()

    buffer = []
    in_signal = False
    signal_count = 0

    try:
        while True:
            if ser.in_waiting:
                line = ser.readline().decode("utf-8", errors="replace").rstrip("\r\n")

                if "IR SIGNAL RECEIVED" in line:
                    in_signal = True
                    buffer = []
                    continue

                if "END SIGNAL" in line:
                    in_signal = False
                    signal_count += 1

                    # Parse the signal
                    parsed = parse_signal(buffer)

                    if not parsed["raw_timing"]:
                        print("  [warning] No timing data captured, skipping")
                        continue

                    n_values = len(parsed["raw_timing"])
                    print(f"\n  Signal #{signal_count} captured!")
                    print(f"  Protocol: {parsed['protocol']}")
                    print(f"  Hash: {parsed['hash']}")
                    print(f"  Raw values: {n_values}")
                    # Decode hex bytes for display
                    hex_bytes = decode_hex_bytes(parsed["raw_timing"])
                    hex_str = " ".join(f"{b:02X}" for b in hex_bytes)
                    print("  Data bytes:", hex_str)

                    # Check for duplicate signal
                    dup_label = find_duplicate_signal(parsed["raw_timing"], codes)
                    if dup_label:
                        print(f"  [duplicate] Same signal already recorded as '{dup_label}'")
                        choice = input("  (s)kip / (k)eep both with new name / (o)verwrite? [s]: ").strip().lower()
                        if choice == 'o':
                            print(f"  Overwriting '{dup_label}'...")
                            codes[dup_label]["raw_timing"] = parsed["raw_timing"]
                            codes[dup_label]["pronto"] = parsed["pronto"]
                            codes[dup_label]["hash"] = parsed["hash"]
                            codes[dup_label]["hex_bytes"] = [f"{b:02X}" for b in hex_bytes]
                            codes[dup_label]["timestamp"] = time.strftime("%Y-%m-%d %H:%M:%S")
                            with open(OUTPUT_FILE, 'w', encoding="utf-8") as f:
                                json.dump(codes, f, indent=2)
                            print(f"  Updated '{dup_label}'")
                        elif choice == 'k':
                            label = input("  New label: ").strip()
                            if label:
                                codes[label] = {
                                    "raw_timing": parsed["raw_timing"],
                                    "pronto": parsed["pronto"],
                                    "protocol": parsed["protocol"],
                                    "hash": parsed["hash"],
                                    "hex_bytes": [f"{b:02X}" for b in hex_bytes],
                                    "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                                }
                                with open(OUTPUT_FILE, 'w', encoding="utf-8") as f:
                                    json.dump(codes, f, indent=2)
                                print(f"  Saved as '{label}' ({len(codes)} total)")
                        else:
                            print("  Skipped.")
                        print()
                        print("  Ready for next signal...")
                        continue

                    # Ask for label
                    label = input("  Label (or 's' to skip, 'r' to redo): ").strip()

                    if label.lower() == 's':
                        print("  Skipped.")
                        continue
                    elif label.lower() == 'r':
                        print("  Ready to redo - send the signal again.")
                        continue
                    elif label == '':
                        label = f"signal_{signal_count}"

                    # Check for duplicate label
                    if label in codes:
                        print(f"  [exists] Label '{label}' already used")
                        choice = input("  (o)verwrite / (r)ename / (s)kip? [r]: ").strip().lower()
                        if choice == 'o':
                            print(f"  Overwriting '{label}'...")
                        elif choice == 's':
                            print("  Skipped.")
                            continue
                        else:
                            label = input("  New label: ").strip()
                            if not label:
                                print("  Skipped.")
                                continue

                    # Save
                    codes[label] = {
                        "raw_timing": parsed["raw_timing"],
                        "pronto": parsed["pronto"],
                        "protocol": parsed["protocol"],
                        "hash": parsed["hash"],
                        "hex_bytes": [f"{b:02X}" for b in hex_bytes],
                        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                    }

                    # Write to file after each capture
                    with open(OUTPUT_FILE, 'w', encoding="utf-8") as f:
                        json.dump(codes, f, indent=2)

                    print(f"  Saved as '{label}' ({len(codes)} total codes)")
                    print()
                    print("  Ready for next signal...")
                    continue

                if in_signal:
                    buffer.append(line)

    except KeyboardInterrupt:
        print()
        print()
        print("=" * 60)
        print(f"Recording complete! {len(codes)} codes saved to {OUTPUT_FILE}")
        print("=" * 60)
        print()
        print("Recorded codes:")
        for name in sorted(codes.keys()):
            n = len(codes[name].get("raw_timing", []))
            print(f"  {name:30s} ({n} values)")
        print()

    finally:
        ser.close()


if __name__ == "__main__":
    main()
