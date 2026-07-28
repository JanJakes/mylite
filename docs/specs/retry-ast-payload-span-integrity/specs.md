# Retry AST Payload-Span Integrity

## Status

Specified; implementation and release qualification are pending.

## Summary

Several compatibility retries parse a valid statement prefix with Lemon,
extend or annotate the resulting AST, and then publish that AST for the full
statement. `mylite_sql_ast_rebase_source_length()` currently updates only each
node's primary source span. It does not update source spans embedded in type
payloads, and `mylite_sql_ast_spans_are_within_source()` validates only primary
spans.

As a result, a retry-produced AST can pass the final parser invariant while
its width, length, precision, or scale span still describes the shorter prefix
source. Cloning such an AST into an owned snapshot rejects the inconsistent
payload. A partitioned `CREATE TABLE` containing the complete span-bearing
type family reproduces the defect: parsing succeeds and the primary-span
validator returns true, but cloning the statement subtree returns false.

This feature makes primary and payload source spans one AST-wide invariant.
Every operation that validates, changes source length, or snapshots an AST
uses the same exhaustive payload-span inventory.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Initial AST and source-span contract:
  `docs/specs/mysql-parser-scaffold/specs.md`
- Existing iterative AST snapshot design:
  `packages/libmylite/src/sql/mylite_ast.c`
- Follow-up review finding `SQL-03`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 `CREATE TABLE` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 data types:
  <https://dev.mysql.com/doc/refman/8.4/en/data-types.html>
- MySQL 8.4 partitioning:
  <https://dev.mysql.com/doc/refman/8.4/en/partitioning.html>
- MySQL 8.4 `ALTER TABLE` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4.9 observations pinned by
  `packages/libmylite/tests/mysql_retry_ast_payload_span_integrity_expectations.sh`.

The specification is independently authored from public behavior and MyLite
source. It does not copy MySQL parser or server implementation source.

## Observed MySQL 8.4.9 Behavior

MySQL does not expose parser AST source pointers or source-length fields.
Runtime comparison therefore pins the accepted DDL and the semantic meaning
of every syntax token represented by a MyLite payload span.

The pinned runtime accepts one partitioned `CREATE TABLE` containing:

- deprecated integer display width `INT(11)`;
- `VARCHAR(12)`, `CHAR(3)`, `TEXT(12)`, `BINARY(4)`, and `VARBINARY(5)`;
- `BIT(6)` and deprecated `YEAR(4)`;
- `DECIMAL(10,2)` and deprecated `FLOAT(9,3)`;
- `DATETIME(4)`, `TIMESTAMP(5)`, and `TIME(6)`.

It publishes warnings `1681`, `1287`, and `1681` in statement order for the
three deprecated forms. `INFORMATION_SCHEMA.COLUMNS` reports the declared
numeric precision and scale, temporal fractional precision, and character
lengths. `TEXT(12)` is normalized to `tinytext`; integer display width and
`YEAR(4)` do not survive in `COLUMN_TYPE`.

## Current Baseline

The native reproducer parses the same partitioned `CREATE TABLE` through the
partition-placeholder prefix retry and observes:

```text
status=ok retries=1 spans_valid=1 snapshot_cloned=0
```

The prefix AST's primary spans are changed from the prefix source length to the
full statement length. Its active payload spans retain the prefix source
length, but the current validator does not inspect them. The existing snapshot
implementation does inspect and rebase payload spans, detects the mismatch,
and rejects the clone.

## Scope

This feature covers:

- every `struct mylite_sql_source_span` embedded in an AST node payload;
- AST-wide source-span validation;
- atomic source-length rebasing after a successful prefix parse;
- owned subtree snapshot cloning;
- all current prefix retry paths:
  - tableless `SELECT ... LIMIT`;
  - repeated `SELECT` locking clauses;
  - `ALTER TABLE` option tails;
  - `CREATE TABLE ... PARTITION BY` placeholders;
- parser-success validation after any compatibility retry;
- an explicit regression inventory for every current payload span.

This feature does not:

- change accepted SQL syntax or MySQL-visible DDL semantics;
- retain partition clauses in the MyLite AST;
- change parser retry resource budgets (`SQL-02`);
- change syntax-error text or diagnostic truncation (`SQL-04`);
- change the Lemon stack limit (`SQL-05`);
- reorganize retry recognizers (`ARCH-03`);
- change public ABI, serialized data, dependencies, or SQLite.

## Span Inventory

Every node always has one primary `span`. The following payloads additionally
own source spans:

| AST kind | Payload spans |
| --- | --- |
| `INTEGER_TYPE` | display width |
| `VARCHAR_TYPE` | length |
| `CHAR_TYPE` | length |
| `TEXT_TYPE` | length |
| `BINARY_STRING_TYPE` | length |
| `BIT_TYPE` | length |
| `YEAR_TYPE` | width |
| `DECIMAL_TYPE` | precision, scale |
| `APPROXIMATE_TYPE` | precision, scale |
| `DATE_TYPE` | fractional precision |
| `DATETIME_TYPE` | fractional precision |
| `TIMESTAMP_TYPE` | fractional precision |
| `TIME_TYPE` | fractional precision |

An absent optional payload span is the canonical all-zero span. It remains
all-zero during validation, source-length rebasing, and snapshot cloning.
Every active span has nonnull text and participates in the full invariant.

## AST-Wide Invariant

For an AST associated with `(source, source_length)`, every active primary or
payload span must satisfy:

```text
offset <= source_length
length <= source_length - offset
span.text == source + offset
span.source_length == source_length
```

The invariant applies to every allocated AST node, not only nodes reachable
from the published root. This preserves the current arena-wide validation
contract and detects partially constructed or detached nodes.

`mylite_sql_ast_spans_are_within_source()` returns false on the first invalid
primary or payload span. The final success check in `mylite_sql_parse()` then
rejects an inconsistent retry result instead of publishing it.

## Shared Span Registration

The AST implementation must have one authoritative mapping from node kind to
payload-span member offsets. Validation, source-length rebasing, and snapshot
cloning all iterate that mapping. Separate switches are not acceptable because
they can drift when a payload gains another span.

The mapping contains no heap allocation and adds constant work per node: one
primary span plus at most two payload spans. A static capacity assertion and a
native inventory test document the current maximum. Adding a new payload span
requires updating the mapping and the exhaustive type-family test.

## Source-Length Rebase

`mylite_sql_ast_rebase_source_length(ast, source, old_length, new_length)`:

1. rejects `new_length < old_length`;
2. validates every active primary and payload span against `source` and
   `old_length`;
3. changes `source_length` to `new_length` on every active span;
4. leaves offsets, lengths, and pointers unchanged;
5. performs no mutation when validation fails.

The two-pass rule preserves atomicity. Callers never observe an AST with only
some source lengths changed.

## Snapshot Contract

Snapshot cloning retains its iterative traversal and owned source copy. For
every cloned node it:

- copies the node;
- rebases every active primary and payload span into the owned source;
- converts offsets from full-source coordinates to snapshot-subtree
  coordinates;
- sets every active span's source length to the snapshot source length;
- rejects a span outside the snapshotted root interval;
- preserves canonical all-zero optional spans.

The same registered span inventory drives this operation. After cloning,
validating the snapshot nodes against `snapshot.source` and the snapshot
root's source length must succeed without access to the original SQL buffer.

## Grammar And Runtime Behavior

No Lemon grammar, lexer, analyzer, runtime, metadata, or storage change is
required. This is an AST ownership and validation correction around existing
syntax.

Direct parsing, execution, buffered execution, and prepare continue to return
their existing MySQL-compatible results for valid statements. If a future
retry constructs an invalid span, parsing fails with the existing syntax
status rather than publishing unsafe internal pointers. Allocation behavior
and fatal-status precedence remain unchanged.

## Tests

The MySQL 8.4.9 fixture pins:

- exact warnings for the deprecated display-width, `YEAR(4)`, and floating
  precision forms;
- column-type normalization;
- numeric precision and scale;
- character maximum lengths;
- temporal fractional precision;
- successful connection reuse.

Native tests must cover:

- the full partition-placeholder retry reproducer;
- every span-bearing payload kind in the inventory;
- active-span source pointer, offset, length, and source-length equality;
- canonical all-zero optional spans;
- successful snapshots after the original SQL buffer is overwritten;
- validation rejection for wrong pointer, source length, offset, and length in
  both primary and payload spans;
- atomic no-mutation behavior when any later payload span is invalid;
- the `ALTER TABLE` option-tail prefix retry with a span-bearing type;
- primary-grammar ASTs as a non-retry control;
- snapshot interval rejection for a payload span outside the selected root;
- deep iterative snapshot behavior and allocator-failure cleanup.

Qualification must include focused tests in Development, Debug-CI, Release,
ASan/UBSan, and deterministic allocator profiles; all parser native suites;
the pinned parser MySQL fixtures; parser fuzzing; formatting; full static
analysis; ABI/install-consumer checks; and the production size gate.
