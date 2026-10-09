# Verification record

Initial prototype checks were performed on Linux with GCC 13.3.0, CMake 4.4.4,
Ninja 1.13.2, and SDL3 3.4.18. The local SDL build uses its software renderer and
dummy video driver. It cannot validate a physical monitor, touch input, or
operating-system file dialogs.

## Verified locally

- Core/platform tests run in the local debug build. Parameterized cases
  exercise all four gates with zero through four inputs and every active-input
  count, relay behavior, delayed rising/falling edges, crossing isolation,
  deterministic insertion order, and coordinate boundaries.
- Document tests cover malformed input, duplicate cells, coordinate overflow,
  resource limits, comments, CRLF, round trips, save replacement, and failed saves.
- Editor tests cover stroke transactions, undo/redo branching, memory limits,
  rotations, flips, sparse capture, paste overflow, and overlapping selection moves.
- Clipboard codec checks cover full coordinate extents, preserved empty borders,
  every truncated prefix and single-byte bit flip of a sample message, and
  structurally invalid messages with otherwise correct checksums.
- The cross-process clipboard test checks concurrent writers, numbered slots,
  default-slot caching, late joiners, live survivors after a peer is killed,
  normal last-member cleanup, restart after a crash, and damaged payload recovery.
- The desktop smoke test injects SDL keyboard and mouse events, then verifies
  play/pause, stepping, drawing, undo/redo, reset, custom speed validation,
  clipboard menus, shared copy/paste, stable placement previews, failure-safe cut,
  selection movement, five button bindings, touch-tagged pointer events,
  eyedropper sampling, rebound panning, and a rendered frame. New/Open requests
  are checked for document preservation, and a real child executable runs the
  headless smoke workflow independently.
- The expanded SDL workflow covers snapped polylines and retracing, connected
  selection, Shift-add/Alt-subtract selection, sparse duplication, example-menu
  navigation, screen holds/releases, own-pencil Signal placement, and dropped-file opens.
- Communicator checks cover adjacency groups, previous-tick sending, every byte
  value through both protocol decoding and a simulated file-output circuit,
  reserved commands, queue bounds, I/O failures, EOF resumption, merge/split file
  ownership, and reset without rewinding streams.
- CLI integration checks parse JSON statistics, CSV tick traces, SVG diagrams,
  six generated lessons, and failure exit codes from real files.
- Desktop CTest covers core/platform, CLI, cross-process clipboards, recovery
  processes, installed-product checks, and SDL events. The installed app runs
  from a fresh directory containing spaces and renders nine distinct UI states.
- A local CPack archive was generated and its SHA-256, executable locations,
  six lessons, manuals, and exact SDL notice bytes were verified. CI performs
  these checks before uploading native preview packages.
- A starter-circuit screenshot was rendered from the application and visually
  inspected. Small-font rasterization and connection drawing were corrected.
- AddressSanitizer and UndefinedBehaviorSanitizer checks run separately from the
  normal build. Local leak detection is unavailable because this environment
  blocks LeakSanitizer's `/proc` task inspection; the GitHub job keeps leak
  detection enabled so that check can run on a normal runner.
- The initial sanitizer suites passed with ASan and UBSan enabled. See the
  0.5 record below for the expanded current suites.

## Cross-platform milestone

[Run 37956417854](https://github.com/michaelbawuah/Gatehaven/actions/runs/37956417854)
passed all eight jobs on 9 October 2026 at commit `f2a48c4`: core/platform tests
on Linux, macOS, and Windows; desktop builds, event tests, child-process checks,
and screenshot rendering on all three; Linux ASan/UBSan including leak detection;
and the simulation benchmark. The macOS desktop compiler was AppleClang
21.0.0.21000101 and the Windows compiler was MSVC 19.51.36260.0.

This records a specific tested revision. For later changes, check their own
Actions run. Automated dummy-driver and touch-tagged events do not substitute
for physical monitor, touchscreen, or native file-dialog acceptance.

## Initial performance measurement

One release-build run of the wire-chain workload (10,000 cells, 20 timed steps,
one untimed warm-up) took 123.609 ms total, or 6.18046 ms per step. The benchmark
verifies that every cell is powered before reporting a result. This is a local
baseline, not a cross-machine score or proof of performance parity.
See [the repeated 0.2 measurements](performance.md) for chain, grid, gate, and
communicator workloads, including run-to-run ranges and remaining profiling work.

## Still to verify

- GitHub results for each current commit; see the Actions page rather than
  assuming a configured job has passed.
- Real desktop interaction and file-dialog behavior on Windows and macOS.
- Linux X11 and Wayland sessions, native dialogs, per-monitor scaling, and touch.
- File-dialog cancellation and shutdown on each supported operating system.
- Platform and compiler versions for release targets, including each CPU architecture.
- ThreadSanitizer for any future shared state or simulation workers.
- Native file associations, signing, and clean-machine installation. Automated
  package tests validate contents and execution on the build host only.
- Native GPU/physical latency and comprehensive acceptance against the remaining
  product brief; active protocol and software-render results are recorded below.

## 0.3 candidate verification

- Differential simulation compares seeded mixed circuits, edits, replacements,
  reset/invalidation, coordinate extremes, and copied feedback engines against
  Gatehaven's frozen earlier implementation. No external source is the oracle.
- Touch tests inject native finger events through SDL coordinate conversion at
  different window sizes, including pinch cancellation and focus loss.
- Recovery tests terminate a native writer, compete for one abandoned snapshot,
  reject malformed/linked files, and preserve data on a failed replacement.
- Keyboard traversal, modal help, Unicode CLI paths, profiling counters, and
  output limits have integration checks. DEB extraction and DMG read-only mount
  checks run before package upload.

Review the current Actions run before using a candidate: earlier green runs do
not validate later commits. At that milestone the remaining brief included `.ccsb` interoperability and
external timing comparisons, now covered within the 0.5 scope below. Real native
dialog/display/touch checks, release signatures, and clean-machine installation
remain open.
New gameplay modes remain deferred until those sandbox requirements are met.

## 0.4 candidate verification

- Editor regressions compare clipped strokes and indexed previews with complete
  edits, including crossings, retracing, sparse holes, transforms and limits.
- Save tests preserve externally changed files when replacement is declined.
  Recovery tests cover deletion confirmation and active-document preservation.
- CLI digests are checked against an independent Python byte encoder. Native
  normalization tests verify BOM/CRLF input, ordering, atomic in-place saves and
  preservation after malformed input. These are Gatehaven format checks.
- Installed products resolve offline help after relocation and include matching
  compiler/architecture/dependency metadata. Windows x64 CI installs, launches,
  inspects its Open With registration, and uninstalls an actual NSIS preview.
- Windows ARM64 has native core and SDL desktop jobs. Static analysis currently
  covers circuit, topology, simulation, and statistics with the runner's Clang 18;
  broader C++23 library-dependent analysis remains follow-up work.

Configured jobs are not evidence until the exact candidate finishes successfully.
A local dummy SDL run does not validate native dialogs, touch hardware, display
scaling, signing, or the full clean-machine matrix.

## 0.5 local verification, 9 October 2026

Clean Linux x86-64 builds with GNU 13.3.0, CMake 4.4.4 and SDL 3.4.18 passed:

- **153/153 core/platform test cases**, including compiled nets, stateful codecs,
  dormant endpoint wake-up, diagnostic fingerprints and inspection.
- **9/9 desktop CTest suites**, including installed execution, ten UI snapshots,
  callbacks from a background thread, failed-open preservation, current-level
  selection movement, keyboard construction/interaction and coordinate limits.
- **9/9 Release suites**, including six simulation workloads and the large-editor
  benchmark invariants.
- **7/7 ASan/UBSan suites**. Leak detection is disabled only in this restricted
  local environment; CI still enables it on its runner.
- **Six byte-exact legacy round trips**, checked against an independent decoder.
- **430 matching external observations**, plus the explicitly asserted
  Source/Signal brief/reference difference. The checked-in reports identify
  input hashes, source tree, toolchain, and comparison limits.
- **Seven alternating runs per engine and sample** with 10,000 ticks each. All
  six compile/tick medians improved in that disconnected-endpoint scenario;
  settling explains the largest reductions. This is headless evidence only.
- **TGZ, DEB, and AppImage checks**, including exact hashes, metadata, extraction,
  relocation, desktop self-test, offline guides, legacy CLI and runtime baseline.
- Starter, shortcut help, and keyboard-canvas frames were visually inspected.
  These are software-rendered captures, not physical-display acceptance.

The records describe local source-tree content; GitHub produces new artifacts
and measurements for the published commit. Check the exact Actions run before
using its packages. [Release acceptance](release-checklist.md) separates the
remaining hardware, native dialog, attached-stream, accessibility, clean-machine,
and signing work from automated evidence.

## Active-I/O and rendering follow-up, 9 October 2026

The local follow-up passes **159/159 core/platform cases**, **11/11 desktop
suites**, **11/11 Release suites**, and **8/8 ASan/UBSan suites**. A clean desktop
rebuild was used after a stale local executable lost its execute permission.
No application change was needed for that workspace artifact.

- 430 dormant observations and another 430 with deterministic changing Screen
  inputs pass. The Source/Signal rule difference remains separately asserted.
- Both live-file peers transfer all 256 byte values, acknowledge flushed output,
  block reads at EOF, and resume queued requests in order after file reload.
- Three protocol edge differences are observed and asserted, with their reasons
  in [reference validation](reference-validation.md#observed-protocol-differences).
- Active-screen compile/tick medians improve on all six samples in the recorded
  seven-run, 10,000-tick comparison. Routing measurements verify every live byte.
- Culling checks retain equal visible-cell counts after adding off-screen cells.
  Text batching gives byte-identical help-screen output and improved local frame
  timing. Timing records distinguish Debug/software measurements from CI builds.
- Tick scheduling supports 1,000 requested steps/second at ordinary frame rates,
  bounds catch-up, and yields between steps after a six-millisecond work budget.
- CI now exercises X11 and Wayland through virtual display servers, records
  Release rendering measurements, and uploads active compatibility evidence.
  Those jobs must pass on the exact published head before being treated as proof.

Virtual displays do not certify physical devices, native dialogs, screen readers,
GPU performance or signed clean-machine installation. Those remain in the
[release acceptance matrix](release-checklist.md).

## Accessibility and packaging follow-up, 9 October 2026

Local GNU 13.3 / SDL 3.4.18 checks pass **160/160 core/platform cases**,
**12/12 desktop suites**, **12/12 Release suites**, and **9/9 ASan/UBSan suites**
(the same local leak-detection limitation applies). The installed desktop renders
eleven distinct UI states. A high-contrast frame was visually reviewed; powered
crossing axes retain independent width and color. Preference migration preserves
version-one settings. Focused-control descriptions and the window summary are
covered by desktop event tests, including elapsed-time handling around dialogs.

Linux archive/DEB extraction, checksum, metadata, executable and maintainer-hook
checks pass locally. Two new CI jobs exercise actual install/reinstall/remove/
purge in fresh Ubuntu containers on x64 and ARM64. Windows checks now exercise
same-version replacement and user-file/default-handler preservation; macOS checks
exercise a relocated bundle from another working directory. Those native jobs
must pass on the exact published commit before their results are accepted.

Seven signing-workflow tests cover signature warnings/failures, both rejected
notarization submissions, existing-output preservation, and metadata/credential
selection checks. They use inert fixtures and simulated native tools. Native
installed-stage planning is checked on Windows/macOS; no real certificate or
notarization service has been used. See [signing setup](signing.md) for the
credentialed workflow and [release acceptance](release-checklist.md) for the
remaining physical-device, screen-reader, clean-machine and client checks.
