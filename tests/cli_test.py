"""Check the installed command interface using actual files and parsed outputs."""
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

exe = sys.argv[1]

def run(*args, code=0):
    result = subprocess.run([exe, *map(str, args)], text=True, encoding="utf-8", capture_output=True, timeout=15)
    assert result.returncode == code, (args, result.stdout, result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(prefix="gatehaven cli ") as directory:
    path = Path(directory) / "starter circuit.ghv"
    run("example", path)
    assert "49 cells" in run("check", path)
    stats = json.loads(run("stats", path))
    assert stats["cells"] == 49 and stats["elements"]["source"] == 3
    for name in run("examples").splitlines():
        sample = Path(directory) / (name + ".ghv")
        run("example", name, sample)
        assert json.loads(run("stats", sample))["cells"] > 0
    run("example", "unknown", path, code=2)
    rows = list(csv.DictReader(io.StringIO(run("trace", path, 2))))
    output = [r for r in rows if r["x"] == "8" and r["y"] == "0"]
    assert [r["ports"] for r in output] == ["0", "15"]
    assert len(rows) == 98
    svg = Path(directory) / "diagram.svg"
    run("svg", path, svg)
    root = ET.parse(svg).getroot()
    assert len(root.findall("{http://www.w3.org/2000/svg}g")) == 49
    run("svg", path, Path(directory) / "missing" / "diagram.svg", code=1)
    assert list(csv.DictReader(io.StringIO(run("trace", path, 0)))) == []
    run("run", path, "-1", code=2)
    run("run", path, "1000001", code=2)
    run("check", Path(directory) / "missing", code=1)
print("CLI: sample creation, statistics, traces and failure exit codes passed")

with tempfile.TemporaryDirectory(prefix="gatehaven-unicode-") as directory:
    unicode_path = Path(directory) / "circuit-é-電気.ghv"
    run("example", unicode_path)
    run("check", unicode_path)
    unicode_svg = Path(directory) / "drawing-é-電気.svg"
    run("svg", unicode_path, unicode_svg)
    ET.parse(unicode_svg)

assert "Gatehaven CLI" in run("--help")
with tempfile.TemporaryDirectory(prefix="gatehaven-profile-") as directory:
    path = Path(directory) / "steady.ghv"
    path.write_text("GATEHAVEN 1\n0 0 source\n1 0 wire\n", encoding="utf-8")
    profile = json.loads(run("profile", path, 100))
    assert profile["ticks"] == 100 and profile["powered"] == 2
    assert profile["propagations"] == 2 and profile["settled_ticks"] == 98
    assert json.loads(run("profile", path, 0))["ticks"] == 0
    path.write_text("GATEHAVEN 1\n" + "".join(f"{i} 0 wire\n" for i in range(6)), encoding="utf-8")
    run("trace", path, 1000000, code=2)

info = run("--build-info")
assert "Compiler:" in info and "Target:" in info and "Language: C++23" in info

# Independent encoder for the documented digest byte contract.
def digest_bytes(cells, ticks):
    payload = bytearray([1]) + ticks.to_bytes(8, "little") + len(cells).to_bytes(8, "little")
    for x, y, element, ports, sending, receiving in sorted(cells, key=lambda c: (c[1], c[0])):
        payload += (x & 0xffffffff).to_bytes(4, "little") + (y & 0xffffffff).to_bytes(4, "little")
        payload += bytes([element, ports, sending, receiving])
    value = 14695981039346656037
    for byte in payload:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{value:016x}"
with tempfile.TemporaryDirectory(prefix="gatehaven digest ") as directory:
    path = Path(directory) / "deterministic.ghv"
    path.write_text("GATEHAVEN 1\n-2 -1 source\n-1 -1 wire\n", encoding="utf-8")
    for ticks in (0, 1, 2, 100):
        ports = 15 if ticks else 0
        expected = digest_bytes([(-2, -1, 3, ports, 0, 0), (-1, -1, 1, ports, 0, 0)], ticks)
        assert json.loads(run("digest", path, ticks)) == {"schema": 1, "ticks": ticks, "digest": expected}

with tempfile.TemporaryDirectory(prefix="gatehaven metrics ") as directory:
    path = Path(directory) / "source.ghv"
    path.write_text("GATEHAVEN 1\n0 0 source\n1 0 wire\n", encoding="utf-8")
    assert json.loads(run("profile", path, 3))["frontier_visits"] == 4

with tempfile.TemporaryDirectory(prefix="gatehaven normalize ") as directory:
    source, output = Path(directory) / "source.ghv", Path(directory) / "output.ghv"
    source.write_bytes(b"\xef\xbb\xbfGATEHAVEN 1\r\n# notes\r\n2 0 wire\r\n-1 0 source\r\n")
    run("normalize", source, output)
    assert output.read_bytes() == b"GATEHAVEN 1\n-1 0 source\n2 0 wire\n"
    run("normalize", output, output)
    before = output.read_bytes()
    source.write_text("invalid", encoding="utf-8")
    run("normalize", source, output, code=1)
    assert output.read_bytes() == before

with tempfile.TemporaryDirectory(prefix="gatehaven legacy ") as directory:
    native = Path(directory) / "source.ghv"
    legacy = Path(directory) / "converted.CCSB"
    restored = Path(directory) / "restored.ghv"
    native.write_text("GATEHAVEN 2\n0 0 source 0\n1 0 or 3\n", encoding="utf-8")
    run("convert", native, legacy)
    assert legacy.read_bytes()[:4] == b"CCPG"
    run("convert", legacy, restored)
    assert restored.read_text(encoding="utf-8") == native.read_text(encoding="utf-8")
    run("check", legacy)
    assert json.loads(run("stats", native)) == json.loads(run("stats", legacy))
    assert run("trace", native, 3) == run("trace", legacy, 3)
    run("convert", legacy, legacy)
    assert legacy.read_bytes()[:4] == b"CCPG"
