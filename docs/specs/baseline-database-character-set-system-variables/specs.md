# Baseline Database Character Set System Variables

## Status

This feature specifies a database character-set system-variable baseline for
selected-schema defaults and handle-local assignment compatibility:

- `@@character_set_database`
- `@@collation_database`

It builds on schema selection, the existing `SYSTEM_VARIABLE` lexer/parser
token, scalar session `SELECT` execution, `SHOW VARIABLES`,
system-variable assignment handling, diagnostics lifecycle, and MyLite's
database-default surface.

This is not full database character-set state management. MyLite schemas do
not perform character-set conversion or collation comparison, but the schema
descriptor records admitted default options. These variables expose selected
schema defaults, server fallback defaults, and handle-local session/local
assignments. They do not implement shared mutable global state, persisted
variables, conversion, or collation comparison semantics.

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
- `SHOW VARIABLES LIKE 'character_set_database'` returns the current session
  value, while `SHOW GLOBAL VARIABLES LIKE 'character_set_database'` returns
  the fixed global value.
- After `CREATE DATABASE db; USE db`, the variables return the selected
  database defaults, which are `utf8mb4` and `utf8mb4_0900_ai_ci` for an
  optionless database in the tested runtime.
- After `CREATE DATABASE db DEFAULT CHARACTER SET ascii COLLATE
  ascii_general_ci; USE db`, the session variables return `ascii` and
  `ascii_general_ci`.
- Dropping the selected database leaves `DATABASE()` as `NULL`, and the
  variables again return the server defaults.
- `SET SESSION character_set_database=latin1` changes the session
  `character_set_database` to `latin1`, couples `collation_database` to
  `latin1_swedish_ci`, and emits warning `1681`.
- `SET LOCAL collation_database=utf8mb4_bin` changes the session
  `collation_database` to `utf8mb4_bin`, couples
  `character_set_database` to `utf8mb4`, and emits warning `1681`.
- Integer literals are treated as collation IDs. For example,
  `SET character_set_database=33` yields `utf8mb3` /
  `utf8mb3_general_ci`, with warnings for the `utf8mb3` assignment and for the
  deprecated database variable assignment.
- String digits are treated as names, not IDs, and fail when no matching
  charset or collation name exists.
- `SET character_set_database=DEFAULT` and `SET collation_database=DEFAULT`
  restore the selected-schema defaults when a database is selected, otherwise
  the server fallback defaults. Each database variable assignment emits warning
  `1681`.
- A later `USE db` resets previous direct database variable assignments to the
  selected schema defaults.
- Assigning the `utf8` alias emits warning `3719` before database-assignment
  warning `1681`; assigning a `utf8mb3` charset by canonical name or integer
  collation ID emits warning `1287` before `1681`; assigning a `utf8mb3`
  collation emits warning `3778` before `1681`.
- Unknown charsets fail with error `1115`, SQLSTATE `42000`; unknown
  collations fail with error `1273`, SQLSTATE `HY000`; decimal assignment
  values fail with error `1232`, SQLSTATE `42000`.
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
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` readback for both variables;
- support for no scope, `session`, `local`, and `global` scope qualifiers on
  reads;
- session/local/unscoped `SET` assignment for `DEFAULT`, MySQL charset names
  and aliases, MySQL collation names and aliases, and integer collation IDs;
- coupled charset/collation readback after successful assignment;
- automatic reset of direct database variable overrides on `USE`, so selected
  schema defaults are visible again;
- fixed-global exact/default no-op assignment forms, with value-changing
  global assignments rejected by MyLite's fixed no-op assignment diagnostic;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- selected-schema descriptor defaults where available, and fixed
  `utf8mb4` / `utf8mb4_0900_ai_ci` server fallback values when no schema is
  selected or a global read is requested;
- MySQL-compatible alias, `utf8mb3`, and database-assignment deprecation
  warnings for supported assignment forms;
- MySQL-compatible unknown charset, unknown collation, and incorrect argument
  type diagnostics for covered assignment forms;
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
SHOW VARIABLES LIKE 'character_set_database'
SET SESSION character_set_database=latin1
SET LOCAL collation_database=utf8mb4_bin
SET character_set_database=33
SET collation_database=DEFAULT
SET GLOBAL character_set_database=DEFAULT
```

## Non-Goals

This feature must not implement:

- value-changing shared `GLOBAL` assignments, startup options, persisted
  variables, `SET_VAR` hints, or Performance Schema variable state;
- variables other than `character_set_database` and `collation_database`;
- additional `CREATE DATABASE` / `ALTER DATABASE` charset-collation option
  semantics beyond the descriptor-owned forms already documented elsewhere;
- Performance Schema variable tables or `INFORMATION_SCHEMA`
  character-set/collation tables;
- client charset negotiation through a wire protocol;
- string, text, enum, set, binary, or blob column types;
- character-set conversion, introducer semantics, collation coercibility,
  string comparison semantics, or protocol character-set metadata;
- table-backed variable evaluation, clauses, subqueries, arithmetic,
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
- Runtime execution owns system-variable path parsing, scope validation,
  database default lookup, assignment resolution, and diagnostics for
  unsupported names.
- Schema/catalog code owns selected schema state and schema descriptors. This
  feature reads selected-schema descriptor defaults and clears handle-local
  database variable overrides on `USE`; it does not mutate catalog generation.
- The static MySQL charset/collation catalog owns name, alias, default
  collation, and collation-ID lookup.
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

For this slice, `global` reads return MyLite's fixed embedded server-default
fallback values. Session, local, and unscoped reads return the handle-local
assignment state when one exists, otherwise the selected schema descriptor
defaults, otherwise the fixed server fallback values. This is a deliberate
MyLite limitation: there is no shared mutable global server state or persisted
database variable state.

## Runtime Semantics

The supported variables return:

| Variable | Initial/fallback value |
| --- | --- |
| `character_set_database` | `utf8mb4` |
| `collation_database` | `utf8mb4_0900_ai_ci` |

For current MyLite schemas, `CREATE DATABASE` / `ALTER DATABASE` admitted
charset and collation options update descriptor defaults. `USE` switches these
variables to the selected schema descriptor defaults and clears previous direct
database variable overrides. Dropping the selected schema clears `DATABASE()`
through the existing schema lifecycle behavior; these variables then expose the
server fallback defaults. Session/local/unscoped assignments are handle-local
and nonpersistent.

Assignments resolve values as follows:

- `DEFAULT` resolves to the selected schema defaults when a database is
  selected, otherwise `utf8mb4` / `utf8mb4_0900_ai_ci`.
- `character_set_database=<charset>` stores the canonical charset name and
  that charset's default collation.
- `character_set_database=<integer>` treats the integer as a collation ID,
  stores that collation's charset, and uses that charset's default collation.
- `collation_database=<collation>` stores the canonical collation name and
  that collation's charset.
- `collation_database=<integer>` treats the integer as a collation ID and
  stores that collation with its charset.
- `SET GLOBAL` accepts only exact/default no-op forms resolving to the fixed
  embedded global defaults; value-changing forms return MyLite's fixed no-op
  assignment diagnostic.

Successful scalar database-variable selects:

- return one result row;
- return one result column per select item;
- use the source expression text as each column name;
- use `affected_rows == 0` under the existing row-result convention;
- use result `warning_count == 0` unless they intentionally read a previous
  assignment warning through `@@warning_count`;
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
- unknown charset assignment values: error `1115`, SQLSTATE `42000`;
- unknown collation assignment values: error `1273`, SQLSTATE `HY000`;
- decimal assignment values: error `1232`, SQLSTATE `42000`;
- unsupported value-changing global assignments: deterministic MyLite
  unsupported diagnostic for fixed no-op system variable assignments;
- database variable assignments: warning `1681`, SQLSTATE `HY000`;
- `utf8` charset alias assignment: warning `3719`, SQLSTATE `HY000`, emitted
  before warning `1681`;
- `utf8mb3` charset assignment by canonical name or integer collation ID:
  warning `1287`, SQLSTATE `HY000`, emitted before warning `1681`;
- `utf8mb3` collation assignment: warning `3778`, SQLSTATE `HY000`, emitted
  before warning `1681`;
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
- mutate catalog descriptor rows;
- change catalog generation or SQLite schema generation;
- alter physical user tables;
- alter the `.mylite` preamble or shifted SQLite payload invariant;
- add SQLite fork patches.

## Tests

Add a MySQL expectation script that runs against MySQL 8.4.9 with
`--default-character-set=utf8mb4` and verifies:

- result labels and values for no-scope, session, local, and global forms;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` readback;
- session/local/unscoped assignment by charset name, collation name, integer
  collation ID, user-variable integer value, and `DEFAULT`;
- coupled charset/collation readback after assignment;
- warnings for database assignment, `utf8` aliases, and `utf8mb3`
  charset/collation assignments;
- unknown charset, unknown collation, and incorrect type diagnostics;
- case-insensitive scope and variable names;
- backtick-quoted final variable names;
- no-selected-database fallback;
- selected default database behavior, including non-`utf8mb4` descriptor
  defaults and reset of direct overrides on `USE`;
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
- `SHOW VARIABLES` readback after session assignment;
- assignment by charset name, collation name, integer collation ID,
  user-variable integer value, and `DEFAULT`;
- coupled charset/collation readback after assignment;
- assignment warnings and diagnostics;
- exact/default fixed-global no-op forms and value-changing global rejection;
- `FROM DUAL`;
- mixed scalar variables with `DATABASE()`, `@@warning_count`, `@@error_count`,
  and `ROW_COUNT()`;
- global/session/local scopes and quoted final names;
- selected schema, selected-schema default reset on `USE`, dropped selected
  schema, and close/reopen behavior;
- unknown names and quoted-scope diagnostics;
- warning/error diagnostics clearing;
- file-backed preamble preservation;
- catalog and SQLite schema generation remaining unchanged;
- independent handles.

No public ABI changes are expected.
