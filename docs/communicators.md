# Communicators

Screen, File In, and File Out occupy grid cells. Adjacent cells of the same type
form one communicator. Different types stay separate. A group sends `1` when any
neighboring Signal was powered at the end of the previous tick. The group's
received bit makes all its cells sources when `1`, or ordinary conductors when
`0`. Receiving zero does not insulate the group from an external power source.

Choose **I** (Interact). Hold a screen to receive `1`; release it to receive `0`.
Its brightness displays the bit **sent by the circuit**, independently of the
held input. Moving the pointer between screens transfers the held input. Losing
focus or canceling a gesture releases it.

Click a file communicator with Interact to choose a binary file. File In uses an
Open dialog; File Out uses a Save dialog and replaces the chosen file's contents.
Simulation pauses for these dialogs. File selections belong to the current
window. Documents, undo commands, selections, and clipboards never contain paths
or file handles. Loading a circuit does not grant it access to files.

## Bits and commands

One bit travels in each direction per tick. Idle is `0`. Each frame begins with
`1`, followed by two command bits in written order (most significant first).
Payload bytes travel least significant bit first. For example, writing `A`
(`0x41`) sends `1 00 10000010`.

| Port | Request | Reply |
| --- | --- | --- |
| File In | `1 00` | `1 00` followed by eight bits of the next byte |
| File In | `1 01` | `1 01` followed by one bit: more data available? |
| File Out | `1 00` plus eight data bits | `1 00` after the byte is written and the stream flush succeeds |

Input commands `10` and `11` are reserved and ignored. Output commands other than
`00` consume their full eight-bit payload and are ignored. A receiver must wait
for a reply start bit; do not assume fixed response latency. Replies preserve
request order and never overlap. Each port limits pending requests to 4,096;
overflow or I/O failure latches an error in the status bar.

At EOF, a next-byte request waits. Selecting a new input file resumes the oldest
waiting request from the beginning of the new file; previously delivered bytes
are not replayed. A queued availability query waits behind an earlier blocked
read. Selecting a file retries a latched error. Output acknowledgement confirms
a successful C++ stream flush, not a power-loss durability guarantee.

## Editing and reset

Reset clears power, tick count, protocol framing, replies, and pending requests;
it releases held screens but keeps chosen files and their current positions.
Select the input file again to rewind it. Select an output file again to start a
new output stream (which truncates that file).

Changing a group's shape resets its framing on the next exchange. Merging groups
uses the most recently chosen file still anchored in the merged group. Splitting
keeps a chosen file with the portion containing its anchor cell. Removing or
replacing an anchor closes its file on the next simulation tick. Undo restores
cells but cannot restore discarded handles or undo bytes written to a file.

The protocol engine has exhaustive byte-decoder coverage and real temporary-file
tests. Native file dialogs and physical mouse/touch behavior still require manual
checks on each supported operating system.
