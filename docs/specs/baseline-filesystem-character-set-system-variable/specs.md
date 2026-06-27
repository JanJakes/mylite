# Baseline Filesystem Character Set System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@character_set_filesystem`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's fixed charset variable
surface. MySQL uses this variable when converting file-name string literals for
server-side file operations. MyLite does not implement server-side file import
or export in the baseline yet, so this slice exposes the default `binary`
readback, handle-local session assignment/readback, and a fixed global
boundary without implementing file-name conversion.

This is not full `binary` character-set support. It does not implement binary
string storage, conversions, collations, file import/export, or mutable global
system-variable state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline character set system variables:
  `docs/specs/baseline-character-set-system-variables/specs.md`
- Baseline system character set system variable:
  `docs/specs/baseline-system-character-set-system-variable/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, the binary character set:
  https://dev.mysql.com/doc/refman/8.4/en/charset-binary-set.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_filesystem_character_set_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@character_set_filesystem`, `@@global.character_set_filesystem`,
  `@@session.character_set_filesystem`, `@@local.character_set_filesystem`, and
  `@@CHARACTER_SET_FILESYSTEM` return `binary` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION character_set_filesystem=utf8mb4`, unscoped, `session`, and
  `local` reads return `utf8mb4`, while `global` still returns `binary`.
- `SET SESSION character_set_filesystem=utf8` canonicalizes readback to
  `utf8mb3`; MySQL charset names such as `latin2`, `ucs2`, and `utf16` are
  accepted for this metadata variable even when MyLite does not implement their
  string-conversion semantics.
- Integer assignments are interpreted as collation IDs, so `1` maps to
  `big5`, `33` to `utf8mb3`, and `255` to `utf8mb4`. String digits such as
  `'33'` are charset names and fail with unknown-character-set diagnostics.
- Assignments that use the `utf8` alias emit warning `3719`; assignments that
  resolve directly to `utf8mb3`, including integer collation ID `33`, emit
  deprecation warning `1287`.
- Decimal values fail with error `1232`, SQLSTATE `42000`; unknown charset
  names and unknown collation IDs fail with error `1115`, SQLSTATE `42000`.
- `SET character_set_filesystem=DEFAULT` resets the session value to `binary`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.character_set_filesystem``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@character_set_filesystem + 1`; those forms remain outside this
  MyLite slice.

The official MySQL system-variable documentation classifies
`character_set_filesystem` as a dynamic string variable with global and session
scope and default value `binary`. It is used by MySQL file-name processing for
file-oriented SQL surfaces such as `LOAD DATA`, `SELECT ... INTO OUTFILE`, and
`LOAD_FILE()`, all of which remain out of scope for this feature.

## Scope

The implementation must add:

- runtime recognition of `character_set_filesystem` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- session/local/unscoped `SET character_set_filesystem` for `DEFAULT`, MySQL
  charset names and aliases, and integer collation IDs;
- scalar and `SHOW VARIABLES` readback of the handle-local session value while
  `global` reads remain fixed at `binary`;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- MySQL-compatible unknown-character-set and incorrect-type diagnostics for
  unsupported assignment values;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@character_set_filesystem
SELECT @@character_set_filesystem FROM DUAL
SELECT @@session.character_set_filesystem, @@local.character_set_filesystem
SELECT @@global.character_set_filesystem
SELECT @@session.`character_set_filesystem`, @@`character_set_filesystem`
SELECT @@character_set_filesystem, @@warning_count, ROW_COUNT()
SET SESSION character_set_filesystem=utf8
SET LOCAL character_set_filesystem='latin2'
SET character_set_filesystem=255
SET character_set_filesystem=DEFAULT
SHOW VARIABLES LIKE 'character_set_filesystem'
```

## Non-Goals

This feature must not implement:

- mutable global filesystem charset state, startup options, persisted
  variables, or `SET_VAR` hints;
- variables other than `character_set_filesystem`;
- `character_sets_dir`, `character_set_system`, or
  `default_collation_for_utf8mb4`;
- general `binary` table, column, literal, conversion, or collation support;
- `LOAD DATA`, `LOAD XML`, `SELECT ... INTO OUTFILE`, `SELECT ... INTO
  DUMPFILE`, `LOAD_FILE()`, or file-name conversion semantics;
- Performance Schema variable tables or `INFORMATION_SCHEMA` variable tables;
- client charset negotiation through a wire protocol;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- catalog mutations, storage mutations, SQLite metadata reads, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start and
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation,
  filesystem charset assignment validation/canonicalization, fixed global value
  selection, session override state, and diagnostics for unsupported names and
  values.
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
@@character_set_filesystem
@@session.character_set_filesystem
@@local.character_set_filesystem
@@global.character_set_filesystem
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
table-backed `FROM`, aliases, and general expressions remain outside this
slice.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope, `session`, `local`, or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, global reads return fixed `binary`. Unscoped, `session`, and
`local` reads return the connection-local override when one has been assigned,
or `binary` by default. This is a deliberate MyLite limitation: MyLite has no
mutable server-global file-name character-set state and no server-side file SQL
surfaces.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `character_set_filesystem` global/default | `binary` |
| `character_set_filesystem` session override | canonical MySQL charset name |

The value is a MyLite filesystem-charset metadata placeholder. Session
assignment is connection-local, does not persist across close/reopen, and does
not change `.mylite` storage bytes, catalog rows, or SQLite schema state.
`CREATE DATABASE`, `USE`, table DDL, and DML do not change it.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unknown charset names and unknown integer collation IDs: error `1115`,
  SQLSTATE `42000`;
- decimal assignment values: error `1232`, SQLSTATE `42000`;
- `utf8` alias assignment warning: `3719`, SQLSTATE `HY000`;
- `utf8mb3` assignment warning: `1287`, SQLSTATE `HY000`;
- value-changing `SET GLOBAL character_set_filesystem=...` remains an
  unsupported fixed-global mutation;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@character_set_filesystem` do not emit warnings. Session
assignments emit only the documented charset alias/deprecation warnings.
File-name conversion behavior is out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing charset, diagnostics, version, and engine
  variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- session/local/unscoped `SET`, `DEFAULT`, charset aliases, charset names that
  are known to MySQL, integer collation IDs, string-digit rejection, unknown
  charset diagnostics, decimal type diagnostics, alias/deprecation warning
  counts, `SHOW VARIABLES` session readback, and fixed `SHOW GLOBAL VARIABLES`
  readback;
- selected schema, close/reopen, and independent handles do not leak or persist
  the session value;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation;
- existing parser/runtime/charset/system-variable tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records wider MySQL behavior that this
slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/character-sets.md`;
- `docs/compatibility/sql-file-output.md`.

Do not overclaim `binary` storage/conversion/collation support, mutable global
system variables, server-side file operations, or file-name conversion
semantics.
