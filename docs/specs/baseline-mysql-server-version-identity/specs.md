# Baseline MySQL Server Version Identity

## Status

This slice changes the SQL-visible version identity from MyLite's library
version to the verified MySQL 8.4.9 server identity expected by compatibility
tests:

- `VERSION()`
- `@@version`
- `@@global.version`
- `SHOW VARIABLES` rows for `version`

It also aligns `@@version_comment`, `@@global.version_comment`, and
`SHOW VARIABLES` rows for `version_comment` with the local MySQL 8.4.9
baseline. The public C API `mylite_version()` continues to return
`MYLITE_VERSION_STRING`; only SQL-visible server identity changes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline version function:
  `docs/specs/baseline-version-function/specs.md`
- Baseline version system variables:
  `docs/specs/baseline-version-system-variables/specs.md`
- Runtime system-variable compatibility:
  `docs/compatibility/runtime-system-variables.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, server system variable reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mylite-mysql-849` MySQL runtime:

- `SELECT VERSION()` returns `8.4.9`.
- `SELECT VERSION(), @@version, @@global.version` returns the same string for
  all three expressions.
- `SELECT @@version_comment, @@global.version_comment` returns
  `MySQL Community Server - GPL`.
- `SHOW VARIABLES WHERE Variable_name IN ('version', 'version_comment')`
  returns `version = 8.4.9` and
  `version_comment = MySQL Community Server - GPL`.
- The existing MySQL expectation scripts for the version function and version
  variables pass against that runtime and verify result labels, scope rules,
  argument-count errors, unknown variable errors, and diagnostics behavior.

## Scope

The implementation must:

- keep the public `mylite_version()` API returning `MYLITE_VERSION_STRING`;
- return `8.4.9` for SQL-visible `VERSION()`;
- return `8.4.9` for SQL-visible `@@version` and `@@global.version`;
- return `8.4.9` for `SHOW VARIABLES` rows for `version`;
- return `MySQL Community Server - GPL` for SQL-visible
  `@@version_comment`, `@@global.version_comment`, and `SHOW VARIABLES`
  rows for `version_comment`;
- preserve all existing accepted and rejected syntax for the previous version
  function and version-variable slices;
- preserve result labels, diagnostics, warning counts, row-count semantics,
  file-format invariants, catalog generation, and SQLite schema generation.

## Non-Goals

This slice must not implement:

- protocol handshake version reporting;
- dynamic or configurable server-version values;
- changes to version compile variables beyond the existing fixed placeholder
  slice;
- Performance Schema, information-schema variable tables, or protocol metadata;
- broader scalar expressions over version strings such as `@@version + 1`;
- changes to `mylite_version()`, `MYLITE_VERSION_STRING`, package versioning,
  or the file-format version;
- SQLite function registration or SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. `mylite_version()` remains the MyLite library version.
- Parser/AST: unchanged. Existing `VERSION()` and `SYSTEM_VARIABLE` grammar
  remains authoritative for syntax.
- Runtime scalar execution: maps admitted `VERSION()` AST nodes to the
  SQL-visible MySQL server version constant.
- Runtime system-variable resolver: maps admitted `version` and
  `version_comment` system variables to SQL-visible MySQL server identity
  constants.
- `SHOW VARIABLES`: uses the same SQL-visible constants for row values.
- Catalog, storage, VFS, and SQLite physical storage are not involved. The
  values are runtime constants and must not mutate `.mylite` preamble bytes,
  catalog rows, descriptor caches, or SQLite schema text.

## Grammar

No grammar changes are required. The previous slices already admit the needed
forms:

```lemon
expression ::= VERSION LPAREN RPAREN.
expression ::= VERSION LPAREN function_argument_list RPAREN.
expression ::= SYSTEM_VARIABLE.
```

The argument-count-error branch for `VERSION(...)` continues to produce the
existing MySQL-compatible native-function parameter-count diagnostic.

## Runtime Semantics

Supported SQL-visible values:

| SQL surface | Value |
| --- | --- |
| `VERSION()` | `8.4.9` |
| `@@version` | `8.4.9` |
| `@@global.version` | `8.4.9` |
| `SHOW VARIABLES ... version` | `8.4.9` |
| `@@version_comment` | `MySQL Community Server - GPL` |
| `@@global.version_comment` | `MySQL Community Server - GPL` |
| `SHOW VARIABLES ... version_comment` | `MySQL Community Server - GPL` |

These values are compile-time MyLite SQL compatibility constants for this
baseline. They do not imply a MySQL server process, network protocol, or
distribution package. They are intentionally separate from MyLite's public
library version.

Successful reads keep existing scalar-result conventions:

- one result row for scalar selects;
- source-expression result labels;
- `affected_rows == 0` for row-result statements;
- `warning_count == 0` for successful reads;
- `ROW_COUNT()` state follows the existing result-set rules;
- no catalog, schema, storage, VFS, or SQLite mutation.

## Diagnostics

Diagnostics remain unchanged from the previous slices:

- parsed `VERSION(...)` calls with one or more arguments fail with MySQL error
  `1582`, SQLSTATE `42000`;
- bare `VERSION` remains outside the function slice;
- `@@session.version`, `@@local.version`, `@@session.version_comment`, and
  `@@local.version_comment` fail as global-only variables with error `1238`;
- unknown version-like variables fail with error `1193`;
- quoted scopes and unsupported expression forms keep existing deterministic
  syntax or capability diagnostics.

## Tests

Fast C tests must verify:

- public `mylite_version()` still returns `MYLITE_VERSION_STRING`;
- `VERSION()` returns `8.4.9` in all existing admitted scalar function shapes;
- `@@version` and `@@global.version` return `8.4.9`;
- `@@version_comment` and `@@global.version_comment` return
  `MySQL Community Server - GPL`;
- `SHOW VARIABLES` rows use the SQL-visible MySQL values;
- mixed scalar functions that include `VERSION()` use the SQL-visible MySQL
  value;
- existing diagnostics, result labels, warning counts, row-count behavior,
  file-preamble checks, independent-handle checks, and rejected forms still
  pass.

The existing MySQL expectation scripts for `VERSION()` and version variables
remain the runtime authority and must continue to pass against MySQL 8.4.9.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-system.md`, and
`docs/compatibility/runtime-system-variables.md` to say the SQL-visible
version identity returns the MySQL 8.4.9 compatibility value, not the MyLite
library version. Keep protocol handshake and configurable server-version
behavior out of scope.
