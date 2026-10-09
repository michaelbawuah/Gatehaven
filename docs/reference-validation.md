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

## Explicit specification difference

The product brief (page 7) says Signals conduct only toward wire, crossing, and
Signal cells. The pinned reference engine also conducts directly between a Source
and a Signal. Gatehaven follows the supplied brief: put a wire between them.
This is an intentional behavior difference, covered by a dedicated test and
reported separately from matching cases. Matching the reference on this edge
would require changing the product requirement.

The six published samples do not exercise that conflicting edge. Seeded matching
fixtures exclude direct Source/Signal adjacency, and the report identifies this
restriction. A separate two-cell observation records the expected difference.
No reference source or assets are distributed in Gatehaven's production build.
