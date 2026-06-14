#!/usr/bin/env python3
"""Convert voltas_183v_dzu2_codes.json to Flipper Zero .ir format.

Reads the JSON database of captured IR timing data and outputs a Flipper
Zero compatible .ir file with raw signal entries.

Usage:
    python3 json_to_flipper.py [input.json] [output.ir]

Defaults:
    input:  voltas_183v_dzu2_codes.json
    output: Voltas_183V_DZU2.ir
"""

import json
import sys
import os


def sanitize_name(label: str) -> str:
    """Convert a JSON label to a Flipper-friendly button name.

    Flipper button names should be printable ASCII. Replace underscores
    with underscores (already valid), capitalize for readability.
    """
    # Flipper names are typically short and descriptive
    # Remove temperature/fan context for cleaner names where possible
    return label


def json_to_flipper(input_path: str, output_path: str) -> None:
    """Convert JSON IR database to Flipper .ir format."""
    with open(input_path, "r", encoding="utf-8") as f:
        codes = json.load(f)

    lines = [
        "Filetype: IR signals file",
        "Version: 1",
        "#",
        "# Voltas 183V DZU2 Split AC",
        "# 48-bit NEC-extended protocol, 38kHz carrier",
        f"# {len(codes)} signals",
        "#",
    ]

    for label, entry in codes.items():
        raw = entry["raw_timing"]

        # Flipper raw data is space-separated mark/space timings
        # Our JSON already stores them as alternating mark, space, mark, ...
        data_str = " ".join(str(int(t)) for t in raw)

        lines.append(f"name: {sanitize_name(label)}")
        lines.append("type: raw")
        lines.append("frequency: 38000")
        lines.append("duty_cycle: 0.330000")
        lines.append(f"data: {data_str}")
        lines.append("#")

    # Remove trailing separator
    if lines[-1] == "#":
        lines.pop()

    output = "\n".join(lines) + "\n"

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(output)

    print(f"Converted {len(codes)} signals: {input_path} -> {output_path}")


def main():
    input_path = sys.argv[1] if len(sys.argv) > 1 else "voltas_183v_dzu2_codes.json"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "Voltas_183V_DZU2.ir"

    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found", file=sys.stderr)
        sys.exit(1)

    json_to_flipper(input_path, output_path)


if __name__ == "__main__":
    main()
