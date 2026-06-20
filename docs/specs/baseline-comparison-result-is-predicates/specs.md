# Baseline Comparison Result IS Predicates

This slice implements postfix `IS` predicates whose subject is a planned
comparison predicate, such as `u = 256 IS UNKNOWN`, in the existing
descriptor-backed `WHERE` predicate envelope.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html

## MySQL 8.4.9 Runtime Observations

Observed against MySQL 8.4.9:

```sql
CREATE TABLE postfix_is_probe(a INT NULL);
INSERT INTO postfix_is_probe VALUES (1),(2),(NULL);
SELECT COUNT(*) FROM postfix_is_probe WHERE a=1 IS NOT NULL;
SELECT COUNT(*) FROM postfix_is_probe WHERE a=1 IS UNKNOWN;
SELECT COUNT(*) FROM postfix_is_probe WHERE a=1 IS NOT UNKNOWN;
SELECT COUNT(*) FROM postfix_is_probe WHERE a=1 IS NULL;
```

MySQL returns `2`, `1`, `2`, and `1`. The postfix `IS` predicate tests the
three-valued comparison result: `IS UNKNOWN` and `IS NULL` match rows where the
comparison result is `NULL`; `IS NOT UNKNOWN` and `IS NOT NULL` match rows where
the comparison result is either true or false.

## Scope

In scope:

- `qualified_identifier comparison_operator predicate_comparison_value
  IS NULL`;
- `qualified_identifier comparison_operator predicate_comparison_value
  IS NOT NULL`;
- `qualified_identifier comparison_operator predicate_comparison_value
  IS UNKNOWN`;
- `qualified_identifier comparison_operator predicate_comparison_value
  IS NOT UNKNOWN`;
- execution in the existing single-table, `UPDATE`, `DELETE`, and internal
  no-subquery predicate SQL builder paths where the underlying comparison is
  already supported;
- MySQL-style `UNKNOWN` equivalence to `NULL` for comparison-result tests.

Out of scope:

- postfix `IS TRUE` / `IS FALSE` over comparison results;
- broader expression subjects beyond comparison predicates that MyLite already
  plans;
- new comparison operand conversions, collations, subquery shapes, or metadata.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape.

```lemon
predicate_atom ::=
    qualified_identifier predicate_comparison_operator predicate_comparison_value IS NULL.
predicate_atom ::=
    qualified_identifier predicate_comparison_operator predicate_comparison_value IS NOT NULL.
predicate_atom ::=
    qualified_identifier predicate_comparison_operator predicate_comparison_value IS UNKNOWN.
predicate_atom ::=
    qualified_identifier predicate_comparison_operator predicate_comparison_value IS NOT UNKNOWN.
```

## Implementation Notes

The parser builds an `IS` predicate whose child is the existing comparison
predicate node. The predicate planner reuses the existing comparison planner,
then records the postfix `IS` operator on the planned comparison node. SQL
generation emits the comparison as a parenthesized SQLite expression followed
by `IS NULL` or `IS NOT NULL`, letting SQLite perform row filtering without
materializing rows in MyLite.

`IS UNKNOWN` maps to `IS NULL`; `IS NOT UNKNOWN` maps to `IS NOT NULL`.

## Tests

- `parser_corpus_query_expression_clause_surfaces_test.c` verifies the parser
  accepts the corpus postfix `IS` forms.
- `runtime_parser_corpus_query_expression_clause_surfaces_test.c` verifies
  `IS NULL`, `IS NOT NULL`, `IS UNKNOWN`, and `IS NOT UNKNOWN` result counts over
  a table containing matching, nonmatching, and `NULL` values, including
  equality, range, and null-safe comparisons, plus `UPDATE` and `DELETE`
  predicate reuse.
- `mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`
  records the corresponding MySQL 8.4.9 expected results.
