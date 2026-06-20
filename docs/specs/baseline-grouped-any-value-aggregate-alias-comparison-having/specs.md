# Baseline Grouped ANY_VALUE Aggregate Alias Comparison HAVING

## Status

This slice extends grouped `HAVING` predicates on selected `ANY_VALUE(column)`
aliases from null tests to comparison predicates in the current descriptor-backed
grouped `ANY_VALUE()` envelope:

```sql
SELECT group_column, ANY_VALUE(value_column) AS value
FROM source
GROUP BY group_column
HAVING value comparison_operator literal
```

The selected alias must resolve to exactly one selected `ANY_VALUE()` result.
The `ANY_VALUE()` argument must be an unqualified or source-qualified descriptor
column whose group values are intentionally treated as MySQL's arbitrary
representative value.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `ANY_VALUE()`:
  `docs/specs/baseline-any-value-function/specs.md`
- Baseline grouped `ANY_VALUE()` alias `HAVING`:
  `docs/specs/baseline-grouped-any-value-aggregate-alias-having/specs.md`
- MySQL 8.4 Reference Manual, miscellaneous functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html#function_any-value
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh`
records MySQL 8.4.9 behavior for selected `ANY_VALUE()` alias comparison
predicates. Observed behavior:

- `HAVING selected_any_value_alias = 10`, `<> 10`, and `< 20` filter numeric
  representative values after grouping.
- `HAVING selected_any_value_alias = 'TEN'` uses the current case-insensitive
  nonbinary string collation for admitted ASCII `VARCHAR` values.
- `NULL` representative values follow normal SQL comparison truth semantics and
  do not pass equality or inequality comparisons.
- Repeating `ANY_VALUE(column)` directly in `HAVING` remains a MySQL
  unknown-column diagnostic in the verified envelope.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       ANY_VALUE(descriptor_column) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING alias comparison_operator literal
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

The comparison operator is one of `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and
`>=`.

The value literal is limited to:

- integer or boolean literals for selected `ANY_VALUE()` aliases over integer
  descriptor columns; and
- single-segment non-NUL string literals for selected `ANY_VALUE()` aliases
  over the supported nonbinary string descriptor family.

MyLite extends the grouped `HAVING` comparison grammar narrowly:

```lemon
having_predicate_atom ::= having_operand comparison_operator having_integer_value.
having_predicate_atom ::= having_operand comparison_operator STRING.
```

The string literal production is intentionally limited to a single ordinary
string token. Adjacent string literal concatenation, other literal families, and
expression-valued comparison operands remain outside this slice.

## Runtime Semantics

MyLite lowers grouped `ANY_VALUE(column)` to the resolved physical descriptor
column in the grouped `SELECT` list. The grouped `HAVING` planner resolves the
selected alias to that aggregate result and converts the right-hand literal
against the selected `ANY_VALUE()` argument descriptor. String operands are
emitted through MyLite's registered ASCII `utf8mb4_0900_ai_ci` collation helper
for the admitted comparison subset.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- direct `HAVING ANY_VALUE(column)` expression operands;
- grouped `ANY_VALUE()` expression arguments;
- `ANY_VALUE()` aliases over binary string, temporal, decimal, approximate,
  enum, set, JSON, or spatial descriptor columns;
- broader grouped `HAVING` boolean composition, `BETWEEN`, `IN`, `LIKE`,
  `REGEXP`, subqueries, or row constructors;
- deterministic representative-row selection;
- aggregate windows; or
- new source forms beyond the existing grouped `ANY_VALUE()` support.

Unsupported forms continue to return deterministic MyLite diagnostics.

## Validation

Required verification for this slice:

```sh
sh -n packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh
packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh
cmake --build --preset dev --target mylite_runtime_any_value_function_test
ctest --preset dev -R '^libmylite\.runtime\.any_value_function$' --output-on-failure
git diff --check
cmake --workflow --preset check
```
