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

The follow-up adds live-file and changing-screen comparison, cached endpoint
routing, batched text rendering, viewport measurements, X11/Wayland virtual-display
checks, and a fix for the eight-ticks-per-frame ceiling at high simulation rates.
Protocol edge differences are explicit in the comparison report.

Saved high contrast, focused-control descriptions and window summaries improve
keyboard access. Installer checks now cover Windows replacement, relocated Mac
bundles and fresh Ubuntu container lifecycles. Display diagnostics, native
rendering measurements and a packaged offline acceptance collector support
real-machine verification without automatically approving manual checks.

Real-device input/display/dialog checks, review of known compatibility differences,
screen-reader review, clean-machine installation, and release signing remain open.

The desktop interface now uses embedded Inter regular/semibold typography,
display-scale-aware font filtering, proportional label fitting, and sentence-case
controls. A calmer dotted canvas, component icons, input badges, hover states,
and a compact status bar replace the prototype lettering and dense grid chrome.
Examples and Quick guide are visible header actions with keyboard focus support.
The guide, lesson chooser, speed, clipboard, and recovery panels share a clearer
light layout. Drawing, simulation, saved circuits, and shortcuts remain compatible.

Save As now checks for an existing circuit after adding a missing filename
extension. Canceling that confirmation preserves both circuits. Once closing is
accepted, queued input and simulation updates stop before they can change the
saved document. Desktop verification now follows complete save/close and recovery
sequences, including Unicode names, failed writes, canceled dialogs, undo/redo,
and reopening recovered work.

Canvas navigation now handles native Mac trackpad pinches, keeps zoom anchored
at the pointer, smooths large wheel deltas, and accepts fractional scrolling.
Visible minus, percentage/reset, and plus controls support keyboard focus;
Ctrl/Command plus/minus and Ctrl/Command+0 work too. The zoom ceiling is now 800%.
Placed gates, sources, relays, screens, and file ports use scalable circuit
symbols instead of text tiles. Gate orientation follows nearby connections,
relay contacts reflect conductivity, and powered screens light up. Automated
SDL workflows cover pinch, resized-window coordinate conversion, wheel direction,
keyboard repeat, button hit testing, and cancellation when focus changes.
