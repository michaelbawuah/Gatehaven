import importlib.util
from pathlib import Path
import struct
import sys
import tempfile

spec = importlib.util.spec_from_file_location("audit", Path(__file__).resolve().parents[1] / "tools/audit_legacy.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "all-states.ccsb"
    payload = bytes((kind << 2) | state for state in range(4) for kind in range(1, 14))
    path.write_bytes(struct.pack("<4siii", b"CCPG", 0, 13, 4) + payload)
    result = audit.audit(str(Path(sys.argv[1]).resolve()), path)
    assert result["byte_exact_roundtrip"] and result["occupied"] == 52
    for bad in (b"", struct.pack("<4siii", b"CCPG", 0, -1, 1), path.read_bytes()[:-1]):
        path.write_bytes(bad)
        try:
            audit.audit(str(Path(sys.argv[1]).resolve()), path)
        except ValueError:
            pass
        else:
            raise AssertionError("malformed corpus was accepted")
print("Independent legacy audit passed")
