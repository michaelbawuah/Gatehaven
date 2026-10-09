#!/usr/bin/env python3
"""Audit an explicitly supplied corpus; no reference files are distributed."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ELEMENTS = ("empty", "wire", "crossing", "signal", "source", "positive-relay",
            "negative-relay", "and", "or", "nand", "nor", "screen", "file-input", "file-output")

def audit(cli, path):
    data = path.read_bytes()
    if len(data) < 16:
        raise ValueError("truncated corpus header")
    magic, version, width, height = struct.unpack_from("<4siii", data)
    if magic != b"CCPG" or version != 0 or width < 0 or height < 0 or width * height > 64_000_000:
        raise ValueError("unsupported corpus header")
    if len(data) != 16 + width * height:
        raise ValueError("corpus must have a canonical payload length")
    expected = {}
    for index, byte in enumerate(data[16:]):
        if byte >> 2 >= len(ELEMENTS):
            raise ValueError("unknown corpus cell")
        if byte >> 2:
            expected[(index % width, index // width)] = (ELEMENTS[byte >> 2], byte & 3)
    with tempfile.TemporaryDirectory(prefix="gatehaven-corpus-") as directory:
        native, restored = Path(directory) / "converted.ghv", Path(directory) / "restored.ccsb"
        subprocess.run([cli, "convert", str(path), str(native)], check=True, capture_output=True, timeout=60)
        lines = native.read_text(encoding="utf-8").splitlines()
        stateful = lines[0] == "GATEHAVEN 2"
        actual = {}
        for line in lines[1:]:
            fields = line.split()
            actual[(int(fields[0]), int(fields[1]))] = (fields[2], int(fields[3]) if stateful else 0)
        if actual != expected:
            raise ValueError("native conversion differs from independently decoded input")
        subprocess.run([cli, "convert", str(native), str(restored)], check=True, capture_output=True, timeout=60)
        equal = restored.read_bytes() == data
    return {"sample": path.name, "sha256": hashlib.sha256(data).hexdigest(),
            "width": width, "height": height, "occupied": len(expected),
            "native_cells_match": True, "byte_exact_roundtrip": equal}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args()
    paths = sorted(args.corpus.glob("*.ccsb"))
    if not paths:
        parser.error("corpus contains no .ccsb files")
    records = [audit(str(args.cli.resolve()), path.resolve()) for path in paths]
    print(json.dumps({"schema": 1, "samples": records}, indent=2))
    return 0 if all(record["byte_exact_roundtrip"] for record in records) else 1

if __name__ == "__main__":
    raise SystemExit(main())
