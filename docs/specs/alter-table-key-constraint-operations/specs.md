# ALTER TABLE key and constraint operations

## Scope

This feature specifies the key and constraint subset of `ALTER TABLE` for
MyLite:

- `ADD [CONSTRAINT [symbol]] PRIMARY KEY [index_type] (key_part, ...)
  [index_option] ...`
- `ADD [CONSTRAINT [symbol]] UNIQUE [INDEX | KEY] [index_name]
  [index_type] (key_part, ...) [index_option] ...`
- `ADD {INDEX | KEY} [index_name] [index_type] (key_part, ...)
  [index_option] ...`
- `ADD {FULLTEXT | SPATIAL} [INDEX | KEY] [index_name] (key_part, ...)
  [index_option] ...`
- `DROP PRIMARY KEY`
- `DROP {INDEX | KEY} index_name`
- `RENAME {INDEX | KEY} old_index_name TO new_index_name`
- `ALTER INDEX index_name {VISIBLE | INVISIBLE}`
- `ADD [CONSTRAINT [symbol]] CHECK (expr) [[NOT] ENFORCED]`
- `DROP {CHECK | CONSTRAINT} symbol`
- `ALTER {CHECK | CONSTRAINT} symbol [NOT] ENFORCED`
- `ADD [CONSTRAINT [symbol]] FOREIGN KEY [index_name] (col_name, ...)
  reference_definition`
- `DROP FOREIGN KEY fk_symbol`
- multiple key, constraint, column, and table-option actions in one
  `ALTER TABLE` statement where this task's actions interact with Task 35
  column operations
- schema-qualified and selected-schema table resolution
- metadata updates in the internal column/index catalogs,
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, with `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
  exposed from the CHECK catalog and `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
  exposed from the foreign-key catalog for supported `CREATE TABLE` metadata
- duplicate validation, dependency validation, warning records, affected rows,
  diagnostics, statement atomicity, and implicit-commit boundaries

This task does not implement the complete `ALTER TABLE` statement. These
surfaces remain separate roadmap items or later slices:

- `RENAME TABLE` and `ALTER TABLE ... RENAME TO`
- column-only operations already covered by
  `docs/specs/alter-table-column-operations/specs.md`
- `ALTER COLUMN SET/DROP DEFAULT`, column visibility-only actions, table
  character-set conversion, table options, tablespace actions, and partition
  maintenance
- functional and multi-valued indexes
- optimizer use of physical indexes, index hints, `SHOW INDEX`, and
  `SHOW CREATE TABLE` formatting
- full-text search execution, spatial data/runtime semantics, and parser-plugin
  behavior
- check-expression evaluation beyond the expression subset implemented by
  MyLite at the time of the CHECK slice
- foreign-key enforcement, cascades, locks, generated-column interactions,
  triggers, privileges, replication, and binary logging
- remaining temporary-table DDL transaction exceptions

### First executable implementation slice

The first executable MyLite slice should be narrow but useful for common
application migrations:

- Parse the full Task 36 action surface listed above, plus `ALGORITHM` and
  `LOCK` table options already parsed for Task 35.
- Execute metadata-backed `ADD PRIMARY KEY`, `ADD UNIQUE`, `ADD INDEX` /
  `ADD KEY`, `DROP PRIMARY KEY`, `DROP INDEX` / `DROP KEY`,
  `RENAME INDEX` / `RENAME KEY`, and `ALTER INDEX VISIBLE` / `INVISIBLE`
  against supported MyLite base tables.
- Reuse the standalone `CREATE INDEX` and `DROP INDEX` validation and catalog
  machinery for key parts, prefix lengths, `ASC` / `DESC`, comments,
  visibility, duplicate-index warnings, existing-row unique validation, and
  write-path conflict participation.
- Integrate with the Task 35 shadow-rewrite table model when a primary-key
  operation changes column metadata, when key actions are mixed with column
  actions, or when validation must inspect the final candidate table shape.
- Accept absent or `DEFAULT` `ALGORITHM` / `LOCK` options for execution.
  Reject non-default `ALGORITHM` or `LOCK` values before mutation until MyLite
  has a general online-DDL compatibility contract for `ALTER TABLE`.
- Preserve statement atomicity across all catalog updates, physical table
  rewrites, optional auxiliary SQLite indexes, warnings, diagnostics, and
  affected-row state.
- Return deterministic unsupported diagnostics before mutation for
  `FULLTEXT` and `SPATIAL` until their dedicated catalog/runtime support
  lands.
- Keep unsupported future constraint options source-complete so later work can
  add catalogs and runtime behavior without changing the accepted parse tree.

The first slice should not use SQLite constraints or indexes as the semantic
source of truth. SQLite indexes can be optional performance artifacts only
after MyLite has validated MySQL names, dependencies, duplicate handling,
visibility, and diagnostics.

## Sources

- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `DROP INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-index.html
- MySQL 8.4 Reference Manual, invisible indexes:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-indexes.html
- MySQL 8.4 Reference Manual, CHECK constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-check-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
  table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, atomic DDL:
  https://dev.mysql.com/doc/refman/8.4/en/atomic-ddl.html
- Existing MyLite specs:
  - `docs/specs/alter-table-column-operations/specs.md`
  - `docs/specs/create-drop-index/specs.md`
  - `docs/specs/create-table-indexes/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-on-duplicate-key-update/specs.md`
  - `docs/specs/replace/specs.md`
  - `docs/specs/update-single-table/specs.md`

Observed behavior was verified against Docker container `mylite-mysql-849`
running MySQL `8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9`, version comment
`MySQL Community Server - GPL`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@foreign_key_checks = 1` before the foreign-key toggle probes.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Statement boundary and action ordering

`ALTER TABLE` accepts one or more alter specifications in source order. Key and
constraint actions can be mixed with column actions and table-level DDL
options. MySQL validates the complete operation and applies successful InnoDB
DDL atomically: metadata and storage effects become visible together, and a
failure leaves the table definition and data unchanged.

Ordinary non-temporary `ALTER TABLE` is DDL and has implicit commit boundaries.
MyLite commits any active explicit transaction before `ALTER TABLE` validation
and execution. Successful key and constraint mutations run in their own
statement transaction and leave no explicit transaction active. Validation
failures after the pre-DDL commit also leave the session outside the explicit
transaction; later writes therefore autocommit unless the client starts a new
transaction.

`ALGORITHM` and `LOCK` are statement options rather than key definitions.
MySQL accepts them with many key operations, but exact algorithm selection is
engine-specific. The first MyLite slice should reject non-default values before
mutation so callers do not infer unsupported online-DDL guarantees.

### Primary-key operations

`ADD PRIMARY KEY` creates the table's single primary-key index named
`PRIMARY`. A `CONSTRAINT symbol` clause is accepted, but observed metadata still
uses `PRIMARY` as the constraint and index name.

Adding a primary key:

- fails if the table already has a primary key
- validates that all key-part columns exist
- rejects existing `NULL` values
- rejects duplicate key tuples
- makes key-part column metadata `NOT NULL` silently when the primary key is
  accepted
- creates `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE` rows
- inserts the primary key at the start of MySQL's duplicate-key conflict order
- reports no result set and usually affected rows `0` for the simple verified
  shapes

Verified diagnostics:

| Condition | Code / SQLSTATE |
| --- | --- |
| duplicate existing key tuple while adding a primary key | 1062 / `23000` |
| existing `NULL` value in a primary-key part | 1138 / `22004` |
| second primary key | 1068 / `42000` |
| missing key-part column | 1072 / `42000` |

`DROP PRIMARY KEY` removes the primary-key index and constraint metadata. It
does not make primary-key columns nullable again; observed columns remained
`IS_NULLABLE='NO'` after the primary key was dropped. Dropping a primary key
that is required by an `AUTO_INCREMENT` column fails with error 1075. Future
foreign-key support must also reject dropping a primary or unique parent index
that is needed by a foreign-key relationship.

### Unique, ordinary, full-text, and spatial index addition

`ADD UNIQUE`, `ADD UNIQUE INDEX`, and `ADD UNIQUE KEY` create unique indexes.
If both `CONSTRAINT symbol` and an explicit `index_name` are supplied, observed
metadata uses the explicit index name. If `CONSTRAINT symbol` is supplied and
`index_name` is omitted, observed metadata uses the symbol as the unique index
name. If both are omitted, MySQL generates the index name from the first key
part, with suffixes as needed to avoid name collisions.

Unique indexes:

- reject duplicate non-`NULL` existing key tuples with 1062
- permit multiple `NULL` values in nullable unique key parts
- remain duplicate-enforcing when invisible
- participate in `INSERT`, ODKU, `REPLACE`, and `UPDATE` duplicate checks as
  soon as the DDL succeeds
- are represented as `NON_UNIQUE=0` rows in `INFORMATION_SCHEMA.STATISTICS`

`ADD INDEX` and `ADD KEY` create nonunique secondary indexes. An omitted index
name is generated from the first key part. Redundant indexes with different
names are accepted but produce warning 1831. Duplicate index names fail with
error 1061.

Key parts follow the currently supported MyLite identifier-key-part surface:
identifier column references, optional prefix length, and optional `ASC` or
`DESC`. MySQL reports descending parts with `STATISTICS.COLLATION='D'`,
ascending or omitted order with `COLLATION='A'`, and prefix lengths in
`SUB_PART`.

`ADD FULLTEXT` creates metadata-backed full-text indexes for supported
character and text columns. MySQL reports `INDEX_TYPE='FULLTEXT'`,
`COLLATION=NULL`, ignores prefix lengths in statistics metadata, preserves
`COMMENT` and `WITH PARSER` in `SHOW CREATE TABLE`, warns 124 when InnoDB adds
the hidden full-text document id for the table's first full-text index, rejects
non-text columns with 1283, rejects explicit key-part order with 1221, and
rejects multiple full-text creations in one ALTER with 1795. MyLite mirrors
that metadata and diagnostics while deferring parser-plugin tokenization and
`MATCH ... AGAINST` search behavior.

`SPATIAL` index additions remain valid MySQL syntax with storage and type
restrictions. Runtime probes observed `SPATIAL` metadata with
`INDEX_TYPE='SPATIAL'` over a `POINT NOT NULL SRID 0` column. MyLite still
parses spatial ALTER index forms but returns an unsupported diagnostic before
mutation until spatial types and runtime behavior exist.

### Index drop, rename, and visibility

`DROP INDEX index_name` and `DROP KEY index_name` inside `ALTER TABLE` remove
all key parts for the named index. Missing names fail with error 1091. Dropping
a unique index removes it from later duplicate-key conflict checks. Dropping an
index required by a foreign-key constraint fails with error 1553.

`RENAME INDEX old TO new` and `RENAME KEY old TO new` rename an existing
non-primary index without changing table rows or key parts. The old index name
must exist, the new name must not duplicate any index name in the final table
shape, and neither name can be `PRIMARY`. Unquoted `PRIMARY` in the rename
syntax is a parse error because `PRIMARY` is reserved in that position.

`ALTER INDEX name VISIBLE` and `ALTER INDEX name INVISIBLE` change optimizer
visibility metadata for indexes other than explicit or implicit primary-key
indexes. Visibility does not affect duplicate-key enforcement. A unique
`NOT NULL` index can act as an implicit primary key when no explicit primary
key exists; making it invisible fails with error 3522 until another explicit
primary key exists.

### CHECK constraints

`ADD CHECK` creates a named or generated CHECK constraint. Omitted names are
generated from the next available `<table>_chk_<n>` suffix among existing
generated-pattern names; explicit CHECK names do not advance the generated
counter. MySQL requires CHECK constraint names to be unique per schema. Runtime
probes observed duplicate CHECK names failing with error 3822, even on
different base tables in the same schema.

An enforced CHECK constraint accepts rows for which the expression evaluates to
`TRUE` or `UNKNOWN` and rejects rows for which it evaluates to `FALSE`.
Adding an enforced CHECK to a populated table scans existing rows; if any row
violates the condition, the DDL fails with error 3819 and no constraint
metadata is added. Adding the same expression as `NOT ENFORCED` succeeds and
records metadata without enforcing existing or future rows until it is made
enforced.

`ALTER CHECK symbol ENFORCED` validates existing rows before changing the
constraint to enforced. `ALTER CHECK symbol NOT ENFORCED` is metadata-only for
the verified simple shapes. `DROP CHECK symbol` and `DROP CONSTRAINT symbol`
remove CHECK metadata. Missing CHECK names fail with error 3821.

CHECK metadata appears in `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, including the enforcement flag in
`TABLE_CONSTRAINTS.ENFORCED`.

MySQL allows only deterministic, row-local CHECK expressions. It rejects
subqueries, variables, stored functions, loadable functions, references to
other tables, and `AUTO_INCREMENT` columns in CHECK expressions. The expression
is evaluated under the SQL mode active at evaluation time.

### Foreign-key operations

`ADD FOREIGN KEY` defines a child-table constraint referencing a parent table.
The full MySQL surface includes optional constraint names, optional supporting
index names, child-column lists, referenced table and column lists, and
`ON DELETE` / `ON UPDATE` actions `RESTRICT`, `CASCADE`, `SET NULL`,
`NO ACTION`, and syntactically accepted `SET DEFAULT`.

Naming has two distinct surfaces:

- `CONSTRAINT symbol` names the foreign-key constraint.
- The optional name after `FOREIGN KEY` names or influences the supporting
  child index, but MySQL ignores it as the constraint name when `CONSTRAINT`
  is omitted.

Runtime probes observed:

- `ADD FOREIGN KEY fk_named_idx (pid) REFERENCES parent(id)` created a child
  index named `fk_named_idx` and a generated constraint named
  `child_named_ibfk_1`.
- `ADD CONSTRAINT fk_pid FOREIGN KEY fk_pid_idx (pid) ...` used `fk_pid` as
  both the constraint name and the supporting child index name for the
  verified shape.
- adding a valid foreign key over two existing child rows reported two
  affected rows.
- adding a foreign key with existing child rows that have no parent match
  failed with error 1452 when `foreign_key_checks=1`.
- with `foreign_key_checks=0`, MySQL accepted an otherwise inconsistent
  foreign key without scanning existing rows; re-enabling checks did not
  retroactively validate old rows, but later bad inserts failed.
- `DROP FOREIGN KEY symbol` removed only the foreign-key constraint; the
  supporting child index remained.
- `DROP INDEX supporting_index` while the foreign key existed failed with
  error 1553.
- dropping the referenced parent `PRIMARY` or unique index while the foreign
  key existed also failed with error 1553, including with
  `foreign_key_checks=0`.
- missing foreign-key names failed with error 1091.

Foreign keys require compatible child and parent column definitions, usable
indexes, and storage-engine support. MyLite implements FK-only ADD/DROP shapes
using the foreign-key catalog and existing DML enforcement. MyLite also rejects
dropping child supporting indexes and parent primary/unique indexes recorded in
the foreign-key catalog. Mixed FK actions with supported column/index actions,
broader table/rename/truncate/column dependency checks, and recursive
referential behavior remain deferred.

## MyLite behavior

### Parser and AST

Extend the existing `ALTER TABLE` statement node from Task 35 with key and
constraint action nodes. The AST should retain:

1. target table name
2. ordered alter-item list
3. source span for every item
4. statement-scoped DDL options in source order

Key action nodes:

- `ADD_PRIMARY_KEY`: optional constraint symbol, key parts, index type, and
  index options
- `DROP_PRIMARY_KEY`
- `ADD_UNIQUE_INDEX`: optional constraint symbol, optional index name, index
  type, key parts, and index options
- `ADD_SECONDARY_INDEX`: optional index name, index type, key parts, and index
  options
- `ADD_FULLTEXT_INDEX`: optional index name, key parts, full-text options, and
  ordinary index options for later validation
- `ADD_SPATIAL_INDEX`: optional index name, key parts, and index options
- `DROP_INDEX`: spelling kind (`INDEX` or `KEY`) and target index identifier
- `RENAME_INDEX`: spelling kind, old identifier, and new identifier
- `ALTER_INDEX_VISIBILITY`: target index identifier and requested visibility

Constraint action nodes:

- `ADD_CHECK`: optional constraint symbol, expression, and enforcement state
- `DROP_CHECK_OR_CONSTRAINT`: spelling kind and target constraint symbol
- `ALTER_CHECK_OR_CONSTRAINT`: spelling kind, target symbol, and enforcement
  state
- `ADD_FOREIGN_KEY`: optional constraint symbol, optional supporting index
  name, child column list, reference definition, and reference actions
- `DROP_FOREIGN_KEY`: target foreign-key symbol

The parser should accept MySQL-valid syntax even when runtime execution is
deferred. Semantic analysis, not parsing, detects missing tables, missing
columns, duplicate names, unsupported runtime classes, data violations, and
dependency conflicts.

Malformed list shapes remain parse errors:

- empty action list
- trailing comma
- empty key-part or column lists
- missing `TO` in `RENAME INDEX` / `RENAME KEY`
- missing enforcement keyword after `ALTER CHECK symbol`
- malformed `REFERENCES` clause
- `DROP KEY` outside `ALTER TABLE` as a standalone statement

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature. They are
not copied from MySQL; they are the MyLite AST-oriented grammar shape.

```lemon
alter_table_statement ::= ALTER TABLE table_name alter_table_item_list.

alter_table_item_list ::= alter_table_item.
alter_table_item_list ::= alter_table_item_list COMMA alter_table_item.

alter_table_item ::= alter_table_column_action.
alter_table_item ::= alter_table_key_action.
alter_table_item ::= alter_table_constraint_action.
alter_table_item ::= alter_table_option.

alter_table_key_action ::= ADD opt_constraint_symbol PRIMARY KEY
                           opt_index_type LPAREN key_part_list RPAREN
                           alter_index_option_list.
alter_table_key_action ::= DROP PRIMARY KEY.

alter_table_key_action ::= ADD opt_constraint_symbol UNIQUE opt_index_or_key
                           opt_identifier opt_index_type
                           LPAREN key_part_list RPAREN
                           alter_index_option_list.
alter_table_key_action ::= ADD index_or_key opt_identifier opt_index_type
                           LPAREN key_part_list RPAREN
                           alter_index_option_list.
alter_table_key_action ::= ADD fulltext_or_spatial opt_index_or_key
                           opt_identifier LPAREN key_part_list RPAREN
                           alter_index_option_list.

alter_table_key_action ::= DROP index_or_key identifier.
alter_table_key_action ::= RENAME index_or_key identifier TO identifier.
alter_table_key_action ::= ALTER INDEX identifier index_visibility.

alter_table_constraint_action ::= ADD opt_constraint_symbol CHECK
                                  LPAREN expression RPAREN
                                  opt_check_enforcement.
alter_table_constraint_action ::= DROP check_or_constraint identifier.
alter_table_constraint_action ::= ALTER check_or_constraint identifier
                                  check_enforcement.
alter_table_constraint_action ::= ADD opt_constraint_symbol FOREIGN KEY
                                  opt_identifier
                                  LPAREN identifier_list RPAREN
                                  reference_definition.
alter_table_constraint_action ::= DROP FOREIGN KEY identifier.

opt_constraint_symbol ::= .
opt_constraint_symbol ::= CONSTRAINT.
opt_constraint_symbol ::= CONSTRAINT identifier.

opt_index_or_key ::= .
opt_index_or_key ::= INDEX.
opt_index_or_key ::= KEY.

index_or_key ::= INDEX.
index_or_key ::= KEY.

fulltext_or_spatial ::= FULLTEXT.
fulltext_or_spatial ::= SPATIAL.

check_or_constraint ::= CHECK.
check_or_constraint ::= CONSTRAINT.

opt_identifier ::= .
opt_identifier ::= identifier.

key_part_list ::= key_part.
key_part_list ::= key_part_list COMMA key_part.

key_part ::= identifier opt_key_part_prefix opt_key_part_order.

opt_key_part_prefix ::= .
opt_key_part_prefix ::= LPAREN INTEGER RPAREN.

opt_key_part_order ::= .
opt_key_part_order ::= ASC.
opt_key_part_order ::= DESC.

alter_index_option_list ::= .
alter_index_option_list ::= alter_index_option_list alter_index_option.

alter_index_option ::= index_type.
alter_index_option ::= KEY_BLOCK_SIZE INTEGER.
alter_index_option ::= KEY_BLOCK_SIZE EQ INTEGER.
alter_index_option ::= COMMENT STRING.
alter_index_option ::= VISIBLE.
alter_index_option ::= INVISIBLE.
alter_index_option ::= ENGINE_ATTRIBUTE STRING.
alter_index_option ::= ENGINE_ATTRIBUTE EQ STRING.
alter_index_option ::= SECONDARY_ENGINE_ATTRIBUTE STRING.
alter_index_option ::= SECONDARY_ENGINE_ATTRIBUTE EQ STRING.
alter_index_option ::= WITH PARSER identifier.

opt_index_type ::= .
opt_index_type ::= index_type.

index_type ::= USING index_algorithm.
index_type ::= TYPE index_algorithm.

index_algorithm ::= BTREE.
index_algorithm ::= HASH.

index_visibility ::= VISIBLE.
index_visibility ::= INVISIBLE.

opt_check_enforcement ::= .
opt_check_enforcement ::= check_enforcement.

check_enforcement ::= ENFORCED.
check_enforcement ::= NOT ENFORCED.

reference_definition ::= REFERENCES table_name LPAREN identifier_list RPAREN
                         reference_option_list.

reference_option_list ::= .
reference_option_list ::= reference_option_list reference_option.

reference_option ::= ON DELETE reference_action.
reference_option ::= ON UPDATE reference_action.
reference_option ::= MATCH reference_match_kind.

reference_action ::= RESTRICT.
reference_action ::= CASCADE.
reference_action ::= SET NULL.
reference_action ::= NO ACTION.
reference_action ::= SET DEFAULT.

reference_match_kind ::= SIMPLE.
reference_match_kind ::= FULL.
reference_match_kind ::= PARTIAL.

identifier_list ::= identifier.
identifier_list ::= identifier_list COMMA identifier.

alter_table_option ::= ALGORITHM opt_equal ddl_algorithm.
alter_table_option ::= LOCK opt_equal ddl_lock.

ddl_algorithm ::= DEFAULT.
ddl_algorithm ::= INSTANT.
ddl_algorithm ::= INPLACE.
ddl_algorithm ::= COPY.

ddl_lock ::= DEFAULT.
ddl_lock ::= NONE.
ddl_lock ::= SHARED.
ddl_lock ::= EXCLUSIVE.
```

Deferred MySQL-valid key-part shapes should be explicit parse or runtime
deferrals, depending on the parser's expression readiness:

```lemon
/* Deferred: functional and multi-valued key parts. */
key_part ::= LPAREN expression RPAREN.
key_part ::= LPAREN CAST LPAREN expression AS type_name ARRAY RPAREN RPAREN.
```

### Analyzer and validation

Validation should use a candidate in-memory table model loaded from MyLite
catalogs:

1. Resolve the target schema and table using existing DDL/DML rules.
2. Reject system schemas and non-base-table targets.
3. Load current column metadata ordered by ordinal position.
4. Load current index metadata ordered by index catalog order and key-part
   sequence.
5. Load CHECK and foreign-key metadata when those catalogs exist.
6. Collect `ALGORITHM` and `LOCK` options. Reject non-default values before
   any mutation in the first executable slice.
7. Apply each action to the candidate model in source order.
8. Validate names case-insensitively for tables, columns, and indexes while
   preserving authored spelling for metadata display.
9. Validate primary-key, unique-key, check, and foreign-key constraints against
   existing rows where the executed feature requires it.
10. Build a plan for catalog deletes/inserts, column metadata rewrites,
    optional physical shadow rewrites, warning records, and affected rows.
11. Execute the plan atomically.

Primary-key validation:

- reject a second primary key
- require at least one key part
- reject missing key-part columns
- reject duplicate key-part column names inside one key
- reject existing `NULL` values with MySQL-compatible diagnostics
- reject duplicate existing key tuples
- change key-part column metadata to `NOT NULL` on success
- reject `PRIMARY` as a name for any non-primary index
- preserve existing `NOT NULL` metadata after `DROP PRIMARY KEY`
- reject dropping the primary key when it is required by `AUTO_INCREMENT`
- reserve dependency checks for foreign keys, generated columns, and future
  generated invisible primary keys

Unique and secondary index validation:

- generate omitted names from the first key part, using MySQL-compatible
  suffixes when needed
- if `CONSTRAINT symbol` and `index_name` are both present for a unique index,
  use the explicit index name for current MySQL compatibility
- reject duplicate index names in the final table model with 1061
- reject missing key-part columns with 1072
- use standalone `CREATE INDEX` semantics for prefix length, ordering,
  comments, visibility, engine attributes, duplicate-index warning 1831, and
  HASH fallback warning 3502 where applicable
- validate existing rows before adding a unique index
- treat unique keys with any `NULL` part as non-conflicting
- preserve primary and unique index catalog order for ODKU conflict selection
  and REPLACE delete loops
- make invisible unique indexes continue to participate in duplicate checks

Index drop, rename, and visibility validation:

- `DROP INDEX` and `DROP KEY` require an existing index name
- `DROP INDEX` cannot drop an index required by a foreign-key constraint once
  foreign-key metadata exists
- `RENAME INDEX` and `RENAME KEY` require an existing non-primary old name and
  a non-duplicate non-primary new name
- renaming an index preserves its catalog identity and conflict order
- `ALTER INDEX INVISIBLE` rejects explicit primary keys and unique
  `NOT NULL` indexes acting as implicit primary keys
- visibility changes update metadata only and never disable uniqueness

CHECK validation:

- generate omitted names as `table_chk_N`, using MySQL-compatible numbering
- enforce schema-level name uniqueness for CHECK constraints
- validate expression shape against MySQL CHECK restrictions
- for enforced additions and `ALTER CHECK ... ENFORCED`, scan existing rows
  before metadata mutation
- treat `TRUE` and `UNKNOWN` as passing and `FALSE` as failing
- preserve `NOT ENFORCED` constraints as metadata that does not block writes
- reject missing CHECK names with 3821
- reject duplicate CHECK names with 3822
- reject ambiguous `DROP CONSTRAINT` or `ALTER CONSTRAINT` names when different
  constraint classes share the same symbol
- defer CHECK catalog changes for mixed column/index/table-option ALTER
  statements until after the shadow-table rewrite, so validation sees the final
  candidate row shape and rollback restores all same-statement changes
- report copied rows for mixed enforced `ADD CHECK` statements, while
  preserving metadata-only affected rows for mixed `NOT ENFORCED` additions and
  `DROP CHECK`

Foreign-key validation, once executable:

- resolve child and parent tables, including schema-qualified references
- require matching child and parent column counts
- require compatible data types, signs, charsets, and collations
- require usable child and parent indexes, creating a child index when MySQL
  would do so
- reject prefix indexes, unsupported generated-column references, unsupported
  partitioned-table surfaces, and storage-engine mismatches
- scan existing child rows when `foreign_key_checks=1`
- skip the existing-row scan when `foreign_key_checks=0`, while still
  validating the definition shape
- enforce future inserts, updates, deletes, and parent-key updates once checks
  are enabled
- preserve supporting child indexes after `DROP FOREIGN KEY`
- reject dropping child supporting indexes and parent primary/unique indexes
  with 1553 while dependent foreign keys exist

### Runtime and storage design

The first executable slice should reuse the Task 35 table-candidate model and
the Task 34 standalone index executor:

1. Build a final candidate table model by applying column and key actions in
   source order.
2. Decide whether a physical shadow rewrite is needed. Adding or dropping
   ordinary secondary index metadata usually does not require row copying;
   adding a primary key changes column nullability metadata and may need the
   Task 35 rewrite path if mixed with column actions.
3. Validate existing rows for new primary and unique constraints before
   catalog mutation.
4. Delete and reinsert index catalog rows in final catalog order, compacting
   key-part sequence numbers.
5. Rewrite column catalog rows when primary-key addition changes nullability or
   when mixed column actions already require a rewrite.
6. Update table auto-increment metadata only through existing
   primary-key/auto-increment rules.
7. Create or drop optional auxiliary SQLite indexes after MyLite metadata has
   decided the final index set. These indexes are optimization artifacts only.
8. Commit the statement-owned transaction or savepoint only after all catalog,
   storage, warning, and statement-state effects succeed.

MyLite must not rely on SQLite unique constraints to produce user-visible
duplicate diagnostics. MyLite owns duplicate checking, conflict order,
nullable unique semantics, prefix comparisons, collation behavior, visibility,
and warning state.

### Catalog interactions

`__mylite_index_catalog`:

- insert rows for primary, unique, and nonunique `ALTER TABLE` index additions
- remove rows for dropped indexes and dropped primary keys
- update index names for successful renames
- update visibility metadata for `ALTER INDEX`
- preserve prefix length, order marker, comment, effective index type,
  explicit `USING BTREE` display metadata, visibility, uniqueness, and catalog
  order
- mark `PRIMARY` as the only valid primary-key index name

`__mylite_column_catalog`:

- set primary-key columns to `NOT NULL` when `ADD PRIMARY KEY` succeeds
- keep those columns `NOT NULL` after `DROP PRIMARY KEY`
- update column key markers exposed by `INFORMATION_SCHEMA.COLUMNS`
- coordinate with Task 35 column rewrites when key actions and column actions
  are mixed

Constraint catalogs:

- `__mylite_check_constraint_catalog` stores schema, table, constraint name,
  expression text, enforcement flag, and creation order for supported CHECK
  DDL.
- `__mylite_foreign_key_catalog` stores one row per child column part,
  including schema, child table, constraint name, supporting index name,
  parent schema/table, child and referenced column names, ordinal positions,
  action rules, and match kind for table-level `CREATE TABLE` foreign keys.

Information schema:

- `INFORMATION_SCHEMA.STATISTICS` should reflect all supported index changes
  immediately.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes primary and unique constraints
  from `__mylite_index_catalog`, CHECK rows from the CHECK catalog, and
  table-level `CREATE TABLE` foreign-key rows from the foreign-key catalog.
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes primary and unique key parts
  from `__mylite_index_catalog` and table-level `CREATE TABLE` foreign-key
  child-column rows from the foreign-key catalog.
- `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` exposes CHECK names and clauses from
  the CHECK catalog.
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` exposes table-level
  `CREATE TABLE` foreign-key rows from the foreign-key catalog. ALTER
  foreign-key rows should be added with the ALTER ADD FOREIGN KEY slice.

### Affected rows, diagnostics, and warnings

Successful key and constraint actions produce no result set. Runtime probes
showed:

- simple `ADD PRIMARY KEY`, `ADD INDEX`, `ADD UNIQUE`, `DROP PRIMARY KEY`,
  `DROP INDEX`, `RENAME INDEX`, `ALTER INDEX`, `DROP CHECK`, and
  `DROP FOREIGN KEY` shapes commonly reported affected rows `0`
- adding an enforced CHECK to a two-row table reported affected rows `2`
- adding a foreign key over two existing child rows reported affected rows `2`
- protocol OK details such as `Records`, `Duplicates`, and `Warnings` vary by
  operation and should be treated as protocol-layer work until MyLite exposes
  full OK packet formatting

Required diagnostics and warnings for the first executable and specified
deferred surfaces:

| Condition | MySQL code / SQLSTATE |
| --- | --- |
| no selected schema for unqualified target | 1046 / `3D000` |
| missing schema | 1049 / `42000` |
| missing table | 1146 / `42S02` |
| system schema target | 1044 / `42000` |
| duplicate index name | 1061 / `42000` |
| duplicate key values for primary/unique addition | 1062 / `23000` |
| second primary key | 1068 / `42000` |
| missing key-part column | 1072 / `42000` |
| invalid `AUTO_INCREMENT` key shape or primary-key drop | 1075 / `42000` |
| existing `NULL` in added primary key | 1138 / `22004` |
| missing dropped index/key/foreign key | 1091 / `42000` |
| dropping index needed by a foreign key | 1553 / `HY000` |
| non-text column in a full-text index | 1283 / `HY000` |
| multiple full-text index creations in one ALTER | 1795 / `HY000` |
| invisible explicit or implicit primary key | 3522 / `HY000` |
| CHECK violation | 3819 / `HY000` |
| missing CHECK constraint | 3821 / `HY000` |
| duplicate CHECK constraint name | 3822 / `HY000` |
| foreign-key existing-row violation | 1452 / `23000` |
| duplicate-index warning | warning 1831 |
| HASH index fallback note | note 3502 |
| InnoDB full-text hidden-column warning | warning 124 |

Warnings belong to statement diagnostics and must be inspectable immediately
after the `ALTER TABLE` statement. A later statement may overwrite the warning
area, matching existing MyLite diagnostic behavior.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations. Test
fixtures should create and drop isolated schemas.

### Primary keys

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE pk_add ADD PRIMARY KEY (a)` over distinct non-`NULL` values | Succeeds; `STATISTICS` has `PRIMARY`; `COLUMNS.IS_NULLABLE='NO'`; affected rows `0` for the verified shape. |
| `ADD CONSTRAINT pk_symbol PRIMARY KEY (a)` | Succeeds; metadata names the key and constraint `PRIMARY`. |
| `ADD PRIMARY KEY (a)` over duplicate values | Error 1062; no index metadata is added. |
| `ADD PRIMARY KEY (a)` when any existing `a` is `NULL` | Error 1138; no index metadata is added. |
| adding a second primary key | Error 1068; existing primary key remains unchanged. |
| missing primary-key column | Error 1072; no mutation. |
| `DROP PRIMARY KEY` on a simple primary key | Succeeds; primary-key statistics rows disappear; former key columns remain `NOT NULL`. |
| `DROP PRIMARY KEY` on an `AUTO_INCREMENT PRIMARY KEY` | Error 1075; primary key remains. |

### Unique and secondary indexes

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE t ADD UNIQUE uq_c (c(3)) INVISIBLE` | Succeeds; `STATISTICS.NON_UNIQUE=0`, `SUB_PART=3`, and `IS_VISIBLE='NO'`; later duplicate non-`NULL` prefixes fail. |
| nullable unique index with multiple `NULL` values | Succeeds; later `NULL` inserts remain non-conflicting. |
| `ADD CONSTRAINT uq_symbol UNIQUE KEY uq_index_name (b)` | Succeeds; metadata uses `uq_index_name`. |
| `ADD CONSTRAINT uq_symbol2 UNIQUE (c)` | Succeeds; metadata uses `uq_symbol2`. |
| `ADD UNIQUE (d)` | Succeeds; generated index name starts from first key part `d`. |
| `ADD INDEX (a)` | Succeeds; generated index name starts from first key part `a`. |
| `ADD KEY named_b (b DESC)` | Succeeds; `STATISTICS.COLLATION='D'`. |
| adding an index duplicate by shape with a different name | Succeeds with warning 1831. |
| adding an index with an existing name | Error 1061; no mutation. |
| `ADD FULLTEXT INDEX ft_body (body) WITH PARSER ngram COMMENT 'ft'` | Succeeds for text columns; `STATISTICS.INDEX_TYPE='FULLTEXT'`, `COLLATION=NULL`, `SUB_PART=NULL`; `SHOW INDEX` and `SHOW CREATE TABLE` expose parser/comment metadata; first full-text index warns 124. |
| adding a second full-text index in a later statement | Succeeds without warning 124 when the table already has full-text metadata. |
| `ADD FULLTEXT INDEX ft_n (n)` over an integer column | Error 1283; no index metadata is added. |
| `ADD FULLTEXT INDEX ft_body (body ASC)` | Error 1221; no index metadata is added. |
| adding two full-text indexes in one ALTER | Error 1795; no index metadata is added. |
| `ADD SPATIAL INDEX sp_g (g)` over `POINT NOT NULL SRID 0` | MySQL succeeds with `INDEX_TYPE='SPATIAL'`; MyLite first slice returns unsupported before mutation. |

### Drop, rename, and visibility

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE t DROP KEY a` | Succeeds when index `a` exists; statistics rows disappear; unique conflict checks no longer use it. |
| `ALTER TABLE t DROP INDEX missing_idx` | Error 1091; no mutation. |
| `ALTER TABLE t RENAME INDEX old_name TO new_name` | Succeeds; key parts and data are unchanged; metadata uses the new name. |
| `ALTER TABLE t RENAME KEY old_name TO duplicate_name` | Error 1061; original name remains. |
| `ALTER TABLE t RENAME INDEX PRIMARY TO p2` | Syntax error in MySQL for unquoted `PRIMARY`; quoted primary names must still be rejected semantically. |
| `ALTER TABLE t ALTER INDEX uq_c VISIBLE` | Succeeds; uniqueness remains enforced. |
| making an explicit primary key invisible | Error 3522. |
| making the only unique `NOT NULL` index invisible on a table without an explicit primary key | Error 3522. |
| after adding an explicit primary key, making that unique index invisible | Succeeds; `STATISTICS.IS_VISIBLE='NO'`. |

### CHECK constraints

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE chk_valid ADD CHECK (a > 0)` over rows `1` and `NULL` | Succeeds, generates `chk_valid_chk_1`, and reports affected rows 0. |
| adding an enforced CHECK when an existing row evaluates `FALSE` | Error 3819; no CHECK metadata is added. |
| `ADD CONSTRAINT chk_a_pos_ne CHECK (a > 0) NOT ENFORCED` over invalid rows | Succeeds; `TABLE_CONSTRAINTS.ENFORCED='NO'`. |
| `ALTER CHECK chk_a_pos_ne ENFORCED` while invalid rows exist | Error 3819; enforcement flag remains `NO`. |
| `ALTER CHECK chk_a_pos_ne NOT ENFORCED` | Succeeds for an existing CHECK. |
| `DROP CHECK chk_a_pos_ne` | Succeeds; CHECK metadata disappears. |
| `DROP CONSTRAINT chk_valid_chk_1` for a CHECK | Succeeds; CHECK metadata disappears. |
| `DROP CHECK missing_chk` | Error 3821. |
| duplicate CHECK name in the same schema | Error 3822. |
| enforced CHECK violation on `INSERT` | Error 3819; row is not inserted. |
| enforced CHECK violation on `INSERT IGNORE` | Warning 3819; offending row is skipped. |
| `ADD COLUMN b INT DEFAULT 1, ADD CHECK (b > 0)` over two rows | Succeeds atomically and reports affected rows 2. |
| `ADD COLUMN b INT DEFAULT 1, ADD CHECK (a > 0)` over an invalid existing row | Error 3819; neither the column nor CHECK metadata survives. |
| `ADD INDEX i (a), ADD CHECK (a > 0)` over two valid rows | Succeeds atomically and reports affected rows 2. |
| `ADD CHECK (a > 0) NOT ENFORCED` mixed with an instant column action | Succeeds and reports affected rows 0 for the verified shape. |

### Foreign keys

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ADD CONSTRAINT fk_pid FOREIGN KEY fk_pid_idx (pid) REFERENCES parent(id)` over matching rows | Succeeds for FK-only ALTER, reports the child table row count as affected rows, and records `fk_pid` as the FK constraint. |
| `ADD FOREIGN KEY fk_named_idx (pid) REFERENCES parent(id)` | Creates supporting index `fk_named_idx` when no existing prefix index can be reused and records generated constraint `child_named_ibfk_1`. |
| adding a foreign key with unmatched existing child rows while `foreign_key_checks=1` | Error 1452; no foreign-key metadata is added. |
| adding the same shape while `foreign_key_checks=0` | MySQL accepts without scanning old rows; future bad writes fail after checks are re-enabled. |
| `DROP INDEX fk_named_idx` while the FK depends on it | Error 1553; FK and index remain. |
| `DROP FOREIGN KEY child_named_ibfk_1` | Succeeds; supporting child index remains. |
| `DROP FOREIGN KEY missing_fk` | Error 1091. |

### Mixed actions and atomicity

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE t ADD INDEX i (a), ADD UNIQUE uq (b)` | Both indexes appear together, or neither appears if any later validation fails. |
| adding a primary key and renaming a column in one statement | Validation uses the source-order candidate model and commits column/index metadata together. |
| dropping a column and adding an index on the dropped column in one statement | Fails before mutation with the MySQL-compatible missing-column diagnostic for the final candidate action. |
| creating an index then renaming it in the same statement | The final metadata contains the renamed index if MySQL accepts the exact source-order shape; tests should pin the verified behavior before implementation. |
| unsupported `SPATIAL` action mixed with supported index actions | The whole statement fails before mutation; no partial index or column changes survive. |
| supported `ADD FULLTEXT` mixed with supported column/index actions | The full statement commits atomically, or rolls back if a later supported action fails validation. |
| non-default `ALGORITHM` or `LOCK` in the first MyLite slice | Fails before mutation with a deterministic unsupported diagnostic. |

## Test plan

- Parser tests:
  - every key and constraint action listed in scope
  - optional `CONSTRAINT`, `INDEX`, `KEY`, and omitted-name forms
  - `FULLTEXT`, `SPATIAL`, `WITH PARSER`, visibility, comments, prefix
    lengths, and order markers
  - `DROP CHECK`, `DROP CONSTRAINT`, `ALTER CHECK`, `ALTER CONSTRAINT`,
    `DROP FOREIGN KEY`, `CREATE TABLE` inline `REFERENCES`, `CREATE TABLE`
    table-level `FOREIGN KEY`, and `reference_definition` actions
  - malformed empty lists, trailing commas, missing `TO`, missing
    enforcement keywords, and invalid standalone `DROP KEY`
- Analyzer tests:
  - selected-schema and schema-qualified resolution
  - system-schema access-denied diagnostics
  - duplicate and generated index names
  - missing key columns
  - duplicate and `NULL` primary-key validation
  - unique duplicate validation with nullable parts
  - primary-key/auto-increment rejection
  - invisible primary and implicit-primary rejection
  - statement-order candidate models for mixed column and key actions
  - unsupported placeholder actions failing before mutation
- Runtime comparison tests:
  - `INFORMATION_SCHEMA.STATISTICS` rows after add, drop, rename, and
    visibility changes
  - `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and `KEY_COLUMN_USAGE` rows after
    primary/unique add, drop, and rename operations
  - `INFORMATION_SCHEMA.COLUMNS` nullability and key markers after primary-key
    changes
  - DML duplicate behavior after adding and dropping unique indexes
  - ODKU conflict target selection after source-order primary/unique changes
  - REPLACE delete loops after source-order primary/unique changes
  - warning 1831 for redundant indexes
  - warning/note 3502 for HASH fallback if inherited from standalone
    `CREATE INDEX`
  - exact errors listed in the diagnostics table
  - transaction/savepoint rollback of failed multi-action DDL
- Future CHECK runtime tests:
  - generated names and schema-level duplicate names
  - enforced versus not enforced additions
  - enforcement toggles over existing invalid rows
  - `INSERT`, `INSERT IGNORE`, `UPDATE`, `UPDATE IGNORE`, and `REPLACE`
    interactions
  - expression restrictions and SQL-mode-sensitive evaluation
- Future foreign-key runtime tests:
  - constraint/index naming split
  - compatible and incompatible type definitions
  - parent and child index requirements
  - existing-row validation with `foreign_key_checks=1`
  - no retroactive scan when checks are re-enabled after `0`
  - drop FK preserving supporting indexes
  - drop supporting index rejection
  - referential actions, cascades, and dependency checks across `DROP TABLE`,
    `ALTER TABLE`, `UPDATE`, `DELETE`, and `REPLACE`

## Implementation handoff

Recommended implementation order:

1. Extend the Task 35 `ALTER TABLE` parser/AST for key and constraint actions,
   preserving source spans and unsupported nodes.
2. Add first-slice analyzer support for metadata-backed primary, unique, and
   nonunique index actions using the standalone index validators.
3. Integrate primary-key nullability changes with the Task 35 candidate table
   and shadow-rewrite plan.
4. Add atomic catalog mutation for add/drop/rename/visibility operations.
5. Wire primary/unique catalog order into existing duplicate-key-sensitive DML.
6. Add deterministic unsupported diagnostics for `FULLTEXT` and `SPATIAL`
   execution until those runtime surfaces are implemented.
7. Add parser and runtime comparison tests before marking any row supported.

Keep CHECK and foreign-key rows marked only for the surfaces whose catalogs,
runtime validation, metadata exposure, and MySQL-runtime comparison tests are
implemented end to end.
