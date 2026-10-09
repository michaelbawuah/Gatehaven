#!/usr/bin/env python3
"""Build a relocatable Linux AppImage from an existing tested desktop build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MAX_ASSET_BYTES = 32 * 1024 * 1024

def run(*args, cwd=None, env=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env, text=True, capture_output=True, timeout=240)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed ({result.returncode}): {result.stdout[-4000:]} {result.stderr[-4000:]}")
    return result.stdout

def download(asset, cache):
    target = cache / asset["sha256"]
    if not target.exists():
        with urllib.request.urlopen(asset["url"], timeout=60) as response:
            data = response.read(MAX_ASSET_BYTES + 1)
        if len(data) > MAX_ASSET_BYTES or hashlib.sha256(data).hexdigest() != asset["sha256"]:
            raise ValueError("packaging dependency checksum or size mismatch")
        target.write_bytes(data)
    if target.stat().st_size > MAX_ASSET_BYTES or hashlib.sha256(target.read_bytes()).hexdigest() != asset["sha256"]:
        raise ValueError("cached packaging dependency checksum mismatch")
    target.chmod(0o755)
    return target

def glibc_requirement(text):
    versions = [tuple(map(int, parts)) for parts in re.findall(r"GLIBC_([0-9]+)\.([0-9]+)", text)]
    return max(versions, default=(0, 0))

def verify_linux_baseline(appdir):
    files = [appdir / "usr/bin/gatehaven", appdir / "usr/bin/gatehaven-cli", *(appdir / "usr/lib").glob("*.so*")]
    required = max(glibc_requirement(run("readelf", "--version-info", path)) for path in files)
    if required > (2, 39):
        raise ValueError(f"GLIBC {required[0]}.{required[1]} exceeds the declared 2.39 baseline; use the pinned build host")
    return ".".join(map(str, required))

def stage_appdir(cmake, build, appdir):
    run(cmake, "--install", build, "--config", "Release", "--prefix", appdir / "usr")
    for name in ("gatehaven", "gatehaven-cli"):
        if not (appdir / "usr/bin" / name).is_file(): raise ValueError("a desktop build is required")
    shutil.copy2(ROOT / "packaging/linux/AppRun", appdir / "AppRun")
    (appdir / "AppRun").chmod(0o755)
    shutil.copy2(ROOT / "packaging/linux/com.michaelbaffourawuah.gatehaven.desktop", appdir / "gatehaven.desktop")
    shutil.copy2(ROOT / "packaging/gatehaven.svg", appdir / "gatehaven.svg")
    shutil.copy2(ROOT / "packaging/gatehaven.svg", appdir / ".DirIcon")
    library_dir = appdir / "usr/lib"
    library_dir.mkdir(exist_ok=True)
    bundled = []
    for binary in (appdir / "usr/bin/gatehaven", appdir / "usr/bin/gatehaven-cli"):
        for line in run("ldd", binary).splitlines():
            match = re.match(r"\s*(libstdc\+\+\.so\.6|libgcc_s\.so\.1) => (\S+) ", line)
            if match:
                name, source = match.groups()
                shutil.copy2(source, library_dir / name)
                if name not in bundled: bundled.append(name)
    notices = appdir / "usr/share/gatehaven/third_party"
    if bundled:
        copyright_file = Path("/usr/share/doc/libstdc++6/copyright")
        if not copyright_file.is_file(): raise ValueError("GCC runtime distribution copyright notice is required")
        shutil.copy2(copyright_file.resolve(), notices / "gcc-runtime-copyright.txt")
        for name in ("GPL-3", "LGPL-3"):
            shutil.copy2(Path("/usr/share/common-licenses") / name, notices / (name + ".txt"))
    return bundled

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cache", type=Path)
    args = parser.parse_args()
    if platform.system() != "Linux": parser.error("AppImages are built on Linux")
    arch = platform.machine()
    lock = json.loads((ROOT / "packaging/linux/appimage-tools.json").read_text())
    if arch not in lock["architectures"]: parser.error("supported architectures: x86_64 and aarch64")
    cache = args.cache or args.build / "appimage-cache"
    cache = cache.resolve(); cache.mkdir(parents=True, exist_ok=True)
    assets = lock["architectures"][arch]
    tool, runtime = download(assets["tool"], cache), download(assets["runtime"], cache)
    output = args.output.resolve(); output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gatehaven appimage ") as temporary:
        work = Path(temporary); appdir = work / "Gatehaven.AppDir"
        bundled = stage_appdir(args.cmake, args.build.resolve(), appdir)
        glibc_floor = verify_linux_baseline(appdir)
        run(tool, "--appimage-extract", cwd=work)
        env = dict(os.environ, ARCH=arch)
        candidate = work / "Gatehaven.AppImage"
        run(work / "squashfs-root/AppRun", "--no-appstream", "--runtime-file", runtime,
            appdir, candidate, cwd=work, env=env)
        candidate.chmod(0o755)
        # Validate the exact image before replacing an existing output artifact.
        verify = work / "verify"; verify.mkdir()
        run(candidate, "--appimage-extract", cwd=verify)
        extracted = verify / "squashfs-root"
        if "Gatehaven" not in run(extracted / "AppRun", "--cli", "--version"):
            raise ValueError("the packaged CLI did not identify itself")
        build_information = run(extracted / "AppRun", "--cli", "--build-info")
        if run(extracted / "AppRun", "--build-info") != build_information:
            raise ValueError("desktop and CLI build metadata differ")
        build_metadata = json.loads((extracted / "usr/share/gatehaven/build-metadata.json").read_text())
        if build_metadata["source_revision"] not in build_information:
            raise ValueError("packaged source revision does not match the executable")
        shutil.copy2(candidate, output)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    Path(str(output) + ".sha256").write_text(f"{digest}  {output.name}\n", encoding="utf-8")
    Path(str(output) + ".json").write_text(json.dumps({"schema": 1, "architecture": arch,
        "tool_version": lock["tool_version"], "runtime_revision": lock["runtime_revision"],
        "assets": assets, "bundled_libraries": bundled, "sha256": digest,
        "build_information": build_information, "build_metadata": build_metadata, "glibc_symbol_floor": glibc_floor,
        "system_requirements": "glibc 2.39 or newer; working X11 or Wayland session and native dialog services"}, indent=2) + "\n")
    print(output)
if __name__ == "__main__": main()
