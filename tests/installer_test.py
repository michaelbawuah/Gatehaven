"""Inspect preview installers; Windows installs/uninstalls on its disposable CI host."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
application_license = (Path(__file__).resolve().parents[1] / "LICENSE").read_bytes()
def run(*args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True, timeout=60, **kwargs).stdout

if sys.platform.startswith("linux"):
    import xml.etree.ElementTree as ET
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
            assert (root / "share/gatehaven/LICENSE").read_bytes() == application_license
            mime = ET.parse(root / "share/mime/packages/gatehaven.xml")
            ns = {"m": "http://www.freedesktop.org/standards/shared-mime-info"}
            assert {node.attrib["pattern"] for node in mime.findall(".//m:glob", ns)} == {"*.ghv", "*.ccsb"}
            desktop = (root / "share/applications/com.michaelbaffourawuah.gatehaven.desktop").read_text()
            assert "application/x-ccsb;" in desktop
            assert "Gatehaven" in run(root / "bin/gatehaven-cli", "--version")
            control = Path(directory) / "control"
            run("dpkg-deb", "-e", package, control)
            for hook in ("postinst", "postrm"):
                script = control / hook
                assert script.is_file() and script.stat().st_mode & 0o111
                run("sh", "-n", script)
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
                # Finder-style relocation must retain all resources inside the bundle.
                relocated = Path(directory) / "Applications moved \u00e9" / "gatehaven.app"
                relocated.parent.mkdir()
                run("ditto", apps[0], relocated)
                outside = Path(directory) / "working folder"; outside.mkdir()
                executable = relocated / "Contents/MacOS/gatehaven"
                resources = relocated / "Contents/Resources"
                env = dict(os.environ, SDL_VIDEODRIVER="dummy")
                run(executable, "--self-test", cwd=outside, env=env)
                assert Path(run(executable, "--manual-path", cwd=outside).strip()).samefile(resources / "docs/manual.html")
                assert len(list((resources / "samples").glob("*.ghv"))) == 6
                assert (resources / "third_party/SDL3/LICENSE.txt").is_file()
                assert (resources / "LICENSE").read_bytes() == application_license
            finally:
                run("hdiutil", "detach", mount)
elif sys.platform == "win32":
    import winreg
    def default_handler(extension):
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension) as key:
                return winreg.QueryValueEx(key, "")
        except FileNotFoundError:
            return None
    packages = list(build.glob("Gatehaven-*.exe"))
    assert len(packages) == 1, packages
    for package in packages:
        with tempfile.TemporaryDirectory(prefix="gatehaven installer ") as directory:
            root = Path(directory) / "app"
            defaults = {extension: default_handler(extension) for extension in (".ghv", ".ccsb")}
            circuit = Path(directory) / "my circuit \u00e9.ghv"
            circuit.write_text("GATEHAVEN 1\n0 0 source\n1 0 wire\n", encoding="utf-8")
            original = circuit.read_bytes()
            # NSIS /D consumes the remaining command line and must be last.
            subprocess.run(f'"{package}" /S /D={root}', check=True, timeout=90)
            try:
                assert "Gatehaven" in run(root / "bin/gatehaven-cli.exe", "--version")
                assert (root / "share/gatehaven/docs/manual.html").is_file()
                assert (root / "share/gatehaven/LICENSE").read_bytes() == application_license
                for extension in (".ghv", ".ccsb"):
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension + "\\OpenWithProgids") as key:
                        assert winreg.QueryValueEx(key, "Gatehaven.Circuit")[0] == ""
                env = dict(os.environ, SDL_VIDEODRIVER="dummy")
                subprocess.run([str(root / "bin/gatehaven.exe"), "--self-test"], env=env, check=True, timeout=60)
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"Software\Classes\Gatehaven.Circuit\shell\open\command") as key:
                    command = winreg.QueryValueEx(key, "")[0]
                    assert str(root / "bin/gatehaven.exe") in command and '"%1"' in command
                # Also preserve unknown files a user put in the installation folder.
                marker = root / "my notes.txt"; marker.write_text("user-owned\n")
                subprocess.run(f'"{package}" /S /D={root}', check=True, timeout=120)
                assert marker.read_text() == "user-owned\n" and circuit.read_bytes() == original
                run(root / "bin/gatehaven-cli.exe", "check", circuit, cwd=directory)
                subprocess.run([str(root / "bin/gatehaven.exe"), "--self-test"], env=env, check=True, timeout=60)
                assert all(default_handler(ext) == value for ext, value in defaults.items())
                for extension in defaults:
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension + "\\OpenWithProgids") as key:
                        assert winreg.QueryValueEx(key, "Gatehaven.Circuit")[0] == ""
            finally:
                uninstaller = root / "Uninstall.exe"
                if uninstaller.exists():
                    subprocess.run(f'"{uninstaller}" /S _?={root}', check=True, timeout=90)
            assert not (root / "bin/gatehaven.exe").exists()
            assert marker.read_text() == "user-owned\n" and circuit.read_bytes() == original
            assert all(default_handler(ext) == value for ext, value in defaults.items())
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
