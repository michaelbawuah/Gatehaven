"""Extract and execute the exact AppImage without requiring host FUSE support."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

image = Path(sys.argv[1]).resolve()
def run(*args, cwd=None, env=None):
    return subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True, capture_output=True, text=True, timeout=90).stdout
expected = Path(str(image) + ".sha256").read_text().split()[0]
assert hashlib.sha256(image.read_bytes()).hexdigest() == expected
metadata = json.loads(Path(str(image) + ".json").read_text())
assert metadata["sha256"] == expected and metadata["runtime_revision"]
assert metadata["build_metadata"]["source_revision"] in metadata["build_information"]
with tempfile.TemporaryDirectory(prefix="gatehaven extracted image ") as directory:
    run(image, "--appimage-extract", cwd=directory)
    root = Path(directory) / "squashfs-root"
    launcher = root / "AppRun"
    assert "Gatehaven" in run(launcher, "--version")
    assert "Gatehaven" in run(launcher, "--cli", "--version")
    manual = Path(run(launcher, "--cli", "--manual-path").strip())
    assert manual.is_file() and root in manual.parents
    assert (root / "usr/share/gatehaven/third_party/appimage-runtime-LICENSE.txt").is_file()
    run(launcher, "--self-test", env=dict(os.environ, SDL_VIDEODRIVER="dummy"))
    path = Path(directory) / "legacy circuit.ccsb"
    run(launcher, "--cli", "example", path)
    assert path.read_bytes()[:4] == b"CCPG"
    run(launcher, "--cli", "check", path)
print("AppImage checksum, extraction, relocation, desktop, manual and legacy CLI verified")
