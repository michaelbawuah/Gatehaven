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
- All three local sanitizer suites pass with ASan and UBSan enabled. The core,
  CLI, and separate clipboard processes are included in that run.

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
- Legacy file import, external protocol compatibility, and comprehensive
  acceptance against the remaining product brief.

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
not validate later commits. The remaining brief requires independently verified
`.ccsb` compatibility, original-application performance comparisons, real native
dialog/display/touch checks, release signatures, and clean-machine installation.
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
