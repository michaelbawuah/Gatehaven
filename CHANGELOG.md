# Changelog

## 0.3 development preview — 9 October 2026

- Indexed circuit topology, reusable propagation buffers, lazy snapshots, cached
  bounds, allocation-free visible rendering, and settled-circuit skipping.
- Native one-finger tools, two-finger pan/pinch, multi-tap selection/centering,
  keyboard traversal of all controls, and safer modal input.
- Automatic unsaved-work snapshots, abandoned-session recovery, and exclusive
  ownership tested with killed processes and competing consumers.
- Unicode Windows CLI arguments, JSON circuit profiling, bounded traces,
  six benchmark workloads, and reference-engine differential tests.
- Linux ARM64 and Intel Mac CI targets; optional unsigned DEB and DMG previews
  with package inspection and nine installed interface snapshots.
- Correct own-pencil Signal placement for communicators and double-click-only
  Panner centering.

Legacy `.ccsb` compatibility, physical hardware acceptance, screen-reader support,
Windows installers, signing/notarization, and external performance parity remain
open. This version is a development preview.

## 0.2 development preview — 9 October 2026

### Circuit editing

- Chained snapped polylines, previews, retracing, and one-step undo.
- Connected-net and physical-circuit selection, additive/subtractive selection,
  sparse transforms, duplication, and selection inversion.
- Saved-revision tracking, atomic preference saves, stable input defaults,
  keyboard zoom, and click-to-center panning.
- Context hints, expanded shortcuts, a manual, and six built-in circuit lessons.

### Simulation and tools

- Grouped screen, file-input, and file-output communicators with ordered serial
  protocols, explicit file selection, EOF resumption, and flush-before-acknowledge.
- JSON circuit statistics, CSV tick traces, SVG diagrams, and named examples.
- Cached element counts and passive-cell fast paths; four measured workloads.
- Expanded deterministic, boundary, malformed-input, real-file, and SDL-event tests.

### Distribution

- A shared version source for desktop, CLI, and package metadata.
- macOS bundle metadata and file-open events; Linux launcher, MIME data, and icon.
- Portable development archives, checksums, bundled lessons/manuals/notices,
  installed-product tests, and archive validation before CI artifact upload.
- Updated editor screenshots, architecture, protocol, build, and verification guides.

This is an unsigned development preview. Legacy `.ccsb` import, full multitouch
navigation, release installers/signing, accessibility review, physical desktop
acceptance, and clean-machine testing remain open. See the [roadmap](docs/roadmap.md).
