# SQLite Fork Type Coercion Foundation

## Status

This is the first executable type-coercion slice for the SQLite-fork direction.
It does not claim complete MySQL assignment conversion. It creates the first
native SQLite hooks that MyLite can use when lowering DML into SQLite writes.

Implemented scope:

- native SQLite scalar hooks for strict assignment coercion to signed integer,
  supported unsigned integer, `DOUBLE`, and `VARCHAR`
- MyLite `INSERT` lowering wraps supported target-column placeholders with those
  hooks
- MySQL 8.4.9 verified success fixture covering numeric strings, numeric-to-text
  conversion, multi-byte `VARCHAR` length, and update assignment coercion

Deferred scope:

- exact MySQL error codes, SQLSTATE, warning records, and `IGNORE` demotion for
  every conversion failure
- full unsigned `BIGINT` above `INT64_MAX`
- `DECIMAL`, temporal, JSON, `ENUM`, `SET`, bit, binary string, and spatial
  assignment conversion
- assignment-aware `UPDATE` lowering that can coerce only changed target columns
  without revalidating unchanged legacy stored values
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
```

Additional strict-mode probes establish the first failure categories:

| Statement shape | MySQL 8.4.9 result |
| --- | --- |
| `TINYINT` assignment of `128` | error 1264, SQLSTATE `22003` |
| `INT UNSIGNED` assignment of `-1` | error 1264, SQLSTATE `22003` |
| `VARCHAR(4)` assignment of five characters | error 1406, SQLSTATE `22001` |
| `DOUBLE` assignment of `'bad'` | error 1265, SQLSTATE `01000` under the default strict DML path |

This slice records the strict failure as a SQLite execution error from the
native hook. Exact MySQL diagnostic codes and warning promotion remain a later
diagnostics slice.

## Runtime Design

MyLite keeps the logical MySQL descriptor authoritative. SQLite remains the row
storage and bytecode executor. The DML lowering layer must therefore emit
target-type coercion at each assignment boundary rather than trusting SQLite
affinity.

For supported `INSERT` assignments, MyLite emits:

```sql
INSERT INTO "physical_table"("i","s","d")
VALUES(
    _mylite_coerce_signed_integer(?, -2147483648, 2147483647),
    _mylite_coerce_varchar(?, 4),
    _mylite_coerce_double(?)
)
```

The current `UPDATE` executor rewrites complete physical rows. Automatic update
coercion is therefore deferred until the lowering layer can wrap only the
changed assignment targets. This avoids revalidating unrelated stored values
when an older table was produced before a given type conversion rule existed.

The functions are private SQLite-fork primitives. They are not MySQL user
functions. They are deterministic for a given value and argument list, and they
return `NULL` unchanged so `NOT NULL` constraints remain owned by the physical
table and higher-level MyLite diagnostics.

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
- successful MyLite public API `UPDATE` behavior for the same fixture shape,
  while assignment-aware native update coercion remains deferred
- native SQLite failure behavior for out-of-range integer, negative unsigned,
  over-length `VARCHAR`, and invalid `DOUBLE`
- MySQL fixture diff against `mysql-basic-type-coercion.expected.tsv`

## Compatibility Status

This feature is `🟡` because it establishes the first native assignment
coercion primitive and uses it from supported MyLite `INSERT` lowering, while
full MySQL assignment conversion, assignment-aware `UPDATE` lowering, and
diagnostics remain deferred.
