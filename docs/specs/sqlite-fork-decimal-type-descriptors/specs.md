# SQLite Fork DECIMAL Type Descriptors

## Status

This slice extends the SQLite-fork column descriptor mechanism with the first
exact fixed-point assignment type:

- `DECIMAL(p,s)` and `NUMERIC(p,s)` precision/scale validation at assignment
- strict rounding half away from zero to the declared scale
- post-round range checks against the declared integer precision
- unsigned decimal rejection of negative values
- canonical fixed-scale text storage for covered writes
- public MyLite DML descriptor loading from cataloged numeric metadata
- direct SQLite descriptor tests and public MyLite CRUD tests covering
  `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`

Deferred scope:

- a compact binary decimal storage format
- decimal-aware comparison, ordering, aggregation, and index keys
- direct SQLite parser preservation of exact MySQL numeric literal text
- exact MySQL message text with schema, table, and row interpolation
- non-strict clipping and warning demotion
- full expression-engine decimal arithmetic

## Sources

- MySQL 8.4 Reference Manual, Fixed-Point Types:
  https://dev.mysql.com/doc/refman/8.4/en/fixed-point-types.html
- MySQL 8.4 Reference Manual, Numeric Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, Out-of-Range and Overflow Handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- SQLite dynamic type system:
  https://www.sqlite.org/datatype3.html
- SQLite fork type coercion and diagnostics specs:
  `docs/specs/sqlite-fork-type-coercion/specs.md` and
  `docs/specs/sqlite-fork-diagnostics-bridge/specs.md`

This specification is independently authored from official documentation,
observed MySQL 8.4.9 runtime behavior, and the current MyLite codebase.

## MySQL 8.4.9 Behavior Baseline

Runtime probes were executed on 2026-05-06 against the official
`mysql:8.4.9` Docker image in container `mylite-mysql-849`, using MySQL's
default strict SQL mode.

For `mysql-decimal-coercion.sql`, MySQL produced the fixture in
`mysql-decimal-coercion.expected.tsv`. The observed behavior establishes:

- `DECIMAL(5,2)` rounds `1.234` to `1.23`, `-1.235` to `-1.24`, and `2.225`
  to `2.23`.
- `DECIMAL(4,0)` rounds `12.5` to `13` and `-12.5` to `-13`.
- `DECIMAL(5,2) UNSIGNED` rejects negative values and still rounds accepted
  values to two fractional digits.
- `CAST(decimal_column AS CHAR)` and the default text protocol both include
  the declared fractional scale for scaled decimal values.

Strict failure probes produced:

| Statement shape | MySQL 8.4.9 result |
| --- | --- |
| `DECIMAL(5,2)` assignment of `1000.00` | error 1264, SQLSTATE `22003` |
| `DECIMAL(5,2)` assignment of `999.995` after rounding | error 1264, SQLSTATE `22003` |
| `DECIMAL(5,2)` assignment of `'abc'` | error 1366, SQLSTATE `HY000` |
| `DECIMAL(5,2)` assignment of `'12abc'` | error 1366, SQLSTATE `HY000` |
| `DECIMAL(5,2) UNSIGNED` assignment of `-0.01` | error 1264, SQLSTATE `22003` |

## Runtime Design

Decimal assignment belongs in the same forked SQLite write boundary as integer,
floating, varchar, and binary-string assignment. SQLite's public extension API
can add scalar functions, but it cannot mutate every native row-write path at a
per-column assignment boundary or enforce precision and scale without generated
SQL wrappers.

The descriptor carries:

- precision `p`, up to MySQL's `65`
- scale `s`, up to MySQL's `30`
- an unsigned flag

The VDBE decimal coercer parses non-`NULL` input into an exact decimal text
model when text is available, rounds the magnitude half away from zero to the
declared scale, checks the post-round integer digit count, and stores a
canonical text value with exactly `s` fractional digits. Zero is stored without
a sign.

For direct SQLite SQL literals, SQLite may already have converted numeric
tokens into integer or floating registers before `OP_MyliteTypeCheck` runs.
The descriptor still handles those values, but exact MySQL decimal literal
fidelity requires a future parser/code-generation hook that preserves MySQL
numeric literal text until assignment coercion. MyLite's current public DML
layer already has the original literal text, so this slice preserves that text
when assigning to decimal columns.

Invalid decimal text publishes MySQL condition 1366 with SQLSTATE `HY000`.
Out-of-range and unsigned-negative assignment publishes condition 1264 with
SQLSTATE `22003`.

## Existing SQLite Extension Surface

SQLite extension APIs are useful for decimal scalar functions and future
decimal-aware collations, but not sufficient for assignment:

- scalar functions require wrapping every generated write expression;
- update hooks and preupdate hooks cannot replace values before record
  construction;
- CHECK constraints can reject range but cannot round and normalize the stored
  value;
- collations affect comparisons, not assignment conversion;
- virtual tables would replace SQLite's ordinary b-tree storage and add a large
  runtime cost.

The descriptor/VDBE fork point remains the right native integration point for
write-time MySQL decimal assignment. A later comparison/index slice will need a
decimal comparison hook so text storage does not leak lexicographic ordering.

## Lemon Grammar Direction

No grammar is added in this slice. MyLite already parses decimal declarations
and records catalog precision/scale. The future SQLite parser fork should parse
the same declarations into `Column` descriptors directly and preserve exact
numeric literal text for assignment code generation.

## Tests

Executable coverage must include:

- direct SQLite descriptor `INSERT` and `UPDATE` with string and numeric inputs
- direct SQLite strict invalid-text, out-of-range, and unsigned-negative
  failures with fork condition publication
- public MyLite `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`
  matching the MySQL fixture
- public MyLite strict failure coverage for invalid decimal text, post-round
  overflow, and unsigned-negative assignment
- MySQL fixture diff against `mysql-decimal-coercion.expected.tsv`

## Compatibility Status

This feature is `🟡`: the native write-path primitive is implemented for basic
`DECIMAL` assignment, but full exact arithmetic, decimal comparisons, compact
storage, non-strict modes, and direct SQLite parser cataloging remain future
work.
