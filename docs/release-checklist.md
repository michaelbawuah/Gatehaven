# Release acceptance checklist

Use the exact candidate commit and archive SHA-256 when recording a result.
Automated native builds and a developer workstation do not substitute for the
manual checks below. A final release requires evidence for each supported target.

| Area | Current evidence | Remaining acceptance |
| --- | --- | --- |
| Core semantics | Parameterized rules, traversal differential tests, external observations | Resolve any newly discovered brief/reference conflicts explicitly |
| Files | Versioned native/clipboard codecs, six exact legacy round trips, bounded malformed-input checks | Additional user circuits and long save/reopen sessions |
| External behavior | 860 dormant/active-screen observations; live byte streams, EOF/reload/order; explicit differences | Additional user circuits, physical interactions, acceptance of listed differences |
| Performance | Repeated dormant/active input timings, verified live I/O, software render/culling measurements | Native GPU rendering, physical input latency and slow-storage behavior |
| Native builds | Core and SDL jobs for Linux x64/ARM64, macOS Intel/ARM64, Windows x64/ARM64 | Exact candidate run must pass; confirm release compiler/OS choices |
| Packaging | Archives/installers and hashes; relocated Mac bundle; Windows reinstall/data preservation; fresh Ubuntu container lifecycle jobs | Exact candidate jobs must pass; install/open/upgrade/uninstall on clean machines |
| Displays | Dummy rendering/coordinate conversion; X11 and Wayland virtual-display CI checks | Confirm candidate display jobs; physical HiDPI/per-monitor moves, resize and minimize |
| Input | Mouse buttons, synthetic multitouch, keyboard cursor, focused-control/window text and saved high contrast | Physical touch, keyboard layouts, screen readers, full contrast/zoom review |
| Native dialogs | Callback cancellation/error/lifetime tests; selection rules | Actual dialogs, Unicode paths, cancel, shutdown while pending on each OS |
| Signing | Credentialed workflow prepared with failure-path tests; ordinary CI previews remain unsigned | Configure identities; run [signing](signing.md), verify downloaded artifacts on clean machines |
| Recovery | Process-kill, ownership, deletion and failed-replacement checks | Real interruption and restore/save workflows on each target |
| Distribution | Dependency notices and reproducible dependency pins | Decide the application license and final release/update policy |

## Manual test record

Use the packaged [desktop acceptance collector](acceptance.md) to capture the
binary identity, display diagnostics and automatic checks, then record each
hands-on result in its offline report. Unperformed checks stay pending.

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
   Use Tab/F8 to describe controls, F9 to navigate cells, F11 to toggle contrast,
   and F12 to read the window summary. Reopen and confirm the contrast preference.
   Review crossings with only one powered axis, high zoom, and screen-reader behavior.
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
