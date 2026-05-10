# Baseline ALTER TABLE ORDER BY

## Status

This feature specifies a narrow `ALTER TABLE ... ORDER BY` slice for MyLite's
current persistent base-table descriptors:

```sql
ALTER TABLE table_name ORDER BY column_name [ASC|DESC][, ...]
```

The statement rebuilds the descriptor-backed physical table in the requested
order. It does not add indexes, clustered keys, persistent ordering metadata,
or general ALTER TABLE multi-action support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing table lifecycle, row-values, DML, result, catalog, and ALTER TABLE
  specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_table_order_by_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `ALTER TABLE t ORDER BY id` succeeds for an InnoDB table, reports
  `ROW_COUNT()` equal to the row count copied by the operation, and leaves
  `@@warning_count == 0`.
- `ASC` is the default direction. `ASC` and `DESC` are both accepted.
- Multiple column names are accepted, each with its own optional direction.
- Backtick-quoted column names are accepted.
- Target-qualified order columns such as `t.id` and schema-target-qualified
  order columns such as `db.t.id` are accepted when they name the target table.
- Unknown order columns fail with error `1054`, SQLSTATE `42S22`, and an
  `Unknown column '<name>' in 'order clause'` diagnostic.
- Wrong table or schema qualifiers on the order key fail as unknown order
  columns.
- `ORDER BY 1`, expression keys, and trailing clauses such as `LIMIT` are
  syntax errors.
- An unqualified target without a selected default database fails with error
  `1046`, SQLSTATE `3D000`.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown target tables fail with error `1146`, SQLSTATE `42S02`.
- Empty tables succeed and report row count `0`.
- Temporary-table forms work in MySQL, but MyLite keeps temporary tables out of
  this baseline slice.

## Scope

The implementation must add:

- parser and AST support for one single-action `ALTER TABLE ... ORDER BY`
  statement;
- one or more descriptor column order keys;
- optional `ASC` and `DESC` direction per key, with omitted direction treated
  as ascending;
- unqualified, target-table-qualified, and target-schema-table-qualified order
  columns;
- unqualified and schema-qualified target table resolution through the existing
  selected/default schema policy;
- reserved `_mylite_*` target-name rejection before physical SQL generation;
- persistent base-table descriptor validation through the existing catalog
  ownership boundary;
- deterministic unknown column diagnostics for unknown order keys and wrong
  qualifiers;
- a SQLite-side physical table rebuild that copies all descriptor columns in
  `ORDER BY` order;
- affected-row reporting equal to the number of rows copied;
- `warning_count == 0` for successful statements;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- temporary tables;
- views or non-base-table object kinds;
- multi-action `ALTER TABLE`;
- mixing `ORDER BY` with `ADD`, `DROP`, `CHANGE`, `MODIFY`, `RENAME`,
  `ALTER COLUMN`, charset/collation options, `ENGINE`, `FORCE`, `ALGORITHM`,
  `LOCK`, or partition options;
- expression order keys, ordinal order keys, string-literal order keys,
  functions, collations, `NULLS FIRST` / `NULLS LAST`, or arbitrary
  expression evaluation;
- physical indexes, primary keys, clustered keys, optimizer hints, or
  persistent order metadata;
- `ORDER BY` table options for MyISAM or other storage engines;
- metadata locks, implicit commit behavior, binary logging, privileges, or
  storage-engine-specific online DDL algorithms;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns the statement
  boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  `ROW_COUNT()` state.
- Lexer/parser/AST own syntax admission for this single ALTER action and its
  order-key list. Parser code remains independent of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner code resolves the target table and all order keys against
  MyLite descriptors before any generated SQLite SQL is prepared.
- The catalog remains authoritative for schemas, tables, object kinds, physical
  names, and descriptor column order. This feature does not mutate catalog
  rows, descriptor versions, catalog generation, or descriptor caches.
- Runtime execution generates SQLite DDL/DML only from descriptors and stable
  physical table names. It does not consult SQLite schema text to discover
  logical columns.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.
- SQLite owns physical row storage and performs the ordered copy through a
  generated `INSERT INTO ... SELECT ... ORDER BY ...` statement. MyLite does
  not materialize the table rows in C memory.

## Supported SQL Grammar

The feature extends the existing limited `ALTER TABLE` grammar with one
single-action form:

```sql
ALTER TABLE table_name ORDER BY alter_table_order_item [, alter_table_order_item] ...

alter_table_order_item:
    qualified_identifier [ASC | DESC]
```

`table_name` may be unqualified or schema-qualified. `qualified_identifier`
may be an unqualified column name, the target table followed by a column name,
or the target schema and target table followed by a column name.

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= alter_table_order_by_statement(B). {
    A = B;
}

alter_table_order_by_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ORDER BY alter_table_order_item_list(O). {
    A = mylite_sql_parser_make_alter_table_order_by_statement(state, A1, T, O);
}

alter_table_order_item_list(A) ::= alter_table_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
alter_table_order_item_list(A) ::= alter_table_order_item_list(L) COMMA alter_table_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

alter_table_order_item(A) ::= qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, K, D);
}
```

The grammar intentionally excludes expression keys, ordinal keys, empty key
lists, `LIMIT`, and comma-separated ALTER actions.

## Resolution Semantics

Unqualified target table names require the currently selected schema.
Schema-qualified target table names use the explicit schema and do not require
a selected schema. Missing default schema, unknown explicit schema, and unknown
table diagnostics follow the existing table lifecycle policy.

Target schemas and tables with reserved `_mylite_*` names are rejected before
catalog lookup or physical SQL generation. Only persistent base-table
descriptors are supported.

Order keys are resolved against the target descriptor column list:

- unqualified `col` resolves to target column `col`;
- `table.col` resolves only when `table` matches the target table name;
- `schema.table.col` resolves only when both schema and table match the target;
- wrong qualifiers and unknown columns fail with unknown-order-column
  diagnostics.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog name policy. SQLite schema text is not consulted.

## Runtime Semantics

For a supported statement, MyLite rebuilds the physical table in a single
SQLite transaction:

1. Create a temporary physical rowid table with the same descriptor column
   names, integer physical type, and `NOT NULL` clauses as the current physical
   table.
2. Copy all descriptor columns using `INSERT INTO temp (...) SELECT ... FROM
   original ORDER BY key [ASC|DESC], ...`.
3. Drop the original physical table.
4. Rename the temporary physical table to the original stable physical name.

Generated SQL uses quoted SQLite identifiers for physical table names and
descriptor column names. The admitted grammar contains only identifiers and
directions, so no value parameters are required.

The copy remains inside SQLite. MyLite does not allocate row buffers for the
table contents or implement ordering in C. The operation changes physical row
layout, so `sqlite_schema_generation` is incremented after success. Catalog
generation and descriptor versions do not change.

Successful statements return the existing non-row statement result:

- zero result columns;
- zero result rows;
- `affected_rows` equal to the number of rows copied;
- `warning_count == 0`.

Current descriptor-backed `SELECT` without `ORDER BY` often observes SQLite
rowid scan order. This feature may therefore change MyLite's visible row order
for later unordered reads, matching the tested MySQL 8.4.9 behavior for the
baseline keyless table cases. For equal sort keys without later keys, MyLite
does not promise a deterministic tie order beyond the SQLite ordered-copy
result.

## Diagnostics

The implementation must preserve or add deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown explicit schema;
- unknown target table;
- reserved target schema or table names;
- unsupported object kind;
- unknown order column and wrong order qualifiers;
- unsupported order expressions or ordinal keys;
- physical SQLite failures during create, copy, drop, or rename;
- allocation failures;
- public API misuse if the public surface changes.

Successful supported statements produce no warnings.

## Storage, File Format, and SQLite Policy

This feature uses public SQLite SQL through MyLite's existing connection. It
does not require SQLite extension APIs or fork hooks. The `.mylite` preamble
and shifted SQLite payload invariants are preserved by the existing file-backed
VFS and SQLite transaction machinery.

Generated MyLite user tables must remain ordinary SQLite rowid tables. The
temporary rebuild table is created without `WITHOUT ROWID`; after rename, the
stable physical table name still identifies an ordinary rowid table.

## Tests

Tests must cover:

- parser acceptance for one-key, multi-key, `ASC`, `DESC`, quoted,
  table-qualified, and schema-table-qualified order keys;
- parser rejection for ordinal keys, expression keys, empty keys, `LIMIT`, and
  mixed ALTER actions;
- successful physical ordering with default direction, explicit `ASC`,
  `DESC`, multi-key ordering, nullable integer columns, duplicate key ties
  without overclaiming tie behavior, quoted columns, schema-qualified targets,
  and empty tables;
- affected row count, `ROW_COUNT()`, warning count, zero result columns, and
  zero result rows;
- unknown order columns and wrong qualifiers;
- missing default schema, unknown explicit schema, unknown table, and reserved
  target names;
- persistence after close/reopen;
- behavior after table rename and after drop;
- descriptor/catalog generation preservation and SQLite schema generation
  increment;
- `.mylite` preamble preservation;
- independent file-backed handles with independent ordered row state;
- unsupported temporary-table and wider ALTER syntax diagnostics;
- full existing parser/runtime/file-format/VFS/catalog lifecycle regressions.
