# Baseline Integer Expression Defaults

This feature adds a narrow expression-default slice for descriptor-owned
persistent base tables. It is intended to cover common `CREATE TABLE` shapes
such as `INT NOT NULL DEFAULT (1 + 2)` without claiming general MySQL expression
defaults.

The feature builds on the existing literal-default, integer-family type,
`ALTER COLUMN SET/DROP DEFAULT`, descriptor cloning, `SHOW COLUMNS`,
`SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and DML `DEFAULT` keyword
support.

## Compatibility Authority

Normative behavior comes from:

- MySQL 8.4 Reference Manual, data type default values:
  <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Runtime probes against MySQL `8.4.9`, captured by
  `packages/libmylite/tests/mysql_baseline_integer_expression_defaults_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `DEFAULT (1 + 2)` is accepted in column definitions.
- `DEFAULT 1 + 2` is a syntax error.
- `ALTER TABLE t ALTER a SET DEFAULT (2 * 5)` is accepted.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report expression defaults
  with `EXTRA = DEFAULT_GENERATED`.
- `SHOW CREATE TABLE` renders expression defaults as `DEFAULT ((expr))`, while
  `DEFAULT (NULL)` renders as `DEFAULT (NULL)`.
- Omitted-column inserts and `DEFAULT` DML values evaluate the expression and
  report `ROW_COUNT() = 1`, `@@warning_count = 0` for in-range deterministic
  values.
- `INT NOT NULL DEFAULT (NULL)` is accepted at DDL time; later omitted/default
  inserts fail with `ERROR 1048 (23000): Column 'a' cannot be null`.
- MySQL accepts some expression defaults whose result is out of range and
  raises range errors when the default is materialized.

## Supported Surface

MyLite supports:

- persistent base-table descriptors only;
- `DEFAULT (expr)` in supported column definitions for integer-family
  descriptor columns;
- `ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT (expr)` for one
  unqualified integer-family descriptor column;
- `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`, `ALTER TABLE ... MODIFY
  [COLUMN]`, `ALTER TABLE ... CHANGE [COLUMN]`, `CREATE TABLE ... LIKE`, and
  the current descriptor-copying `CREATE TABLE ... SELECT` paths when they use
  the existing column-definition or descriptor-clone machinery;
- integer-family target descriptors already supported by MyLite:
  `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, their admitted
  aliases, and their admitted `UNSIGNED` forms within MyLite's current signed
  64-bit physical row-value envelope;
- constant expression operands limited to decimal integer literals with optional
  unary sign and `NULL`;
- operators limited to unary `+`, unary `-`, binary `+`, `-`, `*`, `DIV`, `%`,
  infix `MOD`, nested parentheses, and `MOD(left, right)`;
- expression results that are either `NULL` or an in-range integer for the
  target descriptor at statement planning time;
- metadata that preserves the expression-default identity through
  `SHOW COLUMNS`, `SHOW CREATE TABLE`, `DESCRIBE`, `EXPLAIN table`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- omitted-column `INSERT`, explicit `DEFAULT` in supported `INSERT`, `REPLACE`,
  and one-assignment `UPDATE` paths, and descriptor cloning with the stored
  expression default.

This slice intentionally does not support:

- general expression defaults;
- expression defaults for `DECIMAL`, approximate, temporal, `CHAR`, `VARCHAR`,
  `TEXT`, binary, JSON, spatial, enum, set, or generated columns;
- string, decimal, approximate, hex, bit, temporal, or boolean expression
  operands beyond existing literal-default support;
- functions other than `MOD(left, right)`;
- column references, subqueries, parameters, user variables, system variables,
  stored functions, loadable functions, arbitrary built-ins, CTEs, or casts;
- current timestamp defaults, `ON UPDATE`, generated columns, check
  constraints, triggers, cascades, privileges, replication warnings, or SQL-mode
  dependent evaluation;
- delayed range checking for out-of-range expression results. MyLite currently
  rejects out-of-range evaluated expression defaults during planning instead of
  accepting the DDL and failing only when the default is materialized.

## Ownership Boundaries

- Public API: no ABI or public header changes. Successful statements use the
  existing non-row result conventions. Errors and warnings flow through existing
  diagnostics.
- Parser/AST: accept `DEFAULT (expr)` as a column default value. The parser does
  not decide whether the expression is in the supported subset.
- Analyzer/planner: validates that the expression is constant, integer-family,
  and inside the admitted operator/literal subset; evaluates deterministic
  results once at DDL/ALTER planning time; records expression metadata in the
  planned column descriptor.
- Catalog: remains authoritative for logical column descriptors. It stores a
  distinct expression-default kind plus the expression text and, for non-`NULL`
  expression defaults, the evaluated integer value.
- Runtime DML: materializes defaults from catalog descriptors. It never
  re-parses the stored expression text during ordinary DML.
- Result builders: render expression-default metadata from descriptor state.
- Storage/VFS: unchanged. Physical SQLite tables store ordinary row values and
  know nothing about MySQL expression-default metadata.
- SQLite: no fork patch and no SQLite expression-default execution. MyLite owns
  the subset and uses public SQLite prepared statements and bound values for
  physical writes.

## Grammar

The MySQL-facing syntax admitted by this feature is:

```text
column_definition:
    column_name integer_column_type column_attributes

column_attribute:
    DEFAULT literal_default
  | DEFAULT NULL
  | DEFAULT ( integer_default_expression )

alter_table_set_default:
    ALTER TABLE table_name ALTER column_name SET DEFAULT default_value
  | ALTER TABLE table_name ALTER COLUMN column_name SET DEFAULT default_value

default_value:
    literal_default
  | NULL
  | ( integer_default_expression )

integer_default_expression:
    decimal_integer_literal
  | NULL
  | + integer_default_expression
  | - integer_default_expression
  | integer_default_expression + integer_default_expression
  | integer_default_expression - integer_default_expression
  | integer_default_expression * integer_default_expression
  | integer_default_expression DIV integer_default_expression
  | integer_default_expression % integer_default_expression
  | integer_default_expression MOD integer_default_expression
  | MOD(integer_default_expression, integer_default_expression)
  | ( integer_default_expression )
```

MyLite Lemon-syntax snippets:

```text
column_default(A) ::= DEFAULT(D) column_default_value(V).

column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}
```

The Lemon grammar intentionally reuses `expression` so syntax is preserved for
future expression-default phases. The planner rejects constructs outside
`integer_default_expression`.

## Semantics

Expression defaults are descriptor defaults. They do not change existing rows
unless the surrounding statement already materializes defaults:

- `CREATE TABLE` stores descriptor metadata only.
- `ALTER TABLE ... ADD [COLUMN]` backfills existing rows through the same
  descriptor-default path as literal defaults.
- `ALTER TABLE ... MODIFY` and `CHANGE` update the descriptor default while
  preserving row values unless an existing row validation or rebuild is already
  required by the wider statement.
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT (expr)` is catalog-only and
  reports zero affected rows and zero warnings.
- `CREATE TABLE ... LIKE` and supported descriptor-copying CTAS preserve the
  stored expression-default kind, expression text, and evaluated value.

For non-`NULL` results, MyLite evaluates the constant expression once when the
column definition or `ALTER ... SET DEFAULT` is planned. The result must fit the
target descriptor's current physical range:

| Target descriptor | Supported expression result range |
| --- | --- |
| signed `TINYINT` | `-128..127` |
| unsigned `TINYINT` | `0..255` |
| signed `SMALLINT` | `-32768..32767` |
| unsigned `SMALLINT` | `0..65535` |
| signed `MEDIUMINT` | `-8388608..8388607` |
| unsigned `MEDIUMINT` | `0..16777215` |
| signed `INT` / `INTEGER` | `-2147483648..2147483647` |
| unsigned `INT` / `INTEGER` | `0..4294967295` |
| signed `BIGINT` | `-9223372036854775808..9223372036854775807` |
| unsigned `BIGINT` | `0..9223372036854775807` in the current physical envelope |

`DEFAULT (NULL)` stores a `NULL` expression-default descriptor. It is allowed
for nullable and non-nullable integer descriptors. Materializing it behaves like
assigning explicit `NULL`: nullable targets store `NULL`; non-nullable targets
raise `1048 (23000)` in strict paths and use the existing `INSERT IGNORE`
implicit zero plus warning path where that path is already supported.

Division or modulo by zero, scalar arithmetic overflow, unsupported operands,
and unsupported operators are invalid defaults for this slice.

## Metadata Rendering

Descriptor rendering rules:

- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` put the stored expression text
  in `Default` and include `DEFAULT_GENERATED` in `Extra`.
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` contains the stored expression
  text and `EXTRA` contains `DEFAULT_GENERATED`.
- `SHOW CREATE TABLE` renders `DEFAULT (` + stored expression text + `)`. For
  stored expression text `(1 + 2)`, this produces `DEFAULT ((1 + 2))`. For
  stored expression text `NULL`, this produces `DEFAULT (NULL)`.
- Auto-increment columns keep the existing MySQL-compatible hidden-default
  rendering behavior; this feature does not add `CREATE TABLE` auto-increment
  defaults.

MyLite preserves source spelling for admitted expression metadata except for
minor normalization already performed by the parser or value converter. It does
not attempt full MySQL expression pretty-printing in this slice.

## Diagnostics

Expected diagnostics:

- syntax error for `DEFAULT 1 + 2` and malformed parenthesized defaults;
- invalid default for expression defaults on unsupported target types;
- invalid default for unsupported expression nodes: identifiers, column
  references, subqueries, parameters, variables, unsupported functions,
  unsupported operators, casts, and non-integer literals;
- invalid default for division/modulo by zero or scalar arithmetic overflow;
- invalid default for evaluated results outside the target descriptor range;
- `1048 (23000)` for materializing a stored `NULL` expression default into a
  `NOT NULL` target in strict DML paths;
- existing unknown schema/table/column, reserved name, duplicate column,
  unsupported object kind, allocation, catalog, and SQLite failure diagnostics
  from the surrounding statement planners.

Supported in-range expression-default statements report `warning_count == 0`.

## Physical SQLite Handling

Generated SQLite table definitions continue to use MyLite physical column types
and stable physical table names. Expression-default metadata is not emitted as a
SQLite column default. When a physical write needs a default value, MyLite binds
the evaluated integer or binds `NULL` through prepared SQLite statements.

This keeps SQLite storage close to the existing optimal path: no full-table
materialization is introduced by ordinary inserts, updates, replaces, selects,
or descriptor introspection. The only row-touching paths are the existing
`ALTER TABLE ... ADD COLUMN` backfill or rebuild paths that already materialize
rows for DDL reasons.

## Tests

Add a focused plain C runtime test for expression defaults. Cover:

- parser acceptance for `DEFAULT (1 + 2)` and rejection of `DEFAULT 1 + 2`;
- successful `CREATE TABLE` defaults for `INT`, `INTEGER`, `BIGINT`, and
  unsigned integer-family targets within the current physical range;
- unary, binary, `DIV`, `%`, infix `MOD`, `MOD()`, nested parentheses, and
  `NULL` expression defaults;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata, including `DEFAULT_GENERATED`;
- omitted-column `INSERT` and explicit `DEFAULT` in supported DML paths;
- `DEFAULT (NULL)` into nullable and non-nullable columns;
- boundary values and out-of-range diagnostics;
- unsupported expression nodes: identifiers, column references, strings,
  decimals, floats, hex, bit, functions other than `MOD`, subqueries,
  parameters, variables, casts, boolean operators, and arithmetic division by
  zero;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT (expr)`;
- `ADD COLUMN`, `MODIFY COLUMN`, `CHANGE COLUMN`, `CREATE TABLE ... LIKE`, and
  supported CTAS descriptor copying where the existing statement subset admits
  the surrounding syntax;
- close/reopen persistence and independent file-backed handles;
- catalog migration from the previous schema version and zero-initialized
  cleanup paths.

The MySQL expectation script must pass against MySQL 8.4.9 before the MyLite
runtime expectations are treated as fixed.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/type-system-literals-conversion.md`;
- `docs/compatibility/operators.md` only for the exact admitted arithmetic
  operator reuse in expression-default contexts.

Do not overclaim general expression defaults, column-reference defaults,
function defaults, SQL-mode-dependent defaults, delayed range checking, or
non-integer target types.
