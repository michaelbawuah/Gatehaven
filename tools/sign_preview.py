#!/usr/bin/env python3
"""Prepare or sign a staged Gatehaven preview using existing OS credential stores.

The default prints a plan. --execute uses the native platform's real verification
tools and creates a new output directory only after every gate succeeds.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
import zipfile


def manifest(root):
    return {str(p.relative_to(root)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob("*")) if p.is_file()}


def inspect_stage(stage, target):
    if not stage.is_dir() or stage.is_symlink():
        raise ValueError("stage must be an installed directory")
    for path in stage.rglob("*"):
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise ValueError("stage must contain only regular files and directories")
    resources = stage / "share/gatehaven"
    metadata = json.loads((resources / "build-metadata.json").read_text(encoding="utf-8"))
    expected_system = "Windows" if target == "windows" else "Darwin"
    if (metadata.get("schema") != 1 or metadata.get("product") != "Gatehaven"
            or metadata.get("target", {}).get("system") != expected_system
            or not re.fullmatch(r"[0-9a-f]{40}", metadata.get("source_revision", ""))):
        raise ValueError("stage must identify a Gatehaven commit and the requested native target")
    binaries = ["bin/gatehaven-cli.exe", "bin/gatehaven.exe"] if target == "windows" else [
        "bin/gatehaven-cli", "gatehaven.app/Contents/MacOS/gatehaven"]
    for relative in binaries + ["share/gatehaven/docs/manual.html", "share/gatehaven/third_party/SDL3/LICENSE.txt"]:
        if not (stage / relative).is_file() or not (stage / relative).stat().st_size:
            raise ValueError("missing installed resource: " + relative)
    if target == "windows":
        if {p.relative_to(stage).as_posix().lower() for p in stage.rglob("*") if p.suffix.lower() == ".exe"} != set(binaries):
            raise ValueError("unexpected executable in the portable Windows stage")
    else:
        app_metadata = json.loads((stage / "gatehaven.app/Contents/Resources/build-metadata.json").read_text(encoding="utf-8"))
        if app_metadata != metadata:
            raise ValueError("app and CLI metadata differ")
        if any(p.suffix.lower() in (".dylib", ".so", ".framework") for p in stage.rglob("*")):
            raise ValueError("this workflow supports the static SDL bundle layout only")
    return metadata


def validate_options(target, identity, timestamp, profile):
    if target == "windows":
        if not re.fullmatch(r"[0-9a-fA-F]{40}", identity):
            raise ValueError("Windows identity must be the certificate's 40-hex thumbprint")
        url = urlsplit(timestamp or "")
        if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password:
            raise ValueError("supply an RFC 3161 timestamp URL without credentials")
    elif not identity.startswith("Developer ID Application:") or not profile:
        raise ValueError("macOS requires a Developer ID Application identity and an existing notary keychain profile")


class Runner:
    def __init__(self):
        self.checks = []

    def __call__(self, *args, timeout=300):
        command = list(map(str, args))
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        self.checks.append({"command": command, "exit_code": result.returncode,
                            "stdout": result.stdout, "stderr": result.stderr})
        # SignTool's warning exit code is deliberately a failure, including a missing timestamp.
        if result.returncode:
            raise RuntimeError(f"Verification/signing command failed ({result.returncode}): {command[0]}\n{result.stderr}\n{result.stdout}")
        return result.stdout


def windows_package(stage, delivery, identity, timestamp, signtool, run):
    for name in ("gatehaven.exe", "gatehaven-cli.exe"):
        binary = stage / "bin" / name
        run(signtool, "sign", "/s", "My", "/sha1", identity, "/fd", "SHA256", "/tr", timestamp,
            "/td", "SHA256", "/d", "Gatehaven", binary)
        run(signtool, "verify", "/pa", "/all", "/tw", binary)
    for dll in sorted(p for p in stage.rglob("*") if p.suffix.lower() == ".dll"):
        # Keep Microsoft's redistributable signatures; never re-sign third-party DLLs.
        run(signtool, "verify", "/pa", "/all", "/tw", dll)
    artifact = delivery / "Gatehaven-Windows-signed-preview.zip"
    with zipfile.ZipFile(artifact, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(stage.rglob("*")):
            if path.is_file(): archive.write(path, path.relative_to(stage))
    return artifact, []


def notarize(artifact, profile, run):
    result = json.loads(run("xcrun", "notarytool", "submit", artifact, "--keychain-profile", profile,
                           "--wait", "--output-format", "json", timeout=1800))
    if result.get("status") != "Accepted" or not result.get("id"):
        raise RuntimeError("Apple did not accept the notarization submission: " + json.dumps(result))
    return {"artifact": artifact.name, "id": result["id"], "status": result["status"]}


def macos_package(stage, delivery, identity, profile, run):
    app = stage / "gatehaven.app"
    for binary in (stage / "bin/gatehaven-cli", app):
        run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", identity, binary)
        run("codesign", "--verify", "--deep", "--strict", "--verbose=2", binary)
    submission = stage.parent / "notary-submission.zip"
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", stage, submission)
    receipts = [notarize(submission, profile, run)]
    run("xcrun", "stapler", "staple", app)
    run("xcrun", "stapler", "validate", app)
    run("spctl", "--assess", "--type", "execute", "--verbose=2", app)
    artifact = delivery / "Gatehaven-macOS-signed-preview.dmg"
    run("hdiutil", "create", "-volname", "Gatehaven", "-srcfolder", stage, "-format", "UDZO", artifact)
    run("codesign", "--force", "--timestamp", "--sign", identity, artifact)
    receipts.append(notarize(artifact, profile, run))
    run("xcrun", "stapler", "staple", artifact)
    run("xcrun", "stapler", "validate", artifact)
    run("codesign", "--verify", "--strict", "--verbose=2", artifact)
    run("spctl", "--assess", "--type", "open", "--context", "context:primary-signature", "--verbose=2", artifact)
    run("hdiutil", "verify", artifact)
    return artifact, receipts


def execute(stage, output, target, identity, timestamp=None, profile=None, signtool="signtool", run=None):
    validate_options(target, identity, timestamp, profile)
    if sys.platform != ("win32" if target == "windows" else "darwin"):
        raise ValueError("execute signing on the matching native OS")
    if output.exists() or not output.parent.is_dir():
        raise ValueError("output must be a new directory inside an existing parent")
    metadata = inspect_stage(stage, target)
    before = manifest(stage)
    run = run or Runner()
    with tempfile.TemporaryDirectory(prefix="gatehaven-sign-", dir=output.parent) as temporary:
        work = Path(temporary)
        copy = work / "stage"
        shutil.copytree(stage, copy)
        if manifest(copy) != before: raise ValueError("stage changed while it was being copied")
        delivery = work / "delivery"; delivery.mkdir()
        if target == "windows":
            artifact, receipts = windows_package(copy, delivery, identity, timestamp, signtool, run)
        else:
            artifact, receipts = macos_package(copy, delivery, identity, profile, run)
        if manifest(stage) != before: raise ValueError("original stage changed during signing")
        digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
        report = {"schema": 1, "kind": "signed-preview", "release_acceptance_complete": False,
                  "artifact": artifact.name, "sha256": digest, "build_metadata": metadata,
                  "signing_identity": identity, "notarization": receipts, "checks": run.checks,
                  "input_files": before, "signed_files": manifest(copy)}
        (delivery / (artifact.name + ".sha256")).write_text(f"{digest}  {artifact.name}\n", encoding="utf-8")
        (delivery / "signing-evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        # mkdir is exclusive. Never replace an existing output, even an empty one.
        output.mkdir()
        try:
            for item in delivery.iterdir(): shutil.move(str(item), output / item.name)
        except BaseException:
            shutil.rmtree(output)
            raise
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=("windows", "macos"))
    parser.add_argument("stage", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--identity", required=True)
    parser.add_argument("--timestamp-url")
    parser.add_argument("--notary-profile")
    parser.add_argument("--signtool", default="signtool")
    parser.add_argument("--execute", action="store_true", help="perform real signing and verification on the native OS")
    args = parser.parse_args()
    try:
        validate_options(args.target, args.identity, args.timestamp_url, args.notary_profile)
        metadata = inspect_stage(args.stage, args.target)
        if args.output.exists(): raise ValueError("output directory already exists")
        if args.execute:
            report = execute(args.stage.resolve(), args.output.absolute(), args.target, args.identity,
                             args.timestamp_url, args.notary_profile, args.signtool)
            print(json.dumps({"output": str(args.output), "artifact": report["artifact"], "sha256": report["sha256"]}, indent=2))
        else:
            print(json.dumps({"mode": "plan-only", "target": args.target, "stage": str(args.stage.resolve()),
                              "output": str(args.output.absolute()), "build_metadata": metadata,
                              "identity": args.identity, "timestamp_url": args.timestamp_url,
                              "notary_profile": args.notary_profile,
                              "steps": ["Copy the installed stage; preserve original files",
                                        "Sign app and CLI using existing OS credentials",
                                        "Verify signatures and timestamps" if args.target == "windows" else
                                        "Require Apple acceptance; staple and assess app, then DMG",
                                        "Write signed preview, checksum, command evidence and file hashes"],
                              "release_acceptance_complete": False}, indent=2))
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__": main()
