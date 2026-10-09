# Release acceptance checklist

Use the exact candidate commit and archive SHA-256 when recording a result.
Automated native builds and a developer workstation do not substitute for the
manual checks below. A final release requires evidence for each supported target.

| Area | Current evidence | Remaining acceptance |
| --- | --- | --- |
| Core semantics | Parameterized rules, traversal differential tests, external observations | Resolve any newly discovered brief/reference conflicts explicitly |
| Files | Versioned native/clipboard codecs, six exact legacy round trips, bounded malformed-input checks | Additional user circuits and long save/reopen sessions |
| External behavior | All six reference samples; 100 seeded circuits; known Source/Signal difference asserted | Attached input/output streams and user-driven screen interactions |
| Performance | Repeated same-host compilation and headless tick timings | Rendering, continuously changing circuits, active file I/O and interactive latency |
| Native builds | Core and SDL jobs for Linux x64/ARM64, macOS Intel/ARM64, Windows x64/ARM64 | Exact candidate run must pass; confirm release compiler/OS choices |
| Packaging | Archives, DEB, DMG, Windows x64 NSIS, Linux AppImage, hashes and relocation checks | Install/open/upgrade/uninstall on clean machines |
| Displays | Dummy-driver rendering and coordinate conversion at multiple sizes | Real HiDPI/per-monitor moves, resize, minimize, X11 and Wayland |
| Input | Mouse buttons, synthetic multitouch, keyboard cursor and native text inspection | Physical touch, keyboard layouts, screen readers, contrast/zoom review |
| Native dialogs | Callback cancellation/error/lifetime tests; selection rules | Actual dialogs, Unicode paths, cancel, shutdown while pending on each OS |
| Signing | Preview artifacts are deliberately marked unsigned | Configure release identities; sign/verify Windows and sign/notarize/staple macOS |
| Recovery | Process-kill, ownership, deletion and failed-replacement checks | Real interruption and restore/save workflows on each target |
| Distribution | Dependency notices and reproducible dependency pins | Decide the application license and final release/update policy |

## Manual test record

Record the OS version, CPU architecture, graphics backend, scaling, device,
compiler, source revision, artifact hash, scenario, result, and reproduction steps
for failures. Leave untested scenarios explicitly pending.

1. Install/extract in a clean profile; run the app and CLI from outside the install
   directory. Open offline help and all six lessons.
2. Open `.ghv` and `.ccsb` by dialog, drag/drop, and registered Open With action.
   Use spaces and non-ASCII filenames. Save As each format, close, reopen, and
   check layout/levels and legacy-origin translation.
3. Edit with mouse, keyboard cursor, and touch. Copy between two instances, close
   one, and continue in the survivor. Check undo/redo and clipboard transforms.
4. Move between monitors, resize, minimize/restore, and test one- and two-finger
   gestures. Confirm no stuck drawing or screen holds after focus loss.
5. Cancel each native file dialog and close while a dialog is pending. Confirm
   circuits do not choose communicator paths; connect explicit test files only.
6. Exercise active serial input/output, reopen changed files, merge/split groups,
   reset without rewinding, and verify final bytes against an independent peer.
7. Terminate an unsaved instance, restore it, save it, and ensure active windows
   never appear as abandoned recoveries. Decline overwrite of an externally
   modified document and confirm both versions survive.
8. Verify signatures using the platform's ordinary checks, then install/update/
   uninstall. Preserve circuits and user data, existing default associations,
   and operating-system protections throughout.

Release credentials and physical devices are not part of the automated preview
pipeline. Do not label a preview signed, notarized, universally portable, or fully
accessible until the corresponding acceptance evidence exists.
