# Parser Corpus Expression Operator And Temporal Surfaces

This slice reduces remaining MySQL server-test parser-corpus failures in
general expression syntax. The goal is deterministic parser fallback routing
for common MySQL expression forms that currently fail before semantic analysis,
while keeping executable support limited to behavior MyLite already implements
correctly.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### Deprecated Logical Operators

MySQL accepts `&&` as logical `AND` and, unless `PIPES_AS_CONCAT` is enabled,
`||` as logical `OR`. MyLite already tokenizes these operators and has narrow
executable paths for selected logical and `PIPES_AS_CONCAT` expression
surfaces, but the broad query expression grammar remains intentionally narrow.

This slice recognizes otherwise failed query statements containing complete
scalar forms such as:

- `expr && expr`
- `expr || expr` when parsed as logical OR

Recognized failed statements become unsupported-utility placeholders. Existing
normally parsed `PIPES_AS_CONCAT` and logical-expression paths remain
unchanged. This slice does not add executable logical `&&`/`||` semantics or
new deprecation warnings.

### LIKE Expression Forms

MySQL accepts `bit_expr [NOT] LIKE simple_expr [ESCAPE simple_expr]` and
`bit_expr SOUNDS LIKE bit_expr` as predicate expressions. MyLite already has
limited executable `LIKE` predicate support and expression-level `LIKE`, but
it misses common corpus shapes:

- expression-level `NOT LIKE`;
- expression-level `LIKE ... ESCAPE ...`;
- table-backed predicate `column LIKE column` and `column LIKE column ESCAPE`
  admission;
- `SOUNDS LIKE` expression admission.

This slice recognizes these well-formed surfaces after normal parser failure.
Executable `LIKE` support remains limited to the existing descriptor and scalar
envelopes. Unsupported newly admitted forms fail with explicit unsupported
diagnostics rather than syntax errors.

### Typed Temporal Literals In Predicates

MySQL accepts standard temporal literal introducers with optional whitespace,
for example `DATE '2024-01-01'`, `TIME '01:02:03'`, and
`TIMESTAMP '2024-01-01 01:02:03'`. MyLite already tokenizes these as typed
temporal literal introducers in selected expression and DML value contexts.

This slice recognizes otherwise failed query statements containing typed
temporal literal introducer/string pairs, including predicate-like corpus
shapes such as `IN (DATE'...')`. It does not broaden executable temporal
predicate conversion or distinguish the introducer from ordinary quoted
temporal values outside existing supported paths.

### Interval Arithmetic Admission

MySQL uses `INTERVAL expr unit` as an interval expression and permits temporal
arithmetic with `+` and `-`, including `date + INTERVAL expr unit`,
`date - INTERVAL expr unit`, and `INTERVAL expr unit + date`. MyLite already
parses interval arguments inside temporal functions and window frames, but not
general interval expressions.

This slice recognizes otherwise failed query statements containing the common
`INTERVAL <single-token value> unit` surface. Runtime execution of general
temporal interval arithmetic remains unsupported unless the statement already
matches an existing documented temporal-function slice.

## MyLite Grammar Snippets

These snippets describe the intended future MyLite-owned Lemon grammar shape.
This slice does not install the broad grammar directly because the current
monolithic Lemon grammar and runtime planners use narrower expression subsets;
the implemented behavior is post-failure placeholder classification.

```lemon
expression ::= expression LOGICAL_AND expression.
expression ::= expression LOGICAL_OR expression.

expression ::= expression LIKE expression.
expression ::= expression LIKE expression ESCAPE expression.
expression ::= expression NOT LIKE expression.
expression ::= expression NOT LIKE expression ESCAPE expression.
expression ::= expression SOUNDS LIKE expression.

expression ::= INTERVAL expression date_interval_unit.

predicate_value ::= TEMPORAL_LITERAL_INTRODUCER STRING.
predicate_atom ::= qualified_identifier LIKE qualified_identifier escape_opt.
predicate_atom ::= qualified_identifier NOT LIKE qualified_identifier escape_opt.
```

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser fallback and routing work:

- existing executable scalar and descriptor `LIKE`, logical, and temporal
  predicate forms keep their current behavior;
- recognized broad expression forms that fail normal parsing return a
  deterministic unsupported diagnostic through the unsupported-utility
  placeholder path;
- parser placeholders are not used for malformed operator tails, unbalanced
  parentheses, or incomplete `ESCAPE` clauses.

## Tests

MySQL 8.4.9 expectations cover representative results for `&&`, `||`, `NOT
LIKE`, `LIKE ... ESCAPE`, `SOUNDS LIKE`, typed temporal literals, and interval
arithmetic syntax. MyLite parser tests cover placeholder AST acceptance and
preservation of malformed syntax errors. Runtime tests cover unsupported
diagnostics for the newly admitted placeholder surfaces.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility by converting recognized expression
operator and temporal literal syntax failures into explicit unsupported
placeholders. It does not implement full MySQL expression planning, full
collation-aware `LIKE`, phonetic `SOUNDS LIKE` execution, broad typed temporal
literal semantics, general interval arithmetic, or expression metadata.
