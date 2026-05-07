# Standalone CREATE INDEX and DROP INDEX

## Scope

This feature adds standalone index DDL over tables created by MyLite's
supported `CREATE TABLE` subset.

Full feature surface:

- `CREATE [UNIQUE | FULLTEXT | SPATIAL] INDEX index_name [index_type]
  ON table_name (key_part [, key_part ...]) [index_option ...]
  [algorithm_option | lock_option] ...`
- `DROP INDEX index_name ON table_name [algorithm_option | lock_option] ...`
- schema-qualified and selected-schema table resolution
- metadata updates in `__mylite_index_catalog`,
  `INFORMATION_SCHEMA.STATISTICS`, and constraint metadata views derived from
  unique-index rows
- nonunique, unique, full-text, and spatial index classes
- identifier key parts with optional prefix length and `ASC` or `DESC`
- later-position index type, deprecated earlier-position index type, and
  `TYPE` synonym behavior
- index options `KEY_BLOCK_SIZE`, `USING`/`TYPE`, `WITH PARSER`, `COMMENT`,
  `VISIBLE`, `INVISIBLE`, `ENGINE_ATTRIBUTE`, and
  `SECONDARY_ENGINE_ATTRIBUTE`
- table-copying/concurrency clauses `ALGORITHM` and `LOCK`
- duplicate-index warnings, duplicate-name errors, missing-object diagnostics,
  warnings, affected rows, implicit commits, and atomicity
- interaction with `INSERT`, `INSERT ... ON DUPLICATE KEY UPDATE`, `REPLACE`,
  `UPDATE`, and future optimizer/index-hint behavior

First executable implementation slice:

- execute `CREATE [UNIQUE] INDEX [index_type] index_name [index_type]
  ON table_name (identifier_key_parts) [index_option ...]
  [algorithm_option | lock_option] ...`
- execute `DROP INDEX index_name ON table_name
  [algorithm_option | lock_option] ...`
- support existing MyLite base tables in user schemas only
- support identifier key parts with prefix lengths and `ASC`/`DESC` metadata
- support `USING BTREE`, `USING HASH`, `TYPE BTREE`, and `TYPE HASH` in the
  MySQL-accepted positions; record the effective metadata type as `BTREE` for
  MyLite's InnoDB-compatible table model and preserve explicit BTREE display
  metadata for `SHOW CREATE TABLE`
- support `KEY_BLOCK_SIZE`, `COMMENT`, `VISIBLE`, `INVISIBLE`, and
  `SECONDARY_ENGINE_ATTRIBUTE` syntax and metadata where the current catalog
  can represent it; reject `ENGINE_ATTRIBUTE` for the scoped InnoDB runtime
- update unique-index conflict surfaces used by `INSERT`, ODKU, `REPLACE`, and
  `UPDATE` immediately after successful `CREATE UNIQUE INDEX`, and remove
  them after successful `DROP INDEX`
- validate existing rows before accepting a new unique index
- implement statement-level atomicity for catalog updates and any physical
  SQLite index changes

Deferred from the first slice:

- executable `FULLTEXT` and `SPATIAL` indexes
- `WITH PARSER` runtime behavior
- functional key parts and multi-valued indexes
- optimizer use and index hints
- complete type/prefix/collation validation and storage-engine warning fidelity
- primary-key drop when it interacts with `AUTO_INCREMENT`, generated invisible
  primary keys, foreign keys, generated columns, triggers, and privileges
- exact protocol OK information beyond current public statement metadata

## Sources

- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `DROP INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-index.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, invisible indexes:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-indexes.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, atomic DDL:
  https://dev.mysql.com/doc/refman/8.4/en/atomic-ddl.html
- Existing MyLite specs:
  - `docs/specs/create-table-indexes/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-on-duplicate-key-update/specs.md`
  - `docs/specs/replace/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@innodb_autoinc_lock_mode = 2`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Statement shape and relation to ALTER TABLE

`CREATE INDEX` and `DROP INDEX` are standalone statements, but MySQL maps them
to `ALTER TABLE` operations internally. They cause implicit commits like other
nontemporary DDL statements. For InnoDB tables, successful table DDL is atomic:
metadata and storage-engine effects are committed together or rolled back
together if the DDL operation fails.

Standalone `CREATE INDEX` requires an explicit index name:

| Statement | MySQL behavior |
| --- | --- |
| `CREATE INDEX idx_a ON t (a)` | accepted |
| `CREATE INDEX ON t (a)` | syntax error 1064 |
| `CREATE INDEX IF NOT EXISTS idx ON t (a)` | syntax error 1064 |
| `DROP INDEX idx_a ON t` | accepted |
| `DROP INDEX IF EXISTS idx_a ON t` | syntax error 1064 |
| `DROP KEY idx_a ON t` | syntax error 1064 |

The `KEY` synonym is not a standalone `DROP` statement form. `DROP KEY` exists
inside `ALTER TABLE`, which remains covered by the later ALTER TABLE key-action
task.

### Index classes

`CREATE INDEX` creates a nonunique index. `CREATE UNIQUE INDEX` creates a
unique index. `CREATE FULLTEXT INDEX` and `CREATE SPATIAL INDEX` create
full-text and spatial indexes subject to type and storage-engine validation.

Observed InnoDB metadata:

| Statement | `STATISTICS.NON_UNIQUE` | `STATISTICS.INDEX_TYPE` |
| --- | --- | --- |
| `CREATE INDEX idx_a ON t (a)` | `1` | `BTREE` |
| `CREATE UNIQUE INDEX uq_a ON t (a)` | `0` | `BTREE` |
| `CREATE FULLTEXT INDEX ft_c ON t (c) WITH PARSER ngram` | `1` | `FULLTEXT` |
| `CREATE SPATIAL INDEX sp_g ON t (g)` over a `GEOMETRY NOT NULL SRID 0` column | `1` | `SPATIAL` |

Unique indexes reject duplicate non-`NULL` key values when added to a populated
table. Nullable unique parts permit multiple `NULL` values. Unique indexes are
enforced even when the index is invisible.

`FULLTEXT` indexes are storage-engine and column-type constrained. Prefix
lengths are not meaningful for full-text indexes. `WITH PARSER parser_name` is
accepted only for full-text indexes.

`SPATIAL` indexes are constrained to spatial columns, must be single-column for
the documented InnoDB/MyISAM spatial index surface, require indexed columns to
be `NOT NULL`, prohibit prefix lengths, and cannot be unique or primary-key
indexes.

### Key parts, prefix lengths, and order

Standalone index key parts follow the same supported identifier-key-part
surface already used by MyLite's `CREATE TABLE` index metadata:

```sql
CREATE INDEX idx_c_prefix ON t (c(3) DESC, a ASC);
```

In `INFORMATION_SCHEMA.STATISTICS`, descending parts report `COLLATION='D'`,
ascending or omitted-order parts report `COLLATION='A'`, and prefix lengths
report in `SUB_PART`. MySQL interprets prefix lengths as characters for
nonbinary string columns and bytes for binary string columns; full type,
charset, and byte-limit validation is storage-engine-sensitive.

Malformed key-part lists are syntax errors:

| Statement | MySQL behavior |
| --- | --- |
| `CREATE INDEX idx_empty ON t ()` | syntax error 1064 |
| `CREATE INDEX idx_trailing ON t (a,)` | syntax error 1064 |
| `CREATE INDEX idx_missing_col ON t (missing_col)` | error 1072 |

Functional key parts and multi-valued indexes are MySQL-valid surfaces, but
they require expression, generated-column, JSON-array, and virtual-index
support that MyLite does not have yet.

### Index names, generated names, and duplicates

Standalone `CREATE INDEX` has no generated-name form because the index name is
mandatory. MySQL's generated `first_column`, `first_column_2`, ... naming
applies to unnamed index definitions in `CREATE TABLE` and `ALTER TABLE`, not
to standalone `CREATE INDEX`.

Index names are compared case-insensitively within a table for duplicate-name
validation. Creating a second index with an existing name fails with error
1061, even if the key parts differ.

MySQL allows redundant indexes with different names. If a new index duplicates
an existing index definition, the DDL succeeds and records warning 1831. The
warning message names the newly created index and says duplicate indexes are
deprecated and may be disallowed in a future release. Runtime probes observed
this warning for same-key nonunique indexes and for full-text duplicate shapes.

The primary key index name is always `PRIMARY`. Dropping it through standalone
DDL requires a quoted identifier:

```sql
DROP INDEX `PRIMARY` ON t;
```

Unquoted `PRIMARY` in `DROP INDEX PRIMARY ON t` is a syntax error because
`PRIMARY` is reserved. Quoted primary-key drop succeeds for a simple primary
key with no `AUTO_INCREMENT` dependency. If the primary key supports an
`AUTO_INCREMENT` column, dropping it fails with error 1075 because the auto
column must remain indexed.

### Index types and index options

MySQL accepts index type clauses in two positions:

```sql
CREATE INDEX idx_pre USING BTREE ON t (a);
CREATE INDEX idx_post ON t (a) USING BTREE;
CREATE INDEX idx_type ON t (a) TYPE BTREE;
CREATE INDEX idx_type_pre TYPE BTREE ON t (a);
```

The pre-`ON` position is deprecated. If index type is written in both the early
and late positions, the later option wins. `TYPE` is accepted as a synonym for
`USING`, but `USING` is preferred.

For InnoDB, `BTREE` is the supported ordinary index type. `USING HASH` is
accepted for ordinary indexes but records note 3502 and falls back to the
storage-engine default. `RTREE` is recognized by MySQL's parser family for
spatial-related syntax, but it is not a first-slice MyLite ordinary index type.
`SHOW CREATE TABLE` displays explicit `USING BTREE` for standalone indexes
created with BTREE syntax. It does not display a type clause for default BTREE
or HASH fallback indexes.

Post-key-list options:

| Option | MySQL behavior |
| --- | --- |
| `KEY_BLOCK_SIZE [=] integer` | accepted; index-level effect is engine-specific and not supported by InnoDB |
| `COMMENT 'text'` | accepted; shown in `STATISTICS.INDEX_COMMENT` |
| `VISIBLE` / `INVISIBLE` | accepted for non-primary indexes; default visible |
| `ENGINE_ATTRIBUTE [=] 'json-or-empty-string'` | InnoDB rejects index-level `ENGINE_ATTRIBUTE` with error 3981 after validating the attribute string as JSON. Invalid JSON reports error 3980 first. |
| `SECONDARY_ENGINE_ATTRIBUTE [=] 'json-or-empty-string'` | accepted for the scoped InnoDB probes and not displayed by `SHOW CREATE TABLE` |
| `WITH PARSER parser_name` | accepted only for full-text indexes |

`COMMENT = 'text'` is a syntax error. String-valued engine attributes with
invalid JSON are validation errors in MySQL; for valid `ENGINE_ATTRIBUTE`
JSON, the first MyLite slice reports the InnoDB storage-engine rejection.

### ALGORITHM and LOCK

`CREATE INDEX` and `DROP INDEX` accept any number of `ALGORITHM` and `LOCK`
clauses after the index definition:

```sql
CREATE INDEX idx ON t (a) ALGORITHM=INPLACE LOCK=NONE;
CREATE INDEX idx2 ON t (b) LOCK=NONE ALGORITHM=INPLACE;
DROP INDEX idx ON t ALGORITHM=INPLACE LOCK=NONE;
```

The clauses have the same meaning as for `ALTER TABLE`. MyLite's first slice
should parse and validate the accepted keywords, but no concurrency/copying
behavior is meaningful in the embedded runtime. `ALGORITHM=DEFAULT`,
`ALGORITHM=INPLACE`, `ALGORITHM=COPY`, `LOCK=DEFAULT`, `LOCK=NONE`,
`LOCK=SHARED`, and `LOCK=EXCLUSIVE` should be accepted as embedded no-op
modifiers once the target index operation itself is supported.

### Object resolution and diagnostics

Observed diagnostics:

| Condition | Code | SQLSTATE | Behavior |
| --- | --- | --- | --- |
| unqualified table with no selected schema | 1046 | `3D000` | error before mutation |
| qualified missing schema | 1049 | `42000` | error before mutation |
| missing table in selected or explicit existing schema | 1146 | `42S02` | error before mutation |
| system schema target, such as `information_schema.TABLES` | 1044 | `42000` | access denied before mutation |
| missing key part column | 1072 | `42000` | error before mutation |
| duplicate index name | 1061 | `42000` | error before mutation |
| adding unique index over duplicate existing values | 1062 | `23000` | error before mutation |
| missing index on drop | 1091 | `42000` | error before mutation |
| dropping primary key needed by auto-increment | 1075 | `42000` | error before mutation |

MyLite does not implement privileges, but system schema writes should still use
MySQL-compatible access-denied diagnostics.

### Metadata

Successful standalone index creation adds one `INFORMATION_SCHEMA.STATISTICS`
row per key part:

- `TABLE_CATALOG='def'`
- table schema and table name from the target table
- `NON_UNIQUE=0` for primary/unique, `1` otherwise
- `INDEX_SCHEMA` equal to the target schema
- `INDEX_NAME` equal to the authored index name, except primary key uses
  `PRIMARY`
- `SEQ_IN_INDEX` starting at `1`
- `COLUMN_NAME` for identifier key parts
- `COLLATION='A'` or `'D'` for ordered key parts; `NULL` for full-text rows
  observed in probes
- `SUB_PART` for prefix lengths
- `INDEX_TYPE` as `BTREE`, `FULLTEXT`, `SPATIAL`, or another storage-engine
  method where applicable
- `INDEX_COMMENT` from `COMMENT`
- `IS_VISIBLE='YES'` or `'NO'`

Successful standalone unique index creation also appears in the first
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` slice as a unique constraint and in
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` as one row per column key part. Both views
derive those rows from `__mylite_index_catalog`; nonunique indexes do not
produce constraint rows. Successful `DROP INDEX` removes the same derived rows
when the dropped index is unique.

Dropping an index removes all statistics rows for that index. If the dropped
index is unique, later writes no longer treat it as a duplicate-key conflict
surface.

### DML conflict interactions

Standalone unique indexes immediately affect all duplicate-key-sensitive write
paths:

- `INSERT` rejects duplicate non-`NULL` key values after `CREATE UNIQUE INDEX`
  succeeds.
- `INSERT ... ON DUPLICATE KEY UPDATE` selects its conflicting row by primary
  and unique index order. Runtime probes showed that changing standalone unique
  creation order changes which row is updated when a candidate conflicts with
  two different rows.
- `REPLACE` deletes all rows that conflict with any primary or unique index,
  including standalone-created unique indexes.
- Dropping a unique index removes it from later `INSERT`, ODKU, `REPLACE`, and
  `UPDATE` duplicate checks.

MyLite's conflict detection must use its own catalog order rather than
SQLite-reported constraint order. This is required for MySQL-compatible ODKU
target-row selection and REPLACE delete loops.

### Physical storage

MySQL creates physical secondary index structures. MyLite should not blindly
lower the first slice to SQLite `CREATE INDEX`/`DROP INDEX` as the semantic
source of truth. SQLite indexes can help performance, but MyLite still owns:

- MySQL object resolution and diagnostics
- case-insensitive duplicate index-name validation
- MySQL duplicate-index warning 1831
- prefix-index semantics
- nullable unique-key behavior
- conflict order across primary and unique indexes
- visibility metadata
- full-text and spatial deferrals
- future `SHOW INDEX`, optimizer, trigger, foreign-key, and generated-column
  behavior

The first implementation may create auxiliary SQLite nonunique indexes for
ordinary full-column BTREE-compatible key parts as an optimization. Physical
unique SQLite indexes should be used only when they cannot replace MyLite's
explicit duplicate checks and cannot produce user-visible diagnostics in the
wrong order. Prefix unique indexes, nullable composite keys, collation-sensitive
indexes, invisible indexes, full-text indexes, spatial indexes, functional key
parts, and multi-valued indexes should remain metadata-only until their MySQL
semantics are implemented explicitly.

## MyLite behavior

### Parser and AST

Add top-level statement nodes for `CREATE INDEX` and `DROP INDEX`.

`CREATE INDEX` records the index class (`ordinary`, `unique`, `fulltext`, or
`spatial`) on the statement node. Its children are:

1. index name
2. optional pre-`ON` index type
3. target table name
4. key-part list
5. index-option list, including post-key-list index type options
6. DDL algorithm/lock option list

`DROP INDEX` node children:

1. index name
2. target table name
3. DDL algorithm/lock option list

The parser must preserve source order for index options and algorithm/lock
clauses. Later semantic analysis chooses the effective index type when multiple
type options are present.

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature and are
independently authored for MyLite's parser:

```lemon
create_index_statement ::= CREATE INDEX identifier opt_pre_index_type
                           ON table_name LPAREN key_part_list RPAREN
                           index_option_list ddl_table_option_list.

create_index_statement ::= CREATE UNIQUE INDEX identifier opt_pre_index_type
                           ON table_name LPAREN key_part_list RPAREN
                           index_option_list ddl_table_option_list.

create_index_statement ::= CREATE FULLTEXT INDEX identifier opt_pre_index_type
                           ON table_name LPAREN key_part_list RPAREN
                           fulltext_index_option_list ddl_table_option_list.

create_index_statement ::= CREATE SPATIAL INDEX identifier opt_pre_index_type
                           ON table_name LPAREN key_part_list RPAREN
                           index_option_list ddl_table_option_list.

opt_pre_index_type ::= .
opt_pre_index_type ::= index_type.

key_part_list ::= key_part.
key_part_list ::= key_part_list COMMA key_part.

key_part ::= identifier opt_key_part_prefix opt_key_part_order.

opt_key_part_prefix ::= .
opt_key_part_prefix ::= LPAREN INTEGER RPAREN.

opt_key_part_order ::= .
opt_key_part_order ::= ASC.
opt_key_part_order ::= DESC.

index_option_list ::= .
index_option_list ::= index_option_list index_option.

index_option ::= index_type.
index_option ::= KEY_BLOCK_SIZE INTEGER.
index_option ::= KEY_BLOCK_SIZE EQ INTEGER.
index_option ::= COMMENT STRING.
index_option ::= VISIBLE.
index_option ::= INVISIBLE.
index_option ::= ENGINE_ATTRIBUTE STRING.
index_option ::= ENGINE_ATTRIBUTE EQ STRING.
index_option ::= SECONDARY_ENGINE_ATTRIBUTE STRING.
index_option ::= SECONDARY_ENGINE_ATTRIBUTE EQ STRING.

fulltext_index_option_list ::= .
fulltext_index_option_list ::= fulltext_index_option_list index_option.
fulltext_index_option_list ::= fulltext_index_option_list fulltext_index_option.

fulltext_index_option ::= WITH PARSER identifier.

index_type ::= USING index_algorithm.
index_type ::= TYPE index_algorithm.

index_algorithm ::= BTREE.
index_algorithm ::= HASH.

ddl_table_option_list ::= .
ddl_table_option_list ::= ddl_table_option_list ddl_table_option.

ddl_table_option ::= ALGORITHM opt_equal ddl_algorithm.
ddl_table_option ::= LOCK opt_equal ddl_lock.

ddl_algorithm ::= DEFAULT.
ddl_algorithm ::= INPLACE.
ddl_algorithm ::= COPY.

ddl_lock ::= DEFAULT.
ddl_lock ::= NONE.
ddl_lock ::= SHARED.
ddl_lock ::= EXCLUSIVE.

drop_index_statement ::= DROP INDEX identifier ON table_name
                         ddl_table_option_list.
```

The first executable slice should reject or return deterministic unsupported
diagnostics for these MySQL-valid shapes until their dedicated runtime support
exists:

```lemon
/* Deferred: functional and multi-valued key parts. */
key_part ::= LPAREN expression RPAREN.
key_part ::= LPAREN CAST LPAREN expression AS type_name ARRAY RPAREN RPAREN.

/* Deferred runtime: full-text parser behavior. */
fulltext_index_option ::= WITH PARSER identifier.

/* Invalid standalone MySQL syntax, keep rejected. */
create_index_statement ::= CREATE INDEX ON table_name LPAREN key_part_list RPAREN.
create_index_statement ::= CREATE INDEX IF NOT EXISTS identifier ON table_name
                           LPAREN key_part_list RPAREN.
drop_index_statement ::= DROP INDEX IF EXISTS identifier ON table_name.
drop_index_statement ::= DROP KEY identifier ON table_name.
```

### Analysis and validation

Execution validation order:

1. Resolve the target table using the same selected-schema and
   schema-qualified rules as current executable DDL and DML.
2. Reject system schemas before mutation.
3. Load table, column, and index catalog rows.
4. Validate the index name is present and does not duplicate an existing index
   name case-insensitively.
5. Validate all identifier key parts name existing columns.
6. Validate first-slice index class and options.
7. For unique indexes, scan existing rows through MyLite's value comparison
   rules and reject duplicate non-`NULL` key values before metadata mutation.
8. Record duplicate-index warning 1831 when a new index duplicates an existing
   compatible index shape with a different name.
9. Apply catalog and optional physical SQLite side effects atomically.

For `DROP INDEX`:

1. Resolve table and reject system schemas.
2. Find the index name case-insensitively.
3. Reject missing indexes with a MySQL-compatible diagnostic.
4. Reject primary-key drop when it would leave an `AUTO_INCREMENT` column
   unindexed or otherwise violate current table constraints.
5. Delete all catalog rows for the index and remove optional physical SQLite
   indexes atomically.

### Runtime side effects

`CREATE INDEX` and `DROP INDEX` produce no result set. Successful statements
report affected rows `0`. They should update warning state for note 3502
(`USING HASH` fallback), warning 1831 (duplicate index), and any supported
storage-engine option warnings. Exact protocol OK text such as
`Records: 0 Duplicates: 0 Warnings: N` belongs to the eventual protocol layer,
but statement state should retain those counters.

The first implementation should use a statement-owned SQLite transaction or
savepoint for catalog and optional physical-index updates. If validation or a
physical operation fails, no partial catalog rows or SQLite indexes should
survive.

Standalone index DDL commits any active explicit transaction before validation
and execution. Successful mutations run in their own statement transaction and
leave no explicit transaction active. Validation failures after the pre-DDL
commit also leave the session outside the explicit transaction; later writes
therefore autocommit unless the client starts a new transaction.

### Implementation status

The first executable slice is implemented for supported base tables:

- `CREATE INDEX` and `CREATE UNIQUE INDEX` parse and execute for identifier key
  parts on supported persistent and temporary base tables, prefix lengths,
  `ASC`/`DESC`, BTREE/HASH clauses, comments, visibility, engine attributes,
  and embedded no-op `ALGORITHM`/`LOCK` modifiers.
- successful creation writes internal index catalog rows surfaced through
  `INFORMATION_SCHEMA.STATISTICS`, including `NON_UNIQUE`, `SEQ_IN_INDEX`,
  `COLUMN_NAME`, `COLLATION`, `SUB_PART`, `INDEX_TYPE`, `INDEX_COMMENT`, and
  `IS_VISIBLE`.
- standalone unique indexes validate existing rows before mutation and
  participate in later `INSERT`, ODKU, `REPLACE`, and `UPDATE` duplicate
  checks, including prefix-length comparisons and nullable unique-key behavior.
- `DROP INDEX name ON table` removes metadata rows and removes the index from
  later unique-conflict checks.
- standalone `CREATE INDEX` and `DROP INDEX` apply MySQL-compatible
  implicit-commit behavior before validation and execution, including failing
  validation paths and read-only transaction boundaries.
- `DROP INDEX` rejects child supporting indexes and parent primary/unique
  indexes required by foreign-key catalog rows with error 1553, even while
  `foreign_key_checks=0`.
- warning 3502 is recorded for HASH fallback and warning 1831 is recorded for
  redundant index definitions.
- `WITH PARSER` is accepted by the parser only on the full-text standalone
  index shape; full-text parser-plugin behavior remains tied to the deferred
  full-text index surface.

The remaining gaps in the next section are still intentionally unsupported or
metadata-only.

### Compatibility gaps

- `FULLTEXT` and `SPATIAL` standalone index creation can be parsed, but
  executable metadata and runtime search/spatial semantics are deferred from
  the first slice.
- `WITH PARSER` parser-plugin semantics are deferred.
- Functional key parts and multi-valued indexes are deferred until expression,
  JSON, generated-column, and virtual-index machinery exists.
- Prefix-length storage and comparison semantics are partial until MyLite has
  full type, charset, collation, byte-length, and conversion support.
- Physical SQLite indexes are optimization artifacts only; MyLite metadata and
  explicit duplicate-check code remain the compatibility source of truth.
- Optimizer behavior, invisible-index use, index hints, `SHOW INDEX`,
  `SHOW CREATE TABLE`, and Performance Schema/sys index statistics are
  deferred.
- Generated columns, triggers, privileges, partition routing, and online DDL
  concurrency are deferred.
- Full warning records and SQLSTATE/numeric-code public exposure remain tied
  to MyLite's broader diagnostics work.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `CREATE INDEX idx_a ON t (a)` | Succeeds, affected rows `0`, statistics row `NON_UNIQUE=1`, `INDEX_TYPE='BTREE'`. |
| `CREATE UNIQUE INDEX uq_a ON t (a)` over distinct non-`NULL` values | Succeeds, statistics row `NON_UNIQUE=0`, later duplicate inserts fail. |
| `CREATE UNIQUE INDEX uq_a ON t (a)` over duplicate existing values | Error 1062; no index metadata is added. |
| nullable unique index with multiple `NULL` values | Succeeds; later duplicate non-`NULL` values fail, additional `NULL` values do not conflict. |
| `CREATE UNIQUE INDEX uq_a ON t (a) INVISIBLE` | Succeeds; duplicate enforcement still applies. |
| `DROP INDEX uq_a ON t` | Succeeds; later duplicate values on `a` are allowed unless another unique key conflicts. |
| `DROP INDEX fk_child ON child` while `fk_child` supports a foreign key | Error 1553; the foreign key and index remain. |
| `DROP INDEX parent_unique ON parent` while a foreign key references it | Error 1553 even when `foreign_key_checks=0`; the foreign key and index remain. |
| `CREATE INDEX idx_pre USING BTREE ON t (a)` | Succeeds. |
| `CREATE INDEX idx_post ON t (a) USING BTREE` | Succeeds. |
| `CREATE INDEX idx_type ON t (a) TYPE BTREE` | Succeeds. |
| `CREATE INDEX idx_hash ON t (a) USING HASH` on InnoDB | Succeeds with note 3502 and records effective BTREE-compatible metadata. |
| `CREATE INDEX idx_comment ON t (c(3) DESC, a ASC) COMMENT 'hello' INVISIBLE KEY_BLOCK_SIZE = 8` | Succeeds; statistics expose `SUB_PART=3`, `COLLATION='D'/'A'`, comment, and invisible state. `SHOW CREATE TABLE` displays `KEY \`idx_comment\` (\`c\`(3) DESC,\`a\`) COMMENT 'hello' /*!80000 INVISIBLE */`; `KEY_BLOCK_SIZE` is not displayed for the scoped InnoDB probe. |
| `CREATE INDEX idx_secondary_attr ON t (a) SECONDARY_ENGINE_ATTRIBUTE='{}'` | Succeeds without visible `SHOW CREATE TABLE` metadata. |
| `CREATE INDEX idx_engine_attr ON t (a) ENGINE_ATTRIBUTE='{}'` | Error 3981 for InnoDB; no mutation. |
| creating a redundant index with a different name | Succeeds with warning 1831. |
| creating a second index with the same name | Error 1061; no mutation. |
| `CREATE INDEX ON t (a)` | Syntax error. |
| `CREATE INDEX IF NOT EXISTS idx ON t (a)` | Syntax error. |
| `CREATE INDEX idx ON missing (a)` | Missing-table error; no mutation. |
| unqualified table with no selected schema | Error 1046. |
| system schema target | Access denied; no mutation. |
| missing key column | Error 1072; no mutation. |
| empty or trailing-comma key-part list | Syntax error. |
| `COMMENT = 'bad'` | Syntax error. |
| `CREATE FULLTEXT INDEX ft_c ON t (c) WITH PARSER ngram COMMENT 'ft'` | Deferred unsupported or parse-only until full-text metadata/runtime is implemented. |
| `CREATE SPATIAL INDEX sp_g ON t (g)` | Deferred unsupported or parse-only until spatial metadata/runtime is implemented. |
| `CREATE INDEX idx ON t (a) ALGORITHM=INPLACE LOCK=NONE` | Succeeds when ordinary index creation is supported; clauses are no-op modifiers in MyLite. |
| `CREATE INDEX idx ON t (a) LOCK=NONE ALGORITHM=INPLACE` | Same as above; option order accepted. |
| `DROP INDEX idx ON t ALGORITHM=INPLACE LOCK=NONE` | Succeeds when index exists. |
| `DROP INDEX missing_idx ON t` | Error 1091; no mutation. |
| `START TRANSACTION; INSERT; CREATE INDEX idx ON t (a); INSERT; ROLLBACK` | Both inserts and the index creation survive because standalone index DDL commits before execution. |
| `START TRANSACTION; INSERT; CREATE INDEX idx ON t (missing); INSERT; ROLLBACK` | Both inserts survive because the failed index creation commits before validation and leaves the session outside the explicit transaction after the error. |
| `START TRANSACTION; INSERT; DROP INDEX idx ON t; INSERT; ROLLBACK` | Both inserts and the index drop survive because standalone index DDL commits before execution. |
| `START TRANSACTION; INSERT; DROP INDEX missing_idx ON t; INSERT; ROLLBACK` | Both inserts survive because the failed index drop commits before validation and leaves the session outside the explicit transaction after the error. |
| `START TRANSACTION READ ONLY; CREATE INDEX idx ON t (a); ROLLBACK` | Index creation succeeds because the read-only transaction is committed first. |
| `START TRANSACTION READ ONLY; DROP INDEX idx ON t; ROLLBACK` | Index removal succeeds because the read-only transaction is committed first. |
| `DROP INDEX IF EXISTS idx ON t` | Syntax error. |
| `DROP KEY idx ON t` | Syntax error. |
| `DROP INDEX PRIMARY ON t` | Syntax error unless `PRIMARY` is quoted. |
| `DROP INDEX \`PRIMARY\` ON simple_pk` | Succeeds for a simple primary key without dependent auto-increment constraints. |
| `DROP INDEX \`PRIMARY\` ON auto_pk` | Error 1075 when the primary key is required for `AUTO_INCREMENT`. |
| ODKU candidate conflicts with two standalone unique indexes | Updates the row selected by MySQL-compatible unique-index creation/catalog order. |
| `REPLACE` candidate conflicts through two standalone unique indexes | Deletes all conflicting old rows and inserts the candidate. |
| fatal validation failure after starting index creation | Leaves no partial catalog rows or physical SQLite index. |

## Test plan

Parser tests:

- ordinary, unique, full-text, and spatial index class tokens
- required standalone index name
- schema-qualified target tables
- pre- and post-position `USING`/`TYPE` index types
- key parts with prefix lengths, `ASC`, and `DESC`
- `KEY_BLOCK_SIZE`, `COMMENT`, visibility, engine attributes, and
  `WITH PARSER`
- `ALGORITHM` and `LOCK` with and without `=`, in both orders
- `DROP INDEX` with quoted `PRIMARY`
- malformed omitted names, `IF NOT EXISTS`, `IF EXISTS`, `DROP KEY`, empty
  key-part lists, trailing commas, invalid comments, malformed option values,
  and deferred functional/multi-valued key parts

Runtime tests for the first executable slice:

- successful nonunique and unique index creation in selected and
  schema-qualified schemas
- no selected schema, missing table, system schema, missing column, duplicate
  name, duplicate existing unique values, and missing index diagnostics
- implicit commits for successful and failing `CREATE INDEX` / `DROP INDEX`,
  including read-only transaction boundaries
- duplicate redundant-index warning 1831
- `USING HASH` note 3502 under the InnoDB-compatible table model
- metadata rows in `INFORMATION_SCHEMA.STATISTICS` for nonunique, unique,
  prefix, order, comment, visibility, and effective index type, plus
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` rows for unique indexes
- unique-index enforcement for `INSERT`, `INSERT ... SET`, ODKU, `REPLACE`,
  and `UPDATE`
- nullable unique-key parts
- ODKU conflict selection with unique indexes created in different orders
- REPLACE delete-all-conflicting-rows behavior
- drop unique index followed by allowed duplicate writes
- quoted primary-key drop acceptance/rejection cases
- rollback/no-mutation for failed create and drop validation
- optional physical SQLite index creation/removal only when compatible with the
  metadata source of truth

Deferred runtime tests:

- full-text index metadata, parser plugins, `MATCH ... AGAINST`, and full-text
  optimizer behavior
- spatial index validation and spatial lookup behavior
- functional and multi-valued index metadata/conflicts
- full prefix/collation/type conversion fidelity
- index hints, invisible-index optimizer switch behavior, `SHOW INDEX`, and
  `SHOW CREATE TABLE`
- foreign-key, generated-column, trigger, privilege, partition, and online DDL
  interactions

MySQL-runtime comparison tests must verify result rows, error codes, SQLSTATEs,
warning codes/messages, warning ordering, statistics metadata, affected rows,
duplicate-key side effects, and absence of mutation for failed DDL.
