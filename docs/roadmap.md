# Roadmap

Gatehaven uses an independently written C++23 implementation and original
application visuals. The first milestone is a usable, testable desktop sandbox.

## Delivery order

1. Sparse circuit storage, integer coordinates, and deterministic simulation.
2. Native document format, undo/redo, selection, and reusable circuit examples.
3. SDL3 desktop canvas, drawing, navigation, and simulation controls.
4. File dialogs, reliable saves, documentation, and continuous integration.
5. Advanced editing, ten shared clipboards, input bindings, and accessibility.
6. Screen and file communicators with protocol and lifecycle tests.
7. Legacy document import, measured performance work, and compatibility checks.
8. Native installers, signing where credentials are available, and release QA.
9. Optional challenges and guided learning after the sandbox is complete.

## Completion criteria

The final release needs verified circuit semantics, repeatable builds, tests on
all supported systems, measured large-circuit performance, and manual checks of
the full interface. A successful Linux build does not establish Windows or Mac
support. A prototype is not a release candidate.

Each commit should represent a coherent, reviewable change with the appropriate
verification. Release readiness depends on the acceptance criteria.

## Current prototype

The sparse core, gate/relay simulation, native documents, transactional editor,
ten shared session clipboards, and SDL3 desktop canvas are implemented. New/Open
launch independent processes. The desktop has file dialogs, region selection,
rotation/flips, keyboard movement, a clipboard chooser, a custom speed dialog,
five mouse-button bindings plus a separate touch binding, and an eyedropper.

Connected and additive/subtractive selection, sparse duplication, polyline
drawing, beginner hints, and persistent input preferences are implemented.
Screen and file communicators now have protocol, lifecycle, and real-file tests.
Six built-in lessons, CLI traces/statistics/SVG export, install rules, portable
archives, and installed-product checks are available.

Native multitouch, keyboard traversal, recovery snapshots, indexed simulation,
clipped editing previews, offline guides, save-conflict handling, and unsigned
DEB/DMG/Windows x64 installer previews are now implemented. Windows ARM64 has
native CI coverage and a portable archive. State digests and editor benchmarks
add repeatable verification tools.

The 0.5 preview implements legacy import/export, stateful native documents and
clipboards, compiled component simulation, external behavior/headless throughput
comparisons, AppImage previews, keyboard canvas construction, and native cell
inspection. The matching corpus is explicit; the Source/Signal specification
difference is documented and tested separately.

Next: screen-reader review, physical display/touch/dialog acceptance,
clean-machine installation, and release signatures. See the [acceptance matrix](release-checklist.md).
Active-screen comparisons, live input/output protocol comparisons, endpoint
routing benchmarks, software rendering measurements, and virtual X11/Wayland
checks are implemented. All six dormant and active-screen sample medians improved
in the recorded local scenarios. Complete product performance parity still
requires native GPU and physical interaction measurements.
