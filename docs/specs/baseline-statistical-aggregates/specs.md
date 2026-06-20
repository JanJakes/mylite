# Baseline Statistical Aggregates

## Scope

Implement the MySQL 8.4.9 statistical aggregate family for MyLite's current
descriptor-backed aggregate envelope:

- `STD(expr)`
- `STDDEV(expr)`
- `STDDEV_POP(expr)`
- `STDDEV_SAMP(expr)`
- `VAR_POP(expr)`
- `VAR_SAMP(expr)`
- `VARIANCE(expr)`

The slice covers one-table aggregate reads, tableless scalar aggregate reads,
supported row-scalar aggregate arguments, optional baseline `WHERE`, aliases,
grouped aggregate forms that already work for `SUM()` and `AVG()`, selected
grouped statistical aggregate-alias ordering, selected statistical
aggregate-expression ordering, and selected grouped statistical aggregate
`HAVING`.

The implementation does not cover executable aggregate windows, `DISTINCT`,
string-to-double conversion warnings, decimal widening metadata, or full MySQL
grouping semantics outside the current grouped aggregate planner envelope.

## Compatibility Authority

Official MySQL 8.4 documentation lists the family as aggregate functions:
`STD()`, `STDDEV()`, and `STDDEV_POP()` return population standard deviation;
`STDDEV_SAMP()` returns sample standard deviation; `VAR_POP()` and
`VARIANCE()` return population variance; `VAR_SAMP()` returns sample variance.
Aggregate functions ignore `NULL` unless stated otherwise.

Reference: <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>

Runtime probes were executed against `mysql:8.4.9` in the
`mylite-mysql-849` container. Observed behavior that defines this slice:

- Population functions divide by `N`.
- Sample functions divide by `N - 1` and return `NULL` when there are fewer
  than two non-`NULL` inputs.
- Empty and all-`NULL` inputs return `NULL`.
- Tableless `SELECT STDDEV_POP(1)` returns `0`; tableless
  `SELECT STDDEV_SAMP(1)` returns `NULL`.
- `STD()` and `STDDEV()` are aliases for `STDDEV_POP()`.
- `VARIANCE()` is an alias for `VAR_POP()`.
- `STDDEV_POP(DISTINCT n)` is a MySQL syntax error.
- Aggregate-window forms are valid MySQL syntax but remain an explicit MyLite
  runtime gap for this slice.
- Selected grouped statistical aggregate aliases and descriptor-column
  aggregate expressions sort by their aggregate double/`NULL` value. `NULL`
  aggregate results sort first for ascending order and last for descending
  order in the verified subset.
- Selected grouped statistical aggregate aliases and repeated selected
  aggregate expressions may be used as grouped `HAVING` operands. Population
  aggregate predicates compare the aggregate double result; sample aggregate
  predicates can be tested with `IS NULL` for groups with fewer than two
  non-`NULL` inputs.

## MyLite Syntax

The function names are not made reserved words. The parser recognizes matching
generic identifier function calls and specializes one-argument calls into
statistical aggregate AST nodes.

Independent Lemon-shape snippet:

```lemon
expression ::= IDENTIFIER LPAREN function_argument_list RPAREN aggregate_window_opt.
/* Builder specialization:
   If IDENTIFIER is STD, STDDEV, STDDEV_POP, STDDEV_SAMP, VAR_POP, VAR_SAMP,
   or VARIANCE and the argument list has exactly one item, return the matching
   statistical aggregate node and attach aggregate_window_opt. */
```

## Runtime Semantics

MyLite plans each aggregate through the existing column and grouped aggregate
paths. The planner maps aliases as follows:

- `STD`, `STDDEV`, `STDDEV_POP` -> population standard deviation
- `STDDEV_SAMP` -> sample standard deviation
- `VAR_POP`, `VARIANCE` -> population variance
- `VAR_SAMP` -> sample variance

SQLite executes the numeric accumulation through MyLite-owned aggregate UDFs
registered on each connection. The aggregate state uses a stable online
variance algorithm over SQLite numeric values and ignores `NULL` arguments.
No SQLite fork hook is required.

For this baseline, MyLite validates descriptor-column arguments with the same
integer descriptor envelope used by `SUM()` / `AVG()`. Supported row-scalar
aggregate arguments are translated to SQLite expressions and evaluated by the
same UDFs. String arguments that MySQL would coerce with truncation warnings
remain outside this slice.

## Diagnostics

Unsupported forms must fail deterministically rather than falling through to
SQLite:

- aggregate windows: existing aggregate-window unsupported diagnostic;
- `DISTINCT`: parser/runtime unsupported where the syntax is admitted;
- unsupported sources, clauses, argument columns, and expression shapes:
  statistical aggregate-specific unsupported messages matching the existing
  aggregate diagnostics style.

## Tests

The MySQL expectation script records:

- alias equivalence;
- population and sample formulas;
- empty, all-`NULL`, and single-row results;
- baseline `WHERE`;
- tableless calls;
- grouped results;
- row-scalar expression arguments;
- `DISTINCT` syntax rejection and window syntax admission.

The C runtime test covers:

- one-table descriptor-column aggregate reads;
- tableless scalar aggregate reads;
- row-scalar aggregate arguments;
- grouped forms;
- selected grouped aggregate-alias ordering;
- selected descriptor-column grouped aggregate-expression ordering;
- selected grouped aggregate-alias and repeated aggregate-expression `HAVING`;
- rename/truncate/drop behavior;
- unsupported diagnostics.
