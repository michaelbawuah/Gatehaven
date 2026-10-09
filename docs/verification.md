# Verification record

Initial prototype checks were performed on Linux with GCC 13.3.0, CMake 4.4.4,
Ninja 1.13.2, and SDL3 3.4.18. The local SDL build uses its software renderer and
dummy video driver. It cannot validate a physical monitor, touch input, or
operating-system file dialogs.

## Verified locally

- 32 core test cases pass in debug and release builds. Parameterized cases
  exercise all four gates with zero through four inputs and every active-input
  count, relay behavior, delayed rising/falling edges, crossing isolation,
  deterministic insertion order, and coordinate boundaries.
- Document tests cover malformed input, duplicate cells, coordinate overflow,
  resource limits, comments, CRLF, round trips, save replacement, and failed saves.
- Editor tests cover stroke transactions, undo/redo branching, memory limits,
  rotations, flips, sparse capture, and paste overflow.
- The desktop smoke test injects SDL keyboard and mouse events, then verifies
  play/pause, stepping, drawing, undo/redo, reset, and a rendered frame.
- A starter-circuit screenshot was rendered from the application and visually
  inspected. Small-font rasterization and connection drawing were corrected.
- AddressSanitizer and UndefinedBehaviorSanitizer checks run separately from the
  normal build. Local leak detection is unavailable because this environment
  blocks LeakSanitizer's `/proc` task inspection; the GitHub job keeps leak
  detection enabled so that check can run on a normal runner.

## Initial performance measurement

One release-build run of the wire-chain workload (10,000 cells, 20 timed steps,
one untimed warm-up) took 123.609 ms total, or 6.18046 ms per step. The benchmark
verifies that every cell is powered before reporting a result. This is a local
baseline, not a cross-machine score or proof of performance parity.

## Still to verify

- GitHub results for each current commit; see the Actions page rather than
  assuming a configured job has passed.
- Desktop compilation and real interaction on Windows and macOS.
- Linux X11 and Wayland sessions, native dialogs, per-monitor scaling, and touch.
- File-dialog cancellation and shutdown on each supported operating system.
- Platform and compiler versions for release targets, including each CPU architecture.
- ThreadSanitizer for any future shared state or simulation workers.
- Final packages, file associations, signing, and clean-machine installation.
- Legacy file import, communicator protocols, and comprehensive compatibility checks.
