# Baseline UPDATE Index Hints No-Op

## Summary

This phase admits MySQL-style index hints on the target table of the existing
single-table `UPDATE` subset:

```sql
UPDATE table_name USE INDEX|KEY [FOR scope] (...) SET ...
UPDATE table_name FORCE INDEX|KEY [FOR scope] (...) SET ...
UPDATE table_name IGNORE INDEX|KEY [FOR scope] (...) SET ...
```

The hints are parsed, stored in the MyLite AST, validated against MyLite-owned
index descriptors, and ignored by physical SQLite planning. Valid hints do not
change which rows are matched or updated. Invalid hint names and invalid hint
combinations produce MySQL-compatible diagnostics before generated SQLite SQL
is prepared.

This phase does not implement optimizer behavior, SQLite index selection,
`UPDATE` aliases, partitions, multi-table update hints, `DELETE` hints,
optimizer comment hints, new-style optimizer hints, index visibility effects,
or protocol-visible plan metadata.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite compatibility slices:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-select-index-hints-noop/specs.md`
  - `docs/specs/baseline-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-secondary-index-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE` statement: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - Index hints: <https://dev.mysql.com/doc/refman/8.4/en/index-hints.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_index_hints_noop_expectations.sh`
  and must be verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- Single-table `UPDATE` accepts `USE INDEX`, `FORCE INDEX`, and `IGNORE INDEX`
  after the target table reference.
- `KEY` is accepted as a synonym for `INDEX`.
- Optional hint scopes are `FOR JOIN`, `FOR ORDER BY`, and `FOR GROUP BY`.
- `USE INDEX ()` is accepted and the statement executes normally.
- `FORCE INDEX ()` and `IGNORE INDEX ()` are syntax errors
  (`1064 / 42000`).
- Duplicate hint names are accepted.
- Unknown hint names fail with `1176 / 42000` and message shape
  `Key '<name>' doesn't exist in table '<table>'`.
- Ambiguous prefixes use the same unknown-key diagnostic shape.
- Unambiguous prefixes and `PRIMARY` prefixes are accepted.
- Combining `USE INDEX` and `FORCE INDEX` on one table reference fails with
  `1221 / HY000` and `Incorrect usage of USE INDEX and FORCE INDEX`, including
  when scopes differ.
- Successful supported updates report changed-row affected counts and
  `@@warning_count = 0`, exactly like the equivalent unhinted `UPDATE`.

## Supported Scope

Supported:

- existing single-table `UPDATE` target tables:
  - persistent MyLite base tables;
  - session temporary base tables that shadow persistent names under the current
    update lifecycle rules;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- existing supported `SET`, `WHERE`, `ORDER BY`, and `LIMIT` update subsets;
- hint kinds:
  - `USE INDEX`;
  - `USE KEY`;
  - `FORCE INDEX`;
  - `FORCE KEY`;
  - `IGNORE INDEX`;
  - `IGNORE KEY`;
- optional hint scopes:
  - omitted scope;
  - `FOR JOIN`;
  - `FOR ORDER BY`;
  - `FOR GROUP BY`;
- hint names from identifiers, quoted identifiers, and unquoted `PRIMARY`;
- empty hint name lists only for `USE INDEX|KEY ()`;
- descriptor-backed validation of named primary-key and secondary-index
  descriptors already supported by MyLite;
- MySQL-compatible case-insensitive exact-name and unambiguous-prefix matching
  for hint names, including `PRIMARY` prefixes;
- MySQL-compatible diagnostics for unknown index names and combined
  `USE INDEX` plus `FORCE INDEX` on one update target;
- successful supported hints as no-op planner directives with no warnings.

Deferred:

- any physical optimizer effect or SQLite index selection;
- aliases on single-table `UPDATE` targets;
- partitions, `LOW_PRIORITY`, `IGNORE`, CTEs, query modifiers, or joined update
  expansion;
- `DELETE` target hints;
- multi-table update hints beyond the current joined-update parser/planner
  envelope;
- index hints on `information_schema` virtual tables;
- optimizer comments and new-style optimizer hints such as `JOIN_INDEX()`;
- metadata exposing selected indexes, optimizer traces, cost estimates,
  privilege effects, or warnings for ignored hints.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns call validation, result-handle
  ownership, diagnostics access, and public misuse behavior.
- Statement context: owns the statement boundary, diagnostics, warnings,
  affected rows, and `ROW_COUNT()`. Successful hinted updates leave
  `warning_count == 0` and report the same changed-row count as the equivalent
  unhinted update.
- Lexer/parser/AST: admits the target hint grammar and records hint nodes in a
  reusable table-source node without assigning execution meaning to them.
- Analyzer/planner: resolves the target table through existing descriptor
  policy, validates hint names through MyLite catalog index descriptors, rejects
  unsupported combinations before generated SQLite SQL is built, and discards
  valid hints after validation.
- Catalog module: remains the authority for logical table and index names.
  Hint validation reads descriptors only; it does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: unchanged. Generated SQL remains descriptor-built
  from stable physical table names and bound parameters. No SQLite fork patch or
  public extension hook is required.
- Storage/VFS/file format: unchanged. Hints do not affect `.mylite` preamble
  bytes or shifted SQLite payload invariants.

## Grammar

Supported target shape:

```sql
UPDATE table_name [index_hint ...]
SET assignment_list
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]
```

Supported hint shapes:

```sql
USE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
USE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
USE INDEX [FOR JOIN|ORDER BY|GROUP BY] ()
USE KEY   [FOR JOIN|ORDER BY|GROUP BY] ()
FORCE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
FORCE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
IGNORE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
IGNORE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
```

MyLite Lemon-syntax snippets:

```lemon
update_statement(A) ::=
    UPDATE(U) update_table_source(T) SET update_assignment_list(S)
    where_clause_opt(W) order_clause_opt(O) update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_update_statement(
        state,
        U,
        (struct mylite_sql_update_statement_parts){
            .target_table = T,
            .assignment_list = S,
            .where_clause = W,
            .order_clause = O,
            .limit_clause = L,
        });
}

joined_update_table_source(A) ::=
    update_table_source(S). {
    A = S;
}

joined_update_table_source(A) ::=
    table_name(N) AS identifier(AL) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}

update_table_source(A) ::= table_name(N) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, NULL, IH);
}
```

`table_index_hints_opt`, `table_index_hint_list`, `table_index_hint`,
`index_hint_keyword`, and `index_hint_scope_opt` are the existing
independently authored MyLite productions used for joined update and `SELECT`
table sources. Single-table update aliases remain outside this feature's
syntax and runtime scope.

The AST keeps ordinary unhinted updates unchanged. When hints are present, the
target child is a `MYLITE_SQL_AST_FROM_TABLE` node containing the table name and
the index-hint list. The single-table update planner must extract the table
name from that wrapper before schema/table resolution and validate the hint list
after resolution.

## Name Resolution And Diagnostics

Target table resolution is unchanged from the current single-table `UPDATE`
implementation:

- unqualified names use the selected/default schema policy;
- schema-qualified names resolve against the named schema;
- reserved `_mylite_*` schema/table names are rejected by the existing target
  resolution layer before SQLite SQL generation;
- unknown schemas and unknown tables keep the current MySQL-compatible
  diagnostics;
- unsupported object kinds remain rejected once such descriptors exist.

Hint names resolve after the target table descriptor is known:

- matching is ASCII case-insensitive;
- exact matches are accepted;
- one matching prefix is accepted;
- zero or multiple matching prefixes fail with `1176 / 42000`;
- `PRIMARY` names the primary-key index descriptor;
- temporary-table index descriptors are used for temporary targets, and
  persistent catalog descriptors are used for persistent targets;
- duplicate names are accepted;
- `USE` plus `FORCE` on the same update target fails with `1221 / HY000`
  regardless of scope;
- `IGNORE` may appear with `USE` or `FORCE`.

## Physical SQLite Handling

Valid hints are planner no-ops. MyLite must continue to generate the same
physical update plan as for the equivalent unhinted statement:

- target, assignment, predicate, ordering, and limit resolution remain
  descriptor-driven;
- generated SQLite identifiers are quoted through existing helpers;
- assignment, predicate, and limit values are bound parameters, not interpolated
  literals;
- physical table names continue to use stable MyLite-owned names;
- hints must not alter row ordering or tie behavior beyond the current
  descriptor-driven `UPDATE ... ORDER BY ... LIMIT` implementation.

No SQLite `INDEXED BY`, `NOT INDEXED`, fork patch, or new extension API is used
in this phase.

## Result Behavior

For supported successful hinted updates:

- no row result set is returned;
- `column_count == 0`;
- `row_count == 0`;
- `affected_rows` is the same changed-row count as the equivalent unhinted
  update;
- `warning_count == 0`;
- `ROW_COUNT()` and `@@warning_count` observe the same state as MySQL 8.4.9 for
  the verified subset.

## Test Plan

Add MySQL-runtime-verified expectations and focused C tests for:

- `USE INDEX`, `USE KEY`, `FORCE INDEX`, `FORCE KEY`, `IGNORE INDEX`, and
  scoped hints on single-table `UPDATE`;
- `USE INDEX ()` accepted;
- `FORCE INDEX ()` and `IGNORE INDEX ()` rejected as syntax errors;
- duplicate names accepted;
- `PRIMARY` and unambiguous prefixes accepted;
- missing names and ambiguous prefixes rejected with `1176 / 42000`;
- `USE` plus `FORCE` rejected with `1221 / HY000`;
- changed-row affected count, warning count, no result rows, and row-state
  equivalence to unhinted updates;
- `ORDER BY ... LIMIT` update behavior with hints remaining no-op;
- schema-qualified and unqualified target resolution under hinted updates;
- temporary-table target validation if temporary index descriptors are present;
- close/reopen persistence for rows updated through a hinted statement;
- parser AST shape for hinted and unhinted update targets;
- existing parser, select-hint, update lifecycle, joined-update lifecycle, and
  full check workflow regressions.
