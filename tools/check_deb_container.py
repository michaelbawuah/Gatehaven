#!/usr/bin/env python3
"""Install a preview DEB in a fresh Ubuntu container and preserve the lifecycle evidence."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("packages", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--image", default="ubuntu:24.04")
args = parser.parse_args()
packages = sorted(args.packages.resolve().glob("gatehaven_*.deb"))
if len(packages) != 1: parser.error("expected exactly one Gatehaven DEB")
repo = Path(__file__).resolve().parents[1]
subprocess.run(["docker", "pull", args.image], check=True, timeout=180)
image = json.loads(subprocess.run(["docker", "image", "inspect", args.image], check=True,
                                  capture_output=True, text=True, timeout=30).stdout)[0]
with tempfile.TemporaryDirectory(prefix="gatehaven-container-evidence-") as directory:
    # Install tools only inside the disposable container. Host folders are mounted read-only.
    script = ('apt-get -qq update && apt-get -y -qq install python3 desktop-file-utils shared-mime-info '
              '&& python3 /suite/tests/deb_lifecycle_test.py "$1" /evidence/result.json')
    subprocess.run(["docker", "run", "--rm", "--network=bridge",
                    "-e", "DEBIAN_FRONTEND=noninteractive", "-e", "GATEHAVEN_DISPOSABLE_TEST=1",
                    "--mount", f"type=bind,source={repo},target=/suite,readonly",
                    "--mount", f"type=bind,source={packages[0].parent},target=/packages,readonly",
                    "--mount", f"type=bind,source={directory},target=/evidence",
                    image["Id"], "bash", "-c", script, "gatehaven-package-test", "/packages/" + packages[0].name],
                   check=True, timeout=360)
    evidence = json.loads((Path(directory) / "result.json").read_text())
    evidence["container_image"] = {"requested": args.image, "id": image["Id"], "digests": image.get("RepoDigests", [])}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evidence, indent=2) + "\n")
