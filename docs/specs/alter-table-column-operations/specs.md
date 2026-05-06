# ALTER TABLE column operations

## Scope

This feature specifies MyLite support for the column-changing subset of
`ALTER TABLE`:

- `ALTER TABLE table_name ADD [COLUMN] column_definition [FIRST | AFTER col]`
- `ALTER TABLE table_name DROP [COLUMN] column_name`
- `ALTER TABLE table_name RENAME COLUMN old_name TO new_name`
- `ALTER TABLE table_name CHANGE [COLUMN] old_name new_name column_definition
  [FIRST | AFTER col]`
- `ALTER TABLE table_name MODIFY [COLUMN] column_definition
  [FIRST | AFTER col]`
- multiple column operations in one `ALTER TABLE` statement
- schema-qualified and selected-schema table resolution
- column type, nullability, default, comment, visibility, and
  `AUTO_INCREMENT` interactions where they affect column actions
- metadata changes in `__mylite_column_catalog`, `__mylite_index_catalog`,
  `INFORMATION_SCHEMA.COLUMNS`, and `INFORMATION_SCHEMA.STATISTICS`
- physical table rewrite, statement atomicity, affected rows, diagnostics, and
  warning records
- dependency handling for existing primary, unique, and secondary index
  metadata, with reserved integration points for generated columns, check
  constraints, triggers, and foreign keys

The full MySQL feature surface is broader than this task. The following remain
separate roadmap features or later slices:

- `ALTER TABLE ... ADD/DROP/RENAME/ALTER INDEX`, `ADD/DROP PRIMARY KEY`,
  `ADD/DROP UNIQUE`, and other key or constraint actions from Task 36
- `ALTER TABLE ... RENAME TO` from Task 37
- `ALTER COLUMN SET DEFAULT`, `ALTER COLUMN DROP DEFAULT`,
  `ALTER COLUMN SET VISIBLE`, and `ALTER COLUMN SET INVISIBLE`
- table options such as `CONVERT TO CHARACTER SET`, `DEFAULT CHARACTER SET`,
  `ORDER BY`, `FORCE`, tablespace actions, and partition maintenance
- full `ALGORITHM` and `LOCK` compatibility
- generated-column execution, check constraints, triggers, and foreign keys
  until those catalog/runtime features exist
- temporary tables, privileges, metadata locks, replication, binary logging,
  online DDL progress reporting, and storage-engine-specific optimizer effects

### First executable implementation slice

The first MyLite runtime slice should execute common application migrations
without absorbing the later key/constraint task:

- Parse the full column-action syntax listed above, including optional
  `COLUMN`, `FIRST`, `AFTER`, multiple actions, and `ALGORITHM`/`LOCK` clauses.
- Execute actions against supported MyLite base tables only.
- Support `ADD`, `CHANGE`, and `MODIFY` column definitions over the column type
  and attribute subset already executable for `CREATE TABLE`: current numeric,
  string/binary, temporal, nullability, supported defaults, comments, and
  visibility.
- Support ordinary physical column addition, removal, rename, type/attribute
  replacement, and ordinal repositioning.
- Backfill existing rows for added columns from explicit defaults, `NULL`, or
  MySQL-compatible implicit type defaults when a `NOT NULL` column has no
  explicit default.
- Rebuild rows for `CHANGE` and `MODIFY` through the same conversion and
  validation path used by supported inserts/updates. If a conversion path is
  not implemented yet, reject the whole `ALTER TABLE` before mutation with a
  deterministic unsupported diagnostic.
- Update existing primary, unique, and secondary index metadata when dropping
  or renaming indexed columns. Dropping an indexed column removes that key part;
  an index with no remaining key parts is removed.
- Preserve write-path duplicate-key behavior after index metadata rewrites.
- Reject new inline key/constraint creation in `ADD`, `CHANGE`, or `MODIFY`
  until Task 36, except that parsing should retain those nodes so later work can
  reuse the column-definition grammar.
- Reject adding or modifying a column into `AUTO_INCREMENT` when that would
  require new key metadata. Dropping an existing `AUTO_INCREMENT` column should
  clear its column/index/table-auto state if the current catalog can represent
  the result.
- Enforce the all-invisible-table check when changing visibility.
- Keep the operation statement-atomic: either every requested column action,
  catalog update, physical rewrite, warning, and affected-row result is visible
  together, or none of them are.
- Treat non-default `ALGORITHM` and `LOCK` options as parsed but unsupported for
  execution until the dedicated compatibility rows are implemented. Absent and
  `DEFAULT` options may execute through MyLite's table-copy implementation.

This slice should not use SQLite's native `ALTER TABLE` as the semantic source
of truth. SQLite can be an implementation tool only after MyLite has validated
MySQL names, dependencies, defaults, visibility, and diagnostics.

## Sources

- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Invisible Columns:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-columns.html
- MySQL 8.4 Reference Manual, Generated Columns:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html
- MySQL 8.4 Reference Manual, Secondary Indexes and Generated Columns:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-secondary-indexes.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- MySQL 8.4 Reference Manual, InnoDB `AUTO_INCREMENT` handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, atomic DDL:
  https://dev.mysql.com/doc/refman/8.4/en/atomic-ddl.html
- Existing MyLite specs:
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/column-attributes/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/create-table-indexes/specs.md`
  - `docs/specs/create-drop-index/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/update-single-table/specs.md`

Observed behavior was verified against the existing Docker container
`mylite-mysql-849` with MySQL `8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9`, version comment
`MySQL Community Server - GPL`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@innodb_autoinc_lock_mode = 2`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Statement shape and execution boundary

`ALTER TABLE` accepts one or more alter specifications in source order. Column
operations may be mixed with other table, key, constraint, and option
operations, but this task covers only column operations. MySQL treats ordinary
non-temporary `ALTER TABLE` as DDL with implicit commit boundaries. For InnoDB,
successful DDL is atomic: metadata and storage changes become visible together.
If validation or row copying fails, the original table definition and data
remain in place.

The verified MySQL runtime accepted both `ADD c INT` and `ADD COLUMN c INT`.
`DROP c` and `DROP COLUMN c` are equivalent. `CHANGE` and `MODIFY` also accept
the optional `COLUMN` keyword.

When several actions appear in one statement, failure in a later action rolls
back earlier actions from the same statement. A verified example attempted to
drop an existing column and then add a duplicate column name:

```sql
ALTER TABLE dep_ops DROP COLUMN c, ADD COLUMN id INT;
```

The statement failed with error 1060 / `42S21` for the duplicate name and left
both the original `c` column and the original `id` column unchanged.

### `ADD COLUMN`

`ADD COLUMN` appends by default, inserts at ordinal position `1` with `FIRST`,
or inserts immediately after an existing column with `AFTER col`. Existing
columns keep their relative order around the inserted column. Multiple `ADD`
actions in one statement are applied in statement order.

Runtime probes showed:

| Statement | Observed behavior |
| --- | --- |
| `ALTER TABLE basic_ops ADD COLUMN c INT DEFAULT 7 AFTER id` | Succeeds with OK packet `0 rows affected`; existing rows read `c=7`; `COLUMNS.ORDINAL_POSITION` places `c` after `id`. |
| `ALTER TABLE basic_ops ADD COLUMN first_col VARCHAR(5) NOT NULL DEFAULT 'q' FIRST, ADD COLUMN last_col INT NULL` | Succeeds; `first_col` becomes ordinal `1`, `last_col` is appended, and existing rows are backfilled with `q` and `NULL`. |
| `ALTER TABLE t ADD COLUMN nn INT NOT NULL` on a nonempty table | Succeeds; existing rows receive integer `0`; `COLUMN_DEFAULT` remains `NULL` and `IS_NULLABLE='NO'`. |
| `ALTER TABLE err_ops ADD COLUMN a INT` when `a` exists | Error 1060 / `42S21`, `Duplicate column name 'a'`. |
| `ALTER TABLE err_ops ADD COLUMN c INT AFTER missing_col` | Error 1054 / `42S22`, `Unknown column 'missing_col' in 'err_ops'`. |

MySQL uses the full column-definition semantic validator for added columns.
Default expressions, visibility, comments, generated-column definitions,
inline keys, references, and engine-specific attributes are accepted or rejected
as if a new table definition were being built, with extra checks for the
existing rows that need backfill.

### `DROP COLUMN`

`DROP COLUMN` removes the column definition, removes the stored column values,
renumbers remaining ordinal positions, and updates dependent metadata that
MySQL can safely rewrite.

Observed behavior:

| Statement | Observed behavior |
| --- | --- |
| `ALTER TABLE dep_ops DROP COLUMN a` where `a` was the first part of `uq_a` and `idx_ab` | Succeeds; `uq_a` disappears; `idx_ab` remains over `b`; a duplicate-index warning 1831 was produced because `idx_ab(b)` duplicated existing `idx_b(b)`. |
| `ALTER TABLE err_ops DROP COLUMN missing_col` | Error 1091 / `42000`, cannot drop a missing column or key. |
| `ALTER TABLE one_col DROP COLUMN only_col` | Error 1090 / `42000`; MySQL directs users to `DROP TABLE` for deleting all columns. |
| `ALTER TABLE ai_drop DROP COLUMN id` where `id` was `AUTO_INCREMENT PRIMARY KEY` and another column remained | Succeeds; existing rows remain with the other column, the primary-key statistics row disappears, and no column has `auto_increment` in `EXTRA`. |
| `ALTER TABLE gen_ops DROP COLUMN a` when generated column `b` depends on `a` | Error 3108 / `HY000`, generated-column dependency. |
| `ALTER TABLE child_fk DROP COLUMN c` when `c` participates in a foreign key and another column remains | Error 1828 / `HY000`; the column is needed by the named foreign-key constraint. |

Dropping a column from a multi-part index removes only that key part. If the
index becomes empty, the index is dropped. If the remaining index definition is
redundant with another index, MySQL can emit warning 1831 while still applying
the DDL.

### `RENAME COLUMN`

`RENAME COLUMN old_name TO new_name` changes only the column name. It preserves
the column type, nullability, default, comment, visibility, ordinal position,
stored values, and supported index participation.

Observed behavior:

| Statement | Observed behavior |
| --- | --- |
| `ALTER TABLE dep_ops RENAME COLUMN b TO renamed_b` | Succeeds with OK packet `0 rows affected`; `STATISTICS.COLUMN_NAME` for `idx_b` and `idx_ab` changes to `renamed_b`. |
| `ALTER TABLE err_ops RENAME COLUMN missing_col TO x` | Error 1054 / `42S22`. |
| `ALTER TABLE err_ops RENAME COLUMN a TO b` when `b` exists | Error 1060 / `42S21`. |
| `ALTER TABLE gen_ops RENAME COLUMN a TO a2` where generated column `b` references `a` | Error 3108 / `HY000`. |
| `ALTER TABLE child_fk RENAME COLUMN c TO c2` for a foreign-key column | Succeeds in MySQL; `KEY_COLUMN_USAGE.COLUMN_NAME` changes to `c2`. |

Trigger bodies are not automatically rewritten by MySQL. A verified trigger
that referenced `NEW.a` kept that text after `a` was renamed to `a2`; later
`UPDATE` failed with error 1054 / `42S22`, `Unknown column 'a' in 'NEW'`.
MyLite should reserve this behavior for the trigger feature rather than
inventing trigger-body rewrites.

### `CHANGE COLUMN`

`CHANGE COLUMN old_name new_name column_definition` combines rename,
replacement column definition, optional repositioning, and value conversion.
The new definition is complete; omitted attributes from the old definition are
not preserved.

Observed behavior:

| Statement | Observed behavior |
| --- | --- |
| `ALTER TABLE dep_ops CHANGE COLUMN renamed_b b2 BIGINT NOT NULL DEFAULT 5 AFTER id` | Succeeds; `renamed_b` becomes `b2`, type becomes `bigint`, default becomes `5`, nullability becomes `NO`, ordinal moves after `id`, and index metadata follows the new name. The verified OK packet reported `2 rows affected` for two copied rows. |
| `ALTER TABLE attrs CHANGE COLUMN b renamed_b VARCHAR(20)` where `b` had `NOT NULL DEFAULT 'x'` | Succeeds; the new column is nullable with `DEFAULT NULL`. |
| `ALTER TABLE err_ops CHANGE COLUMN missing_col x INT` | Error 1054 / `42S22`. |
| `ALTER TABLE err_ops CHANGE COLUMN a b INT` when `b` exists | Error 1060 / `42S21`. |

Because `CHANGE` supplies a complete replacement definition, implementers must
not copy old defaults, comments, visibility, `ON UPDATE`, `AUTO_INCREMENT`, or
nullability unless the statement explicitly specifies them or MySQL's normal
column-definition rules imply them.

### `MODIFY COLUMN`

`MODIFY COLUMN column_definition` changes the definition of an existing column
identified by the name inside the new definition. It may also reposition the
column. It does not rename the column.

Observed behavior:

| Statement | Observed behavior |
| --- | --- |
| `ALTER TABLE dep_ops MODIFY COLUMN c VARCHAR(20) NOT NULL DEFAULT 'z' FIRST` | Succeeds; `c` becomes ordinal `1`, type becomes `varchar(20)`, default becomes `z`, and existing stored values remain converted/preserved. |
| `ALTER TABLE attrs MODIFY COLUMN a BIGINT` where `a` had `NOT NULL DEFAULT 9` | Succeeds; the new column is nullable with `DEFAULT NULL`. |
| `ALTER TABLE err_ops MODIFY COLUMN missing_col INT` | Error 1054 / `42S22`. |
| `ALTER TABLE inv_ops MODIFY COLUMN a INT INVISIBLE` when it would make every column invisible | Error 4028 / `HY000`, a table must have at least one visible column. |

Like `CHANGE`, `MODIFY` uses a complete replacement definition. Old attributes
are discarded unless the new definition explicitly or implicitly supplies them.

### Defaults and backfill

For `ADD COLUMN`, existing rows are populated as part of the DDL. Verified
behavior for the first slice:

- explicit literal defaults backfill that literal
- nullable columns without explicit defaults backfill `NULL`
- `NOT NULL` columns without explicit defaults backfill the implicit default
  for the data type; for `INT`, the observed value was `0`
- metadata still reports `COLUMN_DEFAULT=NULL` for an implicit default that was
  not authored

For `CHANGE` and `MODIFY`, MySQL validates and converts existing row values to
the replacement definition. Strict-mode conversion errors abort the entire DDL.
Warnings and row-count reporting depend on the conversion path and selected DDL
algorithm. MyLite tests should target specific verified cases rather than
assuming every rewrite reports the same affected-row count.

### Visibility

Column visibility affects metadata and wildcard expansion. The verified
runtime exposes invisible columns through `INFORMATION_SCHEMA.COLUMNS.EXTRA`
as `INVISIBLE`. `SELECT *` omits invisible columns; explicit references can
still select them.

MySQL requires every table to have at least one visible column. This restriction
is checked across the final table definition after all actions in a statement.
For example, a table with one visible and one invisible column rejected making
the visible column invisible, then accepted the same change after the other
column was first made visible.

### `AUTO_INCREMENT`

`AUTO_INCREMENT` remains tied to key metadata:

- adding an `AUTO_INCREMENT` column without making it a key fails with error
  1075 / `42000`
- adding `INT NOT NULL AUTO_INCREMENT PRIMARY KEY FIRST` to a table with no key
  succeeds and populates existing rows with `1`, `2`, `3`, ...
- later inserts continue from the next generated value
- dropping an `AUTO_INCREMENT` primary-key column succeeds if another column
  remains, removes the primary-key statistics row, and clears the table's
  auto-increment state
- MySQL still enforces the one-auto-column rule, valid integer type families,
  no explicit default on an auto column, and key participation

The first MyLite slice should not add new key metadata through column
operations. It should therefore reject adding or modifying into
`AUTO_INCREMENT` unless the implementation also handles the required key action
as part of Task 36. Dropping an existing auto column is a column operation and
should be supported once the metadata cleanup path exists.

### Generated columns, constraints, triggers, and foreign keys

Generated columns introduce expression dependencies between columns. Verified
MySQL behavior rejects dropping or renaming a base column used by a generated
column with error 3108. Dropping a generated column that no other generated
column references succeeds.

Foreign keys are stricter for destructive actions and incompatible definition
changes than for renames. Verified MySQL behavior rejected dropping a child
foreign-key column with error 1828 and a referenced parent column with error
1829, even while `foreign_key_checks=0`. Renaming child or parent columns and
same-type `CHANGE COLUMN` operations are allowed and update
`KEY_COLUMN_USAGE`. `MODIFY` or `CHANGE` operations that make child and parent
column definitions incompatible fail with error 3780, also independent of
`foreign_key_checks`.

Triggers are different: MySQL does not rewrite trigger body text and does not
block the column rename/drop solely because a trigger references that column.
The failure appears later when the trigger executes and resolves a missing
column. MyLite should mirror this once triggers exist.

CHECK constraints and generated columns are future MyLite catalogs. The
`ALTER TABLE` implementation should centralize dependency inspection so future
constraint classes can participate without reworking the column-operation
executor.

### Metadata side effects

Successful column operations update `INFORMATION_SCHEMA.COLUMNS` immediately:

- column names, ordinal positions, data types, defaults, nullability, key
  markers, `EXTRA`, comments, and generation expressions reflect the final
  definition
- ordinal positions are dense and one-based
- dropped columns disappear
- renamed columns keep their original non-name metadata unless changed through
  `CHANGE`
- `CHANGE` and `MODIFY` replace all column attributes with the new definition

`INFORMATION_SCHEMA.STATISTICS` follows supported index metadata:

- renamed columns update `COLUMN_NAME`
- dropped columns are removed from index key-part lists
- remaining key parts are renumbered from `1`
- indexes with no remaining key parts disappear
- duplicate-index warnings may be produced when the rewrite creates redundant
  indexes

## MyLite behavior

### Parser and AST

Add an `ALTER TABLE` statement node that records:

1. target table name
2. ordered alter-item list

Each item is either a column action or a statement-scoped DDL option such as
`ALGORITHM` or `LOCK`. Runtime analysis can collect the options after parsing,
but the AST should retain source order for diagnostics and later MySQL
compatibility checks.

Each action should preserve source span. Column action nodes:

- `ADD_COLUMN`: optional `COLUMN` spelling, column definition, optional
  position
- `DROP_COLUMN`: optional `COLUMN` spelling, target column identifier
- `RENAME_COLUMN`: old identifier and new identifier
- `CHANGE_COLUMN`: optional `COLUMN` spelling, old identifier, replacement
  column definition, optional position
- `MODIFY_COLUMN`: optional `COLUMN` spelling, replacement column definition,
  optional position

`CHANGE` and `MODIFY` reuse the existing column-definition parser. The parser
must not treat the old name in `CHANGE` as part of the replacement definition;
the replacement name is the first identifier in the new definition.

Malformed list shapes are parse errors:

- empty action list
- trailing comma
- missing column definitions
- `RENAME COLUMN old new` without `TO`
- `AFTER` without a target identifier
- multiple `FIRST`/`AFTER` position clauses on one action
- unsupported `ALGORITHM` or `LOCK` value tokens

Semantic validation, not the parser, detects missing tables, duplicate names,
missing `AFTER` targets, dependency conflicts, all-invisible definitions,
conversion failures, and unsupported executable slices.

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature. They are
not copied from MySQL; they are the MyLite AST-oriented grammar shape.

```lemon
alter_table_statement ::= ALTER TABLE table_name alter_table_item_list.

alter_table_item_list ::= alter_table_item.
alter_table_item_list ::= alter_table_item_list COMMA alter_table_item.

alter_table_item ::= alter_table_action.
alter_table_item ::= alter_table_option.

alter_table_action ::= ADD opt_column column_definition opt_column_position.
alter_table_action ::= DROP opt_column identifier.
alter_table_action ::= RENAME COLUMN identifier TO identifier.
alter_table_action ::= CHANGE opt_column identifier column_definition
                       opt_column_position.
alter_table_action ::= MODIFY opt_column column_definition opt_column_position.

opt_column ::= .
opt_column ::= COLUMN.

opt_column_position ::= .
opt_column_position ::= FIRST.
opt_column_position ::= AFTER identifier.

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

The `column_definition` production is the already specified MyLite column
definition shape from the column-type, column-attribute,
primary-key/auto-increment, and create-table-index specs. Runtime support for
inline keys and constraints is intentionally narrower in the first executable
slice.

### Analyzer and validation

Validation should operate on an in-memory table model loaded from the MyLite
catalogs:

1. Resolve the target schema and table using the same rules as `CREATE TABLE`,
   `INSERT`, `UPDATE`, and `DROP TABLE`.
2. Reject system schemas and non-base-table targets.
3. Load current columns ordered by `ordinal_position`.
4. Load current index rows ordered by index catalog order and `seq_in_index`.
5. Apply each action to a candidate model in source order.
6. Validate names case-insensitively while preserving authored spelling.
7. Validate dependency and visibility rules against the final candidate model.
8. Build a rewrite plan that describes physical column sources, default
   backfill expressions, conversions, metadata deletes/inserts, index rewrites,
   warning records, and affected-row accounting.
9. Execute the rewrite plan atomically.

Validation details:

- `ADD` rejects duplicate column names and missing `AFTER` targets.
- `DROP`, `RENAME`, `CHANGE`, and `MODIFY` reject unknown existing columns.
- `RENAME` and `CHANGE` reject new names that collide with another column name
  case-insensitively.
- `DROP` rejects deleting every column.
- final table definitions must retain at least one visible column.
- `CHANGE` and `MODIFY` must treat the new column definition as complete.
- position targets are resolved against the candidate table state at that point
  in the action sequence.
- generated-column, check, and trigger dependency hooks should be explicit even
  before those catalogs exist.
- all unsupported first-slice behavior must fail before mutating storage or
  catalogs.

### Runtime and storage design

Use a table-copy rewrite as the initial implementation strategy:

1. Begin a statement-owned SQLite transaction or savepoint that can roll back
   physical storage and catalog writes together.
2. Create a shadow SQLite table with a fresh internal physical name derived
   from the target schema/table and a rewrite nonce.
3. Define one SQLite column per final MyLite column using the current
   type-affinity mapping from `CREATE TABLE` execution.
4. Copy data from the old physical table into the shadow table:
   - existing preserved columns read from their old physical column
   - renamed columns read from the old physical column and write to the new
     physical column
   - added columns evaluate the backfill expression
   - changed/modified columns pass through MyLite conversion and constraint
     validation before insertion
5. Recreate optional auxiliary SQLite indexes only after MyLite metadata has
   decided which index rows remain. These indexes are optimizations only.
6. Delete and reinsert affected `__mylite_column_catalog` rows using dense
   final ordinals.
7. Delete and reinsert affected `__mylite_index_catalog` rows using dense
   `seq_in_index` per final index.
8. Update `__mylite_table_catalog.auto_increment` when an auto column is added,
   removed, or invalidated by the operation.
9. Swap the shadow table into the target physical name.
10. Commit the statement transaction.

This approach is heavier than MySQL's instant or in-place algorithms, but it
keeps the first slice correct and simple. Later optimization may avoid the
shadow table for metadata-only renames, append-only nullable additions, or
visibility-only changes if tests prove the optimized path preserves all
observable MySQL behavior.

The rewrite must not rely on SQLite column defaults or constraints for
user-visible behavior. MyLite owns default evaluation, nullability checks,
type conversion, generated values, warnings, and diagnostics.

### Metadata model updates

`__mylite_column_catalog`:

- rewrite rows for the target table after every successful action sequence
- preserve old metadata for pure `RENAME`
- replace metadata for `CHANGE` and `MODIFY`
- assign one-based `ordinal_position` values in final visible/invisible order
- record `EXTRA='INVISIBLE'` for invisible ordinary columns
- record `EXTRA` values such as `auto_increment`, `DEFAULT_GENERATED`, and
  generated-column markers when those source features exist

`__mylite_index_catalog`:

- rename key-part `column_name` values after `RENAME` and `CHANGE`
- remove key parts that reference dropped columns
- remove indexes with zero remaining key parts
- compact `seq_in_index`
- preserve index names, visibility, comments, prefix lengths, order markers,
  and uniqueness for remaining parts
- record duplicate-index warning 1831 when the final metadata creates a
  MySQL-warning duplicate shape

`__mylite_table_catalog`:

- update `auto_increment` when an auto column is added or removed
- preserve table-level metadata unrelated to the column operation
- later DDL implicit-commit work may update timestamps and protocol-visible OK
  information; the first slice can leave timestamp fidelity deferred if current
  DDL features do the same

### Affected rows, diagnostics, and warnings

The MySQL OK packet is operation-dependent. Runtime probes showed:

- simple `ADD`, `RENAME`, and some `MODIFY` operations reported `0 rows
  affected`
- `CHANGE` from `INT` to `BIGINT` over two rows reported `2 rows affected`
- dropping an `AUTO_INCREMENT PRIMARY KEY` column from a two-row table reported
  `2 rows affected`
- warnings can affect `ROW_COUNT()` observations if the client runs
  `SHOW WARNINGS` before checking `ROW_COUNT()`

MyLite should store statement diagnostics before unrelated statements can
overwrite them, consistent with existing DML diagnostics specs. Tests that
check warning rows should inspect warnings immediately after the `ALTER TABLE`.

Required first-slice diagnostics include:

| Condition | MySQL code / SQLSTATE |
| --- | --- |
| duplicate target column name | 1060 / `42S21` |
| unknown column for action or `AFTER` target | 1054 / `42S22` |
| dropping a missing column/key | 1091 / `42000` |
| deleting every column | 1090 / `42000` |
| all columns invisible | 4028 / `HY000` |
| invalid `AUTO_INCREMENT` shape | 1075 / `42000` |
| generated-column dependency | 3108 / `HY000`, once generated columns exist |
| child foreign-key column drop | 1828 / `HY000` |
| referenced parent foreign-key column drop | 1829 / `HY000` |
| incompatible child/parent foreign-key column definitions | 3780 / `HY000` |
| duplicate index warning after index rewrite | warning 1831 |

Where MyLite lacks the dependent feature, it should return a deterministic
unsupported diagnostic before mutation rather than silently applying an
incomplete or misleading rewrite.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations. The SQL
examples below are representative; test fixtures should create and drop their
own schema so results are isolated.

### Basic add, positioning, and metadata

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE basic_ops ADD COLUMN c INT DEFAULT 7 AFTER id` | Succeeds; existing rows contain `7`; `COLUMNS.ORDINAL_POSITION` places `c` after `id`; OK affected rows `0` for the verified shape. |
| `ALTER TABLE basic_ops ADD COLUMN first_col VARCHAR(5) NOT NULL DEFAULT 'q' FIRST, ADD COLUMN last_col INT NULL` | Succeeds atomically; first column is ordinal `1`; appended column is last; existing rows contain `q` and `NULL`. |
| `ALTER TABLE t ADD COLUMN nn INT NOT NULL` on a populated table | Succeeds; existing rows contain `0`; `COLUMN_DEFAULT` is `NULL`; `IS_NULLABLE='NO'`. |
| `ALTER TABLE basic_ops ADD COLUMN c INT AFTER missing_col` | Error 1054; no metadata or row changes. |
| duplicate added column name | Error 1060; no metadata or row changes. |

### Drop and dependency behavior

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| drop a column from the first part of a composite index | Remaining key parts stay indexed and are renumbered. |
| drop the only column in an index | The index disappears from `STATISTICS`. |
| drop a column causing duplicate remaining indexes | Statement succeeds and records warning 1831. |
| `ALTER TABLE one_col DROP COLUMN only_col` | Error 1090; table remains unchanged. |
| `ALTER TABLE err_ops DROP COLUMN missing_col` | Error 1091. |
| drop an `AUTO_INCREMENT PRIMARY KEY` column while another column remains | Succeeds; primary index row disappears; no remaining column has `auto_increment`. |
| drop a base column referenced by generated columns | Error 3108 once generated columns exist. |
| drop a child foreign-key column | Error 1828. |
| drop a referenced parent foreign-key column | Error 1829. |

### Rename, change, and modify

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE dep_ops RENAME COLUMN b TO renamed_b` | Succeeds; column metadata and index key parts use `renamed_b`; stored values are unchanged. |
| rename a missing column | Error 1054. |
| rename to an existing column name | Error 1060. |
| rename a generated-column dependency | Error 3108 once generated columns exist. |
| rename a foreign-key column | Succeeds; foreign-key metadata follows the new name. |
| same-type `CHANGE` of a foreign-key column | Succeeds; foreign-key metadata follows the new name when renamed. |
| incompatible `CHANGE` or `MODIFY` of a foreign-key column | Error 3780; physical rows and metadata remain unchanged. |
| `ALTER TABLE dep_ops MODIFY COLUMN c VARCHAR(20) NOT NULL DEFAULT 'z' FIRST` | Succeeds; `c` moves to ordinal `1`, new metadata is used, existing values are preserved/converted. |
| `ALTER TABLE dep_ops CHANGE COLUMN renamed_b b2 BIGINT NOT NULL DEFAULT 5 AFTER id` | Succeeds; name, type, default, nullability, order, and index metadata change together; verified OK affected rows were `2` for two copied rows. |
| `MODIFY a BIGINT` where `a` used to be `NOT NULL DEFAULT 9` | Succeeds; new column is nullable with `DEFAULT NULL`. |
| `CHANGE b renamed_b VARCHAR(20)` where `b` used to be `NOT NULL DEFAULT 'x'` | Succeeds; new column is nullable with `DEFAULT NULL`. |
| `CHANGE v v2 BIGINT` where existing `v` contains `b1` | Error 1366-style `Incorrect integer value`; physical rows and column metadata remain unchanged. |
| `MODIFY v INT NOT NULL` where existing `v` contains `NULL` | Error 1138-style `Invalid use of NULL value`; physical rows and column metadata remain unchanged. |
| `MODIFY s VARCHAR(2)` where existing `s` contains `abcd` | Error 1265-style `Data too long`; physical rows and column metadata remain unchanged. |

### Visibility

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| add or modify one column to `INVISIBLE` while another visible column remains | Succeeds; `COLUMNS.EXTRA` contains `INVISIBLE`; `SELECT *` omits the column. |
| make every column invisible | Error 4028; no mutation. |
| make another column visible and then make the first invisible | Succeeds; final table still has one visible column. |

### `AUTO_INCREMENT`

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE no_key ADD COLUMN id INT NOT NULL AUTO_INCREMENT` | Error 1075. |
| `ALTER TABLE no_key ADD COLUMN id INT NOT NULL AUTO_INCREMENT PRIMARY KEY FIRST` on a no-key table | Full MySQL behavior succeeds and backfills `1..n`; MyLite first slice may defer this until Task 36 because it creates key metadata. |
| insert after adding the auto primary key | MySQL continues at the next generated value. |
| drop the auto primary-key column | Succeeds if at least one other column remains; clears auto metadata and primary index rows. |

### Atomicity and diagnostics lifecycle

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `ALTER TABLE dep_ops DROP COLUMN c, ADD COLUMN id INT` where `id` already exists | Error 1060; `c` remains present and no duplicate `id` appears. |
| failure during row copy or conversion | Whole `ALTER TABLE` rolls back physical rows and all catalog changes. |
| warnings from duplicate-index rewrite | Statement succeeds; warning rows are available immediately after the DDL. |

## Test plan

- Parser tests:
  - every column action with and without optional `COLUMN`
  - `FIRST`, `AFTER`, and omitted position
  - multiple actions and malformed trailing-comma lists
  - `CHANGE` old/new name separation
  - column definitions that reuse current type and attribute grammar
  - parse and preserve `ALGORITHM` and `LOCK` options
  - reject malformed `RENAME COLUMN`, `AFTER`, and option values
- Analyzer tests:
  - selected-schema and schema-qualified table resolution
  - missing selected schema, missing table, system schema, duplicate columns,
    missing columns, and missing `AFTER` targets
  - source-order application of multiple actions
  - all-invisible final table detection
  - unsupported inline key/constraint additions fail before mutation in the
    first slice
- Runtime tests:
  - successful `ADD` with default, implicit default, nullable default, `FIRST`,
    `AFTER`, and multiple adds
  - `DROP` of ordinary, primary-key, unique, secondary-indexed, and
    auto-increment columns where supported
  - `RENAME` updating column and index metadata without changing stored values
  - `CHANGE` and `MODIFY` replacing metadata completely rather than preserving
    old attributes
  - physical row preservation/backfill/conversion through the shadow-table path
  - `INFORMATION_SCHEMA.COLUMNS` and `STATISTICS` side effects
  - affected-row values for the verified OK-packet shapes
  - warning 1831 when dropping a column creates duplicate remaining indexes
  - full rollback on validation, conversion, and catalog-write failures
- Future-feature tests to add when dependencies land:
  - generated-column dependency errors and permitted generated-column drops
  - trigger body non-rewrite and later execution-time missing-column errors
  - check-constraint dependency handling
  - `ALGORITHM` and `LOCK` compatibility
  - temporary table behavior and DDL implicit commit fidelity

## Compatibility gaps and deferred behavior

- MyLite's first slice may use a full table-copy rewrite even when MySQL would
  use instant or in-place DDL. This is acceptable only while observable
  metadata, rows, diagnostics, atomicity, and affected rows match the tested
  MySQL behavior.
- Exact online-DDL `ALGORITHM` and `LOCK` validation is deferred.
- DDL implicit-commit behavior remains part of the broader DDL transaction
  retrofit unless implemented before this feature lands.
- New key and constraint creation through `ADD`, `CHANGE`, or `MODIFY` is
  deferred to Task 36.
- Generated columns, triggers, and check constraints need explicit hooks now but
  full behavior waits for their respective feature catalogs and runtimes.
- Broad type-conversion warning fidelity, non-strict SQL modes, charset
  conversion, row-size validation, and storage-engine-specific limits remain
  deferred unless already implemented by the shared conversion layer.
- Physical SQLite indexes are optional optimizations. They must never replace
  MyLite-owned MySQL metadata, duplicate checks, warning order, or diagnostics.

## Implementation handoff notes

1. Add the parser/AST surface first and keep all new statement nodes
   source-span aware for diagnostics.
2. Build a reusable table-definition model from `__mylite_table_catalog`,
   `__mylite_column_catalog`, and `__mylite_index_catalog`. Later ALTER key,
   constraint, and rename-table work should use the same model.
3. Implement candidate-model transformation and validation before touching
   SQLite storage.
4. Implement a single shadow-table rewrite executor for `ADD`, `DROP`,
   `RENAME`, `CHANGE`, and `MODIFY`; add narrower optimized paths only after
   the full table-copy path is correct.
5. Reuse existing column-definition descriptor code for replacement
   definitions, but ensure `CHANGE` and `MODIFY` do not accidentally preserve
   old attributes.
6. Keep index metadata rewrite separate from physical SQLite indexes. Update
   the MyLite duplicate-key conflict surfaces from rewritten
   `__mylite_index_catalog` rows.
7. Add diagnostics and warning tests before broadening conversion behavior.
8. Update `COMPATIBILITY.md` again only after implementation and
   MySQL-runtime-comparison tests are complete; until then this feature remains
   specified but not supported.
