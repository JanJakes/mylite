# Baseline SHOW INDEX Empty Introspection

## Status

This feature specifies a narrow key-introspection slice for descriptor-backed
persistent base tables that currently have no index descriptors. It adds
`SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` parsing and result construction
on top of `mylite_execute()`, statement context, the MyLite parser scaffold,
file-backed `.mylite` opening, durable catalog descriptors, schema/table
lifecycle, table rename, row values, DML, and existing descriptor-driven
introspection.

The feature is intentionally not index support. It exposes MySQL's `SHOW INDEX`
result column shape and returns zero rows for supported MyLite base tables until
primary key, unique key, secondary index, hidden index, and expression index
descriptors exist.

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
- File-backed MyLite opening and VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline schema, table, row, write, and introspection slices:
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-rename-to/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`,
  `docs/specs/baseline-show-columns-introspection/specs.md`, and
  `docs/specs/baseline-show-like-filters/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW INDEX FROM t`, `SHOW INDEXES FROM t`, and `SHOW KEYS FROM t` are
  aliases.
- `FROM` and `IN` are accepted both before the table name and before an
  explicit trailing schema name.
- `SHOW INDEX FROM db.t` and `SHOW INDEX FROM t FROM db` are equivalent.
- When both the table token and trailing clause name a schema, as in
  `SHOW INDEX FROM db1.t FROM db2`, the trailing schema determines the schema
  and the rightmost table identifier determines the table name.
- A table created without keys returns a successful result set with the
  standard `SHOW INDEX` columns and zero rows.
- Successful no-index `SHOW INDEX` leaves `@@warning_count == 0` and makes
  `ROW_COUNT()` return `-1`.
- Unqualified `SHOW INDEX FROM t` fails with error `1046`, SQLSTATE `3D000`,
  when no default schema is selected.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown tables in a known schema fail with error `1146`, SQLSTATE `42S02`,
  and the message includes the resolved `schema.table` name.
- `SHOW INDEX FROM view_name` succeeds and returns no rows in MySQL. MyLite has
  no view descriptors yet; this slice supports persistent base tables only.
- `SHOW EXTENDED INDEX`, indexed-table rows, and `WHERE` filters are accepted
  by MySQL and remain outside this slice.

The standard result columns observed from MySQL 8.4.9 and documented in the
manual are:

```text
Table
Non_unique
Key_name
Seq_in_index
Column_name
Collation
Cardinality
Sub_part
Packed
Null
Index_type
Comment
Index_comment
Visible
Expression
```

## Scope

The implementation must add:

- parser and AST support for `SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS`;
- `FROM` and `IN` synonyms in the admitted table and schema clauses;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- optional explicit schema resolution for `SHOW INDEX FROM table FROM schema`
  and equivalent `IN` variants;
- descriptor-driven target validation against MyLite catalog table descriptors;
- the MySQL 8.4.9 `SHOW INDEX` 15-column result shape;
- zero result rows for current persistent base tables because no index
  descriptors are supported yet;
- deterministic diagnostics for unsupported syntax and unresolved names;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- primary key, unique key, secondary index, fulltext, spatial, functional,
  expression, prefix, descending, hidden, invisible, generated-column, or
  internal indexes;
- `CREATE INDEX`, `DROP INDEX`, `ALTER TABLE ADD/DROP/RENAME INDEX`, key DDL,
  key descriptor storage, or SQLite physical indexes;
- rows in `SHOW INDEX` for any index kind;
- `SHOW EXTENDED INDEX`;
- `SHOW INDEX ... WHERE`, which is specified separately by
  `baseline-show-index-where`;
- index cardinality/statistics, visibility, comments, packing, prefix length,
  collations, or expression metadata;
- temporary tables, views, privileges, optimizer metadata, `INFORMATION_SCHEMA`
  tables, `mysql` schema tables, arbitrary SQLite metadata reads, arbitrary
  SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW INDEX` is a result-set statement and therefore
  stores `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target schema/table and rejects unsupported
  object kinds using MyLite descriptors.
- The catalog module remains authoritative for schema/table descriptors. This
  slice reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite catalog descriptors.
  It does not query SQLite table metadata, `sqlite_schema`, pragma output, or
  SQLite index metadata.
- The result builder owns the 15 result columns and the empty row set.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} table_name
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} table_name {FROM | IN} identifier
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The second form is the explicit-schema form. The trailing identifier is the
schema name. If the first table name is qualified, the trailing schema still
determines the schema and the rightmost identifier in the first table name
determines the table name.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_index_statement.

show_index_statement ::=
    SHOW show_index_keyword show_table_name_keyword table_name.
show_index_statement ::=
    SHOW show_index_keyword show_table_name_keyword table_name
    show_schema_name_keyword identifier.

show_index_keyword ::= INDEX.
show_index_keyword ::= INDEXES.
show_index_keyword ::= KEYS.

show_table_name_keyword ::= FROM.
show_table_name_keyword ::= IN.
show_schema_name_keyword ::= FROM.
show_schema_name_keyword ::= IN.
```

`INDEXES` remains usable as an identifier where identifier grammar admits
nonreserved keywords. `INDEX` and `KEYS` are reserved keywords and must be
mapped explicitly when they appear in admitted `SHOW INDEX` syntax.

## Schema and Table Resolution

Unqualified table names resolve through the current selected/default schema.
If no schema is selected, MyLite reports `1046` / `3D000` (`No database
selected`).

Schema-qualified table names and explicit `SHOW ... FROM table FROM schema`
forms resolve the named schema directly. When both are present, the trailing
explicit schema wins and any qualifier on the table name is ignored for schema
resolution. If the schema does not exist, MyLite reports `1049` / `42000`
(`Unknown database`). If the table does not exist in a known schema, MyLite
reports `1146` / `42S02` (`Table 'schema.table' doesn't exist`).

Any schema or table name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices.

Only `MYLITE_CATALOG_TABLE_KIND_BASE` is supported in this phase. Once view or
temporary descriptors exist, this spec must be revisited. MySQL returns no rows
for views, while temporary-table behavior must be verified with the eventual
temporary-table descriptor model.

## Result Semantics

Successful `SHOW INDEX` appends the standard 15 result columns and zero rows.
The columns are:

| Ordinal | Column |
| --- | --- |
| 1 | `Table` |
| 2 | `Non_unique` |
| 3 | `Key_name` |
| 4 | `Seq_in_index` |
| 5 | `Column_name` |
| 6 | `Collation` |
| 7 | `Cardinality` |
| 8 | `Sub_part` |
| 9 | `Packed` |
| 10 | `Null` |
| 11 | `Index_type` |
| 12 | `Comment` |
| 13 | `Index_comment` |
| 14 | `Visible` |
| 15 | `Expression` |

No rows are returned because the catalog cannot yet represent keys or indexes.
This is MySQL-compatible for tables created by the current MyLite DDL subset,
which cannot define keys.

Successful statements:

- return a result set with `column_count == 15`;
- return `row_count == 0`;
- leave `affected_rows == 0` under existing MyLite result conventions;
- use `warning_count == 0`;
- make following `ROW_COUNT()` return `-1`.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- lexer/parser errors and unsupported grammar;
- missing default schema;
- unknown explicit schema;
- unknown table;
- reserved schema names;
- reserved table names;
- unsupported object kind;
- allocation failures;
- public API misuse through existing `mylite_execute()` behavior.

Unsupported `SHOW EXTENDED INDEX`, missing table names, missing `FROM`/`IN`,
and malformed schema clauses are syntax errors for this slice. `SHOW INDEX ...
WHERE` is covered by the later `baseline-show-index-where` slice. Supported
successful statements produce no warnings.

## Physical SQLite Handling

This feature generates no SQLite SQL and requires no SQLite fork patches. The
runtime reads only MyLite catalog descriptors and builds a MyLite result object.
SQLite physical tables and indexes are not inspected because SQLite schema text
and `PRAGMA` output are not MySQL metadata authority.

## Compatibility Status

This feature moves only the exact supported subset to partial support:

- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`: descriptor-resolved persistent
  base-table introspection with MySQL 8.4.9 columns and zero rows for current
  no-index tables.

Indexes themselves, index DDL, indexed rows, `SHOW EXTENDED INDEX`, `WHERE`
filters, temporary tables, views, privileges, statistics, and
`INFORMATION_SCHEMA.STATISTICS` remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.show_index_empty_introspection`.

Coverage must include:

- parser/AST acceptance for `SHOW INDEX`, `SHOW INDEXES`, `SHOW KEYS`, `FROM`,
  `IN`, schema-qualified targets, and trailing explicit schema forms;
- parser rejection for missing table names, missing `FROM`/`IN`, `EXTENDED`,
  `WHERE`, and malformed schema clauses;
- successful zero-row results with the exact 15 column labels;
- `ROW_COUNT()` returning `-1` after successful introspection;
- unqualified and schema-qualified target resolution;
- trailing explicit schema winning over a qualifier on the table name;
- missing default schema, unknown schema, unknown table, and reserved names;
- behavior after table rename, `ALTER TABLE ... RENAME`, drop, and reopen;
- unchanged `.mylite` preamble and no catalog/SQLite generation mutation;
- independent file-backed handles;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, schema/table lifecycle, rename, row values, DML, and
  introspection tests still pass.

## Build Integration

Add any new runtime/analyzer/planner sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.show_index_empty_introspection$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.(show_columns_introspection|show_like_filters|table_rename_lifecycle|alter_table_rename_to|basic_table_lifecycle|schema_lifecycle))$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_show_index_empty_introspection_expectations.sh
cmake --workflow --preset check
```

Then review the diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, result shape accuracy, file-format safety, VFS preservation,
zero-init safety, cleanup on failure, scope control, compatibility-matrix
accuracy, and test relevance.
