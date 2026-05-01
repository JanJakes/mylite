# Column attributes

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` column-definition
foundation so supported column types may be followed by common MySQL column
attributes:

- `NULL` and `NOT NULL`
- literal `DEFAULT` values
- parenthesized expression defaults over MyLite's current expression subset
- bare `DEFAULT CURRENT_TIMESTAMP`, `DEFAULT CURRENT_TIMESTAMP()`, and
  `DEFAULT CURRENT_TIMESTAMP(fsp)`
- `ON UPDATE CURRENT_TIMESTAMP`, `ON UPDATE CURRENT_TIMESTAMP()`, and
  `ON UPDATE CURRENT_TIMESTAMP(fsp)`
- `COMMENT 'string'`
- `VISIBLE` and `INVISIBLE`
- `COLUMN_FORMAT DEFAULT`, `COLUMN_FORMAT FIXED`, and `COLUMN_FORMAT DYNAMIC`
- `STORAGE DEFAULT`, `STORAGE DISK`, and `STORAGE MEMORY`

The task remains parse-only. Valid `CREATE TABLE` statements covered by this
feature prepare as `MYLITE_UNSUPPORTED`; no SQLite table is created and no
MyLite catalog rows are written. Executable table DDL, default evaluation,
warning records, catalog storage, generated columns, `AUTO_INCREMENT`, inline
keys, references, checks, table constraints, and table options remain deferred.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/mysql/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Automatic Initialization and Updating for
  `TIMESTAMP` and `DATETIME`:
  https://dev.mysql.com/doc/refman/8.4/en/timestamp-initialization.html
- MySQL 8.4 Reference Manual, Invisible Columns:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-columns.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

### Attribute normalization

Runtime probes against MySQL 8.4.9 show these representative normalizations:

| Declaration fragment | MySQL normalized behavior |
| --- | --- |
| `INT NULL` | nullable column, `SHOW CREATE TABLE` contains `int DEFAULT NULL` |
| `INT NOT NULL` | non-nullable column, no implicit displayed default |
| `INT DEFAULT 7` | `SHOW CREATE TABLE` displays `DEFAULT '7'`; `COLUMN_DEFAULT` is `7` |
| `INT DEFAULT -1` | accepted; default metadata is `-1` |
| `INT DEFAULT +2` | accepted; default metadata is `2` |
| `INT DEFAULT 0x10` | accepted; default metadata is `16` |
| `INT DEFAULT b'101'` | accepted; default metadata is `5` |
| `DECIMAL(5,2) DEFAULT -1.25` | accepted |
| `DOUBLE DEFAULT +1.25e2` | accepted |
| `VARCHAR(20) DEFAULT 'abc'` | string default metadata is `abc` |
| `VARCHAR(20) DEFAULT ''` | empty-string default is accepted |
| `INT DEFAULT (1 + 2)` | expression default, `EXTRA=DEFAULT_GENERATED` |
| `b INT DEFAULT (a + 1)` after `a INT` | expression default may reference an earlier column |
| `TIMESTAMP DEFAULT CURRENT_TIMESTAMP` | accepted; `EXTRA=DEFAULT_GENERATED` |
| `TIMESTAMP(3) DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3)` | accepted; generated default plus on-update metadata |
| `TIMESTAMP DEFAULT CURRENT_TIMESTAMP()` | accepted and normalized like bare `CURRENT_TIMESTAMP` |
| `TIMESTAMP DEFAULT (CURRENT_TIMESTAMP)` | accepted and shown as a parenthesized `now()` expression |
| `INT COMMENT 'hello'` | comment text appears in `COLUMN_COMMENT` |
| `INT INVISIBLE` with another visible column | `SHOW CREATE TABLE` carries an invisible marker; `EXTRA=INVISIBLE` |
| `INT VISIBLE` | normalizes as an ordinary visible column |
| `COLUMN_FORMAT DEFAULT STORAGE DEFAULT` | accepted and normalized away |
| `COLUMN_FORMAT FIXED STORAGE DISK` | retained in `SHOW CREATE TABLE` version comments |
| `COLUMN_FORMAT DYNAMIC STORAGE MEMORY` | retained in `SHOW CREATE TABLE` version comments |

`INFORMATION_SCHEMA.COLUMNS` in the verified MySQL 8.4.9 runtime exposes
visibility through `EXTRA`; it does not include an `IS_VISIBLE` column.

Attribute order is flexible for this feature surface. MySQL accepts
`COMMENT 'x' DEFAULT 1 NOT NULL VISIBLE COLUMN_FORMAT DYNAMIC STORAGE MEMORY`
and normalizes the attributes to its own display order.

### Repetition and conflicts

MySQL accepts repeated or conflicting declarations for several attributes and
uses the last relevant declaration during normalization:

- `INT NULL NOT NULL` is accepted and becomes `NOT NULL`.
- `INT NOT NULL NULL` is accepted and becomes nullable.
- `INT DEFAULT 1 DEFAULT 2` is accepted and the effective default is `2`.
- `VISIBLE INVISIBLE` is accepted when the table still has another visible
  column; the column becomes invisible.
- `INVISIBLE VISIBLE` is accepted and the column becomes visible.

Some conflicts are semantic DDL errors rather than syntax errors:

- `INT NOT NULL DEFAULT NULL` and `INT DEFAULT NULL NOT NULL` error with
  invalid-default diagnostics.
- A one-column table declared as `INT INVISIBLE` errors because a table must
  have at least one visible column.
- `TIMESTAMP(3) DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` errors
  because explicit fractional seconds precision must agree across the column
  declaration.
- `INT DEFAULT CURRENT_TIMESTAMP` parses but errors semantically because the
  automatic timestamp default is not valid for the type.

MyLite records the syntax now and defers these semantic table-DDL validations
until executable `CREATE TABLE` exists.

### Syntax errors and unsupported expression grammar

MySQL rejects the following as syntax:

- `COMMENT 123`
- `COMMENT = 'x'`
- `DEFAULT = 1`
- signed nonnumeric literal defaults such as `DEFAULT +'abc'`,
  `DEFAULT +0x10`, `DEFAULT -NULL`, and `DEFAULT +TRUE`
- `COLUMN_FORMAT COMPRESSED`
- `COLUMN_FORMAT = FIXED`
- `STORAGE FLASH`
- `STORAGE = DISK`
- `TIMESTAMP ON UPDATE`
- `ON CURRENT_TIMESTAMP`
- `ON UPDATE 1`
- `DEFAULT UPPER('x')`
- malformed `CURRENT_TIMESTAMP` precision forms such as
  `CURRENT_TIMESTAMP(-1)`, `CURRENT_TIMESTAMP('1')`, and
  `CURRENT_TIMESTAMP(1,2)`
- oversized `CURRENT_TIMESTAMP(18446744073709551616)` precision tokens

MySQL accepts `VARCHAR(20) DEFAULT (UPPER('x'))`, but MyLite's current
expression grammar has no general function-call expressions. This task
therefore supports parenthesized defaults using the expression subset already
implemented by MyLite plus targeted `CURRENT_TIMESTAMP` expressions; generic
function-call defaults, `INTERVAL`, subqueries, variables, parameters, stored
functions, loadable functions, and full default-expression semantic validation
are deferred.

MySQL also accepts introducer-prefixed string defaults such as
`_utf8mb4'abc'`. MyLite string introducers are not part of the current lexer
grammar and are deferred. Types outside the implemented type set, such as
`BIT`, remain deferred even when their default literal syntax is otherwise
known.

### Keyword treatment

The nonreserved attribute words `comment`, `visible`, `invisible`, `storage`,
`column_format`, `disk`, `memory`, and `dynamic` are valid unquoted identifiers
in MySQL 8.4.9. `fixed` was already handled as a nonreserved type keyword.

The reserved words `default`, `current_timestamp`, `on`, `update`, `null`, and
`not` are not accepted as unquoted column names by this task.

## MyLite behavior

### Parser and AST

MyLite extends the current narrow `CREATE TABLE` grammar from:

```sql
CREATE TABLE table_name (
    column_name column_type
    [, column_name column_type ...]
)
```

to:

```sql
CREATE TABLE table_name (
    column_name column_type column_attribute_list
    [, column_name column_type column_attribute_list ...]
)
```

The column definition AST gains a third child: a column-attribute list. The list
is present even when empty so later table-DDL work can consume one consistent
shape. Each accepted attribute is represented as a child node in source order.

MyLite performs parser-time validation for syntax-shape errors covered by this
task:

- comments require string literals
- `COLUMN_FORMAT` accepts only `DEFAULT`, `FIXED`, or `DYNAMIC`
- `STORAGE` accepts only `DEFAULT`, `DISK`, or `MEMORY`
- `ON UPDATE` requires a `CURRENT_TIMESTAMP` value
- `CURRENT_TIMESTAMP(fsp)` accepts integer precision `0..6` only and rejects
  overflow without wrapping

Semantic DDL checks wait for executable table DDL. Examples include last
attribute wins, invalid default for a specific type, all-invisible table
validation, matching `TIMESTAMP`/`DATETIME` precision across type/default/update
clauses, generated default metadata, warning records, `SHOW CREATE TABLE`
formatting, and `INFORMATION_SCHEMA.COLUMNS` rows.

### Runtime boundary

Preparing a valid parse-only `CREATE TABLE` statement covered by this task
returns `MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No table or catalog side
effect occurs. Malformed or explicitly unsupported syntax remains
`MYLITE_PARSE_ERROR`.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
column_definition ::= identifier column_type column_attribute_list.

column_attribute_list ::= .
column_attribute_list ::= column_attribute_list column_attribute.

column_attribute ::= NULL.
column_attribute ::= NOT NULL.
column_attribute ::= DEFAULT column_default_value.
column_attribute ::= ON UPDATE current_timestamp_value.
column_attribute ::= COMMENT STRING.
column_attribute ::= VISIBLE.
column_attribute ::= INVISIBLE.
column_attribute ::= COLUMN_FORMAT column_format_value.
column_attribute ::= STORAGE column_storage_value.

column_format_value ::= DEFAULT.
column_format_value ::= FIXED.
column_format_value ::= DYNAMIC.

column_storage_value ::= DEFAULT.
column_storage_value ::= DISK.
column_storage_value ::= MEMORY.

column_default_value ::= literal.
column_default_value ::= PLUS numeric_literal.
column_default_value ::= MINUS numeric_literal.
column_default_value ::= current_timestamp_value.
column_default_value ::= LPAREN expression RPAREN.

numeric_literal ::= INTEGER.
numeric_literal ::= DECIMAL.
numeric_literal ::= FLOAT.

expression ::= current_timestamp_value.

current_timestamp_value ::= CURRENT_TIMESTAMP.
current_timestamp_value ::= CURRENT_TIMESTAMP LPAREN RPAREN.
current_timestamp_value ::= CURRENT_TIMESTAMP LPAREN INTEGER RPAREN.
```

The `expression` production referenced here is MyLite's current expression
subset: literals, qualified identifiers, parenthesized expressions, unary `+`
and `-`, and binary `+`, `-`, `*`, and `/`, with the targeted
`CURRENT_TIMESTAMP` expression added by this task.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL fragment | Expected MyLite parse behavior |
| --- | --- |
| `a INT NULL`, `a INT NOT NULL` | parse OK |
| `a INT NULL NOT NULL`, `a INT NOT NULL NULL` | parse OK; semantic last-wins behavior deferred |
| `a INT DEFAULT 7`, `a INT DEFAULT -1`, `a INT DEFAULT +2` | parse OK |
| `a DECIMAL(5,2) DEFAULT -1.25`, `a DOUBLE DEFAULT +1.25e2` | parse OK |
| `a INT DEFAULT 0x10`, `a INT DEFAULT b'101'` | parse OK for existing hex and bit literal tokens |
| `a VARCHAR(20) DEFAULT ''`, `a VARCHAR(20) DEFAULT 'abc'` | parse OK |
| `a INT DEFAULT (1 + 2)`, `a INT, b INT DEFAULT (a + 1)` | parse OK for current expression subset |
| `a TIMESTAMP DEFAULT CURRENT_TIMESTAMP` | parse OK |
| `a TIMESTAMP DEFAULT CURRENT_TIMESTAMP()` | parse OK |
| `a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(0) ON UPDATE CURRENT_TIMESTAMP(0)` | parse OK |
| `a TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6)` | parse OK |
| `a TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6)` | parse OK |
| `a TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` | parse OK |
| `a TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6)` | parse OK |
| `a TIMESTAMP DEFAULT (CURRENT_TIMESTAMP)` | parse OK |
| `a INT DEFAULT 1 DEFAULT 2` | parse OK; semantic last-wins behavior deferred |
| `a INT COMMENT 'hello'` | parse OK |
| `a INT VISIBLE`, `a INT INVISIBLE` | parse OK; all-invisible table validation deferred |
| `a INT COLUMN_FORMAT DEFAULT/FIXED/DYNAMIC` | parse OK |
| `a INT STORAGE DEFAULT/DISK/MEMORY` | parse OK |
| nonreserved attribute words used as column names | parse OK |
| `a INT COMMENT 123` | parse error |
| `a INT COMMENT = 'x'` | parse error |
| `a INT DEFAULT = 1` | parse error |
| `a INT COLUMN_FORMAT COMPRESSED` | parse error |
| `a INT COLUMN_FORMAT = FIXED` | parse error |
| `a INT STORAGE FLASH` | parse error |
| `a INT STORAGE = DISK` | parse error |
| `a TIMESTAMP ON UPDATE` | parse error |
| `a TIMESTAMP ON CURRENT_TIMESTAMP` | parse error |
| `a TIMESTAMP ON UPDATE 1` | parse error |
| `a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(7)` | parse error for this task |
| `a TIMESTAMP ON UPDATE CURRENT_TIMESTAMP(7)` | parse error for this task |
| malformed or overflow `CURRENT_TIMESTAMP(...)` | parse error |
| signed nonnumeric literal defaults | parse error |
| `a VARCHAR(20) DEFAULT UPPER('x')` | parse error |
| `a VARCHAR(20) DEFAULT (UPPER('x'))` | parse error until function-call expressions land |
| `a INT AUTO_INCREMENT`, inline keys, references, checks, generated columns | parse error until later roadmap tasks |

Runtime tests should verify that valid covered `CREATE TABLE` statements
prepare as `MYLITE_UNSUPPORTED` and leave `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` without user-table side effects.

## Compatibility gaps

- Executable `CREATE TABLE`, catalog writes, table storage, implicit commits,
  warning records, `SHOW CREATE TABLE`, and information-schema column rows are
  deferred.
- Default evaluation, type coercion, SQL-mode-sensitive defaults, and invalid
  default diagnostics are deferred.
- Last-wins normalization for repeated `NULL`, `NOT NULL`, `DEFAULT`,
  visibility, `COLUMN_FORMAT`, and `STORAGE` attributes is deferred to
  executable DDL.
- All-invisible table validation is deferred.
- `TIMESTAMP` and `DATETIME` default/on-update precision consistency,
  time-zone behavior, `explicit_defaults_for_timestamp`, and null-assignment
  behavior are deferred.
- Generic function-call expressions, `INTERVAL`, subqueries, variables,
  parameters, stored functions, loadable functions, and default-expression
  semantic restrictions are deferred.
- Introducer-prefixed strings and types not yet implemented by earlier roadmap
  tasks remain deferred.
- Generated columns, `AUTO_INCREMENT`, inline indexes and keys, foreign-key
  references, `CHECK` constraints, `SERIAL DEFAULT VALUE`, and table options are
  deferred to later roadmap tasks.
