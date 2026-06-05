# Baseline AUTO_INCREMENT Lifecycle

## Status

This feature specifies the first descriptor-owned `AUTO_INCREMENT` slice for
file-backed `.mylite` handles. It builds on durable schema/table/column/index
descriptors, the current single-column integer primary-key subset, integer and
`VARCHAR` row storage, `INSERT ... VALUES`, `INSERT ... SET`, `UPDATE`,
`TRUNCATE`, `CREATE TABLE ... LIKE`, `SHOW COLUMNS`, `SHOW CREATE TABLE`,
`SHOW TABLE STATUS`, and the existing zero-argument `LAST_INSERT_ID()` scalar
function.

The feature is intentionally not full MySQL auto-increment support. It supports
one auto-increment column per persistent base table, and that column must be
the current single-column integer-family primary-key column. It does not
implement secondary-index auto-increment columns, mixed-mode multi-row
allocation gaps, `ALTER TABLE ... AUTO_INCREMENT`, `LAST_INSERT_ID(expr)`,
`NO_AUTO_VALUE_ON_ZERO`, insert-id protocol metadata, information-schema
metadata, or replication/concurrency lock-mode behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline VARCHAR type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
- Baseline insert ignore lifecycle:
  `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
- Baseline truncate table lifecycle:
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, InnoDB `AUTO_INCREMENT` handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_auto_increment_lifecycle_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- `id INT AUTO_INCREMENT PRIMARY KEY` creates an `int NOT NULL
  AUTO_INCREMENT` column, renders `auto_increment` in `SHOW COLUMNS` `Extra`,
  renders the primary key in `SHOW INDEX`, and renders `AUTO_INCREMENT` on the
  column in `SHOW CREATE TABLE`.
- `id INT AUTO_INCREMENT, PRIMARY KEY (id)` is accepted and renders the same
  as the inline primary-key form.
- An auto-increment column must be indexed. `CREATE TABLE t (id INT
  AUTO_INCREMENT)` fails with error `1075`, SQLSTATE `42000`, and an
  "auto column ... key" diagnostic.
- Non-integer auto-increment columns fail. `VARCHAR(3) AUTO_INCREMENT PRIMARY
  KEY` fails with error `1063`, SQLSTATE `42000`.
- An auto-increment column cannot declare a default in `CREATE TABLE`.
  `DEFAULT 7 AUTO_INCREMENT` fails with error `1067`, SQLSTATE `42000`.
  Existing `ALTER TABLE ... ALTER id SET DEFAULT 7` syntax is accepted by
  MySQL; the default remains hidden from `SHOW COLUMNS` and `SHOW CREATE
  TABLE`, but later omitted/default values use it as an explicit value.
- Explicit `NULL` on a primary-key auto-increment column fails with the primary
  key `1171` diagnostic before auto-increment allocation matters.
- `AUTO_INCREMENT = 0` table option is accepted and normalizes to the default
  next value `1`; negative table-option values are syntax errors.
- `CREATE TABLE ... AUTO_INCREMENT = 7` persists an initial next value and
  initial status metadata value of `7`, and the first generated row receives
  `7`.
- Omitted auto-increment columns, explicit `NULL`, explicit `0`, and explicit
  `DEFAULT` each generate the next sequence value under MySQL's default SQL
  mode. `NO_AUTO_VALUE_ON_ZERO` changes this upstream, but MyLite's current
  `@@sql_mode` is fixed and does not implement that mode.
- `LAST_INSERT_ID()` returns the first generated value from the most recent
  successful insert statement that generated at least one value. Explicit
  nonzero, non-`NULL` values do not change it.
- Explicit positive values are stored as provided. When such a value is greater
  than the current counter, the next generated value becomes explicit value
  plus one.
- Explicit negative values into signed auto-increment columns are stored as
  normal explicit values in the observed InnoDB/default-mode surface and do not
  update `LAST_INSERT_ID()`.
- In a multi-row insert where every auto-increment target is generated, values
  are consecutive and `LAST_INSERT_ID()` is the first generated value.
- InnoDB's mixed-mode multi-row inserts with some explicit and some generated
  auto values may create gaps. For example, inserting `(1, ...), (NULL, ...),
  (5, ...), (NULL, ...)` yields generated values `2` and `6`, then a later
  generated insert can produce `8`. This baseline defers mixed-mode multi-row
  allocation rather than implementing a non-MySQL-compatible approximation.
- `CREATE TABLE ... LIKE` clones the auto-increment column attribute and
  primary key, but the new empty table starts at the default generated value
  rather than copying the source counter.
- `TRUNCATE TABLE` removes rows and resets the next generated value to `1` for
  the admitted InnoDB/default-mode surface.
- Successfully allocated generated values, explicit high insert values, and
  explicit high update values remain consumed when a user transaction rolls
  back. The affected rows roll back, and `LAST_INSERT_ID()` remains the value
  observed inside the rolled-back transaction, but later generated values do
  not reuse the consumed counter values.
- When a small integer auto-increment column reaches its maximum value, the
  next generated insert fails with duplicate-key error `1062` using the maximum
  key value. When the stored next value already exceeds the column maximum
  before assignment, the generated insert fails with error `1467`, SQLSTATE
  `HY000`.

## Scope

The implementation must add:

- parser and AST support for the `AUTO_INCREMENT` column attribute in the
  limited `CREATE TABLE` column-definition grammar;
- parser and AST support for a `CREATE TABLE ... AUTO_INCREMENT = N` table
  option using nonnegative decimal integer literals;
- descriptor-owned auto-increment metadata for exactly one column per base
  table;
- a durable table-level next-counter value owned by MyLite catalog metadata,
  not SQLite rowid or SQLite `AUTOINCREMENT`;
- support only when the auto-increment column is the current single-column
  integer-family primary-key descriptor column;
- effective `NOT NULL` behavior for admitted auto-increment columns;
- generated-value allocation for omitted, `NULL`, `0`, and `DEFAULT`
  auto-increment column values in `INSERT ... VALUES` and `INSERT ... SET`;
- explicit non-generated integer values in those insert paths, including
  counter advancement when the explicit value is greater than or equal to the
  current next value;
- multi-row `INSERT ... VALUES` when all rows either generate an
  auto-increment value or all rows provide explicit non-generated values;
- deterministic rejection of mixed-mode multi-row inserts that combine
  generated and explicit non-generated auto-increment values in the same
  statement;
- `LAST_INSERT_ID()` session-state updates for the first generated value from
  the most recent successful generated insert statement, preserving the current
  value when a successful insert generates no value or a statement fails;
- statement-atomic rollback of row writes, counter changes, and
  `LAST_INSERT_ID()` changes on failed inserts;
- duplicate-key diagnostics and `INSERT IGNORE` demotion for explicit duplicate
  auto-increment primary-key values through the existing primary-key path;
- duplicate-key or storage-engine-style exhaustion diagnostics for generated
  allocation at or beyond the admitted column's positive range;
- descriptor-backed `SHOW COLUMNS` `Extra = auto_increment`;
- descriptor-backed `SHOW CREATE TABLE` column rendering with
  `AUTO_INCREMENT`, and table-level `AUTO_INCREMENT=N` rendering only when the
  next value is not MySQL's default rendering for the empty/default state;
- descriptor-backed `SHOW TABLE STATUS` `Auto_increment` column for admitted
  auto-increment tables;
- `CREATE TABLE ... LIKE` cloning of the auto-increment column attribute and
  primary key while resetting the target next value to `1`;
- `TRUNCATE TABLE` preserving descriptors and resetting the next generated
  value to `1`;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation for admitted auto-increment
  metadata and rows;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL forms.

## Non-Goals

This feature must not implement:

- auto-increment columns without the current primary-key descriptor;
- auto-increment over `VARCHAR`, non-integer columns, composite keys,
  secondary indexes, generated columns, invisible generated primary keys, or
  temporary tables;
- more than one auto-increment column per table;
- `ALTER TABLE ... AUTO_INCREMENT = N`, adding/dropping/modifying
  auto-increment through `ALTER TABLE`, or key-aware column replacement for
  auto-increment columns;
- full auto-increment default semantics beyond the observed hidden
  `ALTER TABLE ... ALTER column SET DEFAULT literal` surface;
- mixed-mode multi-row auto-increment allocation gaps;
- `INSERT ... SELECT` into auto-increment tables;
- `REPLACE` into auto-increment tables while key-aware `REPLACE` remains
  deferred;
- `ON DUPLICATE KEY UPDATE`, `LOAD DATA`, stored programs, triggers,
  cascades, foreign keys, generated/default expressions, or privileges;
- `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, `innodb_autoinc_lock_mode`, replication, concurrent
  auto-inc lock behavior, crash-recovery gap guarantees, or protocol insert-id
  metadata;
- `LAST_INSERT_ID(expr)` or sequence simulation through expressions;
- information-schema or `mysql` dictionary metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns statement diagnostics, warning count, affected rows,
  prior diagnostics snapshots, and transaction completion. Supported
  auto-increment statements return through existing non-row result conventions.
- Session state owns the current `LAST_INSERT_ID()` value per `mylite_db`
  handle. Insert execution updates this state only after the statement commits.
- Lexer/parser/AST own syntax admission and source spans. They do not inspect
  descriptors, counters, SQLite schema, or physical rows.
- Analyzer/planner code resolves auto-increment attributes against planned
  column definitions, the current single-column primary-key subset, table
  options, existing catalog descriptors, and DML target columns. It decides
  which insert values are generated before any SQLite SQL is generated.
- The catalog module owns durable auto-increment column metadata and next
  counter state. Catalog state is the MySQL metadata authority.
- Result and introspection builders render auto-increment metadata from
  descriptors. SQLite schema text, SQLite rowid state, `sqlite_sequence`,
  `PRAGMA`, and `sqlite_schema` are not user-visible MySQL metadata authority.
- SQLite owns physical row storage and the generated unique index already used
  for primary-key enforcement. MyLite binds generated values as ordinary
  prepared-statement integer parameters.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes catalog and user-row data only inside the shifted SQLite
  payload and must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

The feature extends the current limited `CREATE TABLE` grammar. The grammar is
independent MyLite grammar, not MySQL's complete grammar.

Supported subset:

```sql
CREATE TABLE [IF NOT EXISTS] table_name (
    create_table_item [, create_table_item ...]
) [table_option ...]

create_table_item:
    column_definition
  | PRIMARY KEY ( column_name )

column_definition:
    column_name integer_type column_attribute_list
  | column_name VARCHAR(length) column_attribute_list

column_attribute_list:
    [NULL | NOT NULL]
    [AUTO_INCREMENT]
    [DEFAULT signed_decimal_integer_literal | DEFAULT TRUE | DEFAULT FALSE]
    [PRIMARY KEY]

table_option:
    ENGINE [=] InnoDB
  | [DEFAULT] CHARSET [=] utf8mb4
  | [DEFAULT] CHARACTER SET [=] utf8mb4
  | [DEFAULT] COLLATE [=] utf8mb4_0900_ai_ci
  | AUTO_INCREMENT [=] nonnegative_decimal_integer_literal
```

For implementation simplicity, MyLite may admit `PRIMARY KEY AUTO_INCREMENT`
and `AUTO_INCREMENT PRIMARY KEY` orders, plus `NOT NULL` before or after
`AUTO_INCREMENT`, because these are common MySQL forms and are verified against
MySQL 8.4.9. The semantic analyzer remains responsible for rejecting duplicate
attributes, `DEFAULT` on auto-increment columns, explicit `NULL` primary-key
parts, unsupported types, missing keys, and unsupported table options.

Lemon-syntax sketch:

```lemon
column_definition(A) ::=
    identifier(N) column_type(T) column_attribute_list_opt(L). {
    A = mylite_sql_parser_make_column_definition(state, N, T, L);
}

column_attribute_list_opt(A) ::= .
column_attribute_list_opt(A) ::= column_attribute_list(B).

column_attribute_list(A) ::= column_attribute(B).
column_attribute_list(A) ::= column_attribute_list(B) column_attribute(C).

column_attribute(A) ::= NULL(T).
column_attribute(A) ::= NOT(N) NULL(T).
column_attribute(A) ::= DEFAULT(D) insert_value(V).
column_attribute(A) ::= PRIMARY(P) KEY(K).
column_attribute(A) ::= AUTO_INCREMENT(T).

table_option(A) ::= AUTO_INCREMENT(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_auto_increment_option(state, T, V);
}
```

The actual parser may keep the existing column-definition node kind and append
new children for auto-increment attributes if that best preserves local AST
patterns.

## Descriptor And Catalog Design

The catalog schema advances to version `6`.

`_mylite_catalog_columns` gains:

- `is_auto_increment INTEGER NOT NULL DEFAULT 0`

`_mylite_catalog_tables` gains:

- `auto_increment_next INTEGER NOT NULL DEFAULT 1`

The table-level next value is meaningful only when exactly one column
descriptor for the table has `is_auto_increment = 1`. It stores the next value
MyLite will try to generate. It must remain at least `1`.

The catalog module exposes mutation APIs to:

- insert and replace column descriptors with `is_auto_increment`;
- read materialized descriptors with `is_auto_increment`;
- update a table's `auto_increment_next` inside an active mutation or statement
  transaction;
- reset a table's `auto_increment_next` to `1` for `CREATE TABLE ... LIKE` and
  `TRUNCATE TABLE`.

Version `5` files migrate by adding the two columns with default values.
Earlier supported schema migrations continue to run in order before the
version `5` to `6` additive migration. Fresh catalog bootstrap creates the
version `6` descriptor tables directly.

`CREATE TABLE ... LIKE` clones the auto-increment column attribute but sets the
target table counter to `1`, matching the observed empty-table MySQL surface.
`CREATE TABLE ... SELECT` remains no-key/no-auto for copied descriptors because
it already does not copy primary keys.

## Semantics

### DDL

An admitted auto-increment column:

- must be an integer-family descriptor column;
- must be the same column as the admitted single-column primary key;
- is effectively `NOT NULL`;
- cannot declare a default in `CREATE TABLE`;
- is the only auto-increment column in the table.

If the table option `AUTO_INCREMENT=N` is present, `N = 0` is accepted and
normalizes to next value `1`. Positive `N` is stored as the next counter value
when it is within MyLite's signed-64 physical range, even when it is beyond
the admitted column's positive range. A later generated insert from such an
out-of-range stored next value fails with `1467`/`HY000`, matching observed
MySQL 8.4.9 behavior. Negative, decimal, float, hex, bit, string, parameter,
or expression table-option values are syntax errors or deterministic
unsupported diagnostics for this slice.

### Generated Insert Values

For `INSERT ... VALUES` and `INSERT ... SET`, an auto-increment target value is
generated when the column is omitted, or when the provided value is `NULL`, `0`,
or `DEFAULT`. This follows the current fixed SQL-mode surface where
`NO_AUTO_VALUE_ON_ZERO` is not enabled.

Generated values use the table's durable `auto_increment_next` value. After a
generated value below the column maximum is assigned, the in-statement next
value increments by one. If the generated value is the column maximum, the
counter remains at that maximum so a later generated insert reaches the same
duplicate-key surface MySQL exposes for small integer exhaustion. The first
generated value successfully inserted by the statement is staged as the new
`LAST_INSERT_ID()` value.

Explicit non-generated integer values are validated through the existing
integer-column conversion rules. They are bound as ordinary values. If a
successful explicit value is greater than or equal to the current next counter,
the durable next counter advances to explicit value plus one, except that an
explicit value equal to the column's positive maximum leaves the counter at
that maximum. Values remain subject to the column's supported positive range
and MyLite's signed-64 physical range.
Explicit negative values for signed columns do not advance the counter and do
not update `LAST_INSERT_ID()`.

For multi-row `INSERT ... VALUES`, this slice supports:

- all rows generating the auto-increment column;
- all rows providing explicit non-generated auto-increment values.

It rejects mixed-mode multi-row inserts that combine generated and explicit
non-generated values in one statement, because matching InnoDB's gap behavior
requires a broader allocation model than this baseline.

`INSERT ... SET` is one row, so it supports both generated and explicit forms.

`INSERT IGNORE` follows the existing warning-demotion policy for explicit
duplicate primary-key values. Generated successful values update
`LAST_INSERT_ID()` as normal. Ignored explicit duplicate rows do not update
`LAST_INSERT_ID()`.

If generated allocation starts from a stored next value greater than the
admitted column's positive maximum, the insert fails with error `1467`,
SQLSTATE `HY000`, and the observed "Failed to read auto-increment value from
storage engine" diagnostic. If the next generated value is still the column
maximum and that key already exists, normal primary-key enforcement reports
duplicate-key error `1062` using the maximum key value.

### Counters, Transactions, And Persistence

Insert execution must be statement-atomic. Row writes, counter changes, and the
new session `LAST_INSERT_ID()` value become visible only after the SQLite
statement transaction and catalog counter update commit. On statement failure,
rows and counters remain unchanged, and `LAST_INSERT_ID()` remains unchanged.
After a successful statement inside an explicit user transaction, MyLite tracks
the highest persistent auto-increment counter value outside the user
transaction. If the user transaction later rolls back, rows roll back but the
consumed counter high-water mark is restored so a later generated value is not
reused. `ROLLBACK TO SAVEPOINT` may roll back catalog writes inside SQLite;
the same high-water mark is reconciled before the final `COMMIT`.

The durable counter persists across close/reopen. Deleting rows does not reset
the counter. `TRUNCATE TABLE` resets the counter to `1` after deleting rows.

`UPDATE` of an auto-increment primary-key column remains in scope only for the
existing explicit assignment subset. If a successful update sets the
auto-increment column to a positive value greater than or equal to the current
next counter, MyLite advances the counter to value plus one, except that a
value equal to the column's positive maximum leaves the counter at that
maximum. It does not update `LAST_INSERT_ID()`.

### Metadata

`SHOW COLUMNS` renders `auto_increment` in `Extra` for the auto-increment
column. If future visibility metadata also applies, MyLite must join extra
tokens in MySQL-compatible order verified by tests.

`SHOW CREATE TABLE` renders `AUTO_INCREMENT` on the column line. It renders a
table-level `AUTO_INCREMENT=N` option when the table's next value is greater
than `1`. An empty `CREATE TABLE ... LIKE` clone with next value `1` omits the
table-level option.

`SHOW TABLE STATUS` and `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT` render SQL
`NULL` for implicit auto-increment tables in the verified metadata surface, and
render the separate InnoDB-style status metadata value when an explicit
`AUTO_INCREMENT=N` table option established one. `CREATE TABLE ...
AUTO_INCREMENT=N` also exposes that status value for tables without an
auto-increment column, matching MySQL 8.4.9, while `SHOW CREATE TABLE` still
omits the option when no column can generate values. Metadata predicates over
`AUTO_INCREMENT` may evaluate the durable next counter for implicit
auto-increment tables even when the rendered cell is `NULL`.

`SHOW INDEX` remains driven by the primary-key descriptor and does not need
additional auto-increment-specific rows.

If the existing `ALTER TABLE ... ALTER column SET DEFAULT literal` surface is
used on an auto-increment column, MyLite stores the descriptor default so
omitted/default DML values behave like MySQL's explicit hidden default. That
default is not rendered in `SHOW COLUMNS` or `SHOW CREATE TABLE`.

## Diagnostics

Diagnostics are MySQL-compatible where verified for the admitted subset. For
deferred MySQL forms, deterministic MyLite unsupported diagnostics are allowed
when a MySQL-compatible code has not yet been verified.

- Syntax errors and unsupported grammar: `1064`, SQLSTATE `42000`.
- Missing default schema: `1046`, SQLSTATE `3D000`.
- Unknown schema: `1049`, SQLSTATE `42000`.
- Unknown table: `1146`, SQLSTATE `42S02`.
- Reserved `_mylite_*` target names: existing reserved-name diagnostics.
- Auto-increment without the required key: `1075`, SQLSTATE `42000`, message
  containing "there can be only one auto column and it must be defined as a
  key".
- More than one auto-increment column: `1075`, SQLSTATE `42000`.
- Unsupported auto-increment type: `1063`, SQLSTATE `42000`, message
  containing "Incorrect column specifier".
- Auto-increment with explicit default: `1067`, SQLSTATE `42000`.
- Explicit nullable primary-key auto-increment part: current primary-key
  `1171`, SQLSTATE `42000`.
- Unknown primary-key column: current primary-key `1072`, SQLSTATE `42000`.
- Duplicate primary key definitions: current primary-key `1068`, SQLSTATE
  `42000`.
- Duplicate explicit auto-increment primary-key insert/update: `1062`,
  SQLSTATE `23000`.
- Generated counter exhaustion: `1062`, SQLSTATE `23000`, duplicate entry for
  the column's maximum supported value.
- Unsupported mixed-mode multi-row insert: `1064`, SQLSTATE `42000`, MyLite
  message naming mixed auto-increment inserts.
- Unsupported `INSERT ... SELECT` into auto-increment tables: `1064`,
  SQLSTATE `42000`.
- Unsupported `REPLACE` into auto-increment/primary-key tables: existing
  deterministic primary-key table diagnostic.
- Allocation failure: `MYLITE_NOMEM` plus handle diagnostics.
- Physical SQLite failures: current physical-row/internal diagnostics, after
  rolling back the statement transaction.

Supported in-range operations must report `warning_count == 0`.

## Physical SQLite Handling

Generated MyLite user tables remain ordinary descriptor-built SQLite rowid
tables. This feature does not use SQLite `INTEGER PRIMARY KEY`, SQLite
`AUTOINCREMENT`, or `sqlite_sequence`. The physical primary-key uniqueness path
continues to use generated SQLite unique indexes over stable physical table
names such as `_mylite_user_table_<table_id>`.

Generated SQLite SQL must:

- be built from MyLite descriptors, not SQLite metadata;
- quote every SQLite identifier;
- bind generated and explicit values through prepared-statement parameters;
- bind `NULL` and `TEXT` values according to existing row-value rules;
- update catalog counters using prepared statements in the same statement
  transaction used for the physical insert or update when a counter changes.

No SQLite fork patch is required.

## Tests

Add a fast C runtime test, preferably
`libmylite.runtime.auto_increment_lifecycle`, plus parser coverage and a MySQL
8.4.9 expectation script.

Cover:

- parser acceptance for inline and table-level primary-key auto-increment
  forms, common attribute orders, and `AUTO_INCREMENT=N` table option;
- parser/semantic rejection for unsupported attribute/table-option forms;
- `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`
  metadata;
- generated inserts through omitted column, explicit `NULL`, explicit `0`, and
  explicit `DEFAULT`;
- `INSERT ... SET` generated and explicit forms;
- multi-row all-generated inserts and multi-row all-explicit inserts;
- deterministic rejection of mixed-mode multi-row explicit/generated inserts;
- explicit positive values advancing the counter;
- explicit negative signed values not advancing the counter;
- `LAST_INSERT_ID()` before inserts, after generated inserts, after explicit
  inserts, after failed inserts, and across independent handles;
- duplicate explicit primary-key errors and `INSERT IGNORE` demotion;
- counter exhaustion on a small unsigned integer type;
- out-of-range initial table-option counters producing `1467`;
- hidden `ALTER TABLE ... ALTER column SET DEFAULT` behavior for
  auto-increment metadata and later omitted inserts;
- table option `AUTO_INCREMENT=0`, positive starting values, and the status
  metadata split from generated-value counters;
- schema-qualified and unqualified table resolution through existing policy;
- missing default schema, unknown schema, unknown table, and reserved-name
  diagnostics where auto-increment code touches those paths;
- reopen persistence of rows, counters, and `LAST_INSERT_ID()` session
  isolation;
- explicit transaction rollback and savepoint rollback preserving consumed
  generated, explicit-insert, and explicit-update counter high-water marks
  while rolling back rows;
- `DELETE` preserving the counter and `TRUNCATE` resetting it;
- `CREATE TABLE ... LIKE` cloning the attribute and resetting the counter;
- update of the auto-increment column advancing the counter when the new value
  is larger;
- physical `.mylite` preamble preservation;
- zero-initialized cleanup for new plan/catalog objects;
- regression coverage for existing parser, catalog, row-values, insert,
  update, primary-key, truncate, SHOW, storage, and statement-context tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and:

- `docs/compatibility/sql-table-ddl.md`
- `docs/compatibility/sql-table-dml.md`
- `docs/compatibility/functions-system.md`
- `docs/compatibility/sql-show-statements.md`
- `docs/compatibility/runtime-system-variables.md`

Document only the admitted subset. Do not overclaim secondary-index
auto-increment, mixed-mode gaps, `ALTER TABLE ... AUTO_INCREMENT`,
information-schema metadata, generated invisible primary keys,
`NO_AUTO_VALUE_ON_ZERO`, protocol insert ids, replication behavior, or full
InnoDB lock-mode semantics.
