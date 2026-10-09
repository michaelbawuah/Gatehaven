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
    import plistlib
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
                info = plistlib.loads((apps[0] / "Contents/Info.plist").read_bytes())
                imported = info["UTImportedTypeDeclarations"]
                assert imported[0]["UTTypeTagSpecification"]["public.filename-extension"] == ["ccsb"]
                assert any(entry["LSHandlerRank"] == "Alternate" for entry in info["CFBundleDocumentTypes"])
                app = apps[0] / "Contents/MacOS/gatehaven"
                assert "Gatehaven" in run(app, "--version")
                assert (apps[0] / "Contents/Resources/docs/manual.md").is_file()
            finally:
                run("hdiutil", "detach", mount)
elif sys.platform == "win32":
    import os
    import winreg
    packages = list(build.glob("Gatehaven-*.exe"))
    assert len(packages) == 1, packages
    for package in packages:
        with tempfile.TemporaryDirectory(prefix="gatehaven installer ") as directory:
            root = Path(directory) / "app"
            # NSIS /D consumes the remaining command line and must be last.
            subprocess.run(f'"{package}" /S /D={root}', check=True, timeout=90)
            try:
                assert "Gatehaven" in run(root / "bin/gatehaven-cli.exe", "--version")
                assert (root / "share/gatehaven/docs/manual.html").is_file()
                for extension in (".ghv", ".ccsb"):
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension + "\\OpenWithProgids") as key:
                        assert winreg.QueryValueEx(key, "Gatehaven.Circuit")[0] == ""
                env = dict(os.environ, SDL_VIDEODRIVER="dummy")
                subprocess.run([str(root / "bin/gatehaven.exe"), "--self-test"], env=env, check=True, timeout=60)
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"Software\Classes\Gatehaven.Circuit\shell\open\command") as key:
                    command = winreg.QueryValueEx(key, "")[0]
                    assert str(root / "bin/gatehaven.exe") in command and '"%1"' in command
            finally:
                uninstaller = root / "Uninstall.exe"
                if uninstaller.exists():
                    subprocess.run(f'"{uninstaller}" /S _?={root}', check=True, timeout=90)
            assert not (root / "bin/gatehaven.exe").exists()
            for extension in (".ghv", ".ccsb"):
                try:
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension + "\\OpenWithProgids") as key:
                        winreg.QueryValueEx(key, "Gatehaven.Circuit")
                except FileNotFoundError:
                    pass
                else:
                    raise AssertionError("Uninstaller retained its Open With registration")
else:
    packages = []
for package in packages:
    expected = Path(str(package) + ".sha256").read_text().split()[0]
    assert hashlib.sha256(package.read_bytes()).hexdigest() == expected
    print("Installer verified:", package.name)
