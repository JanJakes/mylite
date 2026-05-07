# SHOW ENGINE STATUS

This feature implements the first executable slice of MySQL's singular storage
engine status statement:

- `SHOW ENGINE engine_name STATUS`

The first runtime surface returns an embedded InnoDB-compatible status row and
MySQL-shaped empty status sets for known engines that expose no status rows in
the verified MySQL runtime. `SHOW ENGINE ... MUTEX` and `SHOW ENGINE ... LOGS`
remain separate deferred surfaces.

## Sources

- MySQL 8.4 Reference Manual, `SHOW ENGINE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-engine.html>
- MySQL 8.4.9 runtime probes run against the local `mylite-mysql-849`
  container, covering `INNODB`, `PERFORMANCE_SCHEMA`, `MEMORY`, `MyISAM`,
  `CSV`, `BLACKHOLE`, `ARCHIVE`, `FEDERATED`, invalid engines, `MUTEX`, `LOGS`,
  casing, metadata, warnings, and rejected suffixes.

## Syntax

```lemon
show_engine_status_statement ::= SHOW ENGINE identifier STATUS.
```

The engine name is a single identifier, including quoted identifiers. `LIKE`,
`WHERE`, and `LIMIT` suffixes are syntax errors for this statement. The `MUTEX`
and `LOGS` variants are intentionally not parsed in this slice.

## Result Metadata

`SHOW ENGINE ... STATUS` returns three columns:

| Column | Type | Charset | Length | Decimals | Nullability |
| --- | --- | --- | --- | --- | --- |
| `Type` | `VAR_STRING` | latin1 / id 8 | 10 | 31 | not null |
| `Name` | `VAR_STRING` | latin1 / id 8 | 512 | 31 | not null |
| `Status` | `VAR_STRING` | latin1 / id 8 | 10 | 31 | not null |

The `Status` column can contain text longer than the declared length in MySQL;
the result-column descriptor still reports length `10`.

## Runtime Semantics

`SHOW ENGINE INNODB STATUS` returns one embedded compatibility row:

| `Type` | `Name` | `Status` |
| --- | --- | --- |
| `InnoDB` | empty string | MyLite embedded status text |

The status text is a deterministic MyLite summary rather than MySQL's volatile
InnoDB monitor output. It must identify the SQLite-backed single-file storage
facade and report transaction, XA, and savepoint capability consistently with
`SHOW ENGINES`.

The following known engine names return an empty result set with the same
metadata:

- `MEMORY`
- `MyISAM`
- `MRG_MYISAM`
- `BLACKHOLE`
- `CSV`
- `ARCHIVE`
- `PERFORMANCE_SCHEMA`

The verified MySQL runtime reports extensive `PERFORMANCE_SCHEMA` memory rows,
but MyLite has no Performance Schema instrumentation table implementation yet.
Returning an empty metadata-compatible result is the first embedded-compatible
surface until that instrumentation exists.

Unknown engines fail with error 1286:

```text
Unknown storage engine '<engine>'
```

Successful `SHOW ENGINE ... STATUS` statements produce no warnings and clear
prior diagnostics as nondiagnostic `SHOW` statements.

## Tests

Tests cover:

- parser acceptance for `SHOW ENGINE INNODB STATUS`, lowercase engine names,
  and quoted engine names
- parser rejection for missing engine names, missing `STATUS`, `MUTEX`, and
  `LIKE` suffixes
- result column names and MySQL 8.4.9-compatible metadata
- canonical `InnoDB` result row and deterministic status text
- known empty-status engines returning zero rows
- unknown engine error 1286 and message shape
- `ENGINES`, `ENGINE`, and `STATUS` remaining usable as identifiers in other
  grammar positions where MyLite allows them

## Compatibility Status

`SHOW ENGINE STATUS` is partially supported. `InnoDB` status is an embedded
placeholder, known empty-status engines return metadata-compatible empty sets,
and unknown engines produce MySQL-compatible diagnostics. Full InnoDB monitor
output, Performance Schema status instrumentation, `MUTEX`, `LOGS`, privileges,
and command counters are deferred.
