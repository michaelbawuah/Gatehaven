# Local compatibility observations

The JSON records contain hashes and observable results, not external circuits or
implementation source. Behavior schema 2 records 430 matching observations and
separately asserts the brief/reference Source-to-Signal difference. Matching
seeded fixtures exclude that edge, as described in each report's limits.

The six corpus records use an independent Python decoder, Gatehaven conversion,
and exact binary round trips. Their input hashes pin the tested sample bytes.
The source tree identifies the measured candidate content independently of local
and published commit metadata. The compiler string and external adapter hash
identify the actual local binaries; CI generates fresh reports for its own head.

These observations exclude attached file streams, physical user interaction, and
per-axis external crossing output. See [the procedure](../../docs/reference-validation.md)
and [release acceptance](../../docs/release-checklist.md) before interpreting them.
