#!/usr/bin/env python3
"""Collect isolated Gatehaven checks and create an offline, manually completed test report."""
import argparse
from datetime import datetime, timezone
import hashlib
import html
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


SCENARIOS = [
    ("first_launch", "Launch and learn", "Launch the installed game normally. Open F1 help and all six F3 lessons. Confirm readable text and a usable window."),
    ("files", "Open, save and reopen", "Open practice-circuit-é.ghv from this report folder. Save As .ghv and .ccsb using new names with spaces and accents. Close and reopen each; check layout and power. Try drag/drop and Open With too."),
    ("editing", "Build and edit", "Draw and erase, make a selection, move/rotate/flip it, then undo and redo. Use F9 and arrows to navigate; Enter applies a tool. Check F10 stepping and F8 inspection."),
    ("windows", "Work across windows", "Use New to open another window. Copy a selection between windows, close one, and keep editing in the survivor. Check the clipboard chooser with Ctrl/Cmd+Shift+V."),
    ("display", "Check the display", "Resize, minimize and restore. Zoom and pan, then move between monitors if available. Record scaling and monitor setup in your notes. Check for clipped text, blur, flicker and stuck input."),
    ("touch", "Try touch input", "On a touchscreen, draw with one finger, then pan/pinch with two. Lift and resume drawing. If this machine has no touchscreen, choose Not applicable and say so; touch acceptance stays open for other devices."),
    ("accessibility", "Read and navigate", "Tab through controls, use F8 descriptions, F9 canvas navigation and F12 summary. Toggle F11 contrast, restart, and confirm it persists. Test your screen reader and record its name/version and any unreachable controls."),
    ("dialogs", "Cancel file dialogs", "Open and cancel each file dialog. Close a test window while a dialog is pending. Check spaces/accented paths. Choose communicator files explicitly; never use important files for output tests."),
    ("file_ports", "Exercise file communication", "Follow the installed communicator guide with disposable input/output files. Check byte order, EOF, changed-file reload, group merge/split and reset without rewinding."),
    ("recovery", "Recover a test circuit", "Use a disposable unsaved circuit to test interruption and recovery. Save restored work. Open one file in two windows, change it in one, then decline overwrite in the other; confirm both versions survive."),
    ("installation", "Check installation and trust", "On the intended clean-machine/profile setup, verify the downloaded artifact's platform signature, installation, Open With, replacement and uninstall. Preserve user files and existing default handlers. Record any blocked or untested part."),
    ("playthrough", "Complete a playthrough", "Build a circuit of your own and use it for at least 20 minutes. Save, close and reopen it. Record confusing controls, crashes, performance problems and whether you would accept this build.")
]
STATUSES = ("pending", "passed", "failed", "blocked", "not_applicable")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""): digest.update(chunk)
    return digest.hexdigest()


def application_path(path):
    path = path.expanduser().resolve()
    if path.is_dir():
        candidates = [path / "Contents/MacOS/gatehaven", path / "gatehaven.app/Contents/MacOS/gatehaven",
                      path / "bin/gatehaven.exe", path / "bin/gatehaven"]
        found = [p for p in candidates if p.is_file()]
        if len(found) != 1: raise ValueError("Choose one installed Gatehaven executable, .app, or installation folder")
        path = found[0]
    if not path.is_file(): raise ValueError("Gatehaven executable not found")
    return path


def run_command(app, arguments, env, timeout=60):
    start = time.monotonic()
    try:
        result = subprocess.run([str(app), *arguments], env=env, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=timeout)
        return {"arguments": arguments, "status": "passed" if result.returncode == 0 else "failed",
                "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr,
                "seconds": round(time.monotonic() - start, 3)}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"arguments": arguments, "status": "failed", "exit_code": None, "stdout": "",
                "stderr": str(error), "seconds": round(time.monotonic() - start, 3)}


def read_json_result(check):
    if check["status"] != "passed": return None
    try:
        value = json.loads(check["stdout"])
        if not isinstance(value, dict): raise ValueError("report must be an object")
        return value
    except (ValueError, TypeError) as error:
        check["status"] = "failed"; check["stderr"] += "\nInvalid JSON report: " + str(error)
        return None


def validate_manual(report):
    expected = [item[0] for item in SCENARIOS]
    manual = report.get("manual", [])
    if not isinstance(manual, list) or any(not isinstance(item, dict) for item in manual) or [item.get("id") for item in manual] != expected:
        raise ValueError("manual checklist does not match this version")
    for item in manual:
        if item.get("status") not in STATUSES or not isinstance(item.get("notes"), str):
            raise ValueError("invalid manual result")
        if item["status"] in ("failed", "blocked", "not_applicable") and not item["notes"].strip():
            raise ValueError("Add notes explaining failed, blocked or inapplicable checks")
    if report.get("release_acceptance_complete") is not False:
        raise ValueError("a single-machine report cannot certify a final release")


def write_html(report, output):
    validate_manual(report)
    template = Path(__file__).with_suffix(".html").read_text(encoding="utf-8")
    data = json.dumps(report, ensure_ascii=True).replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    cards = []
    for key, title, instructions in SCENARIOS:
        options = "".join(f'<option value="{status}">{status.replace("_", " ").capitalize()}</option>' for status in STATUSES)
        cards.append(f'<section class="card"><h2>{html.escape(title)}</h2><p>{html.escape(instructions)}</p>'
                     f'<label for="result-{key}">Result</label><select id="result-{key}" data-id="{key}">{options}</select>'
                     f'<label for="notes-{key}">Observations and setup</label><textarea id="notes-{key}" rows="3"></textarea></section>')
    text = template.replace("__REPORT_DATA__", data).replace("__SCENARIO_CARDS__", "\n".join(cards))
    (output / "report.html").write_text(text, encoding="utf-8")


def collect(app, output, headless=False, benchmark=True, package=None, expected_sha256=None, runner=run_command):
    app = application_path(app)
    output = output.expanduser().absolute()
    if output.exists(): raise ValueError("Choose a new report folder; existing files are never replaced")
    if not output.parent.is_dir(): raise ValueError("The report folder's parent must already exist")
    for parent in app.parents:
        if parent.suffix.lower() == ".app" and output.resolve().is_relative_to(parent):
            raise ValueError("Save reports outside the application bundle")
    package_info = None
    if expected_sha256 and not package: raise ValueError("--expected-sha256 needs --package")
    if package:
        digest = sha256(package)
        if expected_sha256 and digest.lower() != expected_sha256.lower(): raise ValueError("Package checksum does not match")
        package_info = {"name": package.name, "sha256": digest, "expected_checksum_matched": bool(expected_sha256)}
    report = {"schema": 1, "checklist_version": 1, "product": "Gatehaven", "collected_at": datetime.now(timezone.utc).isoformat(),
              "mode": "headless" if headless else "native-session", "candidate": {"binary_name": app.name, "binary_sha256": sha256(app),
              "package": package_info}, "machine": {"system": platform.system(), "release": platform.release(),
              "version": platform.version(), "architecture": platform.machine()}, "automated": [], "manual": [
                  {"id": key, "title": title, "status": "pending", "notes": ""} for key, title, _ in SCENARIOS],
              "tester_notes": "", "release_acceptance_complete": False,
              "limits": ["Manual results are tester observations, not automated certification",
                         "Native backends may run on virtual or remote displays; physical setup must be recorded",
                         "This collector does not install, uninstall, sign, or change operating-system protections"]}
    env = dict(os.environ)
    if headless: env.update(SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software")
    checks = report["automated"]
    checks.append(runner(app, ["--build-info"], env)); report["build_info"] = checks[-1]["stdout"]
    checks.append(runner(app, ["--diagnostics"], env)); report["diagnostics"] = read_json_result(checks[-1])
    diagnostics = report["diagnostics"]
    valid_diagnostics = (isinstance(diagnostics, dict) and diagnostics.get("schema") == 1
                         and diagnostics.get("product") == "Gatehaven" and isinstance(diagnostics.get("display"), dict)
                         and isinstance(diagnostics.get("source_revision"), str) and bool(diagnostics["source_revision"])
                         and diagnostics.get("build_info") == report["build_info"] and report["build_info"].startswith("Gatehaven "))
    if diagnostics is not None and not valid_diagnostics:
        checks[-1]["status"] = "failed"; checks[-1]["stderr"] += "\nDiagnostics and build identity differ"
    if valid_diagnostics:
        report["candidate"]["source_revision"] = diagnostics.get("source_revision")
        if not headless and diagnostics.get("display", {}).get("video_driver") in ("dummy", "offscreen"):
            checks[-1]["status"] = "failed"; checks[-1]["stderr"] += "\nA native session cannot use a headless video driver"
    checks.append(runner(app, ["--self-test" if headless else "--self-test-display"], env))
    checks.append(runner(app, ["--manual-path"], env))
    if checks[-1]["status"] == "passed" and not Path(checks[-1]["stdout"].strip()).is_file():
        checks[-1]["status"] = "failed"; checks[-1]["stderr"] += "\nInstalled manual was not found at the reported path"
    if benchmark:
        checks.append(runner(app, ["--benchmark-render" if headless else "--benchmark-display", "10000", "30"], env, timeout=90))
        report["rendering"] = read_json_result(checks[-1])
        rendering = report["rendering"]
        if rendering is not None and (not valid_diagnostics or rendering.get("source_revision") != report["candidate"].get("source_revision")
                                      or rendering.get("native_display_requested") is not (not headless)):
            checks[-1]["status"] = "failed"; checks[-1]["stderr"] += "\nRendering report identity or mode mismatch"
    if sha256(app) != report["candidate"]["binary_sha256"]:
        checks.append({"arguments": [], "status": "failed", "stderr": "Application changed during collection"})
    report["automated_status"] = "passed" if all(check["status"] == "passed" for check in checks) else "failed"
    report["benchmark_collected"] = benchmark
    output.mkdir()
    (output / "practice-circuit-é.ghv").write_text(
        "GATEHAVEN 2\n-2 0 source 0\n-1 0 wire 0\n0 0 wire 0\n1 0 wire 0\n1 1 signal 0\n1 2 or 3\n2 2 wire 0\n", encoding="utf-8")
    (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_html(report, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("application", type=Path, help="installed executable, macOS .app, or installation directory")
    parser.add_argument("output", type=Path, help="new folder for evidence, checklist and a disposable circuit")
    parser.add_argument("--headless", action="store_true", help="explicit dummy-display checks for CI; never physical acceptance")
    parser.add_argument("--skip-benchmark", action="store_true")
    parser.add_argument("--package", type=Path, help="optional original downloaded package to hash")
    parser.add_argument("--expected-sha256", help="expected hash for --package, copied from a trusted checksum")
    args = parser.parse_args()
    try:
        report = collect(args.application, args.output, args.headless, not args.skip_benchmark, args.package, args.expected_sha256)
        print(f"Automated checks: {report['automated_status']}. Manual checks: {len(SCENARIOS)} pending.")
        print("Open " + str((args.output / "report.html").absolute()))
        return 0 if report["automated_status"] == "passed" else 1
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__": sys.exit(main())
