# Command-line circuit tools

Run `gatehaven-cli --help` for syntax. Paths containing spaces should be quoted.
The Windows utility accepts Unicode paths. Commands return 0 on success, 1 on
an input/output failure, and 2 on invalid command syntax or bounded-work limits.

| Command | Purpose |
| --- | --- |
| `check FILE` | Validate a native document without changing it |
| `stats FILE` | JSON occupied-cell counts, extents and element totals |
| `run FILE STEPS` | Final powered-port state after the requested ticks |
| `trace FILE STEPS` | CSV states for every cell on every tick |
| `digest FILE STEPS` | A portable regression fingerprint; see state-digest.md |
| `profile FILE STEPS` | JSON elapsed time and propagation counters |
| `normalize FILE OUTPUT` | Canonical native text, sorted by y then x |
| `svg FILE OUTPUT` | Static circuit drawing |
| `examples` / `example NAME OUTPUT` | List or create built-in circuits |
| `--build-info` / `--manual-path` | Exact toolchain or installed help location |

Simulation commands default to ten ticks and accept 0 through 1,000,000. Trace
rejects requests above 5,000,000 rows before simulation starts. File endpoints
remain disconnected: circuits cannot choose paths through the command line.

Normalize removes comments, blank lines and a leading UTF-8 BOM, uses LF endings,
and omits transient state. It can replace the input path atomically after a
successful parse. It does not convert `.ccsb` files. Keep a separate copy if
comments matter to you.

For repeatable comparisons, use the same native circuit, explicit tick count,
and digest schema. Profiling includes topology compilation; benchmark tools
separate cold setup from steady simulation. Times depend on the machine and load.

## Legacy interoperability

Every file-reading command accepts native `.ghv` and binary `.ccsb` content,
identified by its header. `convert INPUT OUTPUT.ccsb` writes a legacy document;
`convert INPUT OUTPUT.ghv` preserves signed coordinates and saved/reset levels in
the native format. Export to legacy translates the bounding box to zero.

`state FILE 0` emits CSV observations of the initialized circuit without advancing
it. Power propagates from sources and saved gate levels at tick zero. Subsequent
`state`, `run`, `trace`, and `digest` observations use this same initialization.
Conversion itself preserves stored levels, rather than advancing the simulation.
