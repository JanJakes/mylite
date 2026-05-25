# Baseline SHOW PRIVILEGES

## Summary

This phase adds a narrow `SHOW PRIVILEGES` surface. MyLite has no account
store, role graph, grant descriptors, or privilege enforcement, but clients can
still ask the server which privilege names exist. The supported behavior is a
static MySQL 8.4.9-shaped result listing known privilege names, contexts, and
comments.

This is metadata only. It does not imply that the administrative operations
named by the result are implemented.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline privilege metadata:
  `docs/specs/baseline-information-schema-privileges/specs.md`
- Baseline SHOW GRANTS:
  `docs/specs/baseline-show-grants/specs.md`
- MySQL 8.4 Reference Manual, `SHOW PRIVILEGES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-privileges.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_show_privileges_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW PRIVILEGES` returns columns `Privilege`, `Context`, and `Comment`.
- The result contains 73 rows in the target runtime: the static privileges,
  `Usage`, and installed dynamic privileges.
- Dynamic privilege rows use `Context = 'Server Admin'` and an empty string
  comment.
- Successful `SHOW PRIVILEGES` leaves `@@warning_count == 0` and makes the next
  `ROW_COUNT()` return `-1`.
- `SHOW PRIVILEGES` does not accept `LIKE`, `WHERE`, `FULL`, `FROM`,
  `ORDER BY`, or `LIMIT` clauses.

The expected rows and unsupported syntax diagnostics are pinned by the MySQL
runtime expectation script.

## Scope

The implementation must add:

- parser and AST support for `SHOW PRIVILEGES`;
- a runtime result builder that emits MySQL 8.4.9 column labels and the
  observed privilege rows;
- result behavior through existing public result conventions: row result set,
  affected rows `0`, warning count `0`, and following `ROW_COUNT() = -1`;
- preservation of `PRIVILEGES` as an identifier where MySQL treats it as
  nonreserved;
- fast parser/runtime C tests and a MySQL 8.4.9 expectation artifact;
- compatibility documentation for the exact partial surface.

## Non-Goals

This feature must not implement:

- account storage, authentication, roles, grant descriptors, grant/revoke DDL,
  privilege enforcement, or privilege-dependent filtering;
- `INFORMATION_SCHEMA` changes beyond documentation links to existing
  privilege metadata;
- `SHOW PRIVILEGES` filters, `SHOW FULL PRIVILEGES`, schema-qualified forms,
  ordering, limits, or privilege-dependent rows;
- status counter changes such as `Com_show_privileges`;
- physical SQLite tables, storage-format changes, VFS changes, SQLite
  extension hooks, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics, warning count,
  previous-row-count, and result lifetime behavior apply.
- Lexer/parser/AST: owns admission of exactly `SHOW PRIVILEGES` and rejection
  of unsupported extensions.
- Analyzer/planner: no descriptor resolution is needed.
- Catalog module: no catalog rows are read or written. Privilege names are not
  durable grant descriptors.
- Result builder: emits constant MySQL-shaped text values through
  `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: not used. No SQLite SQL is generated.

## Supported Grammar

This phase adds one SHOW statement:

```sql
SHOW PRIVILEGES
```

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_privileges_statement(B). {
    A = B;
}

show_privileges_statement(A) ::= SHOW(S) PRIVILEGES(P). {
    A = mylite_sql_parser_make_show_privileges_statement(state, S, P);
}
```

Unsupported `SHOW PRIVILEGES LIKE ...`, `SHOW PRIVILEGES WHERE ...`,
`SHOW FULL PRIVILEGES`, `SHOW PRIVILEGES FROM ...`,
`SHOW PRIVILEGES ORDER BY ...`, and `SHOW PRIVILEGES LIMIT ...` remain syntax
errors.

## Result Shape

`SHOW PRIVILEGES` returns three columns:

| Column | Meaning |
| --- | --- |
| `Privilege` | MySQL privilege name |
| `Context` | Scope/context where MySQL uses the privilege |
| `Comment` | MySQL description or empty string for dynamic privileges |

MyLite exposes the MySQL 8.4.9 runtime-observed row list from
`mysql_baseline_show_privileges_expectations.sh`. Static rows include `Alter`,
`Create`, `Select`, `Usage`, and related legacy privileges. Dynamic rows include
names such as `AUDIT_ABORT_EXEMPT`, `BACKUP_ADMIN`,
`SYSTEM_VARIABLES_ADMIN`, and `TELEMETRY_LOG_ADMIN`. Dynamic privilege comments
are empty strings, not SQL `NULL`.

The result is a fixed compatibility catalog and is independent of selected
schema, user-created tables, file-backed reopen, and handle identity.

## Diagnostics

- Unsupported `SHOW PRIVILEGES` grammar forms are syntax errors with the
  existing parser diagnostic shape.
- Allocation failures use existing `MYLITE_NOMEM` and handle diagnostics.
- Public API misuse is unchanged.

## Performance and Storage

The runtime builds a constant 73-row in-memory result directly. It does not
scan user tables, read or write catalog rows, query SQLite metadata, or generate
SQLite SQL. The cost is bounded by allocating and copying the static result
strings.

## Test Plan

Fast C tests must cover:

- parser acceptance for `SHOW PRIVILEGES`;
- `PRIVILEGES` as a table identifier;
- parser rejection for unsupported `LIKE`, `WHERE`, `FULL`, `FROM`,
  `ORDER BY`, and `LIMIT` forms;
- successful runtime column labels, exact first/last/static/dynamic row values,
  row count, warning count, affected rows, and following `ROW_COUNT()`;
- case-insensitive statement spelling;
- behavior with and without a selected schema;
- file-backed reopen and `.mylite` preamble preservation;
- independent handles returning identical static rows.

The MySQL expectation script must verify the full row list, diagnostics, warning
count, and row-count behavior against MySQL 8.4.9 and fail for any other MySQL
version.

## Compatibility Documentation

Update only the exact partial surface:

- `COMPATIBILITY.md` `SHOW PRIVILEGES`;
- `docs/compatibility/sql-show-statements.md`;
- `docs/compatibility/sql-users-privileges.md`;

Do not document account, role, grant management, privilege enforcement, status
counters, or mutable privilege catalogs.
