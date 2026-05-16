# Baseline FULLTEXT Index Metadata Specification

## Summary

This phase adds a metadata-only `FULLTEXT` index slice for persistent MyLite
base tables. The goal is to accept common schema-import DDL that declares
`FULLTEXT` indexes inside `CREATE TABLE`, preserve the descriptor through
table metadata operations, and expose MySQL-shaped metadata through
`SHOW CREATE TABLE`, `SHOW INDEX`, `SHOW COLUMNS`, and the limited
`INFORMATION_SCHEMA.STATISTICS` / `COLUMNS` surfaces.

This is not full-text search. The slice does not add `MATCH ... AGAINST`,
tokenization, stopword tables, parser plugins, optimizer behavior, relevance
ranking, or SQLite FTS tables.

## Compatibility Sources

Normative behavior comes from official MySQL 8.4 documentation and observed
MySQL 8.4.9 runtime probes:

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>

The official documentation defines `FULLTEXT` as an index class for full-text
search that may include only `CHAR`, `VARCHAR`, and `TEXT` family columns. It
also documents that prefix lengths are ignored for `FULLTEXT`, and that
`SHOW INDEX` / `INFORMATION_SCHEMA.STATISTICS` report an index method such as
`FULLTEXT` and `NULL` collation for unsorted indexes.

Runtime expectations are recorded in
`packages/libmylite/tests/mysql_baseline_fulltext_index_metadata_expectations.sh`
and must pass against MySQL 8.4.9. Observed behavior used by this design:

- `FULLTEXT KEY name (title, body)`, `FULLTEXT INDEX name (...)`,
  `FULLTEXT name (...)`, and unnamed `FULLTEXT (...)` are accepted inside
  `CREATE TABLE`.
- unnamed indexes use the first key-part column name with `_2`, `_3`, ...
  suffixes on collisions.
- `FULLTEXT KEY ft_body (body(10))` is accepted, the prefix is ignored, and
  no warning is produced.
- `SHOW CREATE TABLE` renders `FULLTEXT KEY` and omits ignored prefixes.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report
  `Index_type` / `INDEX_TYPE = FULLTEXT`, `Collation` / `COLLATION = NULL`,
  and `Sub_part` / `SUB_PART = NULL`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` report `MUL` for
  columns that are the first part of a `FULLTEXT` index.
- non-character/binary columns in `FULLTEXT` fail with `1283 / HY000` and
  `Column 'name' cannot be part of FULLTEXT index`.
- zero-length `CHAR(0)` and `VARCHAR(0)` full-text key parts fail with
  `1167 / 42000` and `The used storage engine can't index column 'name'`.
- explicit key-part `ASC` or `DESC` fails with `1221 / HY000` and
  `Incorrect usage of spatial/fulltext/hash index and explicit index order`.
- prefix length zero fails with existing `1391 / HY000`.
- `CONSTRAINT name FULLTEXT ...` is not valid syntax in MySQL 8.4.9.
- `CREATE TEMPORARY TABLE ... FULLTEXT ...` fails with `1796 / HY000` and
  `Cannot create FULLTEXT index on temporary InnoDB table`.

## Ownership Boundaries

- Public API: no new ABI. Applications continue to use `mylite_open()`,
  `mylite_execute()`, and existing result accessors.
- Statement context: no new session state. Existing selected/default schema,
  transaction, diagnostics, warning-count, and reserved-name policy applies.
- Parser/AST: admits a narrow table-level `FULLTEXT` index definition and
  represents it distinctly from ordinary secondary and unique indexes.
- Analyzer/planner/runtime: resolves index names and key parts against MyLite
  descriptors before any physical SQL is considered.
- Catalog: MyLite descriptors remain authoritative. `FULLTEXT` is a logical
  index kind stored in `_mylite_catalog_indexes`; index-column rows store the
  ordered descriptor columns but no prefix, direction, parser, visibility, or
  expression metadata.
- Result builder: successful DDL uses existing non-row result conventions with
  zero affected rows and zero warnings for admitted in-create-table forms.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical row storage: no SQLite index, FTS table, trigger, or fork
  patch is created for this slice. Rows remain stored in the existing generated
  MyLite user table.

## Supported Syntax

For this phase, MyLite admits only table-level `FULLTEXT` definitions in
explicit `CREATE TABLE`:

```sql
CREATE TABLE table_name (
  column_definition,
  ...,
  FULLTEXT [KEY | INDEX] [index_name] (fulltext_key_part[, ...])
) [supported_table_options]

fulltext_key_part:
  column_name
  column_name(length)
```

Details:

- persistent base tables only;
- unqualified and schema-qualified table names use existing `CREATE TABLE`
  schema resolution;
- `FULLTEXT`, `FULLTEXT KEY`, and `FULLTEXT INDEX` are accepted;
- an index name may appear with or without `KEY` / `INDEX`;
- omitted names are generated from the first key-part column with suffixes as
  needed;
- key parts must be unqualified descriptor column names;
- key-part columns may be `CHAR(1..255)`, `VARCHAR(1..16383)`, `TINYTEXT`,
  `TEXT`, `MEDIUMTEXT`, or `LONGTEXT`;
- optional positive decimal prefix lengths are parsed for compatibility but
  ignored in stored metadata and rendering;
- successful admitted forms report zero warnings.

MyLite Lemon-syntax sketch:

```lemon
create_table_item ::= fulltext_index_definition.

fulltext_index_definition ::=
    FULLTEXT fulltext_index_keyword_opt index_name_opt
    LPAREN secondary_index_part_list RPAREN.

fulltext_index_keyword_opt ::= .
fulltext_index_keyword_opt ::= KEY.
fulltext_index_keyword_opt ::= INDEX.

secondary_index_part ::= identifier.
secondary_index_part ::= identifier LPAREN INTEGER RPAREN.
```

Unsupported forms:

- full-text indexes on temporary tables;
- standalone `CREATE FULLTEXT INDEX`;
- `ALTER TABLE ... ADD FULLTEXT`;
- parser options such as `WITH PARSER`;
- `USING`, comments, visibility, engine attributes, algorithms, locks, and
  partition clauses;
- `SPATIAL`, hash, multi-valued, or functional indexes;
- expression key parts, table-qualified key parts, ordinal key parts, and
  explicit `ASC` / `DESC`;
- `CONSTRAINT name FULLTEXT ...`;
- `MATCH ... AGAINST` and full-text query semantics.

Standalone and alter-added `FULLTEXT` are deferred intentionally. MySQL emits a
storage-engine warning when adding the first InnoDB full-text index to an
existing table, and MyLite should design that user-visible warning behavior
with the broader add-index path rather than silently accepting it here.

## Name Resolution and Diagnostics

The target table uses existing `CREATE TABLE` resolution:

- unqualified names require a selected/default schema;
- schema-qualified names use the explicit schema;
- unknown schemas and reserved `_mylite_*` schema/table names fail before any
  physical SQL is generated;
- duplicate table names use existing diagnostics.

Index and column names are resolved inside the planned table descriptor:

- explicit index names must be valid identifiers, unique within the table, and
  not `PRIMARY` case-insensitively;
- omitted names use the first key-part column name with `_2`, `_3`, ...
  suffixes when needed;
- key-part column names resolve case-insensitively against planned descriptor
  columns according to the existing catalog identifier policy;
- unknown key columns fail with the existing key-column diagnostic;
- duplicate columns inside a single `FULLTEXT` index fail with the existing
  duplicate-column diagnostic.

Diagnostics for the supported parse surface:

| Case | Diagnostic |
| --- | --- |
| non-character or binary key part | `1283 / HY000`, `Column 'name' cannot be part of FULLTEXT index` |
| zero-length `CHAR` / `VARCHAR` key part | `1167 / 42000`, `The used storage engine can't index column 'name'` |
| explicit `ASC` or `DESC` | `1221 / HY000`, `Incorrect usage of spatial/fulltext/hash index and explicit index order` |
| prefix length zero | existing `1391 / HY000` |
| unknown key column | existing `1072 / 42000` |
| duplicate index name | existing `1061 / 42000` |
| index name `PRIMARY` | existing `1280 / 42000` |
| temporary table `FULLTEXT` | `1796 / HY000`, `Cannot create FULLTEXT index on temporary InnoDB table` |
| unsupported grammar | syntax error or existing deterministic unsupported diagnostic |
| allocation or catalog failure | existing MyLite allocation/internal diagnostics |

## Descriptor and Catalog Model

Add `MYLITE_CATALOG_INDEX_KIND_FULLTEXT` to the internal catalog index kind
enum and migrate `_mylite_catalog_indexes.kind` to admit the new value. A
`FULLTEXT` descriptor stores:

- logical index name;
- table id;
- generated physical-name field for catalog shape stability, though no SQLite
  object is created for this kind;
- `kind = FULLTEXT`;
- `is_unique = false`;
- one index-column row per key part, with ordinal position and column id;
- no prefix length, because MySQL ignores `FULLTEXT` prefixes;
- ascending sort direction as an internal placeholder, never exposed for
  `FULLTEXT`.

`CREATE TABLE ... LIKE` clones full-text descriptors and index-column rows with
target table and column ids. `CREATE TABLE ... SELECT` continues to omit
indexes. `RENAME TABLE` preserves descriptor ids and logical index metadata.
`ALTER TABLE ... RENAME INDEX` and `ALTER TABLE ... DROP INDEX|KEY`, plus
standalone `DROP INDEX name ON table_name`, operate on existing `FULLTEXT`
descriptors because MySQL permits ordinary index rename/drop for full-text
indexes.

Dropping a full-text descriptor performs catalog mutation only; it must not run
SQLite `DROP INDEX` or bump `sqlite_schema_generation` because this slice did
not create a physical SQLite index. Renaming remains catalog-only, matching the
existing rename-index architecture.

## Physical SQLite Handling

No generated physical `CREATE INDEX` is emitted for `FULLTEXT`. This is a
deliberate architecture boundary:

- a SQLite b-tree index would not implement MySQL full-text tokenization,
  parser plugins, stopwords, ranking, or `MATCH ... AGAINST`;
- SQLite FTS virtual tables would require a separate design for data
  synchronization, MySQL parser behavior, and file-format implications;
- a targeted SQLite fork patch is unnecessary for descriptor metadata.

The create-table physical SQL remains the ordinary generated row table. All
identifier quoting and physical table naming rules stay unchanged for the table
itself.

## Introspection

### `SHOW CREATE TABLE`

Render full-text descriptors after primary and unique descriptors and after
ordinary nonunique secondary descriptors in index descriptor order, matching
observed MySQL grouping for the supported subset:

```sql
FULLTEXT KEY `index_name` (`column_name`,`other_column`)
```

Ignored prefixes and directions are not rendered.

### `SHOW INDEX`

Render one row per full-text key part:

- `Non_unique`: `1`;
- `Key_name`: logical index name;
- `Seq_in_index`: descriptor ordinal position;
- `Column_name`: logical column name;
- `Collation`: `NULL`;
- `Cardinality`: `0` baseline placeholder;
- `Sub_part`: `NULL`;
- `Packed`: `NULL`;
- `Null`: `YES` for nullable columns, otherwise empty string;
- `Index_type`: `FULLTEXT`;
- `Comment` and `Index_comment`: empty string;
- `Visible`: `YES`;
- `Expression`: `NULL`.

### `INFORMATION_SCHEMA.STATISTICS`

Use the same descriptor rows as `SHOW INDEX`. `COLLATION` and `SUB_PART` are
`NULL`; `INDEX_TYPE` is `FULLTEXT`; `NON_UNIQUE` is `1`.

### Column Key Metadata

`SHOW COLUMNS.Key` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` report `MUL`
for columns that are the first part of a `FULLTEXT` index unless a primary or
unique key classification already takes precedence. Later key parts do not
receive `MUL` from that same full-text index.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and `KEY_COLUMN_USAGE` do not expose
full-text indexes as constraints.

## Performance and Durability

This slice adds catalog metadata and introspection only. It does not copy row
data, materialize key values, or add per-row write work for DML. DML continues
to execute through existing descriptor-built SQLite statements. Reopen,
independent file-backed handles, and the `.mylite` preamble remain unaffected.

## Tests

Tests must cover:

- MySQL 8.4.9 expectation script for accepted `CREATE TABLE FULLTEXT` forms,
  generated names, ignored prefixes, metadata, drop/rename, clone behavior, and
  representative diagnostics, including temporary-table rejection;
- parser acceptance for `FULLTEXT`, `FULLTEXT KEY`, `FULLTEXT INDEX`, named
  and unnamed table-level forms;
- parser rejection for unsupported standalone/alter add full-text forms until
  separately implemented;
- runtime C coverage for create, show metadata, information schema metadata,
  `SHOW COLUMNS` key metadata, clone, rename/drop, reopen persistence, and no
  physical SQLite index for full-text descriptors;
- diagnostics for unsupported key-part types, explicit direction, zero prefix,
  unknown columns, duplicate names, temporary tables, and reserved names;
- focused regression coverage for existing index, parser, catalog, file-format,
  and runtime lifecycle tests.

## Verification

Before marking the feature done:

1. Run `packages/libmylite/tests/mysql_baseline_fulltext_index_metadata_expectations.sh`.
2. Run the focused parser and runtime full-text metadata CTest entries.
3. Run related index lifecycle CTest entries.
4. Run `cmake --workflow --preset check`.
5. Review the diff for descriptor authority, physical SQLite separation,
   MySQL 8.4.9 evidence, file-format safety, zero-init cleanup, and compatibility
   matrix accuracy.
