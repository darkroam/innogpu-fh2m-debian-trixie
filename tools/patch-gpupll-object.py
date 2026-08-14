#!/usr/bin/env python3
"""Skip the first G0M GPU PLL setup call in an Innogpu 3.3.3.42 object."""

from pathlib import Path
import sys


OLD = bytes.fromhex("e8 09 fd ff ff")
NEW = bytes.fromhex("90 90 90 90 90")


def offsets(data: bytes, needle: bytes) -> list[int]:
    matches: list[int] = []
    start = 0
    while True:
        offset = data.find(needle, start)
        if offset < 0:
            return matches
        matches.append(offset)
        start = offset + 1


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} OBJECT", file=sys.stderr)
        return 2

    target = Path(sys.argv[1])
    data = bytearray(target.read_bytes())
    old_offsets = offsets(data, OLD)

    if len(old_offsets) == 1:
        offset = old_offsets[0]
        data[offset : offset + len(OLD)] = NEW
        target.write_bytes(data)
        print(
            f"patched {target} at file offset {offset:#x}: "
            f'{OLD.hex(" ")} -> {NEW.hex(" ")}'
        )
        return 0

    if not old_offsets and NEW in data:
        print(f"already patched: {target}")
        return 0

    if old_offsets:
        detail = ", ".join(hex(offset) for offset in old_offsets)
        print(f"ERROR: ambiguous patch pattern in {target}: {detail}", file=sys.stderr)
    else:
        print(f"ERROR: patch pattern not found in {target}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
