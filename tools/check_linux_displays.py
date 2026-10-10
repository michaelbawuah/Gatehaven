#!/usr/bin/env python3
"""Exercise SDL's X11 and Wayland window paths in isolated virtual display sessions."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("application", type=Path)
parser.add_argument("--output", type=Path, help="new folder for per-backend diagnostics and benchmark JSON")
args = parser.parse_args()
binary = str(args.application.resolve())
env = {**os.environ, "SDL_VIDEODRIVER": "x11", "SDL_RENDER_DRIVER": "software"}
subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1600x1000x24", binary, "--self-test-display"],
               env=env, check=True, timeout=90)
if args.output:
    args.output.mkdir(parents=True, exist_ok=False)
    for flag, name in (("--diagnostics", "diagnostics"), ("--benchmark-display", "rendering")):
        command = ["xvfb-run", "-a", "-s", "-screen 0 1600x1000x24", binary, flag]
        if flag == "--benchmark-display": command += ["10000", "30"]
        with (args.output / ("x11-" + name + ".json")).open("w") as output:
            subprocess.run(command, env=env, check=True, stdout=output, timeout=90)
with tempfile.TemporaryDirectory(prefix="gatehaven-wayland-") as directory:
    root = Path(directory)
    root.chmod(0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": directory, "WAYLAND_DISPLAY": "gatehaven-test",
           "SDL_VIDEODRIVER": "wayland", "SDL_RENDER_DRIVER": "software"}
    with (root / "weston.log").open("w+") as log:
        compositor = subprocess.Popen(["weston", "--backend=headless-backend.so", "--use-pixman",
                                       "--socket=gatehaven-test", "--idle-time=0"],
                                      env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while not (root / "gatehaven-test").exists():
                if compositor.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Wayland compositor failed to become ready")
                time.sleep(0.05)
            subprocess.run([binary, "--self-test-display"], env=env, check=True, timeout=90)
            if args.output:
                for flag, name in (("--diagnostics", "diagnostics"), ("--benchmark-display", "rendering")):
                    command = [binary, flag] + (["10000", "30"] if flag == "--benchmark-display" else [])
                    with (args.output / ("wayland-" + name + ".json")).open("w") as output:
                        subprocess.run(command, env=env, check=True, stdout=output, timeout=90)
        except BaseException:
            log.flush(); log.seek(0); print(log.read())
            raise
        finally:
            compositor.terminate()
            try: compositor.wait(timeout=5)
            except subprocess.TimeoutExpired:
                compositor.kill(); compositor.wait(timeout=5)
print("X11 and Wayland virtual display checks passed; physical display testing remains pending")
