# Baseline Grouped HAVING IN Predicate

## Status

This slice extends the existing grouped aggregate `HAVING` subset with a
literal-list membership predicate over grouped descriptor columns:

```sql
SELECT group_column [AS alias], aggregate_result
FROM source
GROUP BY group_column
HAVING grouped_column_or_alias IN (literal [, literal ...])
```

It builds on `baseline-having-grouped-aggregate`,
`baseline-group-by-string-column`, and
`baseline-grouped-string-comparison-having`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped aggregate `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_having_grouped_aggregate_expectations.sh`
  and
  `packages/libmylite/tests/mysql_parser_corpus_select_clause_residuals_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes verify:

- `HAVING g IN (1, 2)` filters grouped integer descriptor values after
  grouping and before `ORDER BY` / `LIMIT`.
- A selected grouped-column alias may be the left operand, such as
  `HAVING k IN (1, NULL)`.
- `NULL` list items do not match non-`NULL` grouped values, and ordinary
  `IN` does not match a `NULL` grouped key.
- Grouped nonbinary string columns and selected aliases use the same default
  case-insensitive collation behavior as grouped string comparison predicates
  for the admitted ASCII values.

## Supported Surface

Supported predicate shape:

```sql
having_operand IN (having_in_value [, having_in_value ...])
```

`having_operand` may resolve to:

- an admitted grouped integer descriptor column;
- an admitted grouped nonbinary string descriptor column; or
- a unique selected alias for one of those grouped descriptor columns.

`having_in_value` admits:

- integer literals, optionally prefixed by unary `+` or `-`;
- `TRUE` and `FALSE`;
- `NULL`;
- one ordinary non-NUL string literal token for grouped nonbinary string
  descriptor columns.

The MyLite Lemon grammar for this slice is:

```lemon
having_predicate_atom ::= having_operand IN LPAREN having_in_value_list RPAREN.
having_in_value_list ::= having_in_value.
having_in_value_list ::= having_in_value_list COMMA having_in_value.
having_in_value ::= having_integer_value.
having_in_value ::= STRING.
having_in_value ::= NULL.
```

## Runtime Semantics

The grouped `HAVING` planner resolves the left operand through the existing
grouped descriptor and selected-alias resolver. The right list is converted to
bound planned values using the same descriptor-specific integer and string
conversion rules as the existing grouped `HAVING` comparison slice.

Generated SQLite SQL keeps the descriptor-owned left expression and collation:

```sql
HAVING grouped_column_sql IN (?, ?, ...)
```

SQLite evaluates the predicate during grouped aggregate execution. MyLite does
not materialize grouped rows to filter them and does not need a SQLite fork
hook.

## Non-Goals

This slice does not add:

- `NOT IN`;
- aggregate-result `IN` predicates;
- row-scalar, function, arithmetic, subquery, `VALUES`, row constructor,
  parameter, decimal, float, hex, bit, introduced-literal, temporal, JSON,
  enum, set, binary string, or spatial list values;
- `NULL`-safe membership behavior beyond ordinary SQL `IN`;
- boolean composition around `IN`;
- broad grouped `HAVING` expression planning; or
- new catalog, file-format, public ABI, VFS, or SQLite fork changes.

Unsupported forms continue to use deterministic MyLite diagnostics.

## Validation

Coverage includes:

- MySQL 8.4.9 expectation rows for integer grouped columns, selected aliases,
  `NULL` list items, and grouped string-column aliases;
- fast C runtime rows for grouped-column and alias membership predicates;
- parser-corpus runtime movement for
  `SELECT a FROM t1 GROUP BY a HAVING a IN (10,20)`; and
- focused grouped aggregate and parser-corpus CTest coverage.
