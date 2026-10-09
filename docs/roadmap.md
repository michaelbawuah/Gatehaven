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

Next: independently specified legacy import, full multitouch navigation,
accessibility review, measured large-circuit improvements, native installers,
signing, and hardware/clean-machine acceptance. Performance parity is not
established. The final release still depends on these acceptance gates.
