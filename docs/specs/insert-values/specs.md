# INSERT ... VALUES

## Scope

This feature makes the base `INSERT ... VALUES` form executable for user
tables created by MyLite's supported `CREATE TABLE` subset.

In scope:

- `INSERT [INTO] table_name VALUES (...) [, (...)]`
- `INSERT [INTO] table_name VALUE (...) [, (...)]`
- `INSERT [INTO] table_name VALUES ROW(...) [, ROW(...)]`
- optional column lists, including the empty all-default column list
- schema-qualified and selected-schema target resolution
- validation for missing schema, missing table, system-schema targets,
  duplicate target columns, unknown target columns, wrong row arity, explicit
  `NULL` into `NOT NULL`, and missing/defaulted `NOT NULL` values without a
  default
- deterministic literal values and deterministic defaults already represented
  by the supported `CREATE TABLE` metadata
- row value expressions from MyLite's shared scalar expression subset,
  including arithmetic, scalar function calls, session-stable temporal
  functions, `RAND()`, `UUID()`, user and system variables, and expression
  warning promotion
- `DEFAULT` value tokens, default all-column rows, nullable implicit defaults,
  literal defaults, unary numeric defaults, string defaults, `NULL` defaults,
  and `CURRENT_TIMESTAMP` defaults
- `AUTO_INCREMENT` allocation for omitted, `NULL`, `0`, and `DEFAULT`
  auto-increment values, explicit values, table-option initial values,
  statement affected rows, and session last insert id
- physical SQLite writes through the catalog-derived physical table name
- atomic multi-row inserts

Out of scope:

- `INSERT ... SET`
- `INSERT ... SELECT`, `INSERT ... TABLE`, and standalone `VALUES`
- priority modifiers, `DELAYED`, partitions, and insert-from-query sources
- `INSERT IGNORE`, row aliases, and `ON DUPLICATE KEY UPDATE` are implemented
  in follow-up scoped feature slices
- expression forms outside MyLite's shared scalar expression evaluator
- arbitrary generated default-expression evaluation, including function calls
  such as `concat('a','b')`
- MySQL's complete type conversion, range enforcement, truncation warnings,
  character-set conversion, and temporal validation
- trigger, generated-column, foreign-key, privilege, and warning-record
  behavior
- full unique/index enforcement beyond the deterministic primary/unique checks
  implemented for inserted values in this task
- `auto_increment_increment`, `auto_increment_offset`, overflow behavior, and
  replication-specific auto-increment locking semantics

## Sources

- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `VALUES` statement:
  https://dev.mysql.com/doc/refman/8.4/en/values.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- Existing MyLite specs:
  - `docs/specs/schema-lifecycle/specs.md`
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/column-attributes/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, including Jan's supplemental Task 13 probes and focused
  live checks for syntax, defaults, diagnostics, and atomicity.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

### Syntax

MySQL accepts `INTO` as optional in this form:

```sql
INSERT t VALUES (10, 20);
INSERT INTO t VALUES (10, 20);
```

`VALUE` is a synonym for `VALUES` in an `INSERT` statement and is accepted for
single-row and multi-row inserts. The table-value constructor spelling is also
accepted for inserts:

```sql
INSERT INTO t VALUE (30, 40);
INSERT INTO t VALUES ROW(50, 60), ROW(70, 80);
```

Rows may be all-default rows. MySQL accepts both an explicit empty column list
and, for this insert form, an empty value list without a column list:

```sql
INSERT INTO ai () VALUES ();
INSERT INTO ai VALUES ();
```

### Default values and nullability

When a column list omits table columns, omitted columns receive their explicit
default, `NULL` when nullable, or the next auto-increment value for an
auto-increment column. In the verified strict SQL mode, omitting a required
`NOT NULL` column without a default fails with error 1364. Using a `DEFAULT`
token for such a column fails the same way. Explicit `NULL` into a non-auto
`NOT NULL` column fails with error 1048.

For a table:

```sql
CREATE TABLE ai(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  v VARCHAR(10) DEFAULT 'd',
  n INT NOT NULL DEFAULT 7,
  nullable INT,
  ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) AUTO_INCREMENT=10;
```

`INSERT INTO ai (v) VALUES ('a'), ('b')` inserts ids `10` and `11`,
uses defaults for `n` and `ts`, stores `NULL` in `nullable`, reports
`ROW_COUNT()=2`, and sets `LAST_INSERT_ID()` to `10`. Subsequent all-default
forms allocate following ids.

For a table with `DEFAULT (1 + 2)`, `DEFAULT (concat('a','b'))`, and
`DEFAULT (CURRENT_TIMESTAMP)`, MySQL evaluates all generated default
expressions at write time. MyLite defers arbitrary generated default-expression
evaluation until expression and function execution are implemented; only
deterministic literal/unary/current-timestamp default forms are in scope now.

### AUTO_INCREMENT

For the supported default SQL mode, omitted, `NULL`, `0`, and `DEFAULT`
auto-increment values allocate the next sequence value. Explicit nonzero values
are stored as written and advance the sequence when they are greater than or
equal to the current next value after their row is accepted. The first generated
value in a multi-row statement reserves a statement-sized interval from the
current next value, matching MySQL's simple-insert allocation gaps when explicit
high values appear before generated rows. `LAST_INSERT_ID()` is
connection-local and records the first automatically generated value from a row
accepted by the statement. Explicit nonzero values do not set it, and a
generated value allocated for a row that fails duplicate validation before being
inserted does not replace the previous session value.

When `NO_AUTO_VALUE_ON_ZERO` is enabled, explicit numeric `0` is stored as `0`
instead of allocating a generated value. `NULL`, omitted values, and `DEFAULT`
continue to allocate. See
[NO_AUTO_VALUE_ON_ZERO SQL mode](../no-auto-value-on-zero/specs.md).

With `AUTO_INCREMENT=3`, this probe:

```sql
INSERT INTO ai_explicit VALUES (NULL,10),(0,20),(5,50),(NULL,60);
```

inserts ids `3`, `4`, `5`, and `6`, reports `ROW_COUNT()=4`, and sets
`LAST_INSERT_ID()` to `3`. The next generated value is `7`.

When a multi-row insert later fails, data changes roll back, but generated
auto-increment allocation side effects remain observable. For example, with
`AUTO_INCREMENT=10`, `INSERT INTO ai VALUES (NULL,1),(NULL,1)` against a unique
`u` column fails, leaves the table empty, records the first generated value
`10`, and makes the next successful generated value `12`.

If an accepted explicit high value is followed by a generated row that fails
duplicate validation, MySQL still advances the next generated value, but
`LAST_INSERT_ID()` remains unchanged because the generated row was not accepted.

### Diagnostics

Verified MySQL 8.4.9 diagnostics for the scoped behavior:

- no selected schema: 1046, `No database selected`
- qualified missing schema: 1049, `Unknown database 'schema'`
- target table missing in an existing schema: 1146,
  `Table 'schema.table' doesn't exist`
- system schema target: 1044 / `42000`, access denied before mutation
- duplicate target column list, case-insensitive: 1110,
  `Column 'a' specified twice`
- unknown target column: 1054,
  `Unknown column 'missing_col' in 'field list'`
- wrong value count: 1136,
  `Column count doesn't match value count at row 1`
- missing/defaulted required value: 1364,
  `Field 'a' doesn't have a default value`
- explicit `NULL` into required non-auto column: 1048,
  `Column 'a' cannot be null`
- duplicate primary key value: 1062, duplicate-entry diagnostic
- composite primary or unique key duplicate value: 1062, duplicate-entry
  diagnostic with key parts joined by `-`; nullable unique-key parts containing
  `NULL` remain distinct and do not conflict

For transactional InnoDB tables, a later row failure in a multi-row insert
leaves no partial rows behind. MyLite must preserve this all-or-nothing behavior
for its embedded SQLite storage.

## MyLite behavior

### Parser and AST

MyLite adds an `insert_values_statement` AST node. Children are appended as:

1. target table name
2. column-name list, when one is present, including an empty explicit `()`
3. row list

When the column list is omitted, the row list is the second child. Runtime
copying identifies the optional list by AST node kind rather than by assuming a
placeholder child.

The statement node records no priority, ignore, delayed, partition, alias, or
duplicate-key modifiers in this task. Unsupported modifiers remain parse errors
until specified.

Each row node stores its value children in source order. A value may be any
expression accepted by the current shared scalar expression subset or
`DEFAULT`. Runtime execution supports that shared subset for row values and
still treats generated default-expression evaluation as a separate boundary.

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
insert_values_statement ::= INSERT opt_into table_name opt_insert_column_list
                            insert_values_keyword insert_row_list.

opt_into ::= .
opt_into ::= INTO.

opt_insert_column_list ::= .
opt_insert_column_list ::= LPAREN RPAREN.
opt_insert_column_list ::= LPAREN insert_column_list RPAREN.

insert_column_list ::= identifier.
insert_column_list ::= insert_column_list COMMA identifier.

insert_values_keyword ::= VALUES.
insert_values_keyword ::= VALUE.

insert_row_list ::= insert_row.
insert_row_list ::= insert_row_list COMMA insert_row.

insert_row ::= LPAREN opt_insert_value_list RPAREN.
insert_row ::= ROW LPAREN opt_insert_value_list RPAREN.

opt_insert_value_list ::= .
opt_insert_value_list ::= insert_value_list.

insert_value_list ::= insert_value.
insert_value_list ::= insert_value_list COMMA insert_value.

insert_value ::= expression.
insert_value ::= DEFAULT.
```

### Runtime execution

Preparing a parsed `INSERT ... VALUES` creates a custom statement handle. The
first `mylite_step()` performs validation and side effects; later steps return
`MYLITE_DONE`.

Target resolution:

- Qualified `schema.table` targets the written schema.
- Unqualified table names use the selected default schema.
- A missing selected schema fails with `No database selected`.
- A missing explicit schema fails with `Unknown database 'schema'`.
- A system schema target is rejected with an access-denied diagnostic.
- A missing table in an existing schema fails with
  `Table 'schema.table' doesn't exist`.

Column resolution:

- Column metadata is loaded from `__mylite_column_catalog` ordered by
  `ORDINAL_POSITION`.
- Insert-list names are matched case-insensitively to catalog column names.
  The physical SQLite insert uses the canonical catalog name.
- Duplicate target columns are rejected case-insensitively before mutation.
- Unknown target columns are rejected before mutation.
- An omitted column list maps nonempty rows to all table columns in ordinal
  order. An omitted column list plus an empty row maps to the all-default row.
- An explicit empty column list requires empty rows and maps to the all-default
  row.
- Wrong arity is reported with the 1-based row number.

Value resolution:

- Literal strings are unescaped using the same MyLite literal helper as DDL
  metadata.
- Integer, decimal, float, `TRUE`, and `FALSE` literals are bound as
  deterministic scalar values. Full MySQL type coercion is deferred.
- Non-`DEFAULT` row expressions are evaluated through the shared expression
  evaluator before type binding. Supported examples include arithmetic
  expressions, parenthesized expressions, `CONCAT()`, numeric scalar
  functions such as `ABS()` and `ROUND()`, temporal functions such as `NOW()`
  and `TIMESTAMP()`, `RAND()`, `UUID()`, user variables, and supported system
  variables.
- Expression warnings raised while evaluating row values are promoted using
  the strict DML policy. For example, `SQRT('9x')` fails the insert, records
  the truncation warning, and rolls back the candidate row.
- `NULL` is accepted only for nullable columns and for auto-increment columns.
- `DEFAULT` requests the column's default behavior.
- Omitted nullable columns store `NULL`.
- Omitted or defaulted non-null columns without a default fail unless they are
  auto-increment columns.
- `CURRENT_TIMESTAMP` and bare `NOW()` defaults are evaluated at execution
  time as non-null timestamp strings. Fractional precision is emitted up to the
  target temporal column precision.
- Parenthesized `CURRENT_TIMESTAMP` and `NOW()` defaults are supported and
  stored as MySQL-style `now()` generated defaults.
- Other generated default expressions fail with a deterministic unsupported
  generated-default diagnostic when they are needed for a row.

Storage and side effects:

- The physical table name is derived using the same hex-encoded naming scheme
  as `CREATE TABLE`.
- Each row inserts all table columns in catalog order, after defaults and
  auto-increment allocation are resolved.
- Inserts execute inside one SQLite transaction. Any validation, binding,
  duplicate-key, or SQLite failure rolls back the whole statement.
- `__mylite_table_catalog.AUTO_INCREMENT` is updated to the next sequence value
  when the table has an auto-increment column. Generated values consumed before
  a failed row remain consumed after the row transaction rolls back.
- Statement affected rows are set to the inserted row count after a successful
  insert.
- The session last insert id is updated to the first generated
  auto-increment value from an accepted row. Statements that generate no
  accepted auto-increment value leave the session value unchanged, even when a
  generated value was allocated for a row that then failed before insertion.

### Metadata and public API

`INSERT ... VALUES` does not produce a result set. The public API gains minimal
accessors for write metadata:

```c
int64_t mylite_affected_rows(const mylite_stmt *stmt);
uint64_t mylite_last_insert_id(const mylite_db *database);
```

`mylite_affected_rows()` is statement-owned and reports the affected rows for
the executed statement. `mylite_last_insert_id()` is handle-owned session state
and survives statement finalization.

### Explicit deferred behavior

MyLite intentionally documents these boundaries for Task 13:

- Arbitrary generated default expressions are not evaluated yet. This includes
  arithmetic expressions such as `(1 + 2)` and function calls such as
  `concat('a','b')`.
- Row value expression forms not yet supported by the shared scalar evaluator
  fail with the evaluator's deterministic expression diagnostic.
- Full MySQL type conversion is deferred. Current storage binds deterministic
  scalar values into SQLite columns and relies on SQLite affinity for simple
  numeric text.
- Warning records and information strings such as
  `Records: N Duplicates: 0 Warnings: 0` are deferred.
- Generated columns are deferred because generated-column DDL and runtime
  support do not exist yet.
- Row aliases and `ON DUPLICATE KEY UPDATE` are implemented by the scoped ODKU
  feature. This base `INSERT ... VALUES` slice still owns candidate-row
  construction and insert-path side effects.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `INSERT t VALUES (10, 20)` | Accepted with optional `INTO`. |
| `INSERT INTO t VALUE (30, 40)` | Accepted; `VALUE` is a synonym. |
| `INSERT INTO t VALUES ROW(50,60), ROW(70,80)` | Inserts both rows. |
| `INSERT INTO ai (v) VALUES ('a'), ('b')` | Inserts two rows, defaults omitted columns, affected rows `2`, last insert id first generated id. |
| `INSERT INTO ai VALUES (DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)` | Allocates auto id and applies defaults. |
| `INSERT INTO ai () VALUES ()` | Inserts all-default row. |
| `INSERT INTO ai VALUES ()` | Inserts all-default row. |
| `INSERT INTO ai VALUES (NULL,1),(NULL,1)` with unique `u` | Rolls back rows, records first generated id, and consumes generated ids. |
| `INSERT INTO ai VALUES (20,1),(NULL,1)` with unique `u` | Rolls back rows, consumes the generated id interval, and leaves the previous last insert id unchanged. |
| `INSERT INTO ai VALUES (20,1),(NULL,2)` with `AUTO_INCREMENT=10` | Stores generated id `21`; the next generated value after the statement is `23`. |
| `INSERT INTO ai_explicit VALUES (NULL,10),(0,20),(5,50),(NULL,60)` | Allocates generated ids around explicit value and records first generated id. |
| `INSERT INTO composite_pk VALUES (1,2,'x')` followed by duplicate `(1,2)` | Fails with duplicate entry `1-2` for key `PRIMARY`; `INSERT IGNORE` demotes the duplicate to warning 1062 and leaves the row unchanged. |
| composite nullable unique key with `(NULL,1)`, `(NULL,1)`, `(1,NULL)`, `(1,NULL)`, `(1,2)` | Accepts rows containing `NULL` key parts and rejects a duplicate `(1,2)` with duplicate entry `1-2`. |
| `INSERT INTO dflt (nn,nd,nul,txt) VALUES (1, DEFAULT, DEFAULT, DEFAULT)` | Stores `(1,9,NULL,'hello')`. |
| `INSERT INTO dflt (nn) VALUES (DEFAULT)` | Fails because `nn` has no default. |
| `INSERT INTO scalar_insert (...) VALUES (CONCAT('a','b'), ABS(-3), ROUND(12.345,2), TIMESTAMP(...), RAND(7), UUID())` | Evaluates supported scalar expressions and stores MySQL-compatible values. |
| `INSERT INTO scalar_insert (...) VALUES (CONCAT('c','d'), 2 + 5, UNIX_TIMESTAMP('1970-01-01 00:00:01'), NOW(6), RAND(7), UUID())` | Evaluates arithmetic, epoch, temporal, random, and UUID row expressions. |
| `INSERT INTO scalar_insert (id,n) VALUES (3, SQRT('9x'))` | Fails under strict DML warning promotion and rolls back the row. |
| no selected schema | Execution error containing `No database selected`. |
| qualified missing schema | Execution error containing `Unknown database`. |
| missing table in existing schema | Execution error containing `doesn't exist`. |
| system schema target | Execution error containing `system schema`. |
| duplicate target columns, including quoted/unquoted case variants | Execution error containing `specified twice`. |
| unknown target column | Execution error containing `Unknown column`. |
| wrong value count | Execution error containing `Column count doesn't match value count at row 1`. |
| explicit `NULL` into required non-auto column | Execution error containing `cannot be null`. |
| missing required non-auto column | Execution error containing `doesn't have a default value`. |
| later row failure in multi-row insert | No physical rows are inserted. |
| unsupported generated default expression needed during insert | Deterministic MyLite execution error; no rows inserted. |

## Test plan

- Parser tests:
  - optional `INTO`
  - `VALUE` synonym
  - multi-row ordinary value lists
  - `VALUES ROW(...)` constructors
  - column lists, empty column lists, and empty rows
  - `DEFAULT`, `NULL`, strings, numbers, booleans, unary numeric values, and
    `CURRENT_TIMESTAMP` values
  - malformed missing rows, trailing commas, and unsupported modifiers
- Runtime tests:
  - successful insert into a physical SQLite table through default schema and
    schema-qualified targets
  - optional `INTO`, `VALUE`, and `ROW(...)`
  - column-list case-insensitive resolution
  - defaults for omitted, defaulted, and all-default rows
  - auto-increment omitted, `NULL`, `0`, `DEFAULT`, explicit values, sequence
    advancement, affected rows, and session last insert id
  - failed generated rows that consume sequence values without changing the
    session last insert id
  - missing schema/table/system schema diagnostics
  - duplicate and unknown target-column diagnostics
  - composite primary-key and unique-key duplicate diagnostics, including
    nullable unique-key parts containing `NULL`
  - wrong row arity diagnostics with row number
  - required-column and explicit-null diagnostics
  - atomic multi-row rollback
  - deterministic unsupported generated-default behavior
  - scalar row expressions, statement-stable temporal functions, random and
    UUID functions, and strict warning promotion
