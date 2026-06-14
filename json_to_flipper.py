#!/usr/bin/env python3
"""Convert voltas_183v_dzu2_codes.json to Flipper Zero .ir format.

Generates two files:
  - Voltas_183V_DZU2.ir         Full database (all recorded signals)
  - Voltas_183V_DZU2_Remote.ir  Compact remote (practical button set)

Usage:
    python3 json_to_flipper.py [input.json]

Defaults:
    input: voltas_183v_dzu2_codes.json
"""

import json
import sys
import os

# Compact remote: curated button set with clean names
# Maps: display_name -> json_label
REMOTE_BUTTONS = {
    # Power
    "Power_Off": "power_off",

    # Cool mode -- temperature range
    "Cool_17": "cool_17c",
    "Cool_20": "cool_20c",
    "Cool_22": "cool_22c_fan_high",
    "Cool_24": "cool_24c_fan_high",
    "Cool_26": "cool_26c_fan_high",
    "Cool_27": "cool_27c_fan_high",
    "Cool_28": "cool_28c_fan_high",
    "Cool_30": "cool_30c_fan_high",

    # Fan speeds (at 24C and 27C)
    "Cool_24_Auto": "cool_24c_fan_auto",
    "Cool_24_Low": "cool_24c_fan_low",
    "Cool_24_Med": "cool_24c_fan_med",
    "Cool_24_High": "cool_24c_fan_high",
    "Cool_27_Auto": "cool_27c_fan_auto",
    "Cool_27_Low": "cool_27c_fan_low",
    "Cool_27_Med": "cool_27c_fan_med",
    "Cool_27_High": "cool_27c_fan_high",

    # Other modes
    "Heat_27": "mode_heat_27c_fan_high",
    "Dry_27": "mode_dry_27c_fan_auto",
    "Fan_Only": "mode_fan_high_no_temp",

    # Swing
    "Swing_On": "swing_on_27c_cool_fan_high",
    "Swing_Off": "direction_down_swing_off_27c_cool_fan_high",

    # Features
    "Turbo": "turbo_toggle_27c_cool_fan_high",
    "Sleep": "sleep_27c",
    "Sleep_Off": "sleep_27c_off",
    "Eco": "eco_on_cool",
    "Eco_Off": "eco_off_cool",
    "Display_Toggle": "numeric_display_toggle_27c_cool_fan_high",
    "Display_Off": "numeric_display_off_27c_cool_fan_high",
    "Follow_Me": "follow_me_on",

    # Timers
    "Timer_Off_1h": "timer_off_1hr_27c_cool_fan_high",
    "Timer_Off_2h": "timer_off_2hr_27c_cool_fan_high",
    "Timer_Off_4h": "timer_off_4hr_27c_cool_fan_high",
    "Timer_Off_8h": "timer_off_8hr_27c_cool_fan_high",
    "Timer_Cancel": "cancel_timer",
}


def write_ir_file(signals, output_path, title, count_label):
    """Write a list of (name, raw_timing) tuples to a Flipper .ir file."""
    lines = [
        "Filetype: IR signals file",
        "Version: 1",
        "#",
        f"# {title}",
        "# 48-bit NEC-extended protocol, 38kHz carrier",
        f"# {count_label}",
        "#",
    ]

    for name, raw in signals:
        data_str = " ".join(str(int(t)) for t in raw)
        lines.append(f"name: {name}")
        lines.append("type: raw")
        lines.append("frequency: 38000")
        lines.append("duty_cycle: 0.330000")
        lines.append(f"data: {data_str}")
        lines.append("#")

    if lines[-1] == "#":
        lines.pop()

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    input_path = sys.argv[1] if len(sys.argv) > 1 else "voltas_183v_dzu2_codes.json"

    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found", file=sys.stderr)
        sys.exit(1)

    with open(input_path, "r", encoding="utf-8") as f:
        codes = json.load(f)

    # --- Full database ---
    full_output = "Voltas_183V_DZU2.ir"
    full_signals = [(label, entry["raw_timing"]) for label, entry in codes.items()]
    write_ir_file(
        full_signals,
        full_output,
        "Voltas 183V DZU2 Split AC -- Full Database",
        f"{len(full_signals)} signals",
    )
    print(f"Full:    {len(full_signals)} signals -> {full_output}")

    # --- Compact remote ---
    remote_output = "Voltas_183V_DZU2_Remote.ir"
    remote_signals = []
    missing = []

    for display_name, json_label in REMOTE_BUTTONS.items():
        if json_label in codes:
            remote_signals.append((display_name, codes[json_label]["raw_timing"]))
        else:
            missing.append(f"  {display_name} -> {json_label}")

    write_ir_file(
        remote_signals,
        remote_output,
        "Voltas 183V DZU2 Split AC -- Remote",
        f"{len(remote_signals)} buttons",
    )
    print(f"Remote:  {len(remote_signals)} buttons -> {remote_output}")

    if missing:
        print(f"\nWarning: {len(missing)} buttons not found in JSON:")
        for m in missing:
            print(m)


if __name__ == "__main__":
    main()
