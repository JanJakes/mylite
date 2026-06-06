# Baseline WHERE Scalar Literal Predicates

## Status

This phase extends MyLite's existing descriptor-backed `WHERE` predicate planner
with a small scalar-literal subset needed by common query-builder shapes:

```sql
WHERE TRUE
WHERE FALSE
WHERE NULL
WHERE 1
WHERE -1
WHERE 1 = 1
WHERE 1 = column_name
WHERE NULL <=> column_name
WHERE column_name <=> NULL
WHERE 1 IS TRUE
WHERE NULL IS UNKNOWN
```

The feature is intentionally not a general expression engine. It admits signed
64-bit decimal integer literals, `TRUE`, `FALSE`, and `NULL` in the new
scalar-literal positions. Descriptor-column integer predicates also admit exact
quoted signed integer strings with optional leading and trailing ASCII
whitespace. They additionally admit the WordPress search shape where a
nonempty quoted string is compared to an integer descriptor column and MySQL
coerces the string to a numeric comparison value with warning `1292`. Existing
row-scalar comparison predicates admit exact quoted integer strings on the
comparison right-hand side. Descriptor-column predicates continue to resolve
through MyLite catalog descriptors. SQLite still executes the final physical
filter from generated SQL with bound parameters.

## Sources And Evidence

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline boolean predicate phases:
  `docs/specs/baseline-where-and-predicates/specs.md`,
  `docs/specs/baseline-where-or-predicates/specs.md`,
  `docs/specs/baseline-where-xor-predicates/specs.md`, and
  `docs/specs/baseline-where-is-boolean-predicates/specs.md`
- MySQL 8.4 Reference Manual, expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, comparison functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, type conversion in expression evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, logical operators:
  https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- MySQL 8.4 Reference Manual, operator precedence:
  https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html

MySQL 8.4.9 runtime probes are captured in:

```text
packages/libmylite/tests/mysql_baseline_where_scalar_literal_predicates_expectations.sh
```

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

Supported scalar literal truth predicates:

```sql
WHERE TRUE
WHERE FALSE
WHERE NULL
WHERE integer_literal
WHERE +integer_literal
WHERE -integer_literal
```

Supported scalar literal comparisons:

```sql
scalar_literal comparison_operator scalar_literal
scalar_literal comparison_operator column_reference
column_reference comparison_operator NULL
column_reference comparison_operator exact_integer_string_literal
column_reference comparison_operator truncated_integer_string_literal
row_scalar_expression comparison_operator exact_integer_string_literal
```

`scalar_literal` is one of:

```sql
integer_literal
+integer_literal
-integer_literal
TRUE
FALSE
NULL
```

`comparison_operator` is one of:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

Supported scalar literal `IS` tests:

```sql
scalar_literal IS TRUE
scalar_literal IS FALSE
scalar_literal IS UNKNOWN
scalar_literal IS NOT TRUE
scalar_literal IS NOT FALSE
scalar_literal IS NOT UNKNOWN
scalar_literal IS NULL
scalar_literal IS NOT NULL
```

The feature is available anywhere the shared table predicate planner is already
used:

- table-backed `SELECT`;
- aggregate and grouped aggregate source filters;
- descriptor source filters reused by `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- single-table `DELETE`;
- single-table `UPDATE`;
- currently supported `EXISTS` inner predicates when the same predicate planner
  is used.

## Out Of Scope

This phase does not add:

- string scalar truth predicates, string-to-string scalar comparisons, decimal
  string numeric predicate coercion, decimal/exponent string comparison
  coercion, float, hex, bit, temporal, JSON, parameter, variable, function,
  cast, collation, subquery, or row-constructor scalar predicates;
- descriptor-column bare truth predicates such as `WHERE column_name`;
- expression-left predicates such as `WHERE column_name + 1 = 2`;
- function predicates such as `WHERE ABS(column_name) = 1`;
- table-backed scalar projection expressions beyond the existing documented
  row-scalar projection subsets;
- `HAVING` scalar literal predicates;
- symbolic `!`;
- full MySQL expression resolver behavior, coercion, collations, optimizer
  rewrites, privileges, generated columns, triggers, or SQLite fork patches.

MySQL accepts many broader forms. MyLite must reject them deterministically
until the expression planner owns their semantics.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the admitted surface:

- `WHERE TRUE`, `WHERE 1`, `WHERE +1`, and `WHERE -1` match every source row.
- `WHERE FALSE`, `WHERE 0`, and `WHERE NULL` match no rows.
- `WHERE 1 = 1` matches every source row; `WHERE 1 = 0` matches no rows.
- `WHERE NULL <=> NULL` matches every source row; `WHERE NULL = NULL` matches
  no rows because the result is SQL `NULL`.
- `WHERE 1 = column_name` matches the same rows as
  `WHERE column_name = 1`.
- `WHERE column_name = '1'` and `WHERE column_name = ' 1 '` match the same rows
  as integer literal comparisons and record no warnings.
- `WHERE column_name = 'abc'` coerces the nonempty nonnumeric string to `0`,
  matches the same rows as `WHERE column_name = 0`, and records warning
  `1292 / Truncated incorrect DOUBLE value`.
- `WHERE column_name = '1x'` coerces the integer prefix to `1`, matches the
  same rows as `WHERE column_name = 1`, and records the same `1292` warning.
- Decimal and exponent string comparison coercion remains outside this slice
  rather than being rounded through integer DML conversion.
- For ordered comparisons with a literal on the left, MySQL evaluates the
  predicate normally. MyLite normalizes these by flipping the comparison
  operator and keeping the descriptor column on the left of generated SQL.
- `WHERE NULL <=> column_name` and `WHERE column_name <=> NULL` match rows
  where the descriptor column is `NULL`.
- `WHERE NULL = column_name` and `WHERE column_name = NULL` match no rows.
- `scalar_literal IS TRUE` is true for nonzero non-`NULL` integer literals.
- `scalar_literal IS FALSE` is true for zero.
- `scalar_literal IS UNKNOWN` is true for SQL `NULL`.
- `IS NOT` forms are the logical negation of their positive forms.
- Supported exact scalar-literal predicates record no warnings. Supported
  truncated integer string comparisons record one `1292` warning at statement
  planning time.
- `WHERE` filtering still happens before grouping, aggregation, DML mutation,
  ordering, and limiting.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and cleanup on failure.
- Statement context owns diagnostics reset, warning count, affected rows, and
  previous-diagnostics behavior. Exact forms add no warnings; truncated integer
  string comparisons append one MySQL-shaped `1292` warning.
- Lexer/parser/AST own syntax admission and source spans. They represent the
  new scalar-literal predicate leaves independently of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner owns scalar-literal validation, descriptor column resolution,
  literal-left comparison normalization, unsupported-shape diagnostics, and
  generated physical predicate planning.
- Catalog descriptors remain authoritative for descriptor column resolution.
  SQLite schema text is not consulted for MySQL-visible column names.
- Result building is unchanged. `SELECT` statements return existing row
  results; DML statements use existing affected-row and non-row-result
  conventions.
- SQLite owns physical row scanning, filtering, grouping, ordering, limiting,
  updating, and deleting for generated SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature does not change the file format, catalog generation,
  descriptor versions, or SQLite schema generation.

## Grammar

The MyLite grammar remains deliberately narrower than MySQL's full expression
grammar. Intended Lemon-style extension:

```lemon
predicate_atom(A) ::= predicate_scalar_literal(V). {
    A = V;
}

predicate_atom(A) ::= predicate_scalar_literal(L) comparison_operator(O)
        predicate_scalar_literal(R). {
    A = mylite_sql_parser_make_comparison_predicate(state, L, O, R);
}

predicate_atom(A) ::= predicate_scalar_literal(L) comparison_operator(O)
        qualified_identifier(C). {
    A = mylite_sql_parser_make_comparison_predicate(state, L, O, C);
}

predicate_atom(A) ::= qualified_identifier(C) comparison_operator(O) NULL(N). {
    A = mylite_sql_parser_make_comparison_predicate(state, C, O, N);
}

predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) TRUE(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT TRUE(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) FALSE(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT FALSE(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) UNKNOWN(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT UNKNOWN(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NULL(T).
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT NULL(T).

predicate_scalar_literal(A) ::= INTEGER(T).
predicate_scalar_literal(A) ::= PLUS(P) INTEGER(T).
predicate_scalar_literal(A) ::= MINUS(M) INTEGER(T).
predicate_scalar_literal(A) ::= TRUE(T).
predicate_scalar_literal(A) ::= FALSE(T).
predicate_scalar_literal(A) ::= NULL(T).
```

The actual helper signatures may differ. Parser precedence remains the existing
predicate precedence: atoms, keyword `NOT`, `AND` / `&&`, `XOR`, then
`OR` / `||`.

## Runtime And SQLite Handling

Scalar literal truth predicates lower to a planned row-scalar truth predicate
with one bound parameter:

```sql
WHERE (?1)
```

The bound value is an `INTEGER` for signed integer and boolean literals or SQL
`NULL` for `NULL`. SQLite's truth filtering for integer `0`, nonzero integers,
and `NULL` matches the admitted MySQL truth subset.

Scalar literal comparisons lower to a planned row-scalar comparison:

```sql
WHERE (?1 = ?2)
WHERE (?1 IS ?2)       -- for <=>
```

Existing admitted row-scalar comparison predicates, such as
`CAST(column_name AS SIGNED) > '5'`, use the same planned row-scalar comparison
node. Exact quoted integer strings are decoded, trimmed of ASCII whitespace,
converted to signed 64-bit integers, and bound as SQLite integer parameters.

Literal-left descriptor comparisons normalize to existing descriptor-column
predicate nodes. Equality and inequality operators keep their operator; ordered
operators are flipped:

```sql
1 < column_name   ->   column_name > 1
1 <= column_name  ->   column_name >= 1
1 > column_name   ->   column_name < 1
1 >= column_name  ->   column_name <= 1
```

This preserves descriptor-driven conversion, string collation handling,
temporal normalization, `TIME` second conversion, source qualification, and
indexable column-left SQL where SQLite can use a physical index.

`NULL` comparison literals on descriptor-column comparisons bind SQL `NULL`.
For `<=>`, generated SQLite uses `IS`, matching the admitted null-safe-equality
truth table. For ordinary comparison operators, SQLite returns SQL `NULL`, so
rows do not pass a `WHERE` filter.

Scalar literal `IS` predicates are evaluated at planning time because their
operands are constants in this phase. The planner emits a bound integer `1` or
`0` row-scalar truth predicate. This avoids admitting broader expression-left
truth tests before the expression planner owns those semantics.

All generated SQL uses stable MyLite physical table names, quoted descriptor
identifiers, and numbered bound parameters. No string interpolation of literal
values is allowed. No rows are materialized in MyLite for this feature; SQLite
continues to do the physical scan/filter/update/delete.

## Diagnostics

- Unsupported grammar remains a parse error `1064 / 42000`.
- Missing default schema, unknown schema, unknown table, reserved names, and
  unsupported object kinds are resolved by the enclosing statement planner.
- Unknown descriptor columns in literal-left comparisons report the existing
  MySQL-compatible unknown-column diagnostic for the `WHERE` context.
- Scalar literal integers outside the signed 64-bit range report a
  deterministic MyLite unsupported/out-of-range diagnostic for scalar
  literals.
- Descriptor comparison literal values outside the target descriptor range keep
  the existing descriptor predicate range diagnostics.
- Unsupported literal kinds such as decimal/exponent strings, decimals, floats,
  hex, bit, and parameters are rejected deterministically.
- Unsupported expression shapes such as `column + 1`, `ABS(column)`, variables,
  subqueries, row constructors, and arbitrary functions remain deterministic
  unsupported syntax or runtime diagnostics according to the existing parser
  surface.
- Allocation failures report existing out-of-memory diagnostics.
- Physical SQLite failures report existing physical SQLite diagnostics.

## Tests

Coverage must include:

- parser acceptance for bare scalar literal truth predicates, scalar-literal
  comparisons, literal-left descriptor comparisons, descriptor-column `NULL`
  comparisons, and scalar literal `IS` tests;
- parser/runtime rejection for strings, decimals, floats, hex, bit, parameters,
  column arithmetic, functions, and descriptor-column bare truth predicates;
- `SELECT`, aggregate, grouped aggregate source filter, `DELETE`, and `UPDATE`
  behavior;
- `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`
  source filtering where existing source-select paths admit the predicate;
- `NULL`, `TRUE`, `FALSE`, `0`, nonzero, signed boundary, and out-of-range
  scalar literals;
- exact quoted integer right-hand values for admitted row-scalar comparisons;
- literal-left comparison operator flipping;
- `NULL` comparison behavior for ordinary comparisons and `<=>`;
- warning count remains zero for exact supported forms, and warning `1292`
  appears for supported truncated integer string comparisons;
- no result rows for successful DML and existing affected-row semantics;
- persistence across close/reopen and no `.mylite` preamble mutation;
- independent file-backed handles preserve independent updated row state;
- existing lexer, parser, runtime, storage, catalog, DML, and predicate tests
  remain passing.
