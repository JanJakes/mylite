# Temporal column types

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` column type foundation
with MySQL temporal declarations:

- `DATE`
- `TIME`, `TIME(fsp)`
- `DATETIME`, `DATETIME(fsp)`
- `TIMESTAMP`, `TIMESTAMP(fsp)`
- `YEAR`, `YEAR(4)`
- internal descriptor metadata for `DATA_TYPE`, `COLUMN_TYPE`, storage bytes,
  range text, and `DATETIME_PRECISION`
- parser acceptance and validation for valid declarations

Later executable `CREATE TABLE` work uses these descriptors for metadata and
physical text storage. The SQLite-fork descriptor slices now implement basic
strict write-time `DATE`, `DATETIME(fsp)`, and `TIME(fsp)` assignment
conversion for public MyLite DML and direct annotated SQLite writes.
`TIMESTAMP`, `YEAR`, `CURRENT_TIMESTAMP`, `ON UPDATE`, time zone conversion,
SQL-mode variants, zero-value insertion modes, casts, functions, warning
records, and protocol metadata remain later roadmap work.

## Sources

- MySQL 8.4 Reference Manual, Date and Time Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-type-syntax.html
- MySQL 8.4 Reference Manual, The DATE, DATETIME, and TIMESTAMP Types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, The TIME Type:
  https://dev.mysql.com/doc/refman/8.4/en/time.html
- MySQL 8.4 Reference Manual, The YEAR Type:
  https://dev.mysql.com/doc/refman/8.4/en/year.html
- MySQL 8.4 Reference Manual, Date and Time Type Storage Requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

Temporal declarations normalize to lowercase metadata:

| Declaration | `DATA_TYPE` | `COLUMN_TYPE` | `DATETIME_PRECISION` |
| --- | --- | --- | --- |
| `DATE` | `date` | `date` | `NULL` |
| `TIME` | `time` | `time` | `0` |
| `TIME(0)` | `time` | `time` | `0` |
| `TIME(1)` | `time` | `time(1)` | `1` |
| `TIME(6)` | `time` | `time(6)` | `6` |
| `DATETIME` | `datetime` | `datetime` | `0` |
| `DATETIME(0)` | `datetime` | `datetime` | `0` |
| `DATETIME(1)` | `datetime` | `datetime(1)` | `1` |
| `DATETIME(6)` | `datetime` | `datetime(6)` | `6` |
| `TIMESTAMP` | `timestamp` | `timestamp` | `0` |
| `TIMESTAMP(0)` | `timestamp` | `timestamp` | `0` |
| `TIMESTAMP(1)` | `timestamp` | `timestamp(1)` | `1` |
| `TIMESTAMP(6)` | `timestamp` | `timestamp(6)` | `6` |
| `YEAR` | `year` | `year` | `NULL` |
| `YEAR(4)` | `year` | `year` | `NULL` |

`YEAR(4)` is accepted but deprecated by MySQL warning 1287. MyLite records the
normalized descriptor now and defers warning storage.

`DATE` does not accept a precision. `TIME`, `DATETIME`, and `TIMESTAMP` accept
fractional seconds precision `0..6`; omitted precision defaults to `0`.
`YEAR` accepts only omitted width or `YEAR(4)`.

Invalid declarations observed against MySQL 8.4.9:

- `DATE(0)` and `DATE(1)` are syntax errors.
- `TIME(7)`, `DATETIME(7)`, and `TIMESTAMP(7)` error because maximum precision
  is 6.
- `YEAR(0)`, `YEAR(1)`, `YEAR(2)`, `YEAR(3)`, and `YEAR(5)` error because
  only display width 4 is valid.
- Oversized integer tokens such as `TIME(18446744073709551616)`,
  `DATETIME(18446744073709551616)`, and `YEAR(18446744073709551616)` must be
  rejected without integer wraparound.

Official behavior includes value ranges and zero values:

- `DATE` range is the MySQL date range, with zero-date behavior governed by
  assignment and SQL mode rules.
- `TIME` range includes negative and elapsed-time values and supports fractional
  seconds.
- `DATETIME` stores date and time without time zone conversion.
- `TIMESTAMP` stores an instant with time zone conversion and has special
  default/on-update rules affected by `explicit_defaults_for_timestamp`.
- `YEAR` stores four-digit display years plus `0000`, with input conversion
  rules for one- and two-digit inputs.

Those value semantics are documented here for future implementation context but
are intentionally outside this parse-only task.

## MyLite behavior

### Type descriptor

MyLite extends the internal descriptor with a temporal domain. Given a type
keyword and optional precision/width, it returns:

- canonical temporal family
- lowercase `DATA_TYPE`
- MySQL-compatible `COLUMN_TYPE`
- whether `DATETIME_PRECISION` is present
- effective `DATETIME_PRECISION` for `TIME`, `DATETIME`, and `TIMESTAMP`
- storage byte count for the nonfractional part plus fractional storage where
  applicable
- text range bounds for the type
- whether the declaration used a deprecated alias form such as `YEAR(4)`

Descriptor normalization is internal and not part of the public ABI.

### Parser and AST

MyLite accepts the existing narrow `CREATE TABLE` shape:

```sql
CREATE TABLE table_name (
    column_name column_type
    [, column_name column_type ...]
)
```

Column attributes such as `NULL`, `NOT NULL`, `DEFAULT`, comments, generated
columns, inline keys, table constraints, table options, temporary tables,
`CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT` remain out of scope.

Accepted type syntax for this task:

```sql
temporal_type ::= DATE
temporal_type ::= TIME [ ( fsp ) ]
temporal_type ::= DATETIME [ ( fsp ) ]
temporal_type ::= TIMESTAMP [ ( fsp ) ]
temporal_type ::= YEAR [ ( 4 ) ]
```

`fsp` and `YEAR` width use unsigned integer tokens only. Negative values,
missing values, extra values, and non-integer tokens fail as syntax errors.
MyLite performs parser-time range checks for the declaration limits covered by
this parse-only feature.

### Runtime boundary

Preparing a parse-only `CREATE TABLE` statement covered by this feature returns
`MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No SQLite table is created and
no MyLite catalog rows are written. Task 8 owns column attributes and default
syntax. Task 11 owns executable table DDL, metadata writes, warnings, implicit
commit semantics, and statement side effects.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
column_type ::= temporal_column_type.

temporal_column_type ::= DATE.
temporal_column_type ::= TIME opt_temporal_fsp.
temporal_column_type ::= DATETIME opt_temporal_fsp.
temporal_column_type ::= TIMESTAMP opt_temporal_fsp.
temporal_column_type ::= YEAR opt_year_width.

opt_temporal_fsp ::= .
opt_temporal_fsp ::= LPAREN INTEGER RPAREN.

opt_year_width ::= .
opt_year_width ::= LPAREN INTEGER RPAREN.
```

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or declaration | Expected MySQL-compatible outcome |
| --- | --- |
| `DATE` | `DATA_TYPE=date`, `COLUMN_TYPE=date`, null datetime precision |
| `TIME`, `TIME(0)`, `TIME(1)`, `TIME(6)` | `time`, `time`, `time(1)`, `time(6)` with precision 0, 0, 1, 6 |
| `DATETIME`, `DATETIME(0)`, `DATETIME(1)`, `DATETIME(6)` | `datetime`, `datetime`, `datetime(1)`, `datetime(6)` with precision 0, 0, 1, 6 |
| `TIMESTAMP`, `TIMESTAMP(0)`, `TIMESTAMP(1)`, `TIMESTAMP(6)` | `timestamp`, `timestamp`, `timestamp(1)`, `timestamp(6)` with precision 0, 0, 1, 6 |
| `YEAR`, `YEAR(4)` | `year` metadata; `YEAR(4)` has deferred warning 1287 |
| `DATE(0)`, `DATE(1)` | syntax error |
| `TIME(7)`, `DATETIME(7)`, `TIMESTAMP(7)` | parse error for this parse-only feature |
| `TIME(18446744073709551616)`, `DATETIME(18446744073709551616)` | parse error without integer wraparound |
| `YEAR(0)`, `YEAR(1)`, `YEAR(2)`, `YEAR(3)`, `YEAR(5)` | parse error |
| `YEAR(18446744073709551616)` | parse error without integer wraparound |

An additional runtime probe confirmed that leading zero precision tokens such as
`TIME(00)`, `DATETIME(06)`, `TIMESTAMP(000)`, and `YEAR(004)` are accepted and
normalized exactly like `TIME(0)`, `DATETIME(6)`, `TIMESTAMP(0)`, and `YEAR(4)`.

## Compatibility gaps

- Full temporal DDL formatting, warnings, implicit commits, and SHOW CREATE
  edge cases are deferred.
- `TIMESTAMP` default/nullability/on-update behavior is deferred to the column
  attributes and table DDL tasks.
- Time zone conversion, session time zone state, and `explicit_defaults_for_timestamp`
  are deferred.
- Zero date/time/year insertion modes, SQL modes such as `NO_ZERO_DATE`,
  `NO_ZERO_IN_DATE`, `ALLOW_INVALID_DATES`, and `TIME_TRUNCATE_FRACTIONAL`
  are deferred.
- Temporal literals, casts, functions, arithmetic, result metadata, protocol
  flags, and `TIMESTAMP`/`YEAR` assignment descriptors are deferred.
- Basic `DATE` and `DATETIME(fsp)` assignment conversion is implemented in the
  SQLite fork; see
  `docs/specs/sqlite-fork-temporal-type-descriptors/specs.md`.
- Basic `TIME(fsp)` assignment conversion is implemented in the SQLite fork;
  see `docs/specs/sqlite-fork-time-type-descriptors/specs.md`.
