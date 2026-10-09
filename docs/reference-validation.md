# External reference validation

Gatehaven's production build does not depend on reference source or assets.
A test-only adapter can compile against a separate, unchanged checkout at
`19c00d0bd794c3dd558d501939e55f186f58c9e6`. On Linux it needs a C++17 compiler,
Boost headers, SDL2 headers, and pkg-config. No UI or file-dialog automation is
involved. The adapter calls compile, step, and snapshot on the external engine.

```sh
bash tools/build_reference_adapter.sh /path/to/reference build/reference-adapter
python tools/compare_reference.py build/reference-adapter build/release/gatehaven-cli /path/to/reference/samples > behavior.json
```

The adapter's compiler command preincludes `<cstring>` and `<stdexcept>` because
the older headers relied on transitive standard-library includes. It does not
modify reference source or simulation behavior. Output includes Boolean power
for every occupied cell and conductivity for relays at the requested tick.
Crossing axis isolation is checked separately in Gatehaven's port-level tests.

The comparison also generates seeded, original small circuits covering every
element and all saved/reset bit combinations. Disconnected file ports are tested;
active screen interaction and attached file streams require additional scenarios.
A green comparison applies to its pinned inputs and revision, not every possible
circuit, timing configuration, or platform.

## Behavior correction discovered by seeded comparisons

A Source directly powers an adjacent Signal. Signal edges to gates, relays, and
communicators remain control inputs and do not conduct their output power.
The earlier Gatehaven implementation incorrectly excluded Sources too. The
external engine exposed this in the first seeded fixture. Both the production
engine and the slow Gatehaven traversal oracle now include the confirmed edge.
The slow oracle therefore includes this documented correction to its earlier
behavior; external comparisons remain the independent acceptance evidence.
