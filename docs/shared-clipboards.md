# Shared session clipboards

Gatehaven has ten application clipboards shared by its running instances for the
same OS user. They are separate from the operating system's text/image clipboard.
Ctrl+C/X/V uses slot 0. Ctrl+Shift+C/X/V opens a numbered chooser. Copying to or
pasting from an explicit slot makes slot 0 follow that slot. Copying directly to
0 gives it a value of its own until another explicit slot is used.

The desktop stores session files under `clipboards-v1` in SDL's per-user
application preference directory. A shared `members.lock` is held by every live
instance. `transaction.lock` serializes slot operations and membership changes.
On POSIX these use `flock`; Windows uses `LockFileEx`. Locks are not inherited by
new processes and the OS releases them when a process exits or crashes.

Joining holds the transaction lock before probing the membership lock exclusively.
If the exclusive lock succeeds, this is the first instance and stale payloads are
cleared before acquiring shared membership. Leaving uses the same order: release
membership, probe exclusive membership, and clear payloads if this is the last
instance. Lock files remain in place to avoid creating different lock identities
while another process is opening them. Lock acquisition times out after two
seconds with a retryable error rather than waiting forever.

If all instances crash, payload files can remain on disk until the next launch,
which clears them. They are not a persistent circuit library. Normal final exit
removes slot data, the default index, and any pending temporary file. A failed
cleanup is retried by the next first member.

## Stamp format, versions 1 and 2

All integers are unsigned and little-endian. The maximum is 1,000,000 cells and
9,000,040 bytes per slot. Offsets preserve the selection's empty borders.

| Field | Size | Rule |
| --- | --- | --- |
| Magic | 8 bytes | `GHCLIP01` or `GHCLIP02` |
| Width, height | 8 bytes each | 1 through 2^32; both zero for an empty stamp |
| Cell count | 8 bytes | 0 through 1,000,000 |
| Each cell | 9 bytes | x: 4 bytes, y: 4 bytes, packed element/state: 1 byte |
| Checksum | 8 bytes | FNV-1a 64 over all preceding bytes |

Version 1 uses the element ordinal directly and defaults stored levels to zero.
Version 2 uses bits 0–3 for the ordinal and bits 4–5 for saved/reset state; bits
6–7 must be zero. Writers choose version 1 for an all-zero-state stamp and version
2 otherwise. Copy/cut/duplicate capture live levels while preserving reset
levels; moving and transforming that snapshot retain both. Close older Gatehaven
versions before using the 0.5 preview: mixed-version session sharing is unsupported.

Readers reject unknown/empty element values, duplicate positions, offsets
outside the dimensions, oversized counts, length mismatches, and failed
checksums. The checksum detects corruption; it does not authenticate a sender.
The session directory is private to its user. Do not place it on a shared or
network filesystem; the supported application path is local per-user storage.

Payload writes use a closed temporary file and atomic replacement under the
transaction lock. The default slot is a one-byte index, also replaced atomically.
A process dying between payload and index replacement can leave the previous
default index, but it cannot expose a partially written payload. Failed writes
never delete the source of a cut operation. A paste preview captures one local
snapshot, so another instance cannot change it while it is being positioned.

The Python integration test starts separate compiled peers, coordinates their
operations, forces process exits, and checks both data and cleanup. It runs on
all three core CI platforms and under the Linux sanitizers.
