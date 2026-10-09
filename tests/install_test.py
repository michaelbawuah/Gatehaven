"""Execute the installed product from a fresh path containing spaces."""
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

cmake, build, configuration, has_app = sys.argv[1:]

def run(*args, **kwargs):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=45, **kwargs)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(prefix="gatehaven installed ") as directory:
    root = Path(directory)
    run(cmake, "--install", build, "--config", configuration or "Release", "--prefix", root)
    suffix = ".exe" if sys.platform == "win32" else ""
    cli = root / "bin" / ("gatehaven-cli" + suffix)
    resources = root / "share" / "gatehaven"
    assert "Gatehaven" in run(cli, "--version")
    assert "Gatehaven CLI" in run(cli, "--help")
    for sample in (resources / "samples").glob("*.ghv"):
        run(cli, "check", sample)
    assert len(list((resources / "samples").glob("*.ghv"))) == 6
    assert (resources / "docs" / "manual.md").is_file()
    assert "<main>" in (resources / "docs" / "manual.html").read_text(encoding="utf-8")
    notice = Path("third_party") / "SDL3" / "LICENSE.txt"
    assert (resources / notice).read_bytes() == (Path(__file__).resolve().parents[1] / notice).read_bytes()
    if has_app == "ON":
        if sys.platform == "darwin":
            bundle = root / "gatehaven.app" / "Contents"
            app = bundle / "MacOS" / "gatehaven"
            info = plistlib.loads((bundle / "Info.plist").read_bytes())
            assert info["CFBundleExecutable"] == "gatehaven"
            assert info["UTExportedTypeDeclarations"][0]["UTTypeTagSpecification"]["public.filename-extension"] == ["ghv"]
            assert (bundle / "Resources" / "third_party" / "SDL3" / "LICENSE.txt").is_file()
            assert (bundle / "Resources" / "docs" / "manual.html").is_file()
        else:
            app = root / "bin" / ("gatehaven" + suffix)
        if sys.platform.startswith("linux"):
            desktop = root / "share" / "applications" / "com.michaelbaffourawuah.gatehaven.desktop"
            assert "Exec=gatehaven %f" in desktop.read_text()
            ET.parse(root / "share" / "mime" / "packages" / "gatehaven.xml")
            ET.parse(root / "share" / "icons" / "hicolor" / "scalable" / "apps" / "gatehaven.svg")
        assert "Gatehaven" in run(app, "--help")
        env = dict(os.environ, SDL_VIDEODRIVER="dummy")
        run(app, "--self-test", env=env, cwd=root)
        snapshot = root / "installed-preview.bmp"
        frames = set()
        for state in ("starter", "help", "examples", "hints", "speed", "clipboard", "gate-gallery", "keyboard", "recovery"):
            run(app, "--snapshot", snapshot, state, env=env, cwd=root)
            pixels = snapshot.read_bytes()
            assert pixels[:2] == b"BM"
            frames.add(pixels)
        assert len(frames) == 9, "Distinct UI states rendered identical frames"
print("Installed executables, six lessons, notices, metadata and desktop launch passed")
