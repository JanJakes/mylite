# LOAD DATA Streaming Performance

## Status

Implemented; performance qualification did not pass.

The allocation, memory, correctness, and indexed absolute-time gates pass.
The measured zero-index improvement is below the specified 15% threshold, and
the 100K timing matrix exceeds the timing-noise ceiling. See the
[July 2026 qualification report](../../performance/load-data-streaming-qualification-2026-07.md).

## Objective

Remove avoidable input and field-allocation overhead from the existing
descriptor-backed `LOAD DATA INFILE` subset without changing its syntax,
conversion, diagnostic, transaction, or storage behavior.

The implementation must:

- read the import file in bounded chunks instead of calling `fgetc()` for every
  byte;
- retain field and row storage at its high-water capacity across imported rows;
- keep memory bounded by the largest admitted row and field count rather than
  total file size;
- preserve every existing MySQL-visible result and failure boundary.

This work does not broaden the supported `LOAD DATA` grammar.

## Sources and Independence

- Official MySQL 8.4 Reference Manual,
  [`LOAD DATA` statement](https://dev.mysql.com/doc/refman/8.4/en/load-data.html).
- Existing independently authored
  [baseline LOAD DATA specification](../baseline-load-data-infile/specs.md).
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_load_data_infile_expectations.sh`.
- MyLite Release and profiling benchmark measurements described below.

The specification is independently authored from public behavior and MyLite's
own implementation. It does not use MySQL, MariaDB, Percona, or other
restrictively licensed implementation source.

The MySQL 8.4 manual defines the default input shape as tab-separated fields,
newline-separated rows, and backslash escape processing. It also establishes
that the input is processed row by row, with strictness, row shape, `NULL`,
column conversion, warnings, and error handling affecting the inserted result.
Those semantics constrain the optimized reader; they do not require any
particular byte-at-a-time implementation.

The complete existing expectation script was rerun against MySQL 8.4.9 before
this specification and passed. It verifies exact rows, warnings, errors,
SQLSTATE values, affected rows, strict/nonstrict conversion, missing/excess
fields, `NULL`, temporal values, target resolution, and disabled `LOCAL`
behavior.

## Baseline Evidence

At clean revision `ce436c103`, the profiling Release client imported a
three-integer, zero-index, 100K-row file with these per-sample counters:

- 300,009 MyLite allocations;
- 38,441,224 allocated bytes;
- 100,014 or 100,015 SQLite steps;
- 75.8 to 96.7 ms inside profiled SQLite steps;
- 224.6 to 282.1 ms instrumented total time.

The allocation count is effectively three field buffers per row plus fixed
statement work. Source inspection confirms that each completed field transfers
a freshly grown dynamic string into the row and frees it after execution.
Escaped fields can additionally allocate a decoded copy. Input dispatch calls
`fgetc()` once for every byte even though the C stream may maintain its own
internal buffer.

A separate profiling-disabled Release run used five balanced MyLite/SQLite
samples after one warmup. The 100K-row, zero-index medians were:

| Engine | Median | Per row | Throughput |
| --- | ---: | ---: | ---: |
| MyLite | 209.585 ms | 2.096 us | 477,134 rows/s |
| bundled SQLite comparator | 100.767 ms | 1.008 us | 992,390 rows/s |

The comparator uses `fgets()` plus one retained three-parameter SQLite
statement. These figures are a local pre-change baseline, not a portable
absolute performance claim.

## Compatibility Contract

All behavior in the baseline specification remains authoritative, including:

- default tab, newline, and backslash handling;
- `\N` recognition only for an exact raw field and all other default escape
  mappings;
- empty fields, embedded decoded NUL bytes, final unterminated rows, and no
  synthetic row after a trailing newline;
- `IGNORE n LINES` counting physical rows before conversion;
- strict and nonstrict missing/excess-field behavior;
- integer, text, temporal, `NULL`, default, key, foreign-key, and
  auto-increment conversion;
- exact row numbers in errors and warnings;
- one statement transaction with complete rollback on read, conversion,
  allocation, constraint, or SQLite failure;
- affected rows, warnings, insert ID, `ROW_COUNT()`, updated-time, catalog
  generation, and reopen behavior.

The reader must process successfully read bytes before reporting a later
stream error, matching the existing loop. It must close the file and finalize
the prepared SQLite statement on every path.

## Chunked Reader Design

The reader uses a fixed 16 KiB stack chunk and `fread()`. A chunk is only an
I/O transport boundary:

- fields and rows may span any number of chunks;
- a backslash may be the last byte of one chunk and its escaped byte the first
  byte of the next;
- tab and newline delimiters have the same meaning at every chunk position;
- EOF handling consults `ferror()` and separately completes a pending final
  row.

A small reader-state object owns the current raw-field scratch buffer, reusable
row, reusable planned insert row, physical and loaded row numbers, and the
escape/row-pending flags. Chunk consumption updates that state but does not
perform file I/O, which keeps boundary behavior directly testable.

The file is still streamed. No chunk, row, or decoded value may be retained
after its row has executed except as reusable capacity.

## Reusable Field Storage

The raw-field dynamic string retains its allocation when a delimiter is
reached. Finishing a field resets its logical length to zero instead of
transferring and reinitializing its buffer.

Each reusable row field owns:

- a decoded byte buffer;
- current decoded length;
- retained capacity;
- the `is_null` flag.

The field descriptor array grows geometrically and zero-initializes new slots.
Decoding reserves `raw_length + 1` bytes in the selected field slot and writes
the decoded value directly into that retained storage. An unescaped field uses
one bounded copy. An escaped field decodes with separate read and write
indexes. Exact raw `\N` sets `is_null` without losing the slot's retained
capacity. Every non-NULL decoded field remains NUL-terminated even when its
logical value contains an embedded NUL.

Resetting a row clears logical field state but does not free buffers.
Deinitialization frees every allocated field slot, the field array, and the raw
scratch buffer exactly once.

Text planned values borrow decoded row storage only through the immediate
SQLite step, as in the current optimized import path. Integer conversion only
borrows it during parsing. Temporal conversion helpers that require owned
mutable storage continue receiving their own copy; their ownership contract is
unchanged.

## Memory and Overflow Policy

Memory is proportional to:

- one 16 KiB input chunk;
- the largest raw field observed so far;
- one retained decoded buffer per maximum row field position;
- the existing reusable planned row.

All chunk lengths, field lengths, capacities, and allocation products use
`size_t`. Capacity growth checks addition, multiplication, and doubling before
allocation. Allocation failure returns `MYLITE_NOMEM`, sets the existing
diagnostic, and rolls back the statement.

The change adds no public ABI, dependency, SQLite fork patch, catalog field, or
`.mylite` format change. The fixed input buffer is stack-owned and introduces
no idle connection memory.

## Performance Gates

The profiling client must enforce these counters for the deterministic
three-integer, zero-index import:

- at 10K or more rows, fewer than 64 MyLite allocations for the complete
  `LOAD DATA` statement;
- allocation count does not grow with row count after retained buffers reach
  their high-water capacities;
- at 100K rows, fewer than 1 MiB of MyLite allocation requests;
- exactly one imported SQLite user statement step per row, with existing fixed
  metadata allowance.

Release qualification uses 100K and 1M rows, both zero-index and five-index
tables, five samples, one warmup, CPU affinity, fresh rollback state, and the
existing MyLite/SQLite affected-row checksum comparison. It records input
bytes, rows/s, bytes/s, allocation count/bytes, and peak RSS.

The zero-index 100K median must improve by at least 15% from the paired
pre-change revision. Neither indexed workload may regress by more than 5%.
Peak RSS must remain bounded as row count grows for the same maximum row width;
the 1M run may not retain memory proportional to file size.

Performance gates never weaken correctness, durability, index, or constraint
behavior.

## Test Plan

1. Add chunk-boundary runtime cases for:
   - tab and newline exactly before and after a chunk boundary;
   - backslash as the final byte of a chunk;
   - a field larger than multiple chunks;
   - exact `\N`, escaped delimiters, decoded NUL, trailing backslash, final
     unterminated row, and trailing newline.
2. Retain the complete existing runtime and MySQL 8.4.9 expectation suites.
3. Add a profiling-only allocation regression that imports at least 10K rows
   and proves allocation count is bounded rather than per-row.
4. Run the zero-index and five-index large-dataset smokes with exact affected
   rows and checksums.
5. Run formatting, compiler warnings, static analysis, ASan/UBSan, and the full
   native suite.
6. Collect paired Release 100K/1M timing, throughput, allocation, and peak-RSS
   evidence and update the performance report and remediation checklist.

## Compatibility Matrix Decision

`LOAD DATA INFILE` remains partial (`🟡`) because this change optimizes only the
already supported descriptor-backed subset. Deferred modifiers, custom
field/line clauses, user variables, `SET`, character sets, partitions, and
`LOCAL` transfer remain deferred exactly as documented by the baseline
specification.
