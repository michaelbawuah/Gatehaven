# State digest version 1

`gatehaven-cli digest FILE.ghv STEPS` emits JSON with schema, ticks, and a
16-digit lowercase hexadecimal fingerprint. Use matching files and tick counts
to compare platform builds. File ports remain disconnected in headless runs.

The digest is FNV-1a 64-bit with offset 14695981039346656037 and multiplier
1099511628211, wrapping modulo 2^64. Feed these fields in order, little-endian:

| Field | Bytes |
| --- | --- |
| Schema (1) | 1 |
| Tick count | 8 |
| Occupied cell count | 8 |
| Each cell, sorted by y then x: signed x as uint32, signed y as uint32 | 4 + 4 |
| Element enum ordinal, powered port mask, sending bit, receiving bit | 1 + 1 + 1 + 1 |

Element ordinals are fixed by the native enum: empty=0, wire=1, crossing=2,
source=3, signal=4, positive relay=5, negative relay=6, AND=7, OR=8, NAND=9,
NOR=10, screen=11, file input=12, file output=13. Empty cells are omitted.
Zero ticks includes the circuit layout with zero state bits.

This is a compact regression aid, not a cryptographic integrity or authenticity
check. A mismatch identifies a regression candidate; compare traces to locate it.
It does not by itself establish compatibility with another application's engine.
