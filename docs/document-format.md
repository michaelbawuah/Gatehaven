# Circuit document formats

Gatehaven reads native UTF-8 `.ghv` and binary `.ccsb` content by header, regardless
of the filename. Saving chooses legacy output for a case-insensitive `.ccsb`
extension and native output otherwise. The Save As filter appends an extension
only when the chosen filename has none. Renaming a file does not convert it;
use Save As or `gatehaven-cli convert INPUT OUTPUT`.

## Native versions 1 and 2

Version 1 starts with `GATEHAVEN 1`, followed by `x y element` records. All stored
levels default to zero. Version 2 starts with `GATEHAVEN 2`, followed by
`x y element state`. State is an unsigned decimal integer from 0 through 3:

| Bit | Meaning |
| --- | --- |
| 0 | Level used on Reset |
| 1 | Current level restored on Open |

For relays the level means conductivity. Other active components use it as their
saved output. Power on passive wire/Signal/crossing cells is recomputed from the
network; those saved display bits do not create energy. Sources always supply
power. Opening establishes tick-zero propagation before any simulation step.

```text
GATEHAVEN 2
-2 7 positive-relay 3
-1 7 wire 0
```

The writer emits version 1 when every stored level is zero, and version 2
otherwise. Version 1 files remain readable; versions of Gatehaven before 0.5
cannot read version 2. A desktop save captures live levels and preserves reset
levels. CLI conversion preserves the stored data without advancing simulation.

Coordinates are signed 32-bit decimal integers, sorted by y then x on write.
A coordinate may occur once. Empty cells are omitted. Names are `wire`,
`crossing`, `source`, `signal`, `positive-relay`, `negative-relay`, `and`, `or`,
`nand`, `nor`, `screen`, `file-input`, and `file-output`.

Blank lines and whole-line comments beginning with optional whitespace and `#`
are accepted. Both LF and CRLF work, with an optional UTF-8 BOM before the header.
Writers produce BOM-free LF text. The default reader permits one million occupied
cells, two million lines, and 192 bytes per line. Callers may configure limits.

## Legacy format and limits

See [the binary specification](legacy-format.md) for the byte contract. Import
preserves component types and both level bits. Export writes the occupied
bounding rectangle at `(0,0)`, including empty holes, so it does not retain signed
world coordinates or extra empty borders around the document. The default dense
area budget is 64 million cells, separate from the occupied-cell limit. Large
sparse circuits should use native `.ghv` when their rectangle exceeds that budget.

## Transactional file handling

Path loading accepts regular files, including ordinary symlinks to regular files,
and rejects directories and special streams. A malformed input produces a text
line or binary-offset diagnostic and leaves the active circuit intact.

Saving uses an exclusively created temporary file beside the destination,
checks write/flush/close results, then replaces the destination. A failed export
preserves the old file. Power-loss durability is not guaranteed; file/directory
synchronization remains a separate concern.

Neither format stores machine pointers, file-port paths, protocol queues, the
simulation tick counter, viewport, or editor history. Opening a file cannot
choose or attach a user's input/output streams.
