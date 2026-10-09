"""Verify signing failure boundaries without credentials or native signing tools."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location("sign_preview", Path(__file__).resolve().parents[1] / "tools/sign_preview.py")
sign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sign)


def fixture(root, target):
    metadata = {"schema": 1, "product": "Gatehaven", "source_revision": "a" * 40,
                "target": {"system": "Windows" if target == "windows" else "Darwin"}, "signed_release": False}
    paths = ["share/gatehaven/docs/manual.html", "share/gatehaven/third_party/SDL3/LICENSE.txt"]
    paths += ["bin/gatehaven.exe", "bin/gatehaven-cli.exe", "bin/runtime.dll"] if target == "windows" else [
        "bin/gatehaven-cli", "gatehaven.app/Contents/MacOS/gatehaven"]
    for relative in paths:
        path = root / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"inert fixture")
    for relative in ["share/gatehaven/build-metadata.json"] + ([] if target == "windows" else [
            "gatehaven.app/Contents/Resources/build-metadata.json"]):
        path = root / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(json.dumps(metadata))


class FakeRunner:
    def __init__(self, reject_submission=0, fail_verify=False):
        self.checks = []
        self.submissions = 0
        self.reject_submission = reject_submission
        self.fail_verify = fail_verify

    def __call__(self, *args, **kwargs):
        args = list(map(str, args)); self.checks.append({"command": args, "exit_code": 0})
        if self.fail_verify and ("verify" in args or "--verify" in args): raise RuntimeError("signature failure")
        if args[0] == "signtool" and args[1] == "sign":
            path = Path(args[-1]); path.write_bytes(path.read_bytes() + b" signed")
        if args[:3] == ["xcrun", "notarytool", "submit"]:
            self.submissions += 1
            return json.dumps({"status": "Invalid" if self.submissions == self.reject_submission else "Accepted",
                               "id": str(self.submissions)})
        if args[0] == "ditto" or args[:2] == ["hdiutil", "create"]: Path(args[-1]).write_bytes(b"inert signed artifact")
        return ""


class SigningTests(unittest.TestCase):
    def test_windows_verifies_every_binary_before_archiving(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage with spaces"; fixture(stage, "windows")
            before = sign.manifest(stage); output = Path(directory) / "signed"
            runner = FakeRunner()
            with patch.object(sign.sys, "platform", "win32"):
                report = sign.execute(stage, output, "windows", "b" * 40, "https://timestamp.example", run=runner)
            self.assertEqual(sign.manifest(stage), before)
            self.assertNotEqual(report["signed_files"]["bin/gatehaven.exe"], before["bin/gatehaven.exe"])
            commands = [entry["command"] for entry in runner.checks]
            self.assertEqual([command[1] for command in commands], ["sign", "verify", "sign", "verify", "verify"])
            self.assertTrue(all("/tw" in command for command in commands if command[1] == "verify"))
            self.assertTrue(all("/tr" in command and "/td" in command and "/fd" in command
                                for command in commands if command[1] == "sign"))
            with zipfile.ZipFile(output / report["artifact"]) as archive:
                self.assertEqual(set(archive.namelist()), set(before))
                self.assertEqual(archive.read("bin/runtime.dll"), b"inert fixture")
            self.assertFalse(report["release_acceptance_complete"])

    def test_failed_verification_never_publishes(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"; fixture(stage, "windows")
            before = sign.manifest(stage); output = Path(directory) / "signed"
            with patch.object(sign.sys, "platform", "win32"), self.assertRaises(RuntimeError):
                sign.execute(stage, output, "windows", "b" * 40, "https://timestamp.example", run=FakeRunner(fail_verify=True))
            self.assertFalse(output.exists()); self.assertEqual(sign.manifest(stage), before)

    def test_mac_requires_acceptance_for_app_and_final_dmg(self):
        for rejected in (1, 2, 0):
            with self.subTest(rejected=rejected), tempfile.TemporaryDirectory() as directory:
                stage = Path(directory) / "stage"; fixture(stage, "macos")
                output = Path(directory) / "signed"; runner = FakeRunner(reject_submission=rejected)
                before = sign.manifest(stage)
                with patch.object(sign.sys, "platform", "darwin"):
                    if rejected:
                        with self.assertRaises(RuntimeError):
                            sign.execute(stage, output, "macos", "Developer ID Application: Test", profile="test-profile", run=runner)
                        self.assertFalse(output.exists())
                    else:
                        report = sign.execute(stage, output, "macos", "Developer ID Application: Test", profile="test-profile", run=runner)
                        self.assertEqual(len(report["notarization"]), 2)
                        commands = [entry["command"] for entry in runner.checks]
                        self.assertEqual(sum(command[:3] == ["xcrun", "stapler", "validate"] for command in commands), 2)
                        self.assertEqual(sum(command[:2] == ["spctl", "--assess"] for command in commands), 2)
                        self.assertTrue(all("--timestamp" in command for command in commands if "--sign" in command))
                self.assertEqual(sign.manifest(stage), before)

    def test_existing_output_is_never_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"; fixture(stage, "windows")
            output = Path(directory) / "signed"; output.mkdir()
            marker = output / "keep"; marker.write_bytes(b"existing")
            runner = FakeRunner()
            with patch.object(sign.sys, "platform", "win32"), self.assertRaises(ValueError):
                sign.execute(stage, output, "windows", "b" * 40, "https://timestamp.example", run=runner)
            self.assertEqual(marker.read_bytes(), b"existing"); self.assertEqual(runner.checks, [])

    def test_dirty_metadata_and_unexpected_executables_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory); fixture(stage, "windows")
            extra = stage / "extra.exe"; extra.write_bytes(b"unexpected")
            with self.assertRaises(ValueError): sign.inspect_stage(stage, "windows")
            extra.unlink()
            path = stage / "share/gatehaven/build-metadata.json"
            metadata = json.loads(path.read_text()); metadata["source_revision"] += "-dirty"
            path.write_text(json.dumps(metadata))
            with self.assertRaises(ValueError): sign.inspect_stage(stage, "windows")

    def test_options_cannot_select_an_arbitrary_certificate_or_embed_credentials(self):
        for identity, timestamp in [("", "https://timestamp.example"), ("a" * 40, "file:///tmp/server"),
                                    ("a" * 40, "https://secret:password@timestamp.example")]:
            with self.assertRaises(ValueError): sign.validate_options("windows", identity, timestamp, None)
        with self.assertRaises(ValueError): sign.validate_options("macos", "-", None, "profile")

    def test_native_tool_warnings_fail(self):
        runner = sign.Runner()
        result = subprocess.CompletedProcess(["signtool"], 2, "missing timestamp", "")
        with patch.object(sign.subprocess, "run", return_value=result), self.assertRaises(RuntimeError):
            runner("signtool", "verify", "/tw", "binary.exe")
        self.assertEqual(runner.checks[0]["exit_code"], 2)


if __name__ == "__main__": unittest.main()
