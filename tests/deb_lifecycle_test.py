#!/usr/bin/env python3
"""Destructive package-manager checks restricted to a disposable Docker container."""
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile


def run(*args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True,
                          timeout=180, **kwargs).stdout


def main():
    if not Path("/.dockerenv").is_file() or os.geteuid() != 0 or os.environ.get("GATEHAVEN_DISPOSABLE_TEST") != "1":
        raise SystemExit("Run only through tools/check_deb_container.py in a disposable container")
    package, report_path = map(Path, sys.argv[1:3])
    if run("dpkg-deb", "-f", package, "Package").strip() != "gatehaven":
        raise ValueError("unexpected package identity")
    if subprocess.run(["dpkg-query", "-W", "gatehaven"], capture_output=True).returncode == 0:
        raise ValueError("container already has Gatehaven installed")
    evidence = {"schema": 1, "artifact": package.name, "sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
                "os_release": Path("/etc/os-release").read_text(), "machine": platform.machine(), "checks": []}
    with tempfile.TemporaryDirectory(prefix="gatehaven consumer ") as directory:
        root = Path(directory)
        config = root / "config"; config.mkdir()
        marker = config / "preferences.ghp"; marker.write_text("user-owned settings marker\n")
        circuit = root / "my circuit.ghv"; circuit.write_text("GATEHAVEN 1\n0 0 source\n1 0 wire\n")
        original = circuit.read_bytes()
        env = {**os.environ, "XDG_CONFIG_HOME": str(config), "SDL_VIDEODRIVER": "dummy"}
        run("apt-get", "install", "--no-install-recommends", "-y", str(package))
        evidence["checks"].append("install")
        evidence["build_info"] = run("gatehaven-cli", "--build-info", cwd=root, env=env)
        run("gatehaven-cli", "check", circuit, cwd=root, env=env)
        run("gatehaven", "--self-test", cwd=root, env=env)
        desktop = Path("/usr/share/applications/com.michaelbaffourawuah.gatehaven.desktop")
        run("desktop-file-validate", desktop)
        # Check the cache produced by postinst; the test must not repair it first.
        assert b"application/x-ccsb" in Path("/usr/share/mime/types").read_bytes()
        assert Path("/usr/share/gatehaven/docs/manual.html").is_file()
        evidence["checks"].extend(["installed_cli", "installed_desktop", "mime_registration", "offline_manual"])
        # Same-version reinstall exercises replacement without fabricating an older release.
        run("apt-get", "install", "--reinstall", "--no-install-recommends", "-y", str(package))
        assert circuit.read_bytes() == original and marker.read_text() == "user-owned settings marker\n"
        run("gatehaven-cli", "check", circuit, cwd=root, env=env)
        evidence["checks"].append("reinstall_preserves_user_data")
        run("apt-get", "remove", "-y", "gatehaven")
        assert not Path("/usr/bin/gatehaven").exists() and not desktop.exists()
        assert b"application/x-ccsb" not in Path("/usr/share/mime/types").read_bytes()
        assert circuit.read_bytes() == original and marker.exists()
        evidence["checks"].append("remove_preserves_user_data")
        run("apt-get", "purge", "-y", "gatehaven")
        assert circuit.read_bytes() == original and marker.exists()
        evidence["checks"].append("purge_preserves_user_data")
    evidence["passed"] = True
    evidence["limits"] = ["Fresh Ubuntu container, not a full VM or physical desktop",
                          "Same-version reinstall, not an upgrade from an earlier product release",
                          "Dummy display; native dialogs and file-manager Open With need manual acceptance"]
    report_path.write_text(json.dumps(evidence, indent=2) + "\n")
    print("Fresh-container install, reinstall, removal and purge passed")

if __name__ == "__main__": main()
