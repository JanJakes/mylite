# Baseline Default Temporary Storage Engine System Variable

## Status

This feature specifies a narrow system-variable slice for
`@@default_tmp_storage_engine`.

MyLite remains a single-engine embedded runtime. Supported persistent and
temporary user tables are stored through the MyLite InnoDB-compatible table
path, and explicit non-InnoDB `ENGINE` clauses are already handled by the
storage-engine substitution baseline. This slice exposes MySQL's temporary
default-engine variable for application detection without adding alternate
temporary storage engines.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline default storage engine system variable:
  `docs/specs/baseline-default-storage-engine-system-variables/specs.md`
- Baseline storage engine substitution:
  `docs/specs/baseline-storage-engine-substitution/specs.md`
- Baseline temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, setting the storage engine:
  https://dev.mysql.com/doc/refman/8.4/en/storage-engine-setting.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_default_tmp_storage_engine_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@default_tmp_storage_engine`,
  `@@global.default_tmp_storage_engine`,
  `@@session.default_tmp_storage_engine`, and
  `@@local.default_tmp_storage_engine` return `InnoDB` in the tested default
  MySQL 8.4.9 runtime.
- `SHOW VARIABLES LIKE 'default_tmp_storage_engine'` and
  `SHOW GLOBAL VARIABLES LIKE 'default_tmp_storage_engine'` return `InnoDB`.
- The variable has global and session scope. MySQL accepts
  `SET SESSION default_tmp_storage_engine=MEMORY` and later unscoped,
  session, and local reads return `MEMORY`; the tested global value remains
  `InnoDB`.
- The session value affects `CREATE TEMPORARY TABLE` without an explicit
  engine. After `SET SESSION default_tmp_storage_engine=MEMORY`, MySQL renders
  the temporary table with `ENGINE=MEMORY`; after
  `SET SESSION default_tmp_storage_engine=MyISAM`, it renders
  `ENGINE=MyISAM`.
- `SET SESSION default_tmp_storage_engine=DEFAULT`,
  `SET @@default_tmp_storage_engine='InnoDB'`, and
  `SET LOCAL default_tmp_storage_engine=InnoDB` keep the session value at
  `InnoDB` and report zero warnings.
- `SET SESSION default_tmp_storage_engine=NoSuchEngine` and
  `SET SESSION default_tmp_storage_engine=''` fail with error `1286`,
  SQLSTATE `42000`, and an `Unknown storage engine` message. This SET-time
  validation does not depend on `NO_ENGINE_SUBSTITUTION`.
- Variable and scope names are case-insensitive. Backtick-quoted final
  variable-name components are accepted, while quoted scope names remain a
  syntax error through the existing system-variable parser.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for later diagnostic
  statements.

The official MySQL system-variable documentation classifies
`default_tmp_storage_engine` as a dynamic enumeration variable with global and
session scope. MyLite intentionally keeps the value fixed for now because it
does not implement real MEMORY, MyISAM, or other alternate temporary engines.

## Scope

The implementation must add:

- runtime recognition of `default_tmp_storage_engine` inside the existing
  scalar `SELECT` system-variable subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` rows through the existing
  descriptor-driven system-variable list;
- fixed value `InnoDB` for every supported scope and SHOW row;
- one-row scalar result sets with existing source-span column labels;
- MySQL-compatible unknown-variable diagnostics for unsupported names and
  deterministic quoted-scope rejection through the existing parser;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@default_tmp_storage_engine
SELECT @@default_tmp_storage_engine FROM DUAL
SELECT @@session.default_tmp_storage_engine, @@local.default_tmp_storage_engine
SELECT @@global.default_tmp_storage_engine
SELECT @@session.`default_tmp_storage_engine`, @@`default_tmp_storage_engine`
SHOW VARIABLES LIKE 'default_tmp_storage_engine'
SHOW GLOBAL VARIABLES LIKE 'default_tmp_storage_engine'
```

## Non-Goals

This feature must not implement:

- mutable `SET default_tmp_storage_engine`, startup options, persisted
  variables, `SET_VAR` hints, or shared mutable global engine state;
- real MEMORY, MyISAM, CSV, ARCHIVE, or other alternate temporary storage
  engines;
- default temporary engine routing that changes physical table storage;
- durable or session-local engine metadata for implicit temporary tables
  outside the existing InnoDB-compatible rendering;
- `disabled_storage_engines`, engine plugin loading, or Performance Schema
  variable tables;
- wider expression use of system-variable values, table-backed variable
  evaluation, aliases, clauses, subqueries, parameters, or arbitrary SQLite
  pass-through;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to own live diagnostics, previous diagnostics
  snapshots, and nondiagnostic scalar statement clearing.
- Lexer/parser/AST already admit `SYSTEM_VARIABLE` expressions and preserve
  source spans. No grammar changes are required.
- Runtime execution owns system-variable path parsing, scope validation, fixed
  temporary default-engine value selection, and diagnostics for unsupported
  names.
- SHOW-variable execution owns descriptor enumeration, scope filtering, LIKE
  filtering, and row construction.
- Temporary table descriptors remain authoritative for session-local table
  metadata. This feature does not change temporary table creation, shadowing,
  cleanup, row storage, or generated SQLite SQL.
- Catalog descriptors for persistent tables are not read or mutated. Catalog
  generation and SQLite schema generation must remain unchanged for scalar
  reads and SHOW-variable reads.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not touch preamble bytes or require SQLite extension changes.

## Supported SQL Grammar

This slice reuses the existing system-variable expression grammar:

```sql
system_variable:
    @@ default_tmp_storage_engine
  | @@ scope . default_tmp_storage_engine

scope:
    global
  | session
  | local
```

Relevant independently authored MyLite Lemon-syntax shape, already present in
the parser:

```lemon
expr(A) ::= SYSTEM_VARIABLE(V). {
    A = mylite_sql_parser_make_system_variable_expr(state, V);
}
```

The runtime parses the `SYSTEM_VARIABLE` token body so the same grammar covers
unquoted and quoted final variable-name components. A quoted scope remains
outside this slice.

## Runtime Semantics

`default_tmp_storage_engine` is resolved through the existing
case-insensitive system-variable descriptor table. Supported scalar reads return
the text value `InnoDB` for all supported scopes.

`SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` include one row:

| Variable_name | Value |
| --- | --- |
| `default_tmp_storage_engine` | `InnoDB` |

The value is intentionally fixed. It documents MyLite's current temporary table
storage behavior rather than exposing a mutable setting that could promise a
physical engine MyLite cannot provide.

Supported reads do not emit warnings. A successful scalar read keeps existing
system-variable diagnostics behavior: any `@@warning_count` or `@@error_count`
items in the same result see the previous diagnostics snapshot, and the
statement then clears live diagnostics for following diagnostic statements.

Temporary tables created without an explicit `ENGINE` continue through the
existing InnoDB-compatible temporary table path and render `ENGINE=InnoDB`.
Explicit temporary `ENGINE` clauses continue to be governed by the storage
engine substitution baseline and session `@@sql_mode`.

## Diagnostics

This feature reuses existing diagnostics:

| Case | Diagnostic |
| --- | --- |
| Unknown system variable | `1193 / HY000`, `Unknown system variable '<name>'` |
| Quoted scope component | deterministic unsupported quoted-scope syntax error |
| Unsupported expression shape | existing scalar-expression unsupported diagnostic |
| Allocation failure | existing `MYLITE_NOMEM` path and public diagnostics |
| Public API misuse | existing `MYLITE_MISUSE` handling |

This slice deliberately does not add `SET default_tmp_storage_engine`
diagnostics. Existing fixed-variable SET handling remains authoritative for
what MyLite currently accepts or rejects outside this feature.

## Testing

Runtime tests must cover:

- scalar values for no scope, `global`, `session`, and `local`;
- case-insensitive names and quoted final variable-name components;
- `FROM DUAL`, mixed scalar reads, and selected-schema independence;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` rows and LIKE filters;
- diagnostics clearing after warning and error snapshots;
- unchanged catalog and SQLite schema generations;
- `.mylite` preamble preservation;
- close/reopen readback and independent handles;
- no change to implicit temporary table `SHOW CREATE TABLE` rendering.

The MySQL expectation script must record MySQL 8.4.9 values, SHOW rows,
session mutability evidence, temporary-table effect evidence, invalid-engine
SET diagnostics, quoted-name behavior, and diagnostics lifecycle behavior.

## Compatibility Notes

The supported behavior is useful for applications that inspect the current
temporary default engine. It is intentionally not full MySQL variable
management. Applications that set `default_tmp_storage_engine` to `MEMORY` or
`MyISAM` still require a later feature that either implements compatible
alternate temporary storage behavior or returns a precise embedded-design
diagnostic.
