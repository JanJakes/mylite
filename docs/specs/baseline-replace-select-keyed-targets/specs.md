# Baseline Replace Select Keyed Targets

## Status

This feature extends the existing descriptor-driven `REPLACE ... SELECT` path
from insert-equivalent no-key targets to key-bearing persistent base tables.
The SQL syntax is already admitted by the parser. This slice changes planning
and execution so a table-backed `REPLACE ... SELECT` target can use the same
primary-key, unique-index, prefix-key, auto-increment, and foreign-key
replacement machinery already used by `REPLACE ... VALUES` and `REPLACE ...
SET`.

This is not full MySQL `REPLACE ... SELECT`. This slice keeps the source as
the current single-table descriptor-backed `SELECT` subset. Row-scalar source
behavior is specified separately in
`docs/specs/baseline-replace-select-row-scalar-source/specs.md`; compound,
joined, `TABLE`, partitioned, broader expression, and generated-column target
forms remain deferred.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `REPLACE ... SELECT`:
  `docs/specs/baseline-replace-select-lifecycle/specs.md`
- Baseline key-bearing `REPLACE ... VALUES` / `SET`:
  `docs/specs/baseline-replace-key-lifecycle/specs.md`
- Baseline insert-select keyed targets:
  `docs/specs/baseline-insert-select-keyed-targets/specs.md`
- Current primary-key, unique-index, prefix-index, foreign-key, and
  auto-increment specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Additional probes for keyed targets
show:

- `REPLACE INTO pk SELECT ...` with one primary-key conflict and one inserted
  row reports `ROW_COUNT() == 3`, leaves `@@warning_count == 0`, and stores the
  replacement row plus the new row.
- A same-table exact replacement such as `REPLACE INTO pk SELECT id, v FROM pk`
  reports one affected row for each selected row and leaves stored rows
  unchanged.
- A single-column unique-key conflict behaves like the primary-key case.
- If one selected row conflicts with two different existing rows through two
  unique indexes, both old rows are deleted, one new row is inserted, and
  `ROW_COUNT() == 3`.
- Composite unique keys participate in replacement.
- Unique keys containing `NULL` parts do not conflict; repeated `NULL` unique
  values insert independent rows.
- Prefix unique keys participate in replacement using the indexed prefix.
- Generated `AUTO_INCREMENT` values in the selected row stream advance the
  target counter and set `LAST_INSERT_ID()` when a successful replacement row
  used a generated value; explicit values advance the target counter when they
  exceed the old next value.
- A replacement that would delete a referenced parent row fails with MySQL's
  parent-row foreign-key diagnostic. A replacement that inserts a child row
  without a matching parent fails with MySQL's child-row diagnostic.

Official MySQL documentation describes `REPLACE` as an insert attempt followed
by delete-and-retry while duplicate primary or unique keys are found, with
affected rows equal to inserted plus deleted rows. Documentation also notes that
`REPLACE ... SELECT` depends on source-row order when the source order is not
fully specified. MyLite claims only the visible row and affected-row behavior
covered by the runtime probes.

## Scope

The implementation must add:

- key-bearing target support for the existing
  `REPLACE [LOW_PRIORITY | DELAYED] [INTO] target [(columns)] SELECT ...`
  parser surface;
- persistent base-table targets with current primary-key and unique-index
  descriptors, including composite keys and admitted string/binary prefix
  unique keys;
- current indexed `AUTO_INCREMENT` target behavior for selected `NULL` and
  explicit selected values;
- current child-side and parent-side foreign-key checks for replacement rows;
- repeated conflict deletion when one selected row conflicts with different
  existing rows through different unique descriptors;
- MySQL-compatible affected-row accounting for inserted rows, changed
  replacements, exact replacements, and multi-conflict replacements;
- same-table materialization behavior through the existing internal temporary
  storage path, so selected rows are fixed before replacement deletes/inserts
  mutate the target;
- statement atomicity: any conversion, key, foreign-key, SQLite, or allocation
  failure rolls back the whole `REPLACE ... SELECT`;
- unchanged catalog descriptor rows, descriptor versions, catalog generation,
  and `sqlite_schema_generation`, except legitimate auto-increment counter and
  table updated-time side effects already used by current DML paths.

## Non-Goals

This feature must not implement:

- row-scalar or `DUAL` sources for this keyed table-backed slice; they are
  specified separately in
  `docs/specs/baseline-replace-select-row-scalar-source/specs.md`;
- compound `UNION`, joined, CTE, subquery, or `TABLE` sources for
  `REPLACE ... SELECT`;
- source expression projection, arbitrary functions, parameters, literals, or
  general expression conversion;
- generated-column target writes beyond the current rejected non-`DEFAULT`
  policy inherited from `INSERT ... SELECT`;
- partition selection, table aliases for the target, table-qualified target
  column names, `RETURNING`, triggers, cascades, privileges, binary-log safety
  warnings, or protocol-grade changed-row metadata;
- new index kinds, new constraint kinds, optimizer behavior, or SQLite fork
  patches.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` still returns a non-row result for
  successful replacement, with affected rows and warnings exposed through the
  existing result and diagnostics APIs.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, transaction completion, and rollback on failure.
- Parser/AST: already owns the admitted `REPLACE ... SELECT` syntax and source
  spans. This slice must not broaden grammar.
- Analyzer/planner: resolves target/source schemas, target/source tables,
  target columns, selected descriptor columns, target defaults, auto-increment
  state, foreign keys, and key descriptors through MyLite catalog descriptors.
  SQLite metadata is not authority.
- Runtime: materializes the selected source stream into the existing internal
  SQLite temporary table, validates each materialized row against target
  descriptors, turns each row into a one-row replacement plan, and reuses the
  existing descriptor-driven delete-and-retry replacement executor.
- Catalog: owns descriptors and auto-increment counters. Replacement must not
  mutate descriptor catalog rows; only the target auto-increment counter and
  table updated timestamp may change when existing DML policy requires it.
- SQLite: owns physical scans, sorting, limiting, internal temporary storage,
  physical inserts/deletes, uniqueness checks, and rollback durability.
  Generated SQL uses stable MyLite physical table names, quoted identifiers,
  and bound parameters.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only inside the shifted SQLite payload.

## Supported Grammar

No new grammar is added. The existing MyLite subset remains:

```sql
REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, column_name] ...)]
    SELECT select_item_list
    FROM table_name [AS] alias
    [WHERE predicate]
    [ORDER BY order_key [ASC | DESC]]
    [LIMIT row_count]
```

### MyLite Lemon-Syntax Snippet

The intended grammar is the existing independently authored production:

```lemon
replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, M, T, C, S);
}
```

`INTO` remains optional through the same parser helper as the current
`REPLACE ... SELECT` lifecycle. This phase does not admit `TABLE`, partitions,
target aliases, target-qualified column names, row aliases, or new source
query-expression forms.

## Replacement Semantics

The selected source rows are materialized into MyLite's internal temporary
SQLite table before target mutation. Validation and insertion read that same
temporary table, so same-target reads and source `ORDER BY` / `LIMIT` observe
the selected row stream rather than rows changed by earlier replacements.

For each materialized selected row:

1. Convert selected and omitted values through the existing
   `INSERT ... SELECT` target descriptor conversion path.
2. For selected `NULL` on an auto-increment target column, generate a target
   value using the current auto-increment session state and target descriptor.
3. Attempt the descriptor-built physical insert.
4. If no unique constraint fails, count one affected row.
5. If a primary or unique key conflicts, map the physical constraint back to a
   MyLite key descriptor and delete the conflicting physical row using the
   descriptor key predicate.
6. If the deleted row was exactly equal to the planned replacement row across
   stored descriptor columns, do not add the deleted row to affected rows;
   otherwise add the deleted row count.
7. Validate parent-side foreign keys after each delete and before the retry.
8. Retry the insert until it succeeds or the conflict retry bound is reached.
   The bound is the number of loaded unique descriptors.

Rows whose unique-key tuple contains `NULL` in any key part do not conflict for
that key. Prefix-key conflict detection and deletion reuse the same descriptor
prefix expressions used by current unique-prefix replacement.

If a later selected row conflicts with a row inserted by an earlier selected
row in the same statement, MyLite follows MySQL's row-stream behavior for the
verified ordered cases: the later row may replace the earlier row, and affected
rows reflect the delete plus insert or exact replacement. For ties or no
`ORDER BY`, the source row order is not overclaimed beyond the already
materialized SQLite result order.

## Physical SQLite Handling

MyLite keeps using standard prepared SQLite statements:

```sql
CREATE TEMP TABLE "_mylite_tmp_insert_select_..." AS SELECT ...

INSERT INTO "_mylite_user_table_<table_id>" ("col1", "col2", ...)
VALUES (?1, ?2, ...)

DELETE FROM "_mylite_user_table_<table_id>"
WHERE <descriptor-key-part-1> = ?1 [AND ...]
```

All identifiers are quoted and all row/key/default values are bound. MyLite
does not use SQLite `INSERT OR REPLACE`, triggers, foreign-key enforcement, or
schema text as MySQL compatibility authority. This is a MyLite wrapper and
translation feature over public SQLite prepared statements; no SQLite fork hook
is required.

## Diagnostics

Existing `REPLACE ... SELECT` diagnostics remain unchanged for syntax errors,
missing default schema, unknown schema, unknown table, reserved `_mylite_*`
names, unsupported object kinds, duplicate target columns, unknown target or
source columns, column-count mismatches, unsupported source shapes, unsupported
projection expressions, conversion failures, range failures, `NULL` into
`NOT NULL`, required-column failures, generated-column target writes,
allocation failures, physical SQLite failures, and public API misuse.

This feature changes only the former key-bearing target rejections:

- `REPLACE ... SELECT into primary-key tables is not supported`;
- `REPLACE ... SELECT into unique-index tables is not supported`;
- `REPLACE ... SELECT into foreign-key child tables is not supported` where the
  current descriptor-backed foreign-key checks can validate the row.

Supported key-bearing failures use existing MySQL-compatible diagnostics:

- duplicate-key conflicts that cannot be resolved due to stale descriptors or
  unexpected physical state become deterministic MyLite runtime errors;
- parent-side foreign-key failures use `1451 / 23000`;
- child-side foreign-key failures use `1452 / 23000`;
- supported in-range successful replacements report `warning_count == 0`
  except for already-supported `DELAYED` deprecation warnings.

## Tests

Extend the existing `runtime_replace_select_lifecycle` C test and
`mysql_baseline_replace_select_lifecycle_expectations.sh` script to cover:

- primary-key replacement with one changed existing row plus one inserted row;
- exact same-table replacement affected rows;
- single-column unique replacement;
- multiple unique-key conflicts from one selected row;
- composite unique-key replacement;
- nullable unique-key `NULL` non-conflicts;
- prefix unique-key replacement;
- auto-increment selected `NULL` and explicit selected values, including
  affected rows, `LAST_INSERT_ID()`, and counter persistence;
- parent-side and child-side foreign-key diagnostics;
- source ordering and same-target materialization for verified deterministic
  ordered cases;
- close/reopen persistence, result shape, warning count, `ROW_COUNT()`, and
  preamble preservation through existing lifecycle coverage;
- existing unsupported source shapes still rejected deterministically.

Run at minimum:

1. `cmake --build --preset dev`
2. `ctest --preset dev -R 'libmylite\\.runtime\\.(replace_select_lifecycle|replace_key_lifecycle|insert_select_keyed_targets)' --output-on-failure`
3. `packages/libmylite/tests/mysql_baseline_replace_select_lifecycle_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-table-dml.md`

The wording must remain partial/limited. Do not claim `TABLE`, joins, compound
sources, partitions, target aliases, broad expression sources,
generated-column writes, triggers, cascades, optimizer behavior, privileges, or
full MySQL `REPLACE` compatibility.
