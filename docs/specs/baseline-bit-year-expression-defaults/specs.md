# Baseline BIT and YEAR Expression Defaults

This feature extends the existing integer expression-default machinery to the
two descriptor families that are represented through MyLite-owned conversion
rather than raw SQLite integers: `BIT` and `YEAR`.

The intent is narrow. MyLite should accept common MySQL shapes such as
`BIT(6) DEFAULT (1 + 2)` and `YEAR DEFAULT (2000 + 1)` without claiming general
expression defaults for all type families.

## Compatibility Authority

Normative behavior comes from:

- MySQL 8.4 Reference Manual, data type default values:
  <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
- MySQL 8.4 Reference Manual, bit-value type:
  <https://dev.mysql.com/doc/refman/8.4/en/bit-type.html>
- MySQL 8.4 Reference Manual, year type:
  <https://dev.mysql.com/doc/refman/8.4/en/year.html>
- MySQL 8.4 Reference Manual, expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Runtime probes against MySQL `8.4.9`, captured by the updated
  `packages/libmylite/tests/mysql_baseline_bit_type_expectations.sh` and
  `packages/libmylite/tests/mysql_baseline_year_type_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `BIT(n) DEFAULT (integer_expression)` and `YEAR DEFAULT (integer_expression)`
  are accepted in `CREATE TABLE` column definitions.
- `DEFAULT (NULL)` is accepted for nullable and `NOT NULL` `BIT` / `YEAR`
  columns. Later materialization into a `NOT NULL` column fails with
  `1048 / 23000`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report expression defaults
  with `Extra = DEFAULT_GENERATED`.
- `SHOW CREATE TABLE` renders expression defaults as `DEFAULT ((expr))`, while
  `DEFAULT (NULL)` renders as `DEFAULT (NULL)`.
- Omitted-column inserts and explicit DML `DEFAULT` materialize the evaluated
  expression value.
- MySQL accepts some expression defaults whose result is out of range and raises
  diagnostics when the default is materialized. MyLite keeps the existing
  integer-expression policy for this phase and rejects out-of-range or invalid
  evaluated defaults during DDL planning.

## Supported Surface

MyLite supports:

- persistent and session temporary table descriptors that already admit `BIT`
  and `YEAR` column definitions through existing create/alter paths;
- `DEFAULT (expr)` in supported column definitions for `BIT` and `YEAR`
  descriptor columns;
- `ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT (expr)` for one
  unqualified `BIT` or `YEAR` descriptor column;
- descriptor-copying paths that already preserve default descriptors, including
  `CREATE TABLE ... LIKE` and the current descriptor-inferred
  `CREATE TABLE ... SELECT` subset;
- constant expression operands limited to decimal integer literals with optional
  unary sign and `NULL`;
- operators limited to unary `+`, unary `-`, binary `+`, `-`, `*`, `DIV`, `%`,
  infix `MOD`, nested parentheses, and `MOD(left, right)`;
- expression results that are either `NULL` or valid for the target descriptor
  at statement planning time:
  - `BIT(1..64)` values must fit the declared bit width and be nonnegative;
  - `YEAR` values must be `0`, `1..99` using MySQL's existing two-digit
    conversion rules, or `1901..2155`;
- metadata that preserves expression-default identity through `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `DESCRIBE`, `EXPLAIN table`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- omitted-column `INSERT`, explicit `DEFAULT` in supported `INSERT`,
  `REPLACE`, and `UPDATE` paths, and descriptor cloning with the stored
  expression default.

This slice intentionally does not support:

- general expression defaults;
- expression defaults for decimal, approximate, temporal other than `YEAR`,
  `CHAR`, `VARCHAR`, `TEXT`, binary string, BLOB, JSON, spatial, enum, set, or
  generated columns beyond their existing documented slices;
- string, decimal, approximate, hex, bit-literal, temporal, or boolean
  expression operands beyond existing literal-default support;
- functions other than `MOD(left, right)`;
- column references, subqueries, parameters, user variables, system variables,
  stored functions, loadable functions, arbitrary built-ins, casts, CTEs, or
  general expression coercion;
- delayed materialization-time checking for out-of-range expression results;
- physical SQLite expression defaults or SQLite fork changes.

## Ownership Boundaries

- Public API: no ABI or public header changes. Successful statements use the
  existing non-row result conventions. Errors and warnings flow through existing
  diagnostics.
- Parser/AST: the existing `DEFAULT (expression)` grammar already preserves the
  AST. The parser does not decide whether the expression is supported.
- Analyzer/planner: validates that the expression is constant and in the
  admitted integer-expression subset; evaluates deterministic results once at
  DDL/ALTER planning time; converts the result through the `BIT` or `YEAR`
  descriptor conversion policy; records expression metadata in the planned
  column descriptor.
- Catalog: remains authoritative for logical column descriptors. It stores a
  generated integer-expression default kind with expression text and the
  normalized integer value needed to rematerialize the `BIT` or `YEAR` value.
- Runtime DML: materializes defaults from descriptors and converts the stored
  integer value back through MyLite-owned `BIT` or `YEAR` physical value
  builders. It does not reparse the expression text.
- Result builders: render expression-default metadata from descriptor state.
- Storage/VFS: unchanged. Physical SQLite tables store ordinary `BLOB` or `TEXT`
  row values; SQLite does not know about MySQL expression-default metadata.
- SQLite: no fork patch. MyLite owns conversion and uses public SQLite prepared
  statements and bound values for physical writes.

## Grammar

The MySQL-facing syntax admitted by this feature is:

```text
column_definition:
    column_name bit_or_year_column_type column_attributes

column_attribute:
    DEFAULT literal_default
  | DEFAULT NULL
  | DEFAULT ( bit_year_default_expression )

alter_table_set_default:
    ALTER TABLE table_name ALTER column_name SET DEFAULT default_value
  | ALTER TABLE table_name ALTER COLUMN column_name SET DEFAULT default_value

default_value:
    literal_default
  | NULL
  | ( bit_year_default_expression )

bit_year_default_expression:
    decimal_integer_literal
  | NULL
  | + bit_year_default_expression
  | - bit_year_default_expression
  | bit_year_default_expression + bit_year_default_expression
  | bit_year_default_expression - bit_year_default_expression
  | bit_year_default_expression * bit_year_default_expression
  | bit_year_default_expression DIV bit_year_default_expression
  | bit_year_default_expression % bit_year_default_expression
  | bit_year_default_expression MOD bit_year_default_expression
  | MOD(bit_year_default_expression, bit_year_default_expression)
  | ( bit_year_default_expression )
```

MyLite Lemon-syntax snippets are unchanged from the existing expression-default
parser surface:

```text
column_default(A) ::= DEFAULT(D) column_default_value(V).

column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}
```

The planner rejects constructs outside `bit_year_default_expression`.

## Semantics

Expression defaults are descriptor defaults. They do not change existing rows
unless the surrounding statement already materializes defaults:

- `CREATE TABLE` stores descriptor metadata only.
- `ALTER TABLE ... ADD [COLUMN]` backfills existing rows through the same
  descriptor-default path as literal defaults.
- `ALTER TABLE ... MODIFY` and `CHANGE` update the descriptor default while
  preserving row values unless the wider statement already requires validation
  or rebuild.
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT (expr)` is catalog-only and
  reports zero affected rows and zero warnings.
- Descriptor cloning preserves the expression-default kind, expression text, and
  normalized value.

For `BIT`, non-`NULL` expression results are accepted only when the signed
expression value is nonnegative and fits the declared width:

| Target descriptor | Accepted result range |
| --- | --- |
| `BIT` / `BIT(1)` | `0..1` |
| `BIT(n)` | `0..(2^n - 1)` for `2 <= n <= 63` |
| `BIT(64)` | `0..9223372036854775807` in MyLite's current signed-64 expression envelope |

For `YEAR`, non-`NULL` expression results reuse current numeric `YEAR`
conversion:

| Expression result | Stored value |
| --- | --- |
| `0` | `0000` |
| `1..69` | `2001..2069` |
| `70..99` | `1970..1999` |
| `1901..2155` | same year |

`DEFAULT (NULL)` stores a `NULL` expression-default descriptor. It is allowed
for nullable and non-nullable `BIT` / `YEAR` descriptors. Materializing it
behaves like assigning explicit `NULL`: nullable targets store `NULL`;
non-nullable targets raise `1048 / 23000` in strict paths and use the existing
`INSERT IGNORE` implicit value plus warning path where that path is already
supported.

Division or modulo by zero, scalar arithmetic overflow, unsupported operands,
unsupported operators, and out-of-range evaluated results are invalid defaults
for this slice.

## Metadata Rendering

Descriptor rendering rules:

- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` put the stored expression text
  in `Default` and include `DEFAULT_GENERATED` in `Extra`.
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` contains the stored expression
  text and `EXTRA` contains `DEFAULT_GENERATED`.
- `SHOW CREATE TABLE` renders `DEFAULT (` + stored expression text + `)`. For
  stored expression text `(1 + 2)`, this produces `DEFAULT ((1 + 2))`. For
  stored expression text `NULL`, this produces `DEFAULT (NULL)`.

MyLite preserves source spelling for admitted expression metadata except for
minor normalization already performed by the parser or value converter. For the
admitted modulo forms, both `MOD(left, right)` and infix `MOD` render as `%` in
stored expression text to match observed MySQL 8.4.9 metadata. MyLite does not
attempt full MySQL expression pretty-printing in this slice.

## Diagnostics

Expected diagnostics:

- syntax error for `DEFAULT 1 + 2` and malformed parenthesized defaults;
- invalid default for expression defaults on unsupported target types;
- invalid default for unsupported expression nodes: identifiers, column
  references, subqueries, parameters, variables, unsupported functions,
  unsupported operators, casts, and non-integer literals;
- invalid default for division/modulo by zero or scalar arithmetic overflow;
- invalid default for evaluated results outside the target descriptor range;
- `1048 / 23000` for materializing a stored `NULL` expression default into a
  `NOT NULL` target in strict DML paths;
- existing unknown schema/table/column, reserved name, duplicate column,
  unsupported object kind, allocation, catalog, and SQLite failure diagnostics
  from the surrounding statement planners.

Supported in-range expression-default statements report `warning_count == 0`.

## Physical SQLite Handling

Generated SQLite table definitions continue to use MyLite physical column types
and stable physical table names. Expression-default metadata is not emitted as a
SQLite expression. When a physical write needs a default value, MyLite binds the
canonical `YEAR` text or fixed-width `BIT` blob through prepared SQLite
statements.

`ALTER TABLE ... ADD COLUMN` may need a literal physical SQLite default for
backfill. MyLite renders the evaluated canonical value as a SQLite-compatible
literal for the target physical column, not the MySQL expression text.

No full-table materialization is introduced by ordinary inserts, updates,
replaces, selects, or descriptor introspection. Only existing DDL rebuild or
add-column paths touch existing rows.

## Tests

Coverage must include:

- MySQL-runtime-verified `BIT` and `YEAR` expression default expectations;
- `CREATE TABLE` success for `BIT` and `YEAR` expression defaults;
- unary, binary, `DIV`, `%`, infix `MOD`, `MOD()`, nested parentheses, and
  `NULL` expression defaults;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata, including `DEFAULT_GENERATED`;
- omitted-column `INSERT` and explicit `DEFAULT` in supported DML paths;
- `DEFAULT (NULL)` into nullable and non-nullable columns;
- boundary values and out-of-range diagnostics;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT (expr)`;
- descriptor cloning through `CREATE TABLE ... LIKE` and supported CTAS;
- close/reopen persistence and independent file-backed handles;
- physical `.mylite` preamble preservation;
- parser/runtime regression coverage for existing integer expression defaults,
  `BIT`, `YEAR`, show/introspection, DML defaults, and alter-table paths.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/type-system-literals-conversion.md`;
- `docs/compatibility/sql-show-statements.md`;
- `docs/compatibility/metadata-information-schema.md`.

Do not overclaim general expression defaults, delayed range checking,
expression defaults for other type families, broader scalar expression
coercion, or protocol-grade metadata.
