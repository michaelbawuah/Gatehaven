# 0.5 development preview

This preview adds legacy circuit interoperability and a compiled component
simulation engine. It remains unsigned and requires the acceptance checks in
the release checklist before a final release.

- Read/write `.ccsb`, including saved and reset levels. Native `.ghv` version 2
  and shared clipboard version 2 preserve those levels too; older native files
  and zero-state clipboards remain readable.
- Tick-zero initialization, current-level copy/move/transform/save/recovery,
  component propagation, relay switching, and dormant-endpoint suspension.
- Independent file decoding, six external sample round trips, seeded external
  behavior comparisons, and alternating same-host throughput measurements.
- Native text inspection, crossing/relay hover feedback, keyboard canvas
  construction, and keyboard screen interaction.
- Linux x86-64/ARM64 AppImage previews, all-platform legacy file associations,
  package source-revision metadata, and expanded callback/import failure checks.
- Digest schema 2 includes stored levels and unpowered relay conductivity.

Legacy export stores the occupied rectangle at origin `(0,0)`. Native `.ghv`
keeps signed coordinates. The default legacy dense-area budget is 64 million
cells and the occupied-cell budget is one million. File-port paths never travel
with documents or clipboard payloads.

Gatehaven follows the product brief's Source/Signal insulation rule. The pinned
reference engine differs on direct Source-to-Signal adjacency; a dedicated
comparison records that difference. Add a wire between them when adapting such
a circuit. See [reference validation](reference-validation.md).

Headless measurements exclude rendering and attached file streams. Real-device
input/display/dialog checks, active external protocol acceptance, screen-reader
review, clean-machine installation, and release signing remain open.
