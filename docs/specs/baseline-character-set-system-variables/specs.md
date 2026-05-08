# Baseline Character Set System Variables

## Status

This feature specifies a narrow scalar system-variable slice for MyLite's
connection character-set placeholders:

- `@@character_set_client`
- `@@character_set_connection`
- `@@character_set_results`
- `@@collation_connection`

The slice builds on the existing `SYSTEM_VARIABLE` lexer/parser token, the
scalar session `SELECT` execution path, the statement diagnostics lifecycle,
and the `utf8mb4` / `utf8mb4_0900_ai_ci` charset/collation baseline.

This is not full character-set state management. MyLite exposes the current
connection defaults as read-only scalar values and does not implement `SET`,
`SET NAMES`, conversion, string types, or collation comparison semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline SHOW CHARACTER SET / COLLATION:
  `docs/specs/baseline-show-character-set-collation/specs.md`
- Baseline table charset/collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, connection character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-connection.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime with the client invoked using
`--default-character-set=utf8mb4`:

- `SELECT @@character_set_client, @@character_set_connection,
  @@character_set_results, @@collation_connection` returns `utf8mb4`,
  `utf8mb4`, `utf8mb4`, and `utf8mb4_0900_ai_ci`.
- The same values are returned by `@@session.` and `@@local.` scope forms.
- `@@global.character_set_client`, `@@global.character_set_connection`,
  `@@global.character_set_results`, and `@@global.collation_connection` are
  accepted. With the tested server defaults, they return the same values as the
  session forms.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.character_set_client``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message. For scoped unknown names MySQL reports the
  final component after a recognized scope, for example
  `no_such_charset_variable`.
- A scalar `SELECT` that reads these variables is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- Without an explicit client character-set option, the MySQL command-line
  client may negotiate different session charset values such as `latin1`.
  MyLite's baseline tests therefore pin the reference client to
  `--default-character-set=utf8mb4` to compare against MyLite's explicit
  `utf8mb4` connection baseline.

The official MySQL system-variable documentation classifies these four
variables as string variables with global and session scope. It documents
`character_set_client`, `character_set_connection`, and
`character_set_results` with default value `utf8mb4`; it documents
`collation_connection` as the collation of the connection character set.

## Scope

The implementation must add:

- runtime recognition of the four supported variables inside the existing
  session scalar `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- values sourced from the existing connection-local session state:
  `utf8mb4`, `utf8mb4`, `utf8mb4`, and `utf8mb4_0900_ai_ci`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@character_set_client
SELECT @@character_set_connection, @@collation_connection
SELECT @@session.character_set_results, @@local.collation_connection
SELECT @@global.character_set_client
SELECT @@session.`character_set_client`, @@`collation_connection`
SELECT @@character_set_client, @@warning_count, ROW_COUNT() FROM DUAL
```

## Non-Goals

This feature must not implement:

- `SET`, `SET NAMES`, `SET CHARACTER SET`, `SET_VAR` hints, persisted
  variables, or variable assignment;
- variables other than `character_set_client`, `character_set_connection`,
  `character_set_results`, and `collation_connection`;
- `SHOW VARIABLES`, Performance Schema variable tables, or
  `INFORMATION_SCHEMA` character-set/collation tables;
- client charset negotiation through a wire protocol;
- string, text, enum, set, binary, or blob column types;
- character-set conversion, introducer semantics, collation coercibility,
  string comparison semantics, or protocol character-set metadata;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
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
  lookup from connection state, and diagnostics for unsupported names.
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
@@character_set_client
@@character_set_connection
@@character_set_results
@@collation_connection
@@session.character_set_client
@@session.character_set_connection
@@session.character_set_results
@@session.collation_connection
@@local.character_set_client
@@local.character_set_connection
@@local.character_set_results
@@local.collation_connection
@@global.character_set_client
@@global.character_set_connection
@@global.character_set_results
@@global.collation_connection
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

For this slice, `global` returns the same fixed baseline values as session and
local forms. This is a deliberate MyLite limitation: there is no mutable global
server state, and the exposed values are the embedded `utf8mb4` baseline
already used by table charset/collation features.

## Runtime Semantics

The supported variables return:

| Variable | Value |
| --- | --- |
| `character_set_client` | `utf8mb4` |
| `character_set_connection` | `utf8mb4` |
| `character_set_results` | `utf8mb4` |
| `collation_connection` | `utf8mb4_0900_ai_ci` |

The values are connection-local placeholders initialized when a MyLite handle
opens. Since this slice does not support charset-changing statements, the
values do not change across `CREATE DATABASE`, `USE`, table DDL, DML, or
close/reopen. Independent handles have independent session storage but the
same initialized baseline values.

Successful scalar charset-variable selects:

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
- quoted system-variable scope names: deterministic syntax diagnostic;
- system variables outside the limited scalar `SELECT` subset: deterministic
  unsupported or syntax diagnostics;
- allocation failure while formatting labels, appending columns, or appending
  rows: `HY001`;
- public API misuse remains unchanged.

Successful reads produce no warnings.

## SQLite, Catalog, And File Format Policy

This feature is implemented entirely in MyLite runtime code. It must not:

- query SQLite;
- generate SQLite SQL;
- bind SQLite parameters;
- read or mutate catalog descriptor rows;
- change catalog generation or SQLite schema generation;
- alter physical user tables;
- alter the `.mylite` preamble or shifted SQLite payload invariant;
- add SQLite fork patches.

## Tests

Add a MySQL expectation script that runs against MySQL 8.4.9 with
`--default-character-set=utf8mb4` and verifies:

- result labels and values for no-scope, session, local, and global forms;
- case-insensitive scope and variable names;
- backtick-quoted final variable names;
- quoted-scope rejection;
- unknown-variable diagnostics;
- nondiagnostic scalar-select clearing of warning/error diagnostics;
- compatibility with `ROW_COUNT()` and diagnostics count variables in the same
  select list.

Add a fast C runtime test under `packages/libmylite/tests/`, registered with a
dotted CTest name, covering:

- successful scalar values and labels;
- `FROM DUAL`;
- mixed scalar variables with `@@warning_count`, `@@error_count`, and
  `ROW_COUNT()`;
- global/session/local scopes and quoted final names;
- unknown names and quoted-scope diagnostics;
- warning/error diagnostics clearing;
- file-backed preamble preservation;
- catalog and SQLite schema generation remaining unchanged;
- close/reopen persistence of ordinary data while charset variable values stay
  session defaults;
- independent handles.

No public ABI changes are expected.
