# Baseline SELECT Literal Projection

## Status

This feature specifies a narrow no-source scalar `SELECT` extension:
projection of decimal integer, `NULL`, `TRUE`, and `FALSE` literals with no
table source or with `FROM DUAL`, plus table-backed scalar literal projection
over one descriptor-backed source through the row-scalar select envelope. It
builds on `mylite_execute()`, statement context, the parser scaffold,
scalar/session select execution, row-scalar table planning, `SELECT ALL`,
select-item aliases, and the existing public result conventions.

This is intentionally not general expression projection. It does not add
arithmetic, general table-backed expressions, functions beyond existing scalar
slices, no-source predicates, no-source ordering, no-source limits, or
arbitrary SQLite pass-through.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline `SELECT ALL` modifier:
  `docs/specs/baseline-select-all-modifier/specs.md`
- Baseline select-item alias:
  `docs/specs/baseline-select-item-alias/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, numeric literals:
  https://dev.mysql.com/doc/refman/8.4/en/number-literals.html
- MySQL 8.4 Reference Manual, boolean literals:
  https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html
- MySQL 8.4 Reference Manual, `NULL` values:
  https://dev.mysql.com/doc/refman/8.4/en/null-values.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SELECT` may compute a row without referencing a table. `FROM DUAL` is also
  accepted as a dummy source for expressions that do not reference tables.
- `SELECT 1`, `SELECT +1`, `SELECT -1`, `SELECT NULL`, `SELECT TRUE`, and
  `SELECT FALSE` each return one row.
- `TRUE` and `FALSE` evaluate to `1` and `0` while their default result labels
  preserve the keyword spelling used in the query.
- Decimal integer values are rendered in canonical decimal text:
  leading zeros are removed, unary `+` is removed, unary `-` is kept only for
  nonzero results, and `-0` returns `0`.
- Default result labels for unparenthesized integer literals preserve source
  spelling except unary `+`, whose label is the unsigned integer text.
- Decimal integer literals with up to 81 significant digits returned
  warning-free in runtime probes. A bare 82-significant-digit integer returned
  a 65-digit truncated value and warning `1292`. This baseline supports only
  the warning-free subset and rejects larger literals deterministically.
- `SELECT ALL literal_list` and `SELECT ALL literal_list FROM DUAL` are
  equivalent to omitting `ALL`.
- Select-item aliases set the public column label and keep the same alias
  length/decoding behavior as the select-item alias slice.
- Successful literal projection returns `@@warning_count = 0`, no affected
  rows, and makes a following `ROW_COUNT()` return `-1`.
- MySQL accepts wider forms such as parenthesized literals, string literals,
  decimal literals, floats, hex, bit literals, no-source `ORDER BY`, and
  no-source `LIMIT`. Those remain outside the no-source literal slice.
- `SELECT 1 AS test FROM t WHERE id = 1 LIMIT 1` returns one row when the
  descriptor-backed source predicate matches, and zero rows when it does not.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_select_literal_projection_expectations.sh`.

## Scope

The implementation must add:

- runtime support for `SELECT literal_projection_list`;
- runtime support for `SELECT ALL literal_projection_list`;
- runtime support for the same forms with `FROM DUAL`;
- runtime support for one-table table-backed scalar literal projection over the
  current row-scalar table `WHERE`, `ORDER BY`, and `LIMIT` envelope;
- literal projection items limited to unparenthesized decimal integer literals
  with optional unary `+` or `-`, `NULL`, `TRUE`, and `FALSE`;
- decimal integer rendering for up to 81 significant digits, including
  leading-zero and signed-zero normalization;
- select-item aliases using the existing alias decoding, length, diagnostics,
  and result-label behavior;
- one result row for successful no-source and `DUAL` literal projection;
- public result behavior matching existing row-returning `SELECT` statements;
- warning count `0` for supported in-range literal projections; and
- deterministic diagnostics for unsupported literal/projection forms.

Existing scalar/session function, system-variable, aggregate, descriptor-backed
table select, `SELECT ALL`, and alias behavior must remain unchanged.

## Non-Goals

This feature must not implement:

- general expression evaluation, arithmetic, unary operators outside the
  admitted integer-literal signs, string values, decimal/fixed-point values,
  floats, hex, bit, date/time, JSON, parameters, variables, functions, casts,
  collations, or subqueries;
- parenthesized literal projection, because MySQL has source-label behavior
  that should be specified separately before admission;
- no-source or `DUAL` `WHERE`, `ORDER BY`, `LIMIT`, or `OFFSET`;
- `GROUP BY`, `HAVING`, windows, joins, CTEs, set operations, locking clauses,
  `INTO`, or other select modifiers for literal projection;
- MySQL's warning/truncation behavior for integer literals above 81
  significant digits;
- protocol-grade expression metadata, exact numeric type metadata, charsets,
  decimals, origin metadata, or field flags; or
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful literal selects are row-returning statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/runtime recognizes no-source and `DUAL` scalar select shapes and
  distinguishes supported literal items from wider expression projection.
- The catalog module is not used for supported literal projection and must not
  mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Runtime execution builds one public result row directly. No physical SQLite
  SQL is generated because there is no table source or storage work.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Literal projection must not read or write user data or byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT literal_select_item[, literal_select_item ...]
SELECT ALL literal_select_item[, literal_select_item ...]
SELECT literal_select_item[, literal_select_item ...] FROM DUAL
SELECT ALL literal_select_item[, literal_select_item ...] FROM DUAL
```

`literal_select_item` uses the existing select-item alias grammar:

```sql
literal_select_item:
    literal_projection_value
  | literal_projection_value AS alias
  | literal_projection_value alias

alias:
    identifier
  | quoted_identifier
  | string_literal
```

`literal_projection_value` is:

```sql
literal_projection_value:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
  | NULL
  | TRUE
  | FALSE
```

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= literal.
expression ::= PLUS INTEGER.
expression ::= MINUS INTEGER.

select_item ::= expression.
select_item ::= expression AS select_alias.
select_item ::= expression select_alias.
```

The parser may admit these expression forms wherever the shared select-item
expression grammar is used. The analyzer accepts them in no-source and
`FROM DUAL` literal projection statements and in the documented table-backed
row-scalar literal projection envelope. Existing descriptor-backed table
selects continue to reject non-descriptor projection expressions outside the
documented row-scalar slices.

## Semantics

For supported no-source and `FROM DUAL` literal statements:

- exactly one result row is produced;
- each select item produces one result column;
- `NULL` result values are represented as public `NULL` cells;
- `TRUE` returns text `1`;
- `FALSE` returns text `0`;
- decimal integer values return canonical text:
  - ignore leading zeros for magnitude;
  - return `0` when every digit is zero, regardless of sign;
  - keep a leading `-` only for negative nonzero values;
  - remove a leading `+`;
  - accept at most 81 significant digits after leading zeros are skipped;
- aliases override default result labels;
- without an alias, result labels use MyLite's existing source-span label
  convention for the admitted unparenthesized expression;
- supported statements return `affected_rows == 0`, `warning_count == 0`,
  and a following `ROW_COUNT()` result of `-1`.

`SELECT ALL` remains duplicate-preserving default syntax. Since literal
projection has exactly one implicit row, it has no visible value effect.

For supported table-backed scalar literal projection, source row matching,
ordering, and limiting follow the existing row-scalar table envelope, and each
matched source row contributes one projected result row.

## Diagnostics

Required deterministic diagnostics:

- syntax errors and unsupported grammar use the existing parser/runtime
  diagnostics;
- unsupported table-backed expression projection reports the relevant
  row-scalar or descriptor-backed `SELECT` diagnostic;
- unsupported parenthesized literal projection reports an unsupported literal
  projection expression;
- unsupported string, decimal, float, hex, bit, parameter, arithmetic,
  function, column, qualified identifier, subquery, or general expression
  projection reports unsupported literal projection or the existing
  unsupported select diagnostic, depending on parse shape;
- decimal integer literals above 81 significant digits report an out-of-range
  or unsupported literal diagnostic and do not emit warnings;
- alias decoding and alias length failures use the existing select-item alias
  diagnostics;
- allocation failures return `MYLITE_NOMEM` and set handle diagnostics
  consistently with existing result construction paths; and
- public API misuse remains unchanged because no public surface changes.

## SQLite And Storage

Supported no-source literal projection does not use SQLite execution. It is a
MyLite wrapper/runtime result construction path because there is no table
source, no physical row storage, and no descriptor lookup. Table-backed scalar
literal projection uses the existing row-scalar planner and SQLite-backed table
scan so predicate filtering, ordering, and limiting stay in the same path as
other row-scalar table projections.

No SQLite fork patches are required.

## Tests

Add MySQL-runtime-verified expectations for:

- no-source integer, signed integer, `NULL`, `TRUE`, and `FALSE` projection;
- `FROM DUAL` equivalents;
- explicit `ALL`;
- alias labels on literal projection;
- leading-zero, unary-plus, unary-minus, and signed-zero value normalization;
- 81-significant-digit warning-free projection and 82-significant-digit MySQL
  warning/truncation behavior documented as out of scope;
- warning count and following `ROW_COUNT()`;
- MySQL-accepted but deferred parenthesized, string, decimal, float, hex, bit,
  table-backed, order, and limit forms.

Add fast C tests under `packages/libmylite/tests/` covering:

- result columns, values, row count, column count, warning count, affected rows,
  and following `ROW_COUNT()`;
- no-source and `FROM DUAL` forms;
- `SELECT ALL` forms;
- aliases, including identifier, quoted identifier, and string literal aliases;
- integer normalization boundaries, including 81 significant digits and
  rejection of 82 significant digits;
- unsupported literal/projection forms listed above;
- unchanged descriptor-backed column select behavior and table-backed scalar
  literal projection through the row-scalar table envelope;
- no storage/catalog mutation and `.mylite` preamble preservation for
  file-backed handles; and
- zero-initialized cleanup for any new helper state if helper state is added.

## Compatibility Notes

This slice narrows the previous "no expression-level numeric/boolean
semantics" gap for no-source and `DUAL` literal projection and for
one-table scalar-literal row-scalar projection. It does not make literals
general expressions, and it does not change DML conversion, predicate
conversion, general descriptor-backed table expression projection, or metadata
fidelity.
