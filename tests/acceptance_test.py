"""Acceptance collection must not invent manual passes or overwrite existing evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("acceptance", Path(__file__).resolve().parents[1] / "tools/acceptance.py")
acceptance = importlib.util.module_from_spec(spec); spec.loader.exec_module(acceptance)


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.app = self.root / "gatehaven"; self.app.write_bytes(b"inert executable fixture")
        self.manual = self.root / "manual.html"; self.manual.write_text("guide")
        self.calls = []

    def runner(self, app, arguments, env, timeout=60):
        self.calls.append(arguments)
        mode = arguments[0]
        diagnostics = {"schema": 1, "product": "Gatehaven", "build_info": "Gatehaven test\n", "source_revision": "a" * 40,
                       "display": {"video_driver": env.get("SDL_VIDEODRIVER", "native")}}
        output = {"--build-info": "Gatehaven test\n", "--diagnostics": json.dumps(diagnostics),
                  "--manual-path": str(self.manual), "--self-test": "passed", "--self-test-display": "passed"}
        if mode.startswith("--benchmark"):
            value = json.dumps({"source_revision": "a" * 40, "native_display_requested": mode == "--benchmark-display"})
        else: value = output[mode]
        return {"arguments": arguments, "status": "passed", "exit_code": 0, "stdout": value, "stderr": "", "seconds": 0}

    def test_headless_success_still_leaves_every_manual_check_pending(self):
        output = self.root / "results"
        report = acceptance.collect(self.app, output, headless=True, runner=self.runner)
        self.assertEqual(report["automated_status"], "passed")
        self.assertFalse(report["release_acceptance_complete"])
        self.assertTrue(all(item["status"] == "pending" for item in report["manual"]))
        self.assertEqual(self.app.read_bytes(), b"inert executable fixture")
        self.assertTrue((output / "practice-circuit-é.ghv").exists())
        self.assertEqual(json.loads((output / "evidence.json").read_text(encoding="utf-8")), report)
        acceptance.validate_manual(report)

    def test_failed_command_remains_in_report(self):
        def fail(app, args, env, **kwargs):
            result = self.runner(app, args, env, **kwargs)
            if args[0] == "--self-test": result.update(status="failed", exit_code=1, stderr="test failure")
            return result
        report = acceptance.collect(self.app, self.root / "results", headless=True, runner=fail)
        self.assertEqual(report["automated_status"], "failed")
        self.assertEqual(report["automated"][2]["stderr"], "test failure")

    def test_malformed_or_mismatched_identity_cannot_pass(self):
        for value in ("not json", "{}", '[]', '{"schema":1,"product":"other"}'):
            with self.subTest(value=value):
                def invalid(app, args, env, **kwargs):
                    result = self.runner(app, args, env, **kwargs)
                    if args[0] == "--diagnostics": result["stdout"] = value
                    return result
                report = acceptance.collect(self.app, self.root / ("result-" + str(len(self.calls))), headless=True, runner=invalid)
                self.assertEqual(report["automated_status"], "failed")

    def test_native_request_cannot_accept_dummy_display(self):
        def dummy(app, args, env, **kwargs): return self.runner(app, args, {**env, "SDL_VIDEODRIVER": "dummy"}, **kwargs)
        report = acceptance.collect(self.app, self.root / "results", runner=dummy)
        self.assertEqual(report["automated_status"], "failed")

    def test_existing_output_and_wrong_package_hash_do_not_run_the_app(self):
        output = self.root / "existing"; output.mkdir()
        marker = output / "keep"; marker.write_bytes(b"keep")
        with self.assertRaises(ValueError): acceptance.collect(self.app, output, runner=self.runner)
        with self.assertRaises(ValueError):
            acceptance.collect(self.app, self.root / "new", package=self.app, expected_sha256="0" * 64, runner=self.runner)
        self.assertEqual(self.calls, []); self.assertEqual(marker.read_bytes(), b"keep")
        self.assertFalse((self.root / "new").exists())

    def test_report_never_writes_into_mac_bundle(self):
        app = self.root / "Gatehaven.app/Contents/MacOS/gatehaven"
        app.parent.mkdir(parents=True); app.write_bytes(b"fixture")
        with self.assertRaises(ValueError): acceptance.collect(app.parents[2], app.parent / "results", runner=self.runner)
        self.assertEqual(self.calls, [])

    def test_browser_data_cannot_close_its_script_element(self):
        output = self.root / "results"
        report = acceptance.collect(self.app, output, headless=True, runner=self.runner)
        report["build_info"] = '</script><script>unsafe()</script>&'
        acceptance.write_html(report, output)
        page = (output / "report.html").read_text(encoding="utf-8")
        self.assertNotIn(report["build_info"], page)
        self.assertIn("\\u003c/script\\u003e", page)

    def test_manual_failures_need_notes_and_cannot_certify_release(self):
        report = acceptance.collect(self.app, self.root / "results", headless=True, runner=self.runner)
        for status in ("failed", "blocked", "not_applicable"):
            modified = copy.deepcopy(report); modified["manual"][0]["status"] = status
            with self.assertRaises(ValueError): acceptance.validate_manual(modified)
            modified["manual"][0]["notes"] = "Observed on this setup"; acceptance.validate_manual(modified)
        report["release_acceptance_complete"] = True
        with self.assertRaises(ValueError): acceptance.validate_manual(report)


if __name__ == "__main__": unittest.main()
