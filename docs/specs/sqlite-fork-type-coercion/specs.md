# SQLite Fork Type Coercion Foundation

## Status

This is the first executable type-coercion slice for the SQLite-fork direction.
It does not claim complete MySQL assignment conversion. It creates the first
native SQLite hooks that MyLite can use when lowering DML into SQLite writes.

Implemented scope:

- native SQLite scalar hooks for strict assignment coercion to signed integer,
  supported unsigned integer, `DOUBLE`, and `VARCHAR`
- native SQLite column descriptor hooks that emit VDBE write-time coercion for
  signed integer, supported unsigned integer, `DOUBLE`, `VARCHAR`, `BINARY`,
  `VARBINARY`, text families, blob families, `DECIMAL`, `DATE`, `DATETIME`,
  and `TIME`
- MyLite public write-table loading attaches catalog-derived descriptors before
  preparing physical SQLite writes
- MyLite `INSERT`, `UPDATE`, `REPLACE`, and duplicate-key update lowering uses
  raw physical placeholders and relies on the native descriptor opcode
- SQLite-fork `UPDATE` type checks use SQLite's changed-column mask, so
  explicitly assigned columns are coerced without revalidating unchanged stored
  values
- fork type-check failures publish the first structured MySQL condition codes
  through the SQLite diagnostics bridge
- MySQL 8.4.9 verified success fixture covering numeric strings, numeric-to-text
  conversion, multi-byte `VARCHAR` length, update assignment coercion,
  duplicate-key update coercion, and replacement-row coercion

Deferred scope:

- exact MySQL error messages, row interpolation, complete warning records, and
  `IGNORE` demotion for every conversion failure
- full unsigned `BIGINT` above `INT64_MAX`
- `TIMESTAMP`, `YEAR`, JSON, `ENUM`, `SET`, bit, and spatial assignment
  conversion
- decimal-aware comparison/index ordering, compact decimal storage, and direct
  SQLite parser numeric-literal preservation
- direct SQLite parser/catalog reload into descriptors for SQL executed without
  MyLite's current statement layer
- non-strict SQL mode clipping/truncation behavior
- direct SQLite parser execution of MySQL DDL/DML without MyLite lowering

## Sources

- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Out-of-Range and Overflow Handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, String Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, Numeric Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- SQLite application-defined SQL functions:
  https://www.sqlite.org/c3ref/create_function.html
- SQLite dynamic type system:
  https://www.sqlite.org/datatype3.html
- Existing MyLite native execution plan:
  `docs/architecture/native-sqlite-execution-plan.md`
- SQLite fork binary string type descriptors:
  `docs/specs/sqlite-fork-binary-string-types/specs.md`
- SQLite fork decimal type descriptors:
  `docs/specs/sqlite-fork-decimal-type-descriptors/specs.md`
- SQLite fork temporal type descriptors:
  `docs/specs/sqlite-fork-temporal-type-descriptors/specs.md`
- SQLite fork time type descriptors:
  `docs/specs/sqlite-fork-time-type-descriptors/specs.md`
- SQLite fork text/blob family descriptors:
  `docs/specs/sqlite-fork-text-blob-family-descriptors/specs.md`

This specification is independently authored from official documentation,
observed MySQL 8.4.9 runtime behavior, and the current MyLite codebase. It does
not copy MySQL, MariaDB, Percona, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Behavior Baseline

Runtime probes for this feature were executed on 2026-05-06 against the
official `mysql:8.4.9` Docker image in container `mylite-mysql-849`, using
MySQL's default strict SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

For the fixture in `mysql-basic-type-coercion.sql`, MySQL produced:

```text
after-insert	1	42	7	123	3.25	1
after-insert	2	-5	0	éé	4	0
after-update	1	42	7	123	3.25	1
after-update	2	12	8	99	6.5	0
after-duplicate-update	1	42	7	123	3.25	1
after-duplicate-update	2	9	10	77	8.75	1
after-replace	1	42	7	123	3.25	1
after-replace	2	9	10	77	8.75	1
after-replace	3	11	12	456	9.25	0
```

Additional strict-mode probes establish the first failure categories:

| Statement shape | MySQL 8.4.9 result |
| --- | --- |
| `TINYINT` assignment of `128` | error 1264, SQLSTATE `22003` |
| `INT UNSIGNED` assignment of `-1` | error 1264, SQLSTATE `22003` |
| `VARCHAR(4)` assignment of five characters | error 1406, SQLSTATE `22001` |
| `DOUBLE` assignment of `'bad'` | error 1265, SQLSTATE `01000` under the default strict DML path |

This slice records the strict failure as a SQLite execution error from the
native hook. The first fork diagnostics bridge now publishes MySQL condition
codes and SQLSTATE values for these failures; exact MySQL message text and
warning demotion remain later diagnostics work.

The fixture in
`docs/specs/sqlite-fork-decimal-type-descriptors/mysql-decimal-coercion.sql`
establishes the first supported exact fixed-point assignment behavior:
half-away rounding to scale, post-round overflow errors, unsigned-negative
errors, and canonical fixed-scale display text.

The fixture in
`docs/specs/sqlite-fork-temporal-type-descriptors/mysql-temporal-coercion.sql`
establishes the first supported temporal assignment behavior: canonical and
compact `DATE`/`DATETIME` inputs, two-digit year expansion, fractional-second
rounding to declared `DATETIME(fsp)` precision, date/time carry, invalid
temporal errors, and post-round datetime overflow errors.

The fixture in
`docs/specs/sqlite-fork-time-type-descriptors/mysql-time-coercion.sql`
establishes the first supported `TIME(fsp)` assignment behavior: elapsed-time
hours, negative values, day-plus-time strings, colon abbreviations, compact
numeric forms, fractional rounding, strict range checks, and canonical text
storage.

The fixture in
`docs/specs/sqlite-fork-text-blob-family-descriptors/mysql-text-blob-family-coercion.sql`
establishes the first supported text/blob family assignment behavior: text
byte-capacity checks, multibyte UTF-8 boundary failures, blob byte-capacity
checks, numeric-to-string assignment, SQLite TEXT/BLOB storage class
canonicalization, and 1406/22001 diagnostics.

## Runtime Design

MyLite keeps the logical MySQL descriptor authoritative. SQLite remains the row
storage and bytecode executor. The DML lowering layer must therefore emit
target-type coercion at each assignment boundary rather than trusting SQLite
affinity.

For supported assignments, MyLite attaches descriptors to SQLite's in-memory
table metadata and emits ordinary physical placeholders:

```sql
INSERT INTO "physical_table"("i","s","d")
VALUES(?, ?, ?)
```

SQLite's code generator sees the table descriptors and emits
`OP_MyliteTypeCheck` before record creation. This applies to public MyLite
`INSERT`, single-table `UPDATE`, `REPLACE`, and `ON DUPLICATE KEY UPDATE`
paths that prepare physical SQLite writes after loading the target table.
For `UPDATE`, the fork uses SQLite's existing changed-column map to emit
descriptor checks only for columns in the assignment set. This matches the
assignment boundary: unchanged stored values are copied into the new record but
are not re-coerced solely because another column is updated.

The functions are private SQLite-fork primitives. They are not MySQL user
functions. They are deterministic for a given value and argument list, and they
return `NULL` unchanged so `NOT NULL` constraints remain owned by the physical
table and higher-level MyLite diagnostics.

The source-tree fork also exposes a descriptor path for the same first type
subset. MyLite can attach type descriptors to SQLite's in-memory `Column`
objects through `mylite_sqlite_fork_set_column_type()`. When a table has at
least one descriptor, SQLite emits `OP_MyliteTypeCheck` before record creation
for writes to that table. This moves assignment conversion into the VDBE for
direct SQLite `INSERT` and `UPDATE` statements while preserving ordinary SQLite
affinity for unannotated columns.

## Initial Semantics

### Integer assignment

The signed integer hook accepts SQLite integer, real, text, and blob values that
can be interpreted as a finite numeric value. Decimal values round half away
from zero before range validation, matching observed MySQL assignment behavior
for `INT` and `TINYINT` under strict mode.

The unsigned hook currently supports the SQLite signed-integer storage range.
It rejects negative values and values above the configured maximum. Full
`BIGINT UNSIGNED` values above `INT64_MAX` require a later physical encoding.

### `DOUBLE` assignment

The `DOUBLE` hook accepts finite integer, real, text, and blob values that parse
as a full numeric token. It rejects invalid text and non-finite values. Exact
MySQL warning text and code promotion are deferred.

### `VARCHAR` assignment

The `VARCHAR` hook converts non-`NULL` inputs through SQLite's text
representation and validates the UTF-8 character count against the column's
declared character length. This preserves numeric-to-text insertion for the
supported UTF-8 path and rejects over-length text in the strict path.

### `DECIMAL` assignment

The `DECIMAL` descriptor carries precision, scale, and unsigned state. The VDBE
hook parses exact decimal text when it is available, rounds the magnitude half
away from zero to the declared scale, rejects post-round integer precision
overflow, rejects negative unsigned assignments, and stores canonical
fixed-scale text. Direct SQLite numeric literals may already be approximate by
the time the hook runs; full direct-parser fidelity requires a later
numeric-literal preservation hook in the SQLite parser/code generator.

### `DATE` and `DATETIME` assignment

The `DATE` descriptor validates supported date and datetime-shaped inputs and
stores canonical `YYYY-MM-DD` text. The `DATETIME` descriptor carries
fractional seconds precision `0..6`, validates supported date/time inputs,
rounds fractional seconds to the declared precision, applies carries, rejects
post-round overflow, and stores canonical text with the declared fractional
scale.

Zero-date and zero-in-date behavior currently follows the MySQL default strict
mode fixture and rejects those values. Non-strict SQL modes and warning records
for accepted date truncation remain deferred.

### `TIME` assignment

The `TIME` descriptor carries fractional seconds precision `0..6`. The VDBE
hook accepts elapsed-time strings, negative values, day-plus-time strings,
colon-abbreviated values, compact numeric/text forms, and fractional seconds.
It rounds fractional seconds to the declared precision, rejects values outside
MySQL's `-838:59:59.000000` through `838:59:59.000000` range, canonicalizes
negative zero to positive zero, and stores canonical text.

### Text and blob family assignment

The text-family descriptor carries a byte-capacity limit for `TINYTEXT`,
`TEXT`, `MEDIUMTEXT`, and `LONGTEXT`. The VDBE hook converts non-`NULL` inputs
to UTF-8 text, enforces byte length rather than character count, and keeps
SQLite TEXT storage.

The blob-family descriptor carries a byte-capacity limit for `TINYBLOB`,
`BLOB`, `MEDIUMBLOB`, and `LONGBLOB`. The VDBE hook preserves text/blob bytes,
stringifies numeric inputs, enforces byte length, and stores SQLite BLOB values
without fixed-length padding.

## Lemon Grammar Direction

No new MyLite grammar is introduced in this slice. The long-term fork grammar
work remains the same as the SQLite fork CRUD foundation: MySQL column types and
DML forms should be accepted by SQLite's Lemon grammar directly, and the parser
should attach enough type metadata for the schema builder and VDBE generator to
emit these coercion hooks without MyLite's separate AST lowering.

## Tests

The executable tests must cover:

- direct native SQLite calls to the coercion hooks
- successful MyLite public API `INSERT` coercion for numeric strings,
  numeric-to-`VARCHAR`, UTF-8 character-count length, `DOUBLE`, and `NULL`
- successful MyLite public API `UPDATE`, `ON DUPLICATE KEY UPDATE`, and
  `REPLACE` behavior for the same fixture shape
- native SQLite failure behavior for out-of-range integer, negative unsigned,
  over-length `VARCHAR`, and invalid `DOUBLE`
- native SQLite failure behavior for over-length `BINARY` and `VARBINARY`
  through the binary string descriptor slice
- native SQLite failure behavior for invalid decimal text, post-round decimal
  overflow, and unsigned-negative decimal assignment through the decimal
  descriptor slice
- native SQLite failure behavior for invalid dates, invalid datetimes, and
  post-round datetime overflow through the temporal descriptor slice
- native SQLite failure behavior for invalid and out-of-range `TIME` values
  through the time descriptor slice
- native SQLite failure behavior for over-length text/blob family values
  through the text/blob family descriptor slice
- direct SQLite `INSERT` and `UPDATE` behavior through MyLite column descriptors
  without SQL wrapper functions
- direct SQLite `UPDATE` behavior that coerces assigned descriptor columns
  without revalidating unchanged legacy stored values
- MySQL fixture diffs against `mysql-basic-type-coercion.expected.tsv`,
  `mysql-decimal-coercion.expected.tsv`, and
  `mysql-temporal-coercion.expected.tsv`, and
  `mysql-time-coercion.expected.tsv`, and
  `mysql-text-blob-family-coercion.expected.tsv`

## Compatibility Status

This feature is `🟡` because it establishes the first native assignment
coercion primitive and now uses direct VDBE write-time coercion through MyLite
column descriptors for the common public write paths. Full MySQL assignment
conversion, temporal SQL-mode/warning coverage, decimal-aware comparison/index
ordering, direct SQLite parser/catalog descriptor reload, and exact diagnostics
remain deferred.
