# SHOW ENGINES

## Scope

This feature implements the first executable slice of MySQL's storage-engine
introspection statement:

- `SHOW ENGINES`
- `SHOW STORAGE ENGINES`

Deferred surfaces:

- full MySQL build-dependent engine catalog
- `SHOW ENGINE ...` singular diagnostic/status statements
- storage-engine status counters
- privilege differences, if future MySQL behavior requires them

`INFORMATION_SCHEMA.ENGINES` is implemented by the separate
[INFORMATION_SCHEMA.ENGINES](../information-schema-engines/specs.md) slice and
shares the storage-engine registry described here.

MySQL does not accept `LIKE`, `WHERE`, or `LIMIT` clauses for this statement.
MyLite must keep those forms as syntax errors, not parsed unsupported filters.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW ENGINES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-engines.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- Runtime observations verified by the parent workflow against Docker
  container `mylite-mysql-849`, MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW ENGINES` | Accepted; returns storage-engine status rows. |
| `SHOW STORAGE ENGINES` | Accepted; returns the same columns and rows as `SHOW ENGINES`. |
| `SHOW ENGINES LIKE 'InnoDB'` | Syntax error `1064`, SQLSTATE `42000`. |
| `SHOW ENGINES WHERE Engine = 'InnoDB'` | Syntax error `1064`, SQLSTATE `42000`. |
| `SHOW ENGINES LIMIT 1` | Syntax error `1064`, SQLSTATE `42000`. |
| missing-table error; `SHOW COUNT(*) ERRORS`; `SHOW ENGINES`; `SHOW COUNT(*) ERRORS` | `SHOW ENGINES` is nondiagnostic and clears the earlier error before reporting, so the final error count is `0`. |

The observed MySQL Community Server image returned rows in this order:

1. `ndbcluster`, `NO`
2. `MEMORY`, `YES`
3. `InnoDB`, `DEFAULT`
4. `PERFORMANCE_SCHEMA`, `YES`
5. `MyISAM`, `YES`
6. `FEDERATED`, `NO`
7. `ndbinfo`, `NO`
8. `MRG_MYISAM`, `YES`
9. `BLACKHOLE`, `YES`
10. `CSV`, `YES`
11. `ARCHIVE`, `YES`

The full row set varies by server build and startup options.

Column metadata observed for `SHOW ENGINES`:

| Column | Type | Collation | Length | Nullability |
| --- | --- | --- | ---: | --- |
| `Engine` | `VAR_STRING` | `latin1_swedish_ci` | 64 | not null |
| `Support` | `VAR_STRING` | `latin1_swedish_ci` | 8 | not null |
| `Comment` | `VAR_STRING` | `latin1_swedish_ci` | 80 | not null |
| `Transactions` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |
| `XA` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |
| `Savepoints` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_engines_statement.

show_engines_statement ::= SHOW opt_storage ENGINES.

opt_storage ::= .
opt_storage ::= STORAGE.
```

No filter nonterminal is attached to this statement. `LIKE`, `WHERE`, and
`LIMIT` therefore remain syntax errors, matching the verified MySQL runtime.

`ENGINES` and `STORAGE` must remain available as nonreserved identifiers outside
this production through the existing fallback behavior.

## AST

Add a `show_engines_statement` AST node with no children. The statement span
must cover `SHOW` through `ENGINES`, including the optional `STORAGE` keyword
when present.

The AST does not need to preserve whether `STORAGE` was specified because the
two accepted spellings have identical runtime behavior.

## Runtime Semantics

Rows:

- The result set has exactly six columns in this order:
  - `Engine`
  - `Support`
  - `Comment`
  - `Transactions`
  - `XA`
  - `Savepoints`
- The first three columns are non-null text. The last three columns are text
  when the engine is supported and `NULL` for unsupported engines.
- Successful `SHOW ENGINES` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only result.
- `SHOW ENGINES` is a nondiagnostic statement. Like MySQL, it clears prior
  diagnostics before producing rows.

Support values:

- `DEFAULT` means the engine is supported and is the default table engine.
- `YES` is reserved for future supported non-default engines.
- `NO` means the engine is known but unsupported by this MyLite build.
- `DISABLED` is reserved for future build/runtime-disabled supported engines.

MyLite first-slice registry:

| Engine | Support | Comment | Transactions | XA | Savepoints |
| --- | --- | --- | --- | --- | --- |
| `InnoDB` | `DEFAULT` | `MyLite SQLite-backed transactional engine facade` | `YES` | `NO` | `YES` |
| `MEMORY` | `NO` | `In-memory tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `MyISAM` | `NO` | `MyISAM tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `FEDERATED` | `NO` | `Federated tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `MRG_MYISAM` | `NO` | `Merge MyISAM tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `BLACKHOLE` | `NO` | `Blackhole tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `CSV` | `NO` | `CSV-backed tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `ARCHIVE` | `NO` | `Archive tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |

This registry deliberately exposes only one supported engine. MyLite currently
accepts `ENGINE=InnoDB` for supported `CREATE TABLE` execution and rejects
unsupported engine names. Reporting the other common names as `NO` lets
applications probe for them without implying that MyLite can create such
tables.

`InnoDB` is an embedded compatibility facade over MyLite's SQLite-backed
storage, not a bundled copy of MySQL InnoDB. It reports transaction and
savepoint support because MyLite implements transaction statements and
savepoints. It reports `XA = NO` because MyLite does not implement XA
transactions.

## Storage And Performance

This feature is read-only and requires no file format change. Runtime execution
materializes a small immutable engine registry into a SQLite read statement and
attaches MySQL-compatible result metadata. No mutable process-global state or
new dependency is needed.

## Tests

Parser coverage:

- `SHOW ENGINES`
- `SHOW STORAGE ENGINES`
- syntax rejection for `SHOW ENGINES LIKE 'InnoDB'`
- syntax rejection for `SHOW ENGINES WHERE Engine = 'InnoDB'`
- syntax rejection for `SHOW ENGINES LIMIT 1`
- `ENGINES` and `STORAGE` as unquoted identifiers where fallback permits them

Runtime coverage:

- exact result column names
- MySQL-compatible result metadata for type, collation id, declared length,
  and nullability
- exact row values in the MyLite registry order
- unsupported engines return `NULL` for `Transactions`, `XA`, and `Savepoints`
- `SHOW STORAGE ENGINES` returns the same rows as `SHOW ENGINES`
- `LIKE`, `WHERE`, and `LIMIT` are syntax errors through prepare
- `mylite_affected_rows()` remains `-1`
- previous diagnostics are cleared by successful execution

## Known Incompatibilities

- MyLite exposes a small embedded registry instead of MySQL's full
  build-dependent engine catalog.
- Unsupported engine rows are intentionally reported as `Support = NO` and
  cannot be used by `CREATE TABLE ... ENGINE=...`.
- MyLite's `InnoDB` row is a compatibility facade over SQLite-backed storage.
  It does not imply MySQL InnoDB internals, row locks, foreign-key enforcement,
  tablespaces, redo/undo logs, or XA.
- `INFORMATION_SCHEMA.ENGINES` shares this registry but keeps the current
  MyLite information-schema limitation: only wildcard selection is supported
  until broader metadata-table `SELECT` processing lands.
