# Baseline Database Character Set System Variables

## Status

This feature specifies a narrow scalar system-variable slice for the default
database character-set placeholders:

- `@@character_set_database`
- `@@collation_database`

It builds on schema selection, the existing `SYSTEM_VARIABLE` lexer/parser
token, scalar session `SELECT` execution, diagnostics lifecycle, and MyLite's
fixed `utf8mb4` / `utf8mb4_0900_ai_ci` database-default surface.

This is not full database character-set state management. MyLite schemas do
not yet carry mutable charset/collation options, so these variables expose the
fixed defaults used by current schema descriptors. They do not implement
`ALTER DATABASE`, `CREATE DATABASE` options, `SET`, warnings for deprecated
assignments, or collation comparison semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline current database function:
  `docs/specs/baseline-current-database-function/specs.md`
- Baseline SHOW CREATE DATABASE:
  `docs/specs/baseline-show-create-database/specs.md`
- Baseline server character set system variables:
  `docs/specs/baseline-server-character-set-system-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, server character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-server.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime with the client invoked using
`--default-character-set=utf8mb4`:

- Without a selected database, `SELECT @@character_set_database,
  @@collation_database` returns `utf8mb4` and `utf8mb4_0900_ai_ci` for the
  tested default server.
- `@@global.character_set_database`, `@@session.character_set_database`, and
  `@@local.character_set_database` return `utf8mb4`.
- `@@global.collation_database`, `@@session.collation_database`, and
  `@@local.collation_database` return `utf8mb4_0900_ai_ci`.
- After `CREATE DATABASE db; USE db`, the variables return the selected
  database defaults, which are `utf8mb4` and `utf8mb4_0900_ai_ci` for an
  optionless database in the tested runtime.
- Dropping the selected database leaves `DATABASE()` as `NULL`, and the
  variables again return the server defaults.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.character_set_database``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads these variables is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@character_set_database + 1`; those forms remain outside this
  MyLite slice.

The official MySQL system-variable documentation classifies
`character_set_database` and `collation_database` as string variables with
global and session scope. It documents that the server updates their session
values when the default database changes, and that no default database falls
back to the server character set and collation.

## Scope

The implementation must add:

- runtime recognition of `character_set_database` and `collation_database`
  inside the existing scalar `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed values `utf8mb4` and `utf8mb4_0900_ai_ci` for all currently supported
  MyLite schemas and for the no-selected-schema fallback;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@character_set_database
SELECT @@collation_database FROM DUAL
SELECT @@session.character_set_database, @@local.collation_database
SELECT @@global.character_set_database
SELECT @@session.`character_set_database`, @@`collation_database`
SELECT @@character_set_database, DATABASE(), @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or assignment
  warning semantics;
- variables other than `character_set_database` and `collation_database`;
- `CREATE DATABASE` charset/collation options, `ALTER DATABASE`, or mutable
  schema defaults;
- non-default character sets or collations;
- `SHOW VARIABLES`, Performance Schema variable tables, or
  `INFORMATION_SCHEMA` character-set/collation tables;
- client charset negotiation through a wire protocol;
- string, text, enum, set, binary, or blob column types;
- character-set conversion, introducer semantics, collation coercibility,
  string comparison semantics, or protocol character-set metadata;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- SQLite metadata reads or SQLite fork patches.

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
- Runtime execution owns system-variable path parsing, scope validation, fixed
  database default lookup, and diagnostics for unsupported names.
- Schema/catalog code owns selected schema state and schema descriptors.
  Because current MyLite schemas have fixed defaults, this feature reads no
  catalog rows and does not mutate catalog generation.
- Result builder owns scalar result column labels and one-row text values.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@character_set_database
@@collation_database
@@session.character_set_database
@@session.collation_database
@@local.character_set_database
@@local.collation_database
@@global.character_set_database
@@global.collation_database
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

For this slice, `global`, `session`, and `local` return the same fixed baseline
values. This is a deliberate MyLite limitation: there is no mutable global
server state and no mutable schema charset/collation state.

## Runtime Semantics

The supported variables return:

| Variable | Value |
| --- | --- |
| `character_set_database` | `utf8mb4` |
| `collation_database` | `utf8mb4_0900_ai_ci` |

For current MyLite schemas, `CREATE DATABASE`, `USE`, close/reopen, and
independent handles do not change these values because schema descriptors have
only fixed defaults. Dropping the selected schema clears `DATABASE()` through
the existing schema lifecycle behavior; these variables continue to expose the
same fallback defaults.

Successful scalar database-variable selects:

- return one result row;
- return one result column per select item;
- use the source expression text as each column name;
- use `affected_rows == 0` under the existing row-result convention;
- use result `warning_count == 0`;
- make following `ROW_COUNT()` return `-1`;
- clear diagnostics like other successful nondiagnostic scalar selects;
- do not mutate selected schema, catalog generation, or SQLite schema
  generation.

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
- no-selected-database fallback;
- selected default database behavior;
- dropped selected database fallback;
- quoted-scope rejection;
- unknown-variable diagnostics;
- nondiagnostic scalar-select clearing of warning/error diagnostics;
- compatibility with `ROW_COUNT()` and diagnostics count variables in the same
  select list;
- wider expression forms accepted by MySQL but intentionally outside MyLite.

Add or extend a fast C runtime test under `packages/libmylite/tests/`,
registered with a dotted CTest name, covering:

- successful scalar values and labels;
- `FROM DUAL`;
- mixed scalar variables with `DATABASE()`, `@@warning_count`, `@@error_count`,
  and `ROW_COUNT()`;
- global/session/local scopes and quoted final names;
- selected schema, dropped selected schema, and close/reopen behavior;
- unknown names and quoted-scope diagnostics;
- warning/error diagnostics clearing;
- file-backed preamble preservation;
- catalog and SQLite schema generation remaining unchanged;
- independent handles.

No public ABI changes are expected.
