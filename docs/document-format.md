# Native document format, version 1

Gatehaven saves UTF-8 text with a `.ghv` extension. The first line is exactly
`GATEHAVEN 1`. Subsequent nonblank lines contain `x y element`. Lines whose first
non-whitespace character is `#` are comments. Both LF and CRLF are accepted. A UTF-8 BOM is permitted before the header.
Writers always emit BOM-free LF text.

Coordinates are signed 32-bit decimal integers. Each coordinate pair may occur
once. Empty cells are omitted. Element names are `wire`, `crossing`, `source`,
`signal`, `positive-relay`, `negative-relay`, `and`, `or`, `nand`, `nor`,
`screen`, `file-input`, and `file-output`.

Serialization sorts by y and then x. Documents describe circuit structure;
simulation power and the current tick are intentionally transient.

The default reader permits 1,000,000 occupied cells, 2,000,000 lines, and 192 bytes
per line. Callers can configure these limits. Malformed input returns a line
number and error message without modifying an existing circuit.

Saving writes an exclusively created temporary file beside the destination,
checks write and close errors, then replaces the destination. This avoids
truncating the previous document on a failed write. Power-loss durability is not
guaranteed; a future durability option needs file and directory synchronization.

Legacy `.ccsb` import is not implemented yet. Renaming a legacy file to `.ghv`
does not convert it.

Path-based loading accepts regular files, including ordinary symlinks to them,
and rejects directories and special streams. The header counts toward the line
budget. Communicator paths and protocol queues are never serialized.
