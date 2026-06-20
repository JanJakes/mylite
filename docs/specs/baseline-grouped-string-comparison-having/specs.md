# Baseline Grouped String Comparison HAVING

## Status

This slice extends the existing grouped aggregate `HAVING` subset so grouped
ASCII nonbinary string descriptor columns and their selected aliases can be
compared with ordinary string literals:

```sql
SELECT string_group_column AS alias, COUNT(*) AS c
FROM source
GROUP BY string_group_column
HAVING alias comparison_operator 'literal'
```

It builds on the grouped aggregate, grouped string-column, grouped multiple-key,
and `ANY_VALUE()` alias comparison `HAVING` slices.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped aggregate `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- Baseline grouped string columns:
  `docs/specs/baseline-group-by-string-column/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_group_by_string_column_expectations.sh`
  and
  `packages/libmylite/tests/mysql_parser_corpus_select_clause_residuals_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes verify:

- `HAVING string_alias = 'ALICE'` compares grouped `VARCHAR` values using the
  default case-insensitive nonbinary collation for admitted ASCII values.
- Direct grouped-column operands such as `HAVING name <> 'ALICE'` and
  qualified operands such as `HAVING s.name = 'BOB'` work after grouping.
- `CHAR` grouped aliases compare after MySQL's normal trailing-space behavior.
- Baseline `TEXT` grouped columns compare with the same default collation for
  the probed ASCII values.
- `NULL` grouped string values do not satisfy ordinary comparison predicates.

## Supported Surface

Supported `HAVING` comparison shape:

```sql
having_operand comparison_operator string_literal
```

`comparison_operator` is one of:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

`having_operand` may resolve to:

- a grouped `CHAR`, `VARCHAR`, or baseline `TEXT` descriptor column;
- a source-qualified form of that grouped descriptor column; or
- a unique selected alias for that grouped descriptor column.

`string_literal` is limited to one ordinary, non-NUL string literal token. The
current Lemon grammar admits this through the shared grouped `HAVING` predicate
extension:

```lemon
having_predicate_atom ::= having_operand comparison_operator having_integer_value.
having_predicate_atom ::= having_operand comparison_operator STRING.
```

## Runtime Semantics

The grouped `HAVING` planner resolves the left operand through the existing
grouped descriptor and alias resolver. When the resolved operand is a supported
string descriptor and the right operand is an ordinary string literal, MyLite
decodes the literal into a bound text parameter.

Generated SQLite SQL keeps descriptor-owned collation on the left expression:

```sql
HAVING "name" COLLATE "utf8mb4_0900_ai_ci" = ?
```

SQLite performs grouping and `HAVING` filtering. MyLite does not materialize
source rows to evaluate the predicate and does not require a SQLite fork hook.

## Non-Goals

This slice does not add:

- numeric, decimal, float, hex, bit, temporal, binary string, JSON, enum, set,
  or spatial grouped string comparison coercions;
- adjacent string literal concatenation, character-set introducers, explicit
  `COLLATE`, parameters, functions, row constructors, subqueries, or arbitrary
  right-hand expressions;
- `BETWEEN`, `IN`, `LIKE`, `REGEXP`, boolean composition, or bare truth tests
  in grouped `HAVING`;
- full Unicode collation parity beyond MyLite's current registered ASCII
  `utf8mb4_0900_ai_ci` subset;
- aggregate result string comparisons beyond the existing `ANY_VALUE()` alias
  comparison slice; or
- new source forms, file-format changes, catalog mutation, public ABI changes,
  or SQLite fork patches.

Unsupported forms continue to use deterministic MyLite diagnostics.

## Validation

Coverage includes:

- MySQL 8.4.9 expectation rows for `VARCHAR`, qualified `VARCHAR`, `CHAR`, and
  baseline `TEXT` grouped comparisons;
- fast C runtime rows for grouped string aliases, direct columns, source
  qualifiers, range operators, `NULL` exclusion, and case-insensitive ASCII
  comparisons;
- parser-corpus runtime movement for `SELECT a,b FROM t1 GROUP BY a,b HAVING
  b='hello'`; and
- preservation of the existing deterministic rejection for numeric RHS values
  such as `HAVING name > 1`.
