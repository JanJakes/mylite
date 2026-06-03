# Baseline Composite Unique Prefix Indexes

## Summary

This phase removes the deliberate gap between MyLite's one-part unique prefix
indexes and full-column composite unique indexes. It admits descriptor-owned
composite unique indexes where one or more string key parts use a prefix
length:

```sql
CREATE TABLE t (a VARCHAR(20), b VARCHAR(20), UNIQUE KEY u_ab (a(3), b(4)))
ALTER TABLE t ADD UNIQUE KEY u_ab (a(3), b(4))
CREATE UNIQUE INDEX u_ab ON t (a(3), b(4))
```

The slice keeps the existing ownership model: MyLite descriptors define the
logical key, MyLite validates and formats MySQL diagnostics, and generated
SQLite unique expression indexes enforce the physical tuple. It is not a
general index expansion.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline index prefix key parts:
  `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Baseline unique prefix index lifecycle:
  `docs/specs/baseline-unique-prefix-index-lifecycle/specs.md`
- Baseline composite unique indexes:
  `docs/specs/baseline-composite-unique-indexes/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_composite_unique_prefix_indexes_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `UNIQUE KEY u_ab (a(3), b(2))` succeeds for `VARCHAR` key parts, renders as
  ``UNIQUE KEY `u_ab` (`a`(3),`b`(2))``, reports `SHOW INDEX.Non_unique = 0`,
  and reports `Sub_part` / `SUB_PART` per key part.
- `ALTER TABLE ... ADD UNIQUE KEY ... (a(3), b(2))` and
  `CREATE UNIQUE INDEX ... ON ... (a(3), b(2))` succeed for the same subset,
  report `ROW_COUNT() == 0`, and leave `@@warning_count == 0`.
- Composite prefix uniqueness is checked on the ordered prefix tuple. Inserting
  `('abc999', 'xy11')` after `('abcdef', 'xyzz')` under
  `UNIQUE KEY u_ab (a(3), b(2))` fails with `1062 / 23000` and duplicate-entry
  text `abc-xy`.
- Duplicate diagnostics join formatted key parts with `-` and use the indexed
  prefix text for prefix parts.
- A composite unique key permits duplicate rows when any key part is `NULL`.
- `INSERT IGNORE` skips duplicate prefix tuples, inserts nonconflicting rows,
  and stores warning `1062`.
- `UPDATE` fails atomically when the post-update tuple would conflict with an
  existing row or when the updated row set would create an internal duplicate
  tuple.
- Existing rows are checked before `ALTER TABLE ... ADD UNIQUE` and
  `CREATE UNIQUE INDEX` mutate metadata. Duplicate populated prefix tuples fail
  with `1062 / 23000`.
- Prefix lengths on non-string key parts, zero prefix lengths, oversized bounded
  `CHAR` / `VARCHAR` prefixes, and aggregate key lengths above MySQL's InnoDB
  envelope keep the diagnostics already verified by the prefix-index phases.

Official MySQL documentation defines prefix lengths for nonbinary string index
parts as character counts, states that unique prefix indexes require uniqueness
within the prefix length, and exposes prefix lengths through
`SHOW INDEX.Sub_part` and `INFORMATION_SCHEMA.STATISTICS.SUB_PART`.

## Scope

Supported:

- persistent MyLite base tables only;
- create-time table-level
  `UNIQUE [KEY|INDEX] [index_name] (key_part, key_part[, ...])`;
- single-action
  `ALTER TABLE table_name ADD UNIQUE [KEY|INDEX] [index_name]
  (key_part, key_part[, ...])`;
- standalone
  `CREATE UNIQUE INDEX index_name ON table_name (key_part, key_part[, ...])`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- explicit or generated unique-index names using the existing descriptor policy;
- ordered two-or-more key-part lists where at least one part has a prefix
  length;
- unqualified descriptor-column key parts only;
- full-column key parts for the existing full unique-key descriptor subset;
- prefix key parts on `CHAR`, `VARCHAR`, and baseline `TEXT` family descriptors;
- positive decimal integer prefix lengths without sign;
- ASCII-valued non-`NULL` string key values using MyLite's fixed ASCII subset of
  `utf8mb4_0900_ai_ci`;
- duplicate `NULL` values whenever any key part is `NULL`;
- duplicate enforcement for current `INSERT ... VALUES`, `INSERT ... SET`,
  `INSERT IGNORE`, and single-assignment `UPDATE`;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, index drop paths, limited `INFORMATION_SCHEMA`
  metadata, reopen persistence, independent file-backed handles, and `.mylite`
  preamble preservation;
- no public ABI change.

Deferred:

- primary prefix indexes;
- binary string or BLOB prefix unique indexes;
- non-ASCII unique string key values and full Unicode collation weights;
- descending, functional, expression, table-qualified, ordinal, invisible,
  optioned, commented, parser, fulltext, spatial, algorithm, lock, partition,
  foreign-key, trigger, cascade, privilege, optimizer, and redundant-index
  warning behavior;
- composite unique conflict handling in
  `INSERT ... ON DUPLICATE KEY UPDATE`, which remains limited to one enforced
  single-column key;
- `REPLACE` behavior beyond the existing unique-index scope.

## Ownership Boundaries

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup. Successful supported DDL returns through existing
  no-row result conventions.
- Lexer/parser/AST: existing grammar preserves key-part lists as identifiers
  plus optional integer prefix-length literal nodes. Parser code does not
  inspect descriptors or SQLite schema.
- Analyzer/planner/runtime: resolves schema, table, index name, key columns,
  prefix admissibility, duplicate preflight, DML conflict lookup, and MySQL
  diagnostics from MyLite descriptors before generated SQLite SQL runs.
- Catalog: `_mylite_catalog_indexes` and `_mylite_catalog_index_columns` remain
  authoritative. `prefix_length IS NULL` means full-column part; a positive
  value means a prefix part.
- Result/introspection builders: render unique-key and prefix metadata from
  descriptors.
- SQLite registration: MyLite uses the first-party ASCII
  `utf8mb4_0900_ai_ci` collation callback through public SQLite APIs.
- SQLite physical storage: generated unique indexes use public SQLite
  expression indexes with descriptor-built `substr(column, 1, prefix_length)`
  terms. No SQLite fork patch is required.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Supported Grammar

Existing MyLite grammar already admits the required shape:

```lemon
unique_index_definition ::= UNIQUE unique_index_keyword_opt index_name_opt
                            LPAREN secondary_index_part_list RPAREN.

create_index_statement ::= CREATE UNIQUE INDEX identifier ON table_name
                           LPAREN secondary_index_part_list RPAREN.

alter_table_action ::= ADD UNIQUE unique_index_keyword_opt index_name_opt
                       LPAREN secondary_index_part_list RPAREN.

secondary_index_part_list ::= secondary_index_part.
secondary_index_part_list ::= secondary_index_part_list COMMA secondary_index_part.

secondary_index_part ::= identifier.
secondary_index_part ::= identifier LPAREN integer_literal RPAREN.
```

`prefix_length` is a positive decimal integer literal without sign. Parameters,
expressions, string literals, floating-point literals, hex literals, bit
literals, qualified columns, sort direction, and functional key parts are
outside this slice.

## Validation and Semantics

Target and index-name resolution reuse existing unique and prefix index policy:

- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` logical schema/table/index names are rejected before
  SQLite SQL generation;
- duplicate index names fail with `1061 / 42000`;
- quoted `PRIMARY` index names fail with `1280 / 42000`;
- omitted names derive from the first key column and use `_2`, `_3`, ...
  suffixes.

Key-part validation:

- unknown columns fail with `1072 / 42000`;
- key parts must be unqualified descriptor columns;
- duplicate key-part columns fail with `1060 / 42S21`;
- full-column parts keep the existing accepted full unique-key type set;
- prefix length `0` fails with `1391 / HY000`;
- prefix length on non-string descriptors or greater than a bounded
  `CHAR` / `VARCHAR` descriptor fails with `1089 / HY000`;
- prefix byte contribution uses four bytes per nonbinary character for the
  fixed `utf8mb4` baseline and must fit the 3072-byte InnoDB key envelope;
- `TEXT` family prefixes must fit the type-family byte limit and aggregate key
  envelope;
- full-column `TEXT` unique key parts remain rejected with `1170 / 42000`;
- non-ASCII or embedded-NUL string key values fail with the existing
  string-key unsupported diagnostic before row or catalog mutation.

Duplicate semantics:

- A tuple does not conflict when any key part is `NULL`.
- Non-`NULL` tuples conflict when every descriptor-built full or prefix key
  expression compares equal under MyLite's admitted physical comparison.
- Duplicate diagnostics format each prefix part using the indexed prefix value,
  join parts with `-`, and truncate the composed duplicate-entry value to the
  existing MySQL-observed display envelope.
- Existing-row validation runs before `ALTER TABLE ... ADD UNIQUE` and
  standalone `CREATE UNIQUE INDEX` mutate descriptors or physical indexes.
- Current DML conflict lookup compares the same descriptor-built expressions
  that back the generated SQLite unique index.

## Physical SQLite Handling

Generated physical index SQL is descriptor-built:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" (
  substr("a", 1, 3) COLLATE "utf8mb4_0900_ai_ci",
  substr("b", 1, 2) COLLATE "utf8mb4_0900_ai_ci"
)
```

Full string key parts receive the same collation annotation as existing unique
string keys. Prefix lengths are planner-validated descriptor integers, not
user-interpolated expression text. All generated identifiers are quoted.
Duplicate-validation and DML conflict queries use the same descriptor-built
full/prefix expressions. Catalog mutation, physical index creation, table
identity update, rollback, and SQLite schema-generation increments reuse the
existing index lifecycle code.

## Result Behavior

Successful supported DDL returns:

- no result rows;
- `affected_rows == 0`;
- `warning_count == 0`;
- `ROW_COUNT() == 0`.

Supported successful DML keeps existing insert/update affected-row behavior and
reports `warning_count == 0`, except `INSERT IGNORE`, which skips duplicate
prefix tuples and appends warning `1062`.

## Diagnostics

This phase reuses existing diagnostics for:

- syntax errors and unsupported grammar: `1064 / 42000`;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- unknown key columns: `1072 / 42000`;
- duplicate key-part columns: `1060 / 42S21`;
- duplicate index names: `1061 / 42000`;
- invalid `PRIMARY` secondary-index names: `1280 / 42000`;
- invalid prefix length zero: `1391 / HY000`;
- invalid prefix type or overlong bounded prefix: `1089 / HY000`;
- key length above the current InnoDB envelope: `1071 / 42000`;
- BLOB-family full-column key parts without prefix: `1170 / 42000`; full
  `TEXT` family unique key parts are accepted only by the documented WordPress
  bridge;
- duplicate populated prefix tuples: `1062 / 23000`;
- unsupported non-ASCII string key values: existing MyLite-specific
  unsupported diagnostic;
- unsupported future object kinds, allocation failures, public API misuse, and
  physical SQLite failures through existing runtime diagnostics.

## Tests

Implementation tests must cover:

- create-time, alter-time, and standalone composite unique prefix metadata;
- `SHOW CREATE TABLE`, `SHOW INDEX.Sub_part`, and
  `INFORMATION_SCHEMA.STATISTICS.SUB_PART`;
- duplicate tuple diagnostics for insert, insert-ignore, update existing-row
  conflicts, update internal conflicts, alter-add validation, and create-index
  validation;
- duplicate `NULL` tuple behavior;
- mixed full-column and prefix parts;
- key-length and invalid-prefix diagnostics inherited from prefix-index tests;
- descriptor cloning and drop behavior inherited from existing index tests;
- reopen persistence and `.mylite` preamble preservation;
- existing unique-index, unique-prefix, composite-unique, prefix-index,
  create-index, alter-add-index, alter-add-unique, insert-ignore, update, show,
  information-schema, file-backed opening, VFS, and full check workflows.

## Compatibility

`COMPATIBILITY.md`, `docs/compatibility/sql-table-ddl.md`, and
`docs/compatibility/sql-indexes-constraints.md` should describe this as limited
composite unique prefix support. They must not claim primary prefix indexes,
binary prefix unique indexes, full Unicode collation parity, optimizer
behavior, full `ON DUPLICATE KEY UPDATE`, or general index-option support.
