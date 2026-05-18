# Baseline DEFAULT() Function

## Purpose

This slice adds the first descriptor-owned `DEFAULT(column_name)` function
surface. It complements the existing DML `DEFAULT` keyword support without
turning MyLite into a general expression evaluator.

The behavior is based on the MySQL 8.4 documentation for data type defaults and
the built-in function reference, plus runtime probes against MySQL 8.4.9:

- https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html

## Supported Syntax

The supported grammar is deliberately narrow and independently authored for
MyLite:

```lemon
expression(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R).
insert_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R).
update_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R).
```

`qualified_identifier` may be an unqualified column name, `table.column`, or
`schema.table.column` where the surrounding statement has a single descriptor
table source that matches the qualifier. Whitespace between `DEFAULT` and `(`
is accepted by this grammar.

The following remain unsupported or syntax errors in this slice:

- `DEFAULT()`;
- `DEFAULT(1)`, `DEFAULT('column')`, `DEFAULT(expr)`;
- multiple arguments;
- use as a WHERE predicate operand, ORDER BY key, LIMIT value, DDL default
  expression, or arbitrary expression inside unsupported statement shapes;
- stored routine semantics, privilege semantics, generated columns, triggers,
  and general expression default evaluation.

## Statement Surface

MyLite supports `DEFAULT(column)` in these places:

- descriptor-backed row-scalar `SELECT` projection over one table source;
- `INSERT ... VALUES` and `INSERT ... SET` value positions already supported by
  the current insert planner;
- `REPLACE ... VALUES` and `REPLACE ... SET` value positions that share the
  current insert planner;
- single-table `UPDATE` assignment values, including multiple-assignment lists
  where each assignment is otherwise admitted;
- supported `INSERT ... ON DUPLICATE KEY UPDATE` assignment values.

No-source `SELECT DEFAULT(column)` and `SELECT DEFAULT(column) FROM DUAL` are
diagnosed as unknown columns for the supported function surface.

## Descriptor Ownership

The public API remains unchanged. `mylite_execute()` continues to return normal
non-row statement results for successful DML and row results for successful
`SELECT`.

The parser creates a MyLite AST node for `DEFAULT(column)`. The analyzer and
planner resolve the column against MyLite catalog descriptors. The catalog
descriptor is authoritative for default kind, default literal payload,
nullability, type family, physical table name, and generated SQLite column
names.

The runtime materializes the default value before generating or binding SQLite
statements. SQLite is used only for physical row storage and statement
execution. MyLite does not inspect SQLite schema text, SQLite column defaults,
or `sqlite_schema` to answer `DEFAULT(column)`.

No storage format changes are required. The `.mylite` preamble and shifted
SQLite payload invariants remain unchanged.

## Name Resolution

For table-backed row-scalar `SELECT`, `DEFAULT(column)` resolves using the same
single-source descriptor context as ordinary row-scalar column references:

- unqualified `DEFAULT(c)` resolves against the single source table;
- `DEFAULT(t.c)` resolves when `t` is the table name or active table alias;
- `DEFAULT(schema.t.c)` resolves when no alias is active and the schema/table
  qualifiers match the source table.

For `INSERT`, `REPLACE`, `UPDATE`, and duplicate-key update assignments, the
source descriptor context is the target table. Table aliases are not admitted by
the current DML grammar, so qualifiers must match the target table or target
schema/table.

Unknown source columns use MySQL-compatible unknown-column diagnostics for the
current context. Current descriptor lookup remains ASCII case-insensitive, in
line with the existing catalog policy.

## Default Value Semantics

The source column is validated before statement execution for errors that MySQL
raises independently of whether an outer `UPDATE` matches rows or an
`ON DUPLICATE KEY UPDATE` tail executes:

- unknown source column: `1054 / 42S22`;
- source column has no explicit default and is not an implicit nullable default:
  `1364 / HY000`;
- source column default is a descriptor expression default:
  `3773 / HY000`, with `DEFAULT function cannot be used with default value expressions`.

Supported literal source defaults are materialized through MyLite-owned
descriptor logic:

- integer-family descriptor literal defaults, including signed and unsigned
  values within the current physical range;
- decimal, approximate, string, binary-string, `BIT`, `YEAR`, `DATE`, `TIME`,
  `DATETIME`, `TIMESTAMP`, `ENUM`, `SET`, and JSON defaults already materialized
  by the current descriptor default helper;
- implicit `NULL` for nullable columns with no explicit default;
- `0` for `AUTO_INCREMENT` source columns, matching MySQL 8.4.9
  `DEFAULT(auto_increment_column)` function behavior.

The current descriptor expression defaults (`DEFAULT (1 + 2)` and
`DEFAULT (NULL)`) are intentionally rejected for `DEFAULT(column)`, even though
the existing DML `DEFAULT` keyword can still materialize them for the target
column. This follows MySQL's rule that `DEFAULT(column)` is for literal default
values, not expression defaults.

Current date/time defaults (`CURRENT_TIMESTAMP`, `NOW()`, `CURRENT_DATE`,
`CURRENT_TIME`) remain outside this slice for `DEFAULT(column)`. MySQL has
special result rules for these defaults that differ from ordinary DML
`DEFAULT`, so MyLite returns a deterministic unsupported-feature diagnostic
until that behavior is implemented exactly.

## DML Assignment Semantics

`DEFAULT(source_column)` is evaluated as a constant descriptor value for the
statement position. For `INSERT` and `REPLACE`, the value is converted before
the physical row write, just like existing admitted literals. For `UPDATE`, the
target conversion is performed only when the update has matching rows, so
target-side `NULL` or compatibility errors are not raised for no-match updates.
For duplicate-key update assignments, target conversion is performed only when
a duplicate update is executed.

This slice supports target assignment when the source and target descriptors
are the same column or have the same logical and physical type names. It does
not attempt broader MySQL implicit conversion between unrelated descriptor
families. Unsupported cross-type assignments return a deterministic MyLite
unsupported-feature diagnostic at the point where the assignment would be
converted.

Assigning a materialized `NULL` default into a `NOT NULL` target raises the
current `1048 / 23000` bad-null diagnostic when conversion is reached.

`AUTO_INCREMENT` source columns materialize as integer `0`. In `INSERT` and
`REPLACE`, existing auto-increment planning then applies the current
`NO_AUTO_VALUE_ON_ZERO` behavior to the target column. In `UPDATE`, assigning
`DEFAULT(auto_increment_column)` writes `0` to the target when other key and
constraint checks admit the change.

Successful DML statements keep the existing result API conventions:

- no row result set;
- MySQL-style affected rows as already implemented by the statement path;
- `warning_count == 0` for supported in-range behavior.

Catalog rows, descriptor versions, descriptor caches, catalog generation, and
`sqlite_schema_generation` are not mutated by `DEFAULT(column)` evaluation.

## Diagnostics

Supported diagnostics include:

- syntax errors for invalid argument shapes or unsupported statement syntax:
  existing `1064 / 42000`;
- no default schema, unknown schema, unknown table, reserved MyLite schema/table
  names, and unsupported object kind through the existing statement planners;
- unknown source column: existing `1054 / 42S22`;
- source column without a usable explicit or implicit nullable default:
  existing `1364 / HY000`;
- source column backed by a descriptor expression default:
  new MySQL-compatible `3773 / HY000`;
- current date/time default sources: existing MyLite unsupported-feature
  diagnostic;
- target `NULL` into `NOT NULL`: existing `1048 / 23000`;
- unsupported source/target descriptor conversion: existing MyLite
  unsupported-feature diagnostic;
- physical SQLite, allocation, and public API misuse diagnostics through the
  existing public API and execution layers.

## Physical SQLite Handling

Generated SQLite SQL is unchanged except that additional assignment values may
be admitted before SQL construction. Physical statements continue to:

- target stable physical table names such as `_mylite_user_table_<table_id>`;
- quote generated SQLite identifiers;
- bind materialized values with prepared-statement parameters;
- use existing rowid-limited update shapes where the update planner already
  requires internal rowid-table invariants.

No SQLite fork patch or new SQLite extension hook is required.

## MySQL Runtime Evidence

The MySQL expectation script
`packages/libmylite/tests/mysql_baseline_default_function_expectations.sh`
records representative MySQL 8.4.9 observations:

- `SELECT DEFAULT(c)` returns the source column default once per selected row;
- unqualified, table-qualified, alias-qualified, and schema-qualified source
  references are admitted when they match the single source table;
- no-source `DEFAULT(c)`, unknown source columns, and invalid argument shapes
  produce MySQL diagnostics;
- `DEFAULT(column_with_expression_default)` returns `3773 / HY000`;
- `DEFAULT(non_nullable_column_without_default)` returns `1364 / HY000`;
- source default validation happens even for no-match `UPDATE`;
- target `NULL` conversion errors happen only when the update matches rows;
- insert, replace, update, and duplicate-key update assignments use the source
  default values and preserve existing affected-row and warning-count behavior.

## Compatibility Notes

This feature should move `DEFAULT()` from unsupported to limited support in
`docs/compatibility/functions-default-values.md`, and remove
`DEFAULT(col_name)` from the unsupported notes of the exact DML rows where this
slice admits it.

It must not claim support for general expressions, expression defaults,
generated columns, aliases in DML, CTEs, subqueries as DEFAULT arguments,
general implicit conversion, or full default-expression evaluation.
