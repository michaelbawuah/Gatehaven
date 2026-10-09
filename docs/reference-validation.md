# External reference validation

Gatehaven's production build does not depend on reference source or assets.
A test-only adapter can compile against a separate, unchanged checkout at
`19c00d0bd794c3dd558d501939e55f186f58c9e6`. On Linux it needs a C++23 compiler,
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
element and all saved/reset bit combinations. Disconnected file ports, deterministic screen interaction, and separate live-file
protocol scenarios are tested as described below.
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

The reference adapter is built in C++23 mode too. The initial exploratory report
used C++17 for the external checkout; current measurements and CI use C++23 for
both engines, with extensions disabled. Gatehaven has no C++17 fallback build.

## Active input and real streams

Use `--screens` with `gatehaven_screen_peer` to drive the same seven-tick hold
pattern into every Screen group in both engines. Anchors use the first cell in
y/x order, so engine-specific group indices cannot change the stimulus. All six
samples and the 100 seeded circuits are compared at the same 430 observation
points, in addition to the original dormant-input comparison. The pattern is a
deterministic simulation stimulus; it does not automate a physical mouse.

```sh
bash tools/build_reference_adapter.sh /path/to/reference build/reference-adapter build/reference-io-adapter
python tools/compare_reference.py build/reference-adapter build/release/gatehaven_screen_peer /path/to/reference/samples --screens
python tools/compare_active_io.py build/reference-io-adapter build/release/gatehaven_active_io_peer
```

The independent protocol driver sends all 256 byte values through real input and
output files. It checks pipelined order, every acknowledgement, exact flushed
output bytes, EOF blocking, and reload with an outstanding read followed by an
availability query. Reply latency may vary; bounded deadlines catch a hung peer.
Paths come from the test harness's temporary directory, never a circuit file.

### Observed protocol differences

These are separate assertions in the live comparison, not matching cases:

| Edge | Pinned reference | Gatehaven | Reason |
| --- | --- | --- | --- |
| First availability query on an empty file | Reports available | Reports unavailable | The brief defines the response as whether a byte is available |
| Reset after reading A from AB | Next read returns A | Next read returns B | Preserve the documented Gatehaven stream position; selecting the file again rewinds |
| Reserved output command with an embedded valid header in its payload | Can write a byte from that payload | Consumes and ignores all eight reserved payload bits | Honor the brief's fixed output chunk boundary |

Reset position is a deliberate product difference from the reference. The brief
does not specify its file-position behavior. File group merge/split ownership also
uses Gatehaven's explicit chosen-anchor/latest-selection rule, rather than the
reference's pixel-majority selection. These differences remain part of final
acceptance review; the report does not claim universal compatibility.
