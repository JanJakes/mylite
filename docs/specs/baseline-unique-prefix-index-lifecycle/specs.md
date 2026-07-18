# Baseline Unique Prefix Index Lifecycle Specification

## Summary

This phase admits the next narrow schema/index slice for persistent MyLite base
tables:

```sql
CREATE TABLE t (v VARCHAR(20), UNIQUE KEY u_v (v(3)))
ALTER TABLE t ADD UNIQUE KEY u_v (v(3))
CREATE UNIQUE INDEX u_v ON t (v(3))
```

The slice reuses MyLite's descriptor-owned unique indexes, prefix key-part
descriptors, ASCII `utf8mb4_0900_ai_ci` collation callback, generated SQLite
expression indexes, and current duplicate-key DML handling. It is deliberately
single-part only. Composite unique indexes, primary prefix indexes, and binary
prefix indexes remain separate features; complete UCA 9.0 collation remains
outside this slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline `CHAR` / `VARCHAR` key lifecycle:
  `docs/specs/baseline-char-varchar-key-lifecycle/specs.md`
- Baseline index prefix key parts:
  `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Baseline `ALTER TABLE ... ADD UNIQUE` lifecycle:
  `docs/specs/baseline-alter-table-add-unique-lifecycle/specs.md`
- Baseline `CREATE INDEX` lifecycle:
  `docs/specs/baseline-create-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_unique_prefix_index_lifecycle_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `UNIQUE KEY u_v (v(3))` succeeds for `VARCHAR`, renders as
  ``UNIQUE KEY `u_v` (`v`(3))``, reports `SHOW INDEX.Non_unique = 0`, and
  reports `Sub_part` / `SUB_PART = 3`.
- `UNIQUE KEY u_body (body(4))` succeeds for `TEXT`; full-column `TEXT`
  unique keys still fail without a prefix.
- `ALTER TABLE ... ADD UNIQUE KEY ... (column(prefix))` and
  `CREATE UNIQUE INDEX ... ON ... (column(prefix))` succeed for the same
  one-part string prefix subset, report `ROW_COUNT() == 0`, and leave
  `@@warning_count == 0`.
- Prefix duplicate enforcement uses the indexed prefix, not the full value:
  inserting `abcxyz` into a table that already contains `abcdef` under
  `UNIQUE KEY u_v (v(3))` fails with `1062 / 23000` and duplicate entry text
  `abc`.
- `INSERT IGNORE` skips duplicate prefix rows, inserts nonconflicting rows and
  duplicate `NULL` rows, and reports warnings for skipped duplicates.
- `UPDATE` and `INSERT ... ON DUPLICATE KEY UPDATE` find conflicts using the
  prefix value. The deprecated `VALUES()` assignment form still produces the
  existing MySQL warning count in the tested ODKU shape.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII prefix comparison is
  case-insensitive. For example, `CHAR(5) UNIQUE KEY (c(3))` treats `ABC` and
  `abc` prefixes as duplicates; the duplicate entry text preserves the
  attempted prefix spelling observed from MySQL.
- Prefix unique indexes permit multiple `NULL` values.
- Prefix lengths on non-string columns fail with `1089 / HY000`, prefix length
  `0` fails with `1391 / HY000`, and bounded `VARCHAR` prefixes greater than
  the column length fail with `1089 / HY000` even with `sql_mode = ''`.
- Prefix byte limits still apply. With the default `utf8mb4` baseline,
  `VARCHAR(1000) UNIQUE KEY (v(768))` fits the 3072-byte InnoDB key envelope,
  while prefix `769` fails with `1071 / 42000`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` report `UNI`, not
  `PRI`, for a `NOT NULL` column whose only unique index is a prefix unique
  index, because prefix uniqueness does not prove full-column uniqueness.
- MySQL accepts composite unique prefix indexes such as
  `UNIQUE KEY u_ab (a(3), b(2))`; MyLite defers composite unique indexes in
  this phase.

Official MySQL documentation defines prefix lengths for nonbinary string index
parts as character counts, states that unique prefix indexes require uniqueness
within the prefix, and exposes prefix lengths through `SHOW INDEX.Sub_part` and
`INFORMATION_SCHEMA.STATISTICS.SUB_PART`.

## Scope

Supported:

- persistent MyLite base tables only;
- create-time table-level
  `UNIQUE [KEY|INDEX] [index_name] (column_name(prefix_length))`;
- single-action
  `ALTER TABLE table_name ADD UNIQUE [KEY|INDEX] [index_name]
  (column_name(prefix_length))`;
- standalone
  `CREATE UNIQUE INDEX index_name ON table_name
  (column_name(prefix_length))`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- optional generated names for create-time and alter-time unique definitions
  using the existing index-name policy;
- one unqualified key part only;
- positive decimal integer prefix lengths without sign;
- prefix parts on `CHAR`, `VARCHAR`, and baseline `TEXT` family descriptors;
- valid UTF-8 non-`NULL` string key values using MyLite's shared limited
  `utf8mb4_0900_ai_ci` service;
- duplicate `NULL` values;
- prefix duplicate enforcement for current `INSERT ... VALUES`,
  `INSERT ... SET`, `INSERT IGNORE`, single-assignment `UPDATE`, and the
  existing one-assignment `INSERT ... ON DUPLICATE KEY UPDATE` subset;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, `ALTER TABLE ... DROP INDEX|KEY`,
  standalone `DROP INDEX`, limited `INFORMATION_SCHEMA.COLUMNS`,
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- reopen persistence, independent file-backed handles, `.mylite` preamble
  preservation, and no public ABI change.

Deferred:

- primary prefix indexes;
- full-column `TEXT` unique indexes;
- composite unique indexes, including composite unique prefix indexes;
- complete UCA 9.0 collation weights and locale tailoring;
- binary string and BLOB prefix unique indexes;
- descending, functional, expression, table-qualified, ordinal, invisible,
  optioned, commented, parser, fulltext, spatial, algorithm, lock, partition,
  foreign-key, trigger, cascade, privilege, optimizer, and redundant-index
  warning behavior;
- `REPLACE` into unique-index tables, which remains controlled by the existing
  REPLACE/unique-index scope.

## Ownership Boundaries

- Public API: unchanged. Applications continue to use `mylite_execute()` and
  existing result and diagnostic accessors.
- Statement context: owns diagnostic reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup. Supported successful DDL returns through existing
  no-row result conventions.
- Lexer/parser/AST: existing grammar already preserves unique index parts as
  descriptor-column identifiers plus optional integer prefix-length child
  nodes. Parser code does not inspect descriptors or SQLite schema.
- Analyzer/planner/runtime: resolves table, index name, key column, prefix
  admissibility, and duplicate diagnostics from MyLite descriptors before
  generating physical SQL.
- Catalog: index and index-column descriptors remain authoritative. A positive
  index-column `prefix_length` marks a prefix key part. SQLite schema text is a
  physical artifact only.
- Result/introspection builders: render `UNIQUE KEY` and prefix metadata from
  descriptors, and classify prefix unique columns as `UNI`.
- SQLite registration: MyLite continues to use the first-party
  `utf8mb4_0900_ai_ci` ASCII collation callback registered through public
  SQLite APIs.
- SQLite physical storage: generated physical indexes use public SQLite
  expression-index support with `substr(column, 1, prefix_length) COLLATE ...`.
  No SQLite fork patch is required.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Supported Grammar

Existing MyLite grammar already admits the syntax for this slice:

```lemon
unique_index_definition ::=
    UNIQUE unique_index_keyword_opt index_name_opt
    LPAREN secondary_index_part_list RPAREN.

create_index_statement ::=
    CREATE UNIQUE INDEX identifier ON table_name
    LPAREN secondary_index_part_list RPAREN.

secondary_index_part ::= identifier LPAREN integer_literal RPAREN.
```

Runtime planning keeps the existing "exactly one key part" diagnostic for
unique indexes. Prefix length literals are positive decimal integer literals;
parameters, expressions, signs, strings, floats, hex, bit literals, qualified
columns, directions, and functional key parts are outside this slice.

## Validation and Semantics

Target table and index name resolution reuse the existing unique and prefix
index policies:

- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` logical schema/table names are rejected before SQLite
  SQL generation;
- duplicate index names fail with `1061 / 42000`;
- quoted `PRIMARY` index names fail with `1280 / 42000`;
- omitted names derive from the key column and use `_2`, `_3`, ... suffixes.

Key-part validation:

- unknown columns fail with `1072 / 42000`;
- unique indexes still accept exactly one key part;
- the key part must be an unqualified descriptor column;
- prefix length `0` fails with `1391 / HY000`;
- prefix length on non-string descriptors or greater than a bounded
  `CHAR`/`VARCHAR` descriptor fails with `1089 / HY000`;
- prefix byte contribution uses four bytes per nonbinary character for the
  fixed `utf8mb4` baseline and must fit the 3072-byte InnoDB key envelope;
- `TEXT` family prefixes must fit the type-family byte limit and the aggregate
  key envelope;
- full-column `TEXT` unique indexes remain rejected with `1170 / 42000`;
- invalid UTF-8 or embedded-NUL string key values fail with MyLite's existing
  string-key unsupported diagnostic before the statement mutates rows or
  catalog descriptors.

Duplicate semantics:

- `NULL` key values do not conflict.
- Non-`NULL` values conflict when their descriptor-converted prefix values
  compare equal under MyLite's ASCII `utf8mb4_0900_ai_ci` collation.
- Duplicate diagnostics format the prefix value, not the complete source
  value, truncate the composed duplicate-entry text to the MySQL 8.4.9
  observed 64-byte display envelope for this supported slice, and use the
  existing `Duplicate entry 'value' for key 'table.index'` shape.
- Existing rows are checked before `ALTER TABLE ... ADD UNIQUE` and
  `CREATE UNIQUE INDEX` mutate catalog descriptors.
- Current DML conflict lookup must compare prefix expressions on both sides:
  the stored row expression and the attempted assignment/insert value prefix.

## Physical SQLite Handling

Generated physical index SQL is descriptor-built:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>"
(substr("column_name", 1, <prefix_length>) COLLATE "utf8mb4_0900_ai_ci")
```

Every generated identifier is quoted. Prefix lengths are planner-validated
integers generated from descriptors, not user-interpolated expression text.
Duplicate-validation and DML conflict queries use the same descriptor-built
prefix expression. Catalog mutation, physical index creation, table identity
update, rollback, and SQLite schema-generation increments reuse existing index
lifecycle code.

## Result Behavior

Successful supported DDL returns:

- no result rows;
- `affected_rows == 0`;
- `warning_count == 0`;
- `ROW_COUNT() == 0`.

Successful supported DML preserves existing insert/update affected-row and
warning-count semantics. `INSERT IGNORE` emits one warning for each skipped
duplicate row. The existing `INSERT ... ON DUPLICATE KEY UPDATE` warning for
deprecated `VALUES()` references remains unchanged.

## Tests

The implementation must add focused C runtime coverage plus a MySQL-runtime
expectation script for:

- create-time, alter-time, and standalone unique prefix index metadata;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA` prefix metadata;
- nullable and `NOT NULL` `COLUMN_KEY` behavior, including `UNI` for a
  `NOT NULL` prefix unique column with no primary key;
- `VARCHAR`, `CHAR`, and `TEXT` family prefix duplicate enforcement;
- `INSERT`, `INSERT IGNORE`, `UPDATE`, and existing ODKU conflict paths;
- duplicate `NULL` values;
- existing-row duplicate validation for `ALTER TABLE ... ADD UNIQUE` and
  `CREATE UNIQUE INDEX`;
- prefix length errors, unknown names, duplicate names, reserved names, and
  unsupported composite unique prefix indexes;
- `CREATE TABLE ... LIKE`, drop-index interaction, reopen persistence,
  independent file-backed handles, and `.mylite` preamble safety;
- existing unique-index, index-prefix, create-index, alter-add-unique,
  drop-index, parser, DML duplicate-key, row-value, and full workflow checks.
