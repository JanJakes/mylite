# Baseline SHOW ENGINE STATUS And LOGS

## Status

This feature specifies a narrow embedded introspection slice for
`SHOW ENGINE InnoDB STATUS` and `SHOW ENGINE InnoDB LOGS`. MyLite exposes
MySQL-shaped row-result sets for applications that probe InnoDB availability or
diagnostics during startup, while keeping storage-engine internals owned by
MyLite and SQLite.

The feature is intentionally not full `SHOW ENGINE` support. It does not expose
live InnoDB monitor output, Performance Schema memory rows, mutex statistics,
relay-log or binary-log internals, storage-engine plugins, privilege checks, or
alternate engine implementations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- Baseline SHOW plugins:
  `docs/specs/baseline-show-plugins-metadata/specs.md`
- Baseline SHOW status:
  `docs/specs/baseline-show-status-variables/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW ENGINE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-engine.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_show_engine_status_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- The documented `SHOW ENGINE` statement accepts an engine name followed by
  `STATUS` or `MUTEX`.
- `SHOW ENGINE InnoDB STATUS`, `SHOW ENGINE innodb STATUS`,
  ``SHOW ENGINE `InnoDB` STATUS``, and `SHOW ENGINE 'InnoDB' STATUS` are
  accepted.
- `SHOW ENGINE InnoDB STATUS` returns columns `Type`, `Name`, and `Status`.
- The observed InnoDB status result has one row. `Type` is `InnoDB`, `Name` is
  the empty string, and `Status` is live InnoDB monitor text whose full content
  is runtime-specific.
- After the row result, `ROW_COUNT()` is `-1`, `@@warning_count` is `0`, and
  `@@error_count` is `0`.
- `SHOW ENGINE InnoDB STATUS LIKE '%'` and `SHOW FULL ENGINE InnoDB STATUS` are
  syntax errors.
- `SHOW ENGINE NDB STATUS` fails with error `1286`, SQLSTATE `42000`, and an
  `Unknown storage engine 'NDB'` message when the target runtime has no NDB
  engine.
- `SHOW ENGINE MyISAM STATUS` is accepted by MySQL 8.4.9 and returns the same
  three columns with zero rows because that runtime exposes MyISAM but no status
  rows for it. MyLite deliberately keeps its existing embedded InnoDB-only
  engine policy and rejects non-`InnoDB` engine names in this slice.
- `SHOW ENGINE PERFORMANCE_SCHEMA STATUS` is accepted by MySQL 8.4.9 and returns
  Performance Schema status rows. MyLite defers this separate engine surface.
- `SHOW ENGINE InnoDB MUTEX` is accepted by MySQL 8.4.9 and returns the same
  column labels with zero or more runtime-specific mutex rows. MyLite defers
  mutex statistics.
- `SHOW ENGINE InnoDB LOGS` is accepted by the observed MySQL 8.4.9 runtime and
  returns the same column labels with zero rows. This form is not listed by the
  MySQL 8.4 `SHOW ENGINE` manual page, but the runtime behavior is stable in
  the target 8.4.9 comparison environment used by this project.

## Scope

The implementation must add:

- parser and AST support for `SHOW ENGINE engine_name STATUS` and
  `SHOW ENGINE engine_name LOGS`;
- engine-name decoding for the same identifier and string-literal spellings
  already admitted for `CREATE TABLE ... ENGINE=InnoDB`;
- runtime validation that the decoded engine name is `InnoDB`, compared ASCII
  case-insensitively;
- deterministic rejection of non-`InnoDB` engine names with the existing
  embedded InnoDB-only storage-engine diagnostic;
- a descriptor-independent result builder with MySQL 8.4.9 column labels
  `Type`, `Name`, and `Status`;
- one synthetic status row with `Type = InnoDB`, `Name = ''`, and MyLite-owned
  status text;
- an empty logs result with the same MySQL 8.4.9 column labels;
- tests comparing the admitted MySQL-visible behavior with MySQL 8.4.9;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- live InnoDB monitor text, latch statistics, transaction dumps, lock dumps, or
  physical SQLite diagnostics formatted as MySQL InnoDB monitor output;
- `SHOW ENGINE InnoDB MUTEX`;
- `SHOW ENGINE PERFORMANCE_SCHEMA STATUS`;
- non-`InnoDB` engine status such as MyISAM, NDB, MEMORY, CSV, ARCHIVE,
  BLACKHOLE, or FEDERATED;
- filters, `LIKE`, `WHERE`, `LIMIT`, `FULL`, or query modifiers;
- privileges or `PROCESS` privilege enforcement;
- status counters, diagnostic-area changes beyond the existing row-result
  convention, or warning rows;
- catalog, descriptor, storage-format, VFS, or SQLite fork changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns the statement
  boundary and result-handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  `ROW_COUNT()` state. A successful row-producing `SHOW ENGINE InnoDB STATUS`
  follows the existing row-result convention.
- Lexer/parser/AST own syntax admission and engine-name source spans. Parser
  code remains independent of runtime, catalog, storage, and SQLite.
- Runtime validates the decoded engine name and builds the row-result object.
- The catalog remains authoritative for schemas, tables, columns, and MyLite
  descriptors, but this statement does not consult or mutate it.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  statement is read-only and does not issue SQLite SQL.
- SQLite remains the physical storage engine. This feature is MyLite wrapper
  metadata, not a SQLite extension API or SQLite fork hook.

## Supported SQL Grammar

Supported syntax:

```sql
SHOW ENGINE engine_name STATUS
SHOW ENGINE engine_name LOGS

engine_name:
    identifier
  | quoted_identifier
  | string_literal
```

`engine_name` must decode to `InnoDB`, compared ASCII case-insensitively.

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_engine_status_statement(B). {
    A = B;
}
statement(A) ::= show_engine_logs_statement(B). {
    A = B;
}

show_engine_status_statement(A) ::= SHOW(S) ENGINE option_name(N) STATUS(T). {
    A = mylite_sql_parser_make_show_engine_status_statement(state, S, N, T);
}

show_engine_logs_statement(A) ::= SHOW(S) ENGINE option_name(N) LOGS(L). {
    A = mylite_sql_parser_make_show_engine_logs_statement(state, S, N, L);
}
```

The `option_name` nonterminal is reused intentionally so engine-name decoding
matches the already supported `CREATE TABLE ... ENGINE=InnoDB` path. Unsupported
`SHOW ENGINE ... MUTEX`, `SHOW ENGINE ... STATUS LIKE`,
`SHOW FULL ENGINE ... STATUS`, filters, and additional clauses remain syntax
errors unless a later feature admits them.

## Engine Name Decoding And Validation

Identifier engine names use the existing identifier decoding path. Backtick
doubling is decoded before comparison. String engine names use the existing
table-option string-literal decoding policy. NUL bytes are rejected
deterministically before comparison.

Decoded engine names other than `InnoDB` are rejected with error `1286`,
SQLSTATE `42000`, and `Unknown storage engine '<name>'`. This is
MyLite-specific for engines the reference MySQL server may expose, such as
MyISAM and Performance Schema. The choice follows the existing embedded
InnoDB-only compatibility decision from `CREATE TABLE ... ENGINE`.

## Runtime Semantics

`SHOW ENGINE InnoDB STATUS` and `SHOW ENGINE InnoDB LOGS` build new row-result
objects without reading or mutating catalog rows and without generating SQLite
SQL.

Successful output:

| Column | Value |
| --- | --- |
| `Type` | `InnoDB` |
| `Name` | empty string |
| `Status` | `MyLite embedded InnoDB-compatible storage engine is active` |

The `Status` value is intentionally MyLite-owned placeholder text. It must be
stable, nonempty, independently authored, and must not copy MySQL's live InnoDB
monitor output.

Success leaves `affected_rows == 0`, `warning_count == 0`, and following
`ROW_COUNT() == -1`, matching MyLite's existing public result convention for
row-producing statements and the observed MySQL behavior.

`SHOW ENGINE InnoDB LOGS` returns the same columns with zero rows. It uses the
same diagnostics and row-count conventions as the status result.

## Diagnostics

This slice must cover:

- syntax errors for unsupported `SHOW ENGINE` modifiers and unsupported
  subcommands;
- unknown/non-`InnoDB` engine names through error `1286`, SQLSTATE `42000`;
- malformed or unsupported engine-name tokens through existing parser
  diagnostics;
- NUL-containing engine names through the same unsupported-name policy used by
  table engine options;
- allocation failures while constructing the result through existing
  `MYLITE_NOMEM` diagnostics.

The statement does not use selected/default schema policy, object resolution,
reserved `_mylite_*` names, descriptors, or physical SQLite identifiers.

## Performance And Storage

The implementation is O(1). It allocates one small result object, appends three
column labels, appends one text row for `STATUS` or no rows for `LOGS`, and
performs no catalog scan or SQLite query. No `.mylite` file bytes are written,
and no SQLite fork patch is required.

## Tests

Tests must cover:

- parser support for unquoted, lower-case, backtick-quoted, and string-literal
  `InnoDB` engine names;
- successful runtime output columns, status row values, and empty logs rowset;
- warning count, absence of statement error conditions, and following
  `ROW_COUNT() == -1`;
- no catalog or storage mutation by checking the MyLite preamble remains
  unchanged on a file-backed handle;
- persistence independence by running the statement on independent handles;
- unknown engine diagnostics for a representative non-`InnoDB` engine name;
- deterministic syntax errors for `MUTEX`, `LIKE`, and `FULL` forms deferred
  by this slice;
- the MySQL expectation script against MySQL 8.4.9.
