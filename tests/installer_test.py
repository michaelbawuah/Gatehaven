"""Inspect unsigned preview installers without installing into the host system."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
def run(*args):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True, timeout=60).stdout

if sys.platform.startswith("linux"):
    packages = list(build.glob("gatehaven_*.deb"))
    assert len(packages) == 1, packages
    for package in packages:
        assert run("dpkg-deb", "-f", package, "Package").strip() == "gatehaven"
        assert run("dpkg-deb", "-f", package, "Depends").strip()
        with tempfile.TemporaryDirectory(prefix="gatehaven deb ") as directory:
            run("dpkg-deb", "-x", package, directory)
            root = Path(directory) / "usr"
            assert (root / "bin/gatehaven").is_file()
            assert (root / "share/gatehaven/docs/manual.md").is_file()
            assert "Gatehaven" in run(root / "bin/gatehaven-cli", "--version")
elif sys.platform == "darwin":
    packages = list(build.glob("Gatehaven-*.dmg"))
    assert len(packages) == 1, packages
    for package in packages:
        run("hdiutil", "verify", package)
        with tempfile.TemporaryDirectory(prefix="gatehaven dmg ") as directory:
            mount = Path(directory) / "volume"
            run("hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", mount, package)
            try:
                apps = list(mount.rglob("gatehaven.app"))
                assert len(apps) == 1
                app = apps[0] / "Contents/MacOS/gatehaven"
                assert "Gatehaven" in run(app, "--version")
                assert (apps[0] / "Contents/Resources/docs/manual.md").is_file()
            finally:
                run("hdiutil", "detach", mount)
else:
    packages = []
for package in packages:
    expected = Path(str(package) + ".sha256").read_text().split()[0]
    assert hashlib.sha256(package.read_bytes()).hexdigest() == expected
    print("Installer verified:", package.name)
