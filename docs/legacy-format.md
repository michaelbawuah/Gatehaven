# Legacy circuit interchange

The compatibility target is the `CCPG` version-0 binary layout used by the
v0.4-alpha reference application (revision
`19c00d0bd794c3dd558d501939e55f186f58c9e6`). This document records interoperability
facts; Gatehaven's codec is independently implemented.

The 16-byte header contains ASCII `CCPG`, then version, width, and height as
little-endian signed 32-bit integers. Version is zero. Dimensions are nonnegative
and their product cannot exceed INT32_MAX. Cells follow in row-major order,
one byte per position, including empty positions.

Bits 7..2 identify the element: 0 empty, 1 wire, 2 crossing, 3 Signal, 4 source,
5 positive relay, 6 negative relay, 7 AND, 8 OR, 9 NAND, 10 NOR, 11 screen,
12 file input, 13 file output. Gatehaven's internal source/Signal ordering differs
and must not be used directly as the file identifier.

Bit 1 stores the current logic level (relay conductivity for relays), and bit 0
stores the corresponding reset level. Wire/crossing/Signal display bits do not
establish sources. Source and empty flags are ignored by the reference reader.
Communicator paths, queues, and relay wire-power states are absent from the file.

There is no signed world origin in this format. Export translates the occupied
bounding box to (0,0); native documents retain signed coordinates. Readers and
writers must bound dense area independently of the number of occupied cells.

Format evidence: the pinned reference's CanvasState serialization contract and
six published sample files. The serialization contract alone does not prove
simulation parity. No reference implementation or assets are distributed here.
