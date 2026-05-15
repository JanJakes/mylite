# Baseline Foreign Key Symbol Actions

## Summary

This phase extends MyLite's descriptor-owned foreign-key baseline with two
common MySQL DDL details:

```sql
CREATE TABLE child (
  parent_id INT,
  FOREIGN KEY parent_idx (parent_id) REFERENCES parent(id)
);

CREATE TABLE child (
  parent_id INT,
  CONSTRAINT fk_child_parent FOREIGN KEY (parent_id)
    REFERENCES parent(id) ON DELETE CASCADE ON UPDATE CASCADE
);
```

The implementation stays intentionally smaller than full InnoDB foreign-key
actions. It accepts the optional foreign-key index symbol, records direct
`CASCADE`, `RESTRICT`, and `NO ACTION` action rules in MyLite descriptors, and
implements direct set-based cascade writes for the currently supported
integer-family same-schema persistent base-table FK subset. It does not add
recursive cascades, `SET NULL`, `SET DEFAULT`, `MATCH`, disabled
`foreign_key_checks`, triggers, or SQLite-native FK authority.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Existing descriptor-owned FK implementation:
  `docs/specs/baseline-foreign-key-constraints/specs.md`
- Existing composite FK implementation:
  `docs/specs/baseline-composite-foreign-key-constraints/specs.md`
- Existing FK drop implementation:
  `docs/specs/baseline-drop-foreign-key-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_foreign_key_symbol_actions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `FOREIGN KEY index_name (child_column) REFERENCES parent(parent_column)` is
  accepted in `CREATE TABLE` and `ALTER TABLE ... ADD FOREIGN KEY`.
- When no explicit constraint name is present and no reusable child index
  already exists, the optional FK index symbol names the generated child index.
  The FK constraint name is still generated as `<child_table>_ibfk_N`.
- When an explicit `CONSTRAINT constraint_name` is present and no reusable child
  index exists, the generated child index uses the constraint name. The optional
  FK index symbol is not rendered by `SHOW CREATE TABLE`.
- When a reusable child index already exists, MySQL reuses it and renders the
  existing index name, regardless of the optional FK index symbol.
- `ON DELETE CASCADE` and `ON UPDATE CASCADE` are accepted together or
  independently. `SHOW CREATE TABLE` renders `ON DELETE` before `ON UPDATE`
  when both are non-default.
- `ON DELETE RESTRICT` and `ON UPDATE RESTRICT` are accepted and rendered.
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` reports `RESTRICT`.
- Explicit `ON DELETE NO ACTION` and `ON UPDATE NO ACTION` are accepted, but
  `SHOW CREATE TABLE` omits them. `REFERENTIAL_CONSTRAINTS` reports
  `NO ACTION`.
- Cascaded child writes do not contribute to `ROW_COUNT()` for the parent
  `DELETE` or `UPDATE`; the parent statement reports the parent affected row
  count and zero warnings for successful in-range operations.
- `ON DELETE CASCADE` removes direct child rows matching the deleted parent key
  values. Nullable child FK rows whose key tuple contains `NULL` are not
  matched.
- `ON UPDATE CASCADE` updates direct child FK columns from the old parent key
  tuple to the new parent key tuple. Nullable child FK rows whose key tuple
  contains `NULL` are not matched.
- Default, explicit `NO ACTION`, and `RESTRICT` continue to reject parent
  deletes or parent key updates with `1451 / 23000` when child rows exist.

## Scope

Supported DDL:

- persistent same-schema base tables only;
- existing table-level `CREATE TABLE` foreign keys and
  `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY` forms;
- additionally, unnamed `ALTER TABLE ... ADD FOREIGN KEY ...` only when the
  optional or generated constraint/index names can follow the same descriptor
  policy as `CREATE TABLE`;
- optional FK index symbol immediately after `FOREIGN KEY`;
- optional `ON DELETE` and `ON UPDATE` action clauses in either order;
- action values `CASCADE`, `RESTRICT`, and `NO ACTION`;
- one-or-more compatible integer-family FK parts already supported by the
  existing single-column and composite FK phases;
- direct child-table cascades for parent-side `DELETE` and supported
  single-table `UPDATE` operations that mutate referenced parent key columns;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `REFERENTIAL_CONSTRAINTS` metadata.

Deferred:

- `SET NULL` and `SET DEFAULT`;
- `MATCH` clauses;
- action clauses on unsupported FK shapes such as temporary tables,
  cross-schema references, non-integer references, prefix/expression parts, or
  unsupported object kinds;
- recursive cascades through child tables that are themselves parents in other
  FK descriptors;
- self-referential cascade cycles;
- trigger firing, cascade depth limits, metadata locks, privilege checks, and
  full InnoDB dependency behavior;
- mutable `foreign_key_checks = 0`;
- native SQLite FK enforcement as the user-visible authority.

## Ownership Boundaries

- Public API: no ABI change. Applications continue through `mylite_execute()`
  and existing result/diagnostic accessors.
- Statement context: owns diagnostics, warning counts, affected rows,
  `ROW_COUNT()`, statement transactions, rollback on failure, and cascade
  failure cleanup.
- Parser/AST: admits optional FK index symbols and action clauses, preserving
  child columns, parent table, parent columns, optional constraint name,
  optional child-index symbol, and action rule nodes. It does not inspect
  descriptors or SQLite schema text.
- Analyzer/planner: resolves names from MyLite descriptors, decides generated
  constraint/index names, rejects unsupported action combinations, records
  action rules, and decides whether a parent write can cascade set-based.
- Catalog: existing `_mylite_catalog_foreign_keys.update_rule`,
  `delete_rule`, and `match_option` fields are authoritative. No schema
  migration is required.
- Result builders: render `SHOW CREATE TABLE` and information-schema rows from
  MyLite FK descriptors only.
- SQLite physical storage: stores user rows and ordinary generated child
  indexes. MyLite builds parameterized SQLite statements for cascade mapping,
  child updates/deletes, and validation probes.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Syntax

MyLite Lemon-syntax sketch for the admitted grammar:

```lemon
foreign_key_definition ::=
    constraint_name_opt FOREIGN KEY foreign_key_index_name_opt
    LPAREN foreign_key_part_list RPAREN
    REFERENCES table_name LPAREN foreign_key_part_list RPAREN
    foreign_key_action_clause_list_opt.

foreign_key_index_name_opt ::= .
foreign_key_index_name_opt ::= identifier.

foreign_key_action_clause_list_opt ::= .
foreign_key_action_clause_list_opt ::= foreign_key_action_clause_list.

foreign_key_action_clause_list ::= foreign_key_action_clause.
foreign_key_action_clause_list ::= foreign_key_action_clause_list foreign_key_action_clause.

foreign_key_action_clause ::= ON DELETE foreign_key_reference_option.
foreign_key_action_clause ::= ON UPDATE foreign_key_reference_option.

foreign_key_reference_option ::= CASCADE.
foreign_key_reference_option ::= RESTRICT.
foreign_key_reference_option ::= NO ACTION.

alter_table_add_foreign_key_statement ::=
    ALTER TABLE table_name ADD foreign_key_definition.
```

`ALTER TABLE ... ADD CONSTRAINT name FOREIGN KEY ...` remains admitted because
`foreign_key_definition` includes `constraint_name_opt`. Unnamed
`ALTER TABLE ... ADD FOREIGN KEY ...` follows the same parser shape and uses
the same generated-name policy as create-time unnamed FKs.

The parser may also accept action-clause syntax that the analyzer rejects for
unsupported object kinds or unsafe cascade shapes. Unsupported action values
such as `SET NULL` and `SET DEFAULT` are syntax errors until their behavior is
specified.

## Descriptor Rules

Each planned FK stores:

- `name`: explicit constraint name or generated `<child_table>_ibfk_N`;
- optional child index symbol from `FOREIGN KEY index_name (...)`;
- `update_rule`: `CASCADE`, `RESTRICT`, or `NO ACTION`;
- `delete_rule`: `CASCADE`, `RESTRICT`, or `NO ACTION`;
- `match_option`: still `NONE`.

Child-index naming:

- If a matching child index already exists, reuse it.
- If no matching child index exists and the FK has an explicit constraint name,
  create a child secondary index named after the constraint name.
- If no matching child index exists and the FK has no explicit constraint name
  but has an FK index symbol, create the child index with that symbol.
- If neither an explicit constraint name nor an FK index symbol exists, use the
  existing generated child-index default based on the first child column.

Duplicate generated or requested child index names use the existing
duplicate-key diagnostics. Duplicate FK constraint names continue to use the
existing duplicate-FK diagnostics.

## Direct Cascades

Direct cascades run inside the same MyLite statement transaction as the parent
write. They are MyLite wrapper SQL built on public SQLite prepared statements,
not SQLite native FK actions.

### Delete

Before deleting parent rows, MyLite applies each direct `ON DELETE CASCADE`
descriptor whose parent table is the target table:

1. Build a descriptor-owned subquery for the exact parent rows selected by the
   current `DELETE` plan, including admitted `WHERE`, `ORDER BY`, and `LIMIT`
   behavior.
2. Delete matching direct child rows with a single SQLite `DELETE ... WHERE
   EXISTS (...)` shape.
3. Continue with the parent delete.
4. Run the existing parent-side validation for remaining `RESTRICT` /
   `NO ACTION` descriptors and as a safety check for unsupported combinations.

The cascade child delete does not change the public affected-row count for the
parent statement.

### Update

Before updating parent rows, MyLite applies each direct `ON UPDATE CASCADE`
descriptor whose referenced parent key columns are changed by the supported
single-table `UPDATE` plan:

1. Build a statement-local temporary mapping table containing old parent key
   tuples and their new key tuples for the exact parent rows that will be
   changed, including admitted `WHERE`, `ORDER BY`, `LIMIT`, and changed-row
   filtering.
2. Execute the parent update.
3. Update direct child rows from matching old key tuples to the corresponding
   new key tuple with descriptor-built set-based SQLite `UPDATE` statements.
4. Run existing child-side and parent-side validation as a safety check.

If a parent `UPDATE` does not mutate any referenced key column for a given FK,
no cascade is required for that FK and the current validation path remains
sufficient.

This phase is limited to cascade mappings that can be represented by the
existing update assignment subset without materializing parent or child tables
in process memory. Unsupported recursive cascade shapes fail deterministically
before mutation.

## Rendering

`SHOW CREATE TABLE` renders FK actions from descriptor rules:

- omit `NO ACTION`;
- append `ON DELETE RESTRICT` or `ON DELETE CASCADE` when `delete_rule` is not
  `NO ACTION`;
- append `ON UPDATE RESTRICT` or `ON UPDATE CASCADE` when `update_rule` is not
  `NO ACTION`;
- render `ON DELETE` before `ON UPDATE`.

`INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` already exposes `UPDATE_RULE` and
`DELETE_RULE`; this phase changes the stored descriptor values. `SHOW INDEX`
and `INFORMATION_SCHEMA.STATISTICS` continue to render the reused or generated
child index descriptor.

## Diagnostics

Supported diagnostics:

- syntax errors for unsupported action values (`SET NULL`, `SET DEFAULT`) and
  `MATCH` clauses until those features are specified;
- duplicate action timing such as two `ON DELETE` clauses returns a
  deterministic parse/analyze error before catalog mutation;
- missing default schema, unknown schema/table, duplicate table, duplicate
  index name, duplicate FK constraint name, unknown child/parent columns,
  incompatible descriptors, and missing parent unique keys reuse existing FK
  diagnostics;
- default, `NO ACTION`, and `RESTRICT` parent write violations return existing
  `1451 / 23000`;
- child-side missing parent rows after direct cascades return existing
  `1452 / 23000` if validation finds an inconsistency;
- recursive/self-referential cascade shapes not yet supported return
  deterministic unsupported diagnostics before mutation;
- allocation and physical SQLite failures use existing runtime conventions.

## Performance

Cascades must stay set-based. Direct child deletion/update uses SQLite
`EXISTS` and statement-local mapping tables over physical MyLite-owned tables.
MyLite must not materialize parent or child tables into process memory to
decide cascade side effects. Existing generated child indexes remain ordinary
SQLite indexes and should support child lookups where SQLite's planner can use
them.

No SQLite fork patch is required for this phase. If later recursive cascades,
deeper optimizer integration, or lower-overhead statement hooks need an
extension point, that must be specified separately and kept as a narrow SQLite
fork hook.

## Tests

Add MySQL-runtime-verified expectation coverage and fast C runtime/parser tests
for:

- `CREATE TABLE ... FOREIGN KEY index_name (...) REFERENCES ...`;
- explicit `CONSTRAINT name FOREIGN KEY index_name (...)` child-index naming;
- existing child index reuse when an FK index symbol is present;
- `ALTER TABLE ... ADD FOREIGN KEY index_name (...) REFERENCES ...`;
- `ALTER TABLE ... ADD CONSTRAINT name FOREIGN KEY ... ON DELETE/UPDATE ...`;
- `ON DELETE CASCADE`, `ON UPDATE CASCADE`, both together, both orderings, and
  metadata/rendering;
- `ON DELETE RESTRICT`, `ON UPDATE RESTRICT`, explicit `NO ACTION`, and
  default behavior;
- direct delete cascade over one-column and composite integer FKs;
- direct update cascade over one-column integer FKs and a representative
  composite integer FK where supported update assignment shapes allow it;
- nullable child rows, parent row counts, warning counts, result shape, and
  persistence after reopen;
- unsupported `SET NULL`, `SET DEFAULT`, `MATCH`, duplicate action timings, and
  recursive/self-referential cascade diagnostics;
- existing FK, composite FK, drop FK, update, delete, index, table lifecycle,
  file-format, VFS, parser, and full workflow tests still passing.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-indexes-constraints.md`,
`docs/compatibility/sql-table-ddl.md`, `docs/compatibility/sql-table-dml.md`,
and `docs/compatibility/sql-show-statements.md` for the exact subset.

Do not claim full InnoDB cascades, recursive cascades, `SET NULL`,
`SET DEFAULT`, `MATCH`, mutable `foreign_key_checks`, triggers, metadata locks,
or privilege semantics.
