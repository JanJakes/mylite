# Baseline INSERT SELECT ON DUPLICATE KEY UPDATE

## Summary

This phase connects the existing descriptor-driven `INSERT ... SELECT` source
pipeline with the existing descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` duplicate branch:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED]
    [INTO] table_name [(column_name[, ...])]
    select_source
ON DUPLICATE KEY UPDATE duplicate_assignment[, ...]
```

The implementation remains deliberately narrow. It supports the current
`INSERT ... SELECT` source envelopes and the current duplicate-update
assignment value envelope. MyLite still owns descriptor resolution, value
conversion, duplicate-key selection, warning accounting, affected-row
accounting, transaction rollback, and generated SQLite statement construction.
SQLite remains the physical row store and source materialization engine.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-insert-select-lifecycle/specs.md`
  - `docs/specs/baseline-insert-select-dual-source/specs.md`
  - `docs/specs/baseline-insert-select-keyed-targets/specs.md`
  - `docs/specs/baseline-insert-select-union-source/specs.md`
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-assignments/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-enforced-keys/specs.md`
  - `docs/specs/baseline-insert-duplicate-key-column-assignments/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... SELECT`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-select.html>
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
- Runtime probes captured in
  `packages/libmylite/tests/mysql_baseline_insert_select_on_duplicate_key_update_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for the admitted shape:

- `ON DUPLICATE KEY UPDATE` appears after the `SELECT` / compound source in
  `INSERT ... SELECT`.
- A duplicate branch that changes at least one assignment reports affected rows
  `2`; a duplicate branch that stores current values reports `0`; inserted
  rows report `1`.
- In one statement, selected rows are processed in selected-row order. Later
  source rows see earlier inserts and duplicate updates from the same
  statement.
- `VALUES(column_name)` in the duplicate branch refers to the proposed inserted
  row value for that selected row and records warning `1287` once per admitted
  occurrence, even when the source produces zero rows or the table has no
  enforced key.
- Literal `DEFAULT` and `NULL` duplicate assignment values follow the existing
  duplicate-update conversion rules.
- Unknown duplicate assignment targets and unknown `VALUES()` columns fail with
  `1054 / 42S22` and `field list` context.
- `NULL` into a `NOT NULL` duplicate assignment target fails with
  `1048 / 23000`.
- Out-of-range duplicate assignment conversion fails with `1264 / 22003` and
  rolls the duplicate branch and statement back.
- MySQL accepts `INSERT IGNORE ... SELECT ... ON DUPLICATE KEY UPDATE`, but
  MyLite keeps `IGNORE` outside this slice to preserve the current ODKU
  boundary.
- MySQL's auto-increment allocation for multi-row `INSERT ... SELECT ... ON
  DUPLICATE KEY UPDATE` has reservation behavior that differs from the current
  MyLite insert-select streaming counter path. MyLite rejects auto-increment
  targets for this slice rather than approximating that behavior.

## Scope

The implementation supports:

- existing plain table-backed, row-scalar no-source/`DUAL`, and unparenthesized
  `UNION` / `UNION ALL` `INSERT ... SELECT` source envelopes;
- persistent or shadowing session temporary base-table targets already
  admitted by the current insert-select target path, excluding auto-increment
  targets for this phase;
- target column-list mapping, invisible-column access, omitted-column default
  filling, source materialization, target conversion, strict diagnostics,
  duplicate-key checks, foreign-key child checks, same-table source
  materialization, result shape, table timestamp updates, and file-backed
  persistence from the existing insert-select path;
- one or more distinct unqualified duplicate-update assignment targets;
- current duplicate-update value forms: supported insert literals,
  `NULL`, `TRUE`, `FALSE`, `DEFAULT`, `DEFAULT(column_name)` where already
  accepted by the current duplicate-update path, and same-target
  `VALUES(column_name)`;
- duplicate conflict handling for the current primary-key and unique-index
  descriptor subset, including multiple enforced keys, composite keys, and
  prefix keys already owned by the existing ODKU path;
- MySQL-compatible affected rows for inserted, changed duplicate, and no-op
  duplicate rows in the admitted subset;
- `VALUES()` deprecation warning `1287` once per admitted `VALUES()` occurrence;
- `LOW_PRIORITY` and `HIGH_PRIORITY` as existing no-op modifiers;
- `DELAYED` as the existing immediate insert with warning `3005`;
- all-or-nothing statement behavior inside the existing statement transaction;
- focused MySQL-runtime expectation coverage, C runtime tests, parser tests,
  compatibility documentation, and build registration.

## Non-Goals

This phase does not add:

- `INSERT IGNORE ... SELECT ... ON DUPLICATE KEY UPDATE`;
- auto-increment targets for insert-select duplicate updates;
- new source query shapes beyond the current insert-select table-backed,
  row-scalar, and compound source envelopes;
- target aliases, target partitions, `TABLE` sources, row constructors, CTEs,
  parenthesized query expressions, derived-table sources, joined source forms,
  grouped source forms, global compound ordering/limiting, or branch-local
  compound ordering/limiting;
- table-qualified duplicate assignment targets, duplicate assignment targets,
  qualified or cross-column `VALUES()` references;
- source-column references in the duplicate branch, expression assignments,
  column-to-column assignments, arithmetic assignments, functions beyond
  already admitted duplicate-update value forms, variables, parameters, casts,
  collations, or general expression conversion;
- selected-row warning demotion, broad non-strict conversion, triggers,
  generated columns, recursive foreign-key actions, privilege semantics,
  replication safety metadata, protocol information strings, optimizer
  behavior, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns call validation, statement
  dispatch, result-handle ownership, and failure cleanup.
- Statement context: owns diagnostics, warning count, affected rows,
  `ROW_COUNT()`, `LAST_INSERT_ID()` preservation, transaction completion, and
  non-row result finalization.
- Lexer/parser/AST: admits the optional duplicate-key tail after existing
  insert-select sources and stores it as an AST child. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner: resolves the target table, target columns, source shape,
  source columns, duplicate assignment targets, `VALUES()` references, key
  descriptors, and unsupported shape boundaries from MyLite descriptors before
  physical SQL is generated.
- Catalog: remains authoritative for schemas, table identity, physical names,
  columns, defaults, keys, foreign keys, and auto-increment descriptors. This
  DML feature does not mutate catalog rows except normal table timestamp
  updates caused by successful row writes.
- Runtime: materializes or evaluates the source with the existing
  insert-select path, turns each selected row into a one-row `planned_insert`,
  and routes it through the existing duplicate-update executor. The full
  selected row set is not buffered in C memory.
- SQLite physical storage: owns scans, filtering, ordering, temporary tables,
  and b-tree row mutations for descriptor-built physical statements. Generated
  SQLite identifiers are quoted and values are bound.
- Storage/VFS/file format: unchanged. The feature writes only through the
  shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

The duplicate-key tail applies to the existing insert-select source
nonterminal:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED]
    [INTO] table_name [(column_name[, ...])]
    insert_select_source
ON DUPLICATE KEY UPDATE duplicate_assignment[, ...]

duplicate_assignment:
    column_name = duplicate_update_value

duplicate_update_value:
    supported_insert_value
  | DEFAULT
  | DEFAULT ( column_name )
  | VALUES ( column_name )
```

`insert_select_source` is exactly the current MyLite insert-select source
envelope:

- a supported descriptor-backed single-table `SELECT`;
- a supported no-source or `FROM DUAL` row-scalar `SELECT`;
- a supported unparenthesized compound `SELECT ... UNION ... SELECT` source.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar change, not MySQL's full
grammar:

```lemon
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL, D);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T)
    insert_column_list_opt(C) insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(
        state, I, T, C, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D);
}
```

The no-`INTO` alternatives are equivalent. Runtime rejects `IGNORE` when the
duplicate-key tail is present.

## Planning And Runtime Semantics

Planning follows the existing insert-select order:

1. Resolve the target table and target columns through descriptors.
2. Load columns, primary key, secondary indexes, foreign keys, and
   auto-increment metadata for the target.
3. Resolve the target column list and reject duplicate target columns.
4. Resolve and validate the duplicate-update tail, if present.
5. Reject auto-increment targets when the duplicate-update tail is present.
6. Reject `IGNORE` with a duplicate-update tail.
7. Plan the existing insert-select source shape.

For each selected row:

1. MyLite materializes the selected row through the existing insert-select
   source path.
2. Omitted target values are filled from descriptor defaults only when a source
   row exists.
3. The row is validated and converted using existing insert-select rules.
4. The normal insert attempt runs through the existing prepared physical
   `INSERT`.
5. If no supported duplicate key is hit, affected rows increase by `1`.
6. If a supported duplicate key is hit, the current duplicate-update executor
   converts assignment values, checks second-order key conflicts, executes one
   descriptor-built physical `UPDATE`, and increases affected rows by `2` for
   changed rows or `0` for no-op rows.
7. Any conversion, duplicate-key, foreign-key, physical SQLite, or allocation
   failure rolls back the whole statement.

`VALUES(column_name)` uses the fully planned proposed value for the current
selected row after target column mapping, omitted defaults, and target
conversion. This phase keeps the current same-target restriction, so
`v = VALUES(v)` is supported and `v = VALUES(other)` is rejected.

## Diagnostics

Supported diagnostics use existing MyLite/MySQL-compatible paths:

- missing default schema, unknown schema, unknown target table, and
  `information_schema` write attempts through existing target resolution;
- reserved `_mylite_*` names before generated SQLite SQL;
- unsupported `IGNORE` with ODKU using the existing ODKU unsupported
  diagnostic;
- unsupported auto-increment target with ODKU using a deterministic MyLite
  unsupported diagnostic;
- unknown assignment column or `VALUES()` column with `1054 / 42S22`;
- duplicate assignment targets and qualified assignment/value references using
  current ODKU unsupported diagnostics;
- source planning, source column-count, branch column-count, target/source
  count mismatch, and selected-row validation through existing insert-select
  diagnostics;
- `NULL` into `NOT NULL`, out-of-range assignment values, bad defaults,
  duplicate-key conflicts created by the duplicate branch, and foreign-key
  failures through existing insert and ODKU conversion/constraint diagnostics;
- physical SQLite and allocation failures through existing runtime policies.

Successful supported statements return through the existing non-row result
shape: zero result columns, zero result rows, MySQL-compatible affected rows,
and the accumulated warning count.

## Performance

This phase does not add C-side full-result materialization. Table-backed and
compound sources keep the current SQLite temporary table materialization so
same-table source/target statements use a stable selected row set. Row-scalar
sources materialize at most one row. Each selected row is then streamed through
the existing one-row insert executor and duplicate-update branch. The added
overhead is duplicate-tail planning plus duplicate-branch work only for rows
that hit an enforced key.

No SQLite fork patch is required.

## Test Plan

- MySQL expectation script for table-backed, row-scalar, `DUAL`, compound,
  no-key, no-op, changed duplicate, multiple assignment, zero-row, same-table,
  literal/default/null, warning, diagnostic, and unsupported auto-increment /
  `IGNORE` behavior.
- Parser tests for duplicate-key tails on table-backed, row-scalar, compound,
  modifier, and `IGNORE` insert-select statements.
- Runtime C tests for:
  - changed, no-op, mixed insert/update, and no-key insert paths;
  - `VALUES()` warning count and `SHOW WARNINGS` text;
  - row-scalar and compound sources;
  - same-table materialization;
  - schema-qualified and unqualified target resolution;
  - unknown assignment and `VALUES()` columns;
  - `NULL` into `NOT NULL`, range conversion failure, statement rollback, and
    unsupported auto-increment / `IGNORE`;
  - affected rows, warning count, no result rows, persistence after reopen,
    preamble preservation, and independent file-backed handles.
- Focused parser/runtime CTest entries, the MySQL expectation script, and the
  full `cmake --workflow --preset check`.
