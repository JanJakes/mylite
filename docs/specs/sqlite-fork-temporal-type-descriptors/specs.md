# SQLite Fork Temporal Type Descriptors

## Status

This slice extends the SQLite-fork column descriptor mechanism with the first
temporal assignment types:

- `DATE`
- `DATETIME` and `DATETIME(fsp)`, with fractional seconds precision `0..6`
- strict invalid date/time diagnostics for covered assignments
- fractional-second rounding to the declared precision, including carry into
  the next second, day, month, or year
- canonical text storage for covered writes
- opt-in zero-temporal sentinel acceptance for statement paths that need it
  before full SQL-mode warning demotion exists
- public MyLite DML descriptor loading from cataloged temporal metadata
- direct SQLite descriptor tests and public MyLite CRUD tests covering
  `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`

Deferred scope:

- `TIME`, `TIMESTAMP`, and `YEAR` value assignment descriptors
- session time zone conversion and `TIMESTAMP` default/on-update behavior
- SQL-mode-specific zero date, zero-in-date, invalid-date, and non-strict
  warning behavior
- warning records for accepted-but-adjusted `DATE` assignments
- temporal comparison/index ordering beyond canonical text ordering
- direct SQLite parser/catalog support for MySQL temporal declarations
- exact MySQL message text with schema, table, column, and row interpolation

## Sources

- MySQL 8.4 Reference Manual, Date and Time Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-type-syntax.html
- MySQL 8.4 Reference Manual, The DATE, DATETIME, and TIMESTAMP Types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, Fractional Seconds in Time Values:
  https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
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
default strict SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

For `mysql-temporal-coercion.sql`, MySQL produced the fixture in
`mysql-temporal-coercion.expected.tsv`. The observed behavior establishes:

- `DATE` accepts canonical `YYYY-MM-DD`, compact `YYYYMMDD`, and compact
  two-digit-year forms such as `240101`.
- `DATE` assignment from a datetime-shaped value stores the date part after
  validating and rounding the time portion; `23:59:59.9` can carry into the
  next day.
- `DATETIME` accepts canonical date/time text, `T` as a date/time separator,
  date-only text as midnight, compact `YYYYMMDDHHMMSS`, compact
  `YYMMDDHHMMSS`, and fractional seconds.
- `DATETIME(fsp)` rounds fractional seconds to `fsp` digits rather than
  truncating. Rounding can carry into the next second or day.
- Two-digit years use MySQL's observed window: `00..69` maps to
  `2000..2069`, and `70..99` maps to `1970..1999`.
- The text protocol and `CAST(datetime_column AS CHAR)` display the declared
  fractional scale for `DATETIME(fsp)`.

Strict failure probes produced:

| Statement shape | MySQL 8.4.9 result |
| --- | --- |
| `DATE` assignment of `'2024-02-30'` | error 1292, SQLSTATE `22007` |
| `DATE` assignment of `'0000-00-00'` under default strict mode | error 1292, SQLSTATE `22007` |
| `DATETIME` assignment of `'2024-02-30 00:00:00'` | error 1292, SQLSTATE `22007` |
| `DATETIME` assignment of `'2024-01-01 24:00:00'` | error 1292, SQLSTATE `22007` |
| `DATETIME` assignment that rounds beyond `9999-12-31 23:59:59` | error 1441, SQLSTATE `22008` |

## Runtime Design

Temporal assignment belongs in the same forked SQLite write boundary as the
integer, floating, varchar, binary-string, and decimal descriptors. SQLite
scalar functions can parse temporal values, but they require every write path
to be wrapped manually and cannot cover direct writes through the SQLite parser
or all record-construction sites.

The descriptor carries:

- kind `DATE`, with no fractional precision
- kind `DATETIME`, with fractional seconds precision `0..6`
- flag `ALLOW_ZERO_TEMPORAL`, which allows only the all-zero date sentinel
  (`0000-00-00`) and all-zero datetime sentinel
  (`0000-00-00 00:00:00`) for assignment contexts that MySQL accepts with
  warning semantics

The VDBE coercer converts non-`NULL` input to a byte string, parses the
supported MySQL temporal input forms, validates calendar and time fields,
rounds fractional seconds to the target precision, checks post-round overflow,
and stores canonical text:

- `DATE` as `YYYY-MM-DD`
- `DATETIME` as `YYYY-MM-DD HH:MM:SS`
- `DATETIME(fsp)` as `YYYY-MM-DD HH:MM:SS.ffffff` truncated to the declared
  fractional digit count after rounding

Invalid temporal values publish MySQL condition 1292 with SQLSTATE `22007`.
Post-round datetime overflow publishes condition 1441 with SQLSTATE `22008`.
Zero temporal values without the descriptor flag use the same invalid temporal
diagnostic as the strict MySQL 8.4.9 baseline. The flag does not allow partial
zero dates or zero dates with nonzero times.

MyLite's public DML layer preserves source literal text for `DATE` and
`DATETIME` target columns before binding values into SQLite, so compact and
fractional temporal literals reach the descriptor without premature numeric
rounding. The public bridge currently enables `ALLOW_ZERO_TEMPORAL` for
`INSERT IGNORE` write descriptors so implicit missing-value behavior can store
MySQL's zero temporal sentinel; normal `INSERT`, `UPDATE`, duplicate-update,
and `REPLACE` descriptors remain strict.

## Existing SQLite Extension Surface

SQLite extension APIs remain useful for temporal scalar functions and
application-visible instrumentation, but they are not enough for assignment:

- scalar functions require custom wrapper SQL around every generated write
  expression;
- update and preupdate hooks observe rows too late to replace assigned values;
- CHECK constraints can reject bad values but cannot round, normalize, and
  store MySQL-compatible text;
- collations affect comparisons, not assignment conversion;
- virtual tables would replace ordinary SQLite b-tree storage for user tables
  and add a large runtime cost.

The descriptor/VDBE fork point is therefore the correct native integration
point for write-time temporal assignment. Later temporal work will still need
additional fork points for direct parser type declarations, session time-zone
state, SQL-mode warning demotion, and possibly temporal-aware comparison for
non-canonical storage formats.

## Lemon Grammar Direction

No grammar is added in this slice. MyLite already parses `DATE`,
`DATETIME`, and `DATETIME(fsp)` declarations and records catalog metadata.
The future SQLite parser fork should parse the same declarations into
`Column` descriptors directly so direct SQLite DDL can attach these write
semantics without MyLite's current statement layer.

## Tests

Executable coverage includes:

- direct SQLite descriptor `INSERT` and `UPDATE` with canonical, compact,
  numeric, and fractional inputs
- direct SQLite strict invalid-date, invalid-datetime, and post-round overflow
  failures with fork condition publication
- direct SQLite zero-temporal sentinel acceptance only when the descriptor
  carries `ALLOW_ZERO_TEMPORAL`
- public MyLite `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`
  matching the MySQL fixture
- public MyLite strict failure coverage for invalid dates, invalid datetimes,
  and post-round datetime overflow
- MySQL fixture diff against `mysql-temporal-coercion.expected.tsv`

## Compatibility Status

This feature is `🟡`: the native write-path primitive is implemented for basic
`DATE` and `DATETIME` assignment, but zero-date SQL modes, accepted-assignment
warnings, `TIME`, `TIMESTAMP`, `YEAR`, direct parser integration, and full
temporal protocol metadata remain future work.
