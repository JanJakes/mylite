# Baseline Insert Set Lifecycle

## Status

This feature specifies the next narrow DML slice for file-backed `.mylite`
handles. It adds descriptor-driven `INSERT ... SET` execution on top of
`mylite_execute()`, statement context, MyLite parser scaffolding, shifted
`.mylite` storage, durable catalog descriptors, create/drop/rename table
lifecycle, and the existing integer/`NULL` `INSERT ... VALUES` row-write path.

The feature is intentionally not full MySQL `INSERT` support. It supports one
persistent base-table target, one inserted row, one or more unqualified
assignment targets, supported decimal integer/`NULL` assignment values, and the
same descriptor-driven assignment conversion already used by the row-values
phase.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_set_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT INTO t SET a = 1` and `INSERT t SET a = 1` both succeed.
- Schema-qualified targets work without a selected default schema.
- Unqualified targets without a selected default schema fail with error `1046`,
  SQLSTATE `3D000`, and message `No database selected`.
- Unknown schemas fail with error `1049`, SQLSTATE `42000`; unknown tables
  fail with error `1146`, SQLSTATE `42S02`.
- `SET` assignments identify columns explicitly. Omitted nullable columns
  receive `NULL`; omitted `NOT NULL` columns without an explicit default fail
  with error `1364`, SQLSTATE `HY000`.
- Successful inserts report one affected row, warning count `0`, and make
  `ROW_COUNT()` return `1`.
- Duplicate assignment targets fail with error `1110`, SQLSTATE `42000`.
- Unknown assignment targets fail with error `1054`, SQLSTATE `42S22`.
- Assigning `NULL` to a `NOT NULL` column fails with error `1048`, SQLSTATE
  `23000`.
- Strict-mode integer out-of-range assignments fail with error `1264`,
  SQLSTATE `22003`.
- MySQL accepts wider forms that this slice defers, including table-qualified
  assignment targets, expression assignments, `DEFAULT`, `LOW_PRIORITY`,
  `HIGH_PRIORITY`, and `IGNORE`.
- MySQL `IGNORE` can demote strict errors to warnings and coerce `NULL` for a
  numeric `NOT NULL` column to `0`; that warning-demotion surface is out of
  scope for this slice.

## Scope

The implementation must add:

- parser and AST support for limited `INSERT [INTO] table_name SET ...`;
- optional `INTO` for this `SET` form;
- one inserted row per statement;
- one or more unqualified assignment targets;
- assignment values limited to decimal integer literals with optional unary
  sign and `NULL`;
- unqualified and schema-qualified target table resolution using the existing
  selected/default schema policy;
- persistent base-table descriptors only;
- descriptor-driven assignment target resolution and duplicate target
  detection;
- omitted nullable-column binding as SQL `NULL`;
- omitted `NOT NULL` no-default rejection using the current row-values policy;
- MyLite-owned integer/`NULL` assignment conversion before SQLite binding;
- generated SQLite physical `INSERT` execution built only from descriptors and
  stable physical table names;
- affected-row, warning-count, row-count, and non-row result behavior matching
  the existing row-values insert conventions;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `INSERT ... VALUES` changes beyond any parser helper refactor needed to share
  `INSERT` target syntax;
- `LOW_PRIORITY`, `DELAYED`, `HIGH_PRIORITY`, `IGNORE`, `PARTITION`,
  `ON DUPLICATE KEY UPDATE`, row aliases, `RETURNING`, `INSERT ... SELECT`,
  `INSERT ... TABLE`, `VALUES ROW()`, or `INSERT ... VALUES` row aliases;
- table-qualified assignment targets, aliases, multiple target tables, or
  arbitrary SQLite SQL pass-through;
- expression assignments, column-to-column assignments, arithmetic
  assignments, `DEFAULT`, default expressions, `DEFAULT(col)`, parameters,
  variables, functions, subqueries, string/decimal/float/hex/bit/date/time/json
  assignment values, or general expression evaluation;
- auto-increment, `LAST_INSERT_ID()`, generated columns, check constraints,
  primary/unique/foreign keys, triggers, cascades, indexes, temporary tables,
  views, privileges, warning demotion, non-strict SQL modes, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and transaction completion.
  Successful `INSERT ... SET` copies the inserted row count to the public
  result.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, target tables, and assignment
  columns against MyLite catalog descriptors; rejects unsupported shapes;
  converts supported assignment values; and builds a descriptor-driven physical
  insert plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. `INSERT ... SET` does
  not mutate catalog rows or SQLite schema generation.
- SQLite owns physical b-tree row storage and rollback durability for generated
  prepared statements. SQLite schema text and `PRAGMA` output are not metadata
  authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Inserts occur only inside the shifted SQLite payload and must not touch byte
  range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
INSERT [INTO] table_name
SET assignment[, assignment ...]

assignment:
    column_name = insert_value

insert_value:
    integer_literal
  | + integer_literal
  | - integer_literal
  | NULL
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

Only unqualified assignment targets are supported semantically. The parser may
admit qualified assignment targets so the analyzer can reject them with a
deterministic unsupported diagnostic, but implementation may also reject them
syntactically if that is cleaner and covered by tests.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= insert_set_statement(B). {
    A = B;
}

insert_set_statement(A) ::=
    INSERT(I) into_opt table_name(T) SET assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S);
}

into_opt ::= .
into_opt ::= INTO.

assignment_list(A) ::= assignment(B). {
    A = mylite_sql_parser_make_insert_assignment_list(state, B);
}
assignment_list(A) ::= assignment_list(B) COMMA assignment(C). {
    A = mylite_sql_parser_append_insert_assignment(state, B, C);
}

assignment(A) ::= assignment_target(T) EQUAL insert_value(V). {
    A = mylite_sql_parser_make_insert_assignment(state, T, V);
}

assignment_target(A) ::= identifier(B). {
    A = B;
}
assignment_target(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

insert_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
insert_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_signed_integer_literal(state, P, T);
}
insert_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_signed_integer_literal(state, M, T);
}
insert_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
```

The existing `INSERT ... VALUES` grammar may keep requiring explicit `INTO` in
this phase. Optional `INTO` is required only for the new `SET` form.

## Schema And Table Resolution

`INSERT ... SET` follows the same target resolution policy as `INSERT ...
VALUES`:

- unqualified targets use the selected schema;
- missing selected schema fails with `1046` / `3D000`;
- schema-qualified targets resolve the schema explicitly and do not require a
  selected schema;
- unknown schemas fail with `1049` / `42000`;
- unknown tables fail with `1146` / `42S02`;
- user-authored `_mylite_*` schema and table names are rejected before any
  SQLite SQL is generated;
- targets must be persistent base-table descriptors. Future temporary tables,
  views, or other object kinds are rejected until specified.

## Assignment Resolution

Assignment target resolution uses MyLite column descriptors read in catalog
ordinal order. SQLite metadata is not consulted.

Supported assignment targets are unqualified descriptor column names. Matching
uses the current descriptor name policy: ASCII case-insensitive comparisons
against stored catalog names, without collation-aware identifier folding.

The planner must:

- reject table-qualified assignment targets for this slice;
- reject duplicate assignment targets before conversion or SQLite execution;
- reject unknown assignment targets before conversion or SQLite execution;
- preserve assignment order for duplicate detection and diagnostics, but build
  the physical insert row in descriptor/target order for generated SQL;
- bind omitted nullable columns as SQL `NULL`;
- reject omitted `NOT NULL` columns because MyLite has no explicit default
  descriptors yet.

## Assignment Conversion

The supported assignment values are the same as the row-values phase:

- decimal integer literals with optional unary `+` or `-`;
- `NULL`.

Conversion is MyLite-owned and occurs before SQLite binding. Supported integer
families keep their current physical signed-64-bit encoding:

| Logical descriptor | Accepted assignment range |
| --- | --- |
| `INT` / `INTEGER` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` / `INTEGER UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` in this slice |

`BIGINT UNSIGNED` values above `9223372036854775807` remain out of scope until
MyLite specifies a physical unsigned-64-bit encoding. Out-of-range assignments
fail before writing any row. `NULL` assigned to a nullable descriptor binds SQL
`NULL`; `NULL` assigned to `NOT NULL` fails with the current MyLite
`Column '<name>' cannot be null` diagnostic.

Unsupported values such as `DEFAULT`, strings, decimals, floats, hex, bit
literals, parameters, functions, column references, and arithmetic expressions
are rejected deterministically. MySQL accepts some of these forms; this slice
does not.

## Physical SQLite Handling

The physical write path should reuse the existing descriptor-driven insert
machinery where possible:

- generate SQLite `INSERT INTO "physical_table" ("physical_column", ...)
  VALUES (?, ...)` from descriptors and stable physical table names;
- quote every generated SQLite identifier;
- bind integers with `sqlite3_bind_int64()` and `NULL` with
  `sqlite3_bind_null()`;
- convert every assignment and omitted-column value before the first write;
- execute inside a MyLite-owned write transaction and roll back on any prepare,
  bind, step, allocation, or SQLite failure;
- keep catalog rows, descriptor versions, descriptor caches, catalog
  generation, and `sqlite_schema_generation` unchanged.

No SQLite fork patch is required.

## Result Behavior

Successful `INSERT ... SET` returns through the existing public result
conventions for non-row statements:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows == 1`;
- `warning_count == 0`;
- following `SELECT ROW_COUNT()` returns `1`.

The inserted row must be visible to descriptor-driven `SELECT`, `COUNT(*)`,
filtered `SELECT`, ordered/limited `SELECT`, `UPDATE`, `DELETE`, `TRUNCATE`,
`SHOW CREATE TABLE`, and reopen persistence exactly as an equivalent supported
`INSERT ... VALUES` row.

Failed `INSERT ... SET` statements do not insert a partial row and do not
mutate catalog state.

## Diagnostics

The implementation must cover:

- syntax errors and unsupported grammar;
- public API misuse if any public surface changes, otherwise preserve existing
  public execution/result misuse behavior;
- missing selected schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema or table names;
- unsupported object kind;
- unknown assignment column;
- duplicate assignment column;
- table-qualified assignment target;
- omitted `NOT NULL` no-default column;
- `NULL` into `NOT NULL`;
- unsupported assignment expression or literal;
- integer out-of-range assignment;
- physical SQLite prepare/bind/step failures;
- allocation failures.

Supported in-range inserts must leave warning count `0`.

## Tests

Add a fast plain C test under `packages/libmylite/tests/`, preferably
`runtime_insert_set_lifecycle`, and register it with a dotted CTest name.

Cover:

- successful `INSERT INTO table SET ...` and `INSERT table SET ...`;
- schema-qualified and unqualified target resolution;
- missing default schema, unknown schema, unknown table, and reserved target
  names;
- multiple assignments, descriptor ordering, omitted nullable columns, and all
  current integer families;
- signed/unsigned boundary assignments and out-of-range diagnostics;
- `NULL` assignments, including nullable columns and `NOT NULL` rejection;
- omitted `NOT NULL` no-default rejection;
- duplicate assignment targets, unknown assignment targets, table-qualified
  assignment targets, and unsupported value forms;
- successful result object shape, `affected_rows`, `warning_count`, and
  `ROW_COUNT()`;
- readback through existing descriptor-driven `SELECT`, `COUNT(*)`, filtered
  `SELECT`, ordered/limited `SELECT`, `UPDATE`, and `DELETE` where relevant;
- reopen persistence, table rename interaction, drop interaction, independent
  file-backed handles, and preamble preservation;
- zero-initialized cleanup for new planner/test helper objects.

Also keep the existing lexer, parser, runtime handle, diagnostics, statement
context, result metadata, SQLite bootstrap, file-backed opening, VFS, catalog
foundation, basic table lifecycle, table rename lifecycle, row values, select,
delete, update, truncate, introspection, client-data, and registration tests
passing.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` only for
the exact supported subset. Update `docs/compatibility/operators.md` and
`docs/compatibility/type-system-literals-conversion.md` only if the
implementation changes the documented assignment/literal surface beyond the
existing row-values insert conversion.

Do not overclaim full `INSERT`, defaults, aliases, priorities, `IGNORE`,
partitions, row aliases, `ON DUPLICATE KEY UPDATE`, warning demotion,
expression assignments, table-qualified assignment targets, generated columns,
constraints, indexes, auto-increment, insert-id behavior, privileges, or
non-strict SQL modes.
