# Baseline Version System Variables

## Status

This feature specifies a narrow scalar system-variable slice for MyLite's
version identity:

- `@@version`
- `@@version_comment`

The slice builds on the existing `SYSTEM_VARIABLE` lexer/parser token, the
scalar session `SELECT` execution path, the previously implemented
`VERSION()` function, and the generalized runtime system-variable resolver.

MyLite intentionally exposes MyLite identity rather than impersonating a
MySQL server build. `@@version` returns the public MyLite engine version
string. `@@version_comment` returns a fixed MyLite comment string.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline version function:
  `docs/specs/baseline-version-function/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- Baseline character set system variables:
  `docs/specs/baseline-character-set-system-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variable reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SELECT VERSION(), @@version, @@global.version` returns the same server
  version string. In the local runtime it is `8.4.9`.
- `SELECT @@version_comment, @@global.version_comment` returns the server
  build comment. In the local runtime it is `MySQL Community Server - GPL`.
- The official system-variable reference classifies `version` and
  `version_comment` as global, non-dynamic variables.
- `@@session.version`, `@@local.version`,
  `@@session.version_comment`, and `@@local.version_comment` fail with error
  `1238`, SQLSTATE `HY000`, and a message saying the variable is a
  `GLOBAL variable`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`global`.version``, are syntax
  errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message. For scoped unknown names MySQL reports the
  final component after a recognized scope.
- A scalar `SELECT` that reads these variables is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expressions such as `@@version + 1` and wider select
  clauses such as `LIMIT`; those forms remain outside this MyLite slice.

The expectation script records the exact result labels, scope errors,
quoted-name behavior, diagnostics counts, and the environment-specific MySQL
value shapes against MySQL 8.4.9.

## Scope

The implementation must add:

- runtime recognition of `version` and `version_comment` inside the existing
  session scalar `SELECT` subset;
- support for no scope and `global` scope qualifiers;
- rejection of `session` and `local` qualifiers with MySQL error `1238`;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- `@@version` values sourced from `mylite_version()`;
- `@@version_comment` values sourced from a fixed MyLite-owned string;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@version
SELECT @@global.version
SELECT @@version_comment
SELECT @@global.version_comment
SELECT @@`version`, @@global.`version_comment`
SELECT @@version, VERSION(), @@warning_count, ROW_COUNT() FROM DUAL
```

## Non-Goals

This feature must not implement:

- MySQL server-version impersonation or MySQL build comments;
- protocol handshake version reporting;
- variables other than `version` and `version_comment`;
- `version_compile_machine`, `version_compile_os`,
  `version_compile_zlib`, or version-token variables;
- `SHOW VARIABLES`, Performance Schema variable tables, or
  `INFORMATION_SCHEMA` variable tables;
- `SET`, persisted variables, dynamic variable assignment, or privilege
  checks;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  numeric coercion of version strings, functions over variables, parameters,
  prepared statements, or arbitrary SQLite pass-through;
- catalog mutations, storage mutations, SQLite metadata reads, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, statement row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start and
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation, value
  lookup from the MyLite build/session constants, and diagnostics for
  unsupported names.
- Result builder owns scalar result column labels and one-row text values.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@version
@@version_comment
@@global.version
@@global.version_comment
```

The existing scalar `SELECT` limits continue to apply:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
```

System variables are admitted only when every selected expression is in the
existing scalar expression set. Clauses such as `WHERE`, `ORDER BY`, `LIMIT`,
table-backed `FROM`, aliases, arithmetic, and general expressions remain
outside this slice.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope or `global`;
- it rejects `session` and `local` for the admitted version variables with
  MySQL error `1238`, SQLSTATE `HY000`, and a message containing
  `GLOBAL variable`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

## Runtime Semantics

The supported variables return:

| Variable | Value |
| --- | --- |
| `version` | `mylite_version()` |
| `version_comment` | `MyLite` |

The values are stable process/build identity values. They do not change across
`CREATE DATABASE`, `USE`, table DDL, DML, close/reopen, or independent handles.

Successful scalar version-variable selects:

- return one result row;
- return one result column per select item;
- use the source expression text as each column name;
- use `affected_rows == 0` under the existing row-result convention;
- use result `warning_count == 0`;
- make following `ROW_COUNT()` return `-1`;
- clear diagnostics like other successful nondiagnostic scalar selects;
- do not mutate catalog generation or SQLite schema generation.

`ROW_COUNT()` and diagnostics count variables in the same select list keep the
existing statement-start semantics.

## Diagnostics

Diagnostics follow existing MyLite policy plus MySQL-runtime-verified system
variable errors:

- unknown or unsupported system variables: error `1193`, SQLSTATE `HY000`,
  message containing `Unknown system variable`;
- `session` or `local` scope for `version` and `version_comment`: error
  `1238`, SQLSTATE `HY000`, message containing `GLOBAL variable`;
- quoted system-variable scope names: deterministic syntax diagnostic;
- system variables outside the limited scalar `SELECT` subset: deterministic
  unsupported or syntax diagnostics;
- allocation failure while formatting labels, appending columns, or appending
  rows: `HY001`;
- public API misuse remains unchanged.

Successful reads produce no warnings.

## SQLite, Catalog, And File Format Policy

No generated SQLite SQL is required. The values are MyLite process constants,
not catalog or physical storage state. This feature must not create tables,
register SQLite functions, use SQLite callbacks, read SQLite metadata, change
VFS behavior, or patch SQLite.

File-backed tests must verify that reading version variables preserves the
MyLite preamble and does not change catalog or SQLite schema generations.

## Tests

Fast C tests must cover:

- `@@version`, `@@global.version`, `@@version_comment`, and
  `@@global.version_comment` returning MyLite values;
- `FROM DUAL` returning the same values;
- result labels preserving source text, including case-insensitive names and
  quoted final variable-name components;
- mixed `@@version`, `VERSION()`, `@@warning_count`, `@@error_count`, and
  `ROW_COUNT()` selects;
- diagnostics clearing after warning-only and error diagnostics;
- `@@session.version`, `@@local.version`,
  `@@session.version_comment`, and `@@local.version_comment` returning error
  `1238`;
- unknown unscoped, session-scoped, and global-scoped names returning error
  `1193`;
- quoted scope rejection and unsupported expression forms failing
  deterministically;
- values remaining stable across schema lifecycle operations, close/reopen, and
  independent handles;
- result row count, affected rows, warning count, and non-`NULL` text
  representation;
- file preamble preservation and unchanged catalog/schema generations.

The MySQL expectation script must verify MySQL 8.4.9 result shapes, labels,
scope rules, unknown-variable diagnostics, diagnostics interactions, and
unsupported wider forms. A missing MySQL 8.4.9 runtime blocks implementation.
