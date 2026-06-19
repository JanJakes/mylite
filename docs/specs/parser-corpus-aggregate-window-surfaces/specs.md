# Parser Corpus Aggregate Window Surfaces

This slice reduces high-volume parser failures in MySQL server-test aggregate,
grouping, and window-function query shapes. The work admits common MySQL 8.4.9
syntax into MyLite's AST while preserving the current runtime rule: only the
documented aggregate and window subsets execute; broader forms fail with
explicit unsupported diagnostics.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### Aggregate Argument Placeholders

MySQL aggregate functions accept expressions, literals, and nested expressions
as arguments. MyLite currently executes a narrower descriptor-column subset.
This slice admits common corpus shapes as parser placeholders:

- literal `COUNT(DISTINCT ...)`, `MIN(...)`, `MAX(...)`, `AVG(...)`, `SUM(...)`,
  and bit aggregates;
- simple arithmetic aggregate arguments such as `SUM(a / b)`, `SUM(k + 1)`,
  and `SUM(id + r00 + r01)`;
- aggregate arguments containing cast/conversion and nested aggregate
  expressions where they appear in window or grouped query syntax.

Supported existing descriptor-column aggregate forms keep their current AST
and execution behavior. Newly admitted forms must not be executed by silently
falling through to SQLite-compatible behavior.

### GROUP_CONCAT Argument Lists

MySQL `GROUP_CONCAT()` accepts one or more expressions before optional
aggregate-local `ORDER BY` and `SEPARATOR` clauses, and the `DISTINCT` modifier
may apply to that expression list. MyLite executes supported single-expression
`GROUP_CONCAT(DISTINCT expr ...)` forms in the existing descriptor-backed
envelope and supported multi-expression `GROUP_CONCAT(expr, expr...)` forms by
applying existing per-row `CONCAT()` semantics before aggregation.
`GROUP_CONCAT(DISTINCT expr, expr... ORDER BY ... SEPARATOR ...)` remains a
parser placeholder.

### Grouping Keys

MySQL accepts general expressions as `GROUP BY` keys. MyLite execution remains
descriptor-backed, but this slice admits common parser-only keys from the
corpus:

- parenthesized group keys such as `GROUP BY (k)`;
- string-literal keys such as `GROUP BY 'x'`;
- selected aggregate-window and placeholder expressions where the existing
  grouped executor will reject unsupported execution explicitly.

`WITH ROLLUP` still returns MyLite's existing unsupported diagnostic when it
reaches runtime.

### Window Frames With INTERVAL Bounds

MySQL window frame bounds can use temporal intervals in `RANGE` frames, such as
`RANGE INTERVAL 1 DAY PRECEDING`. This slice admits `INTERVAL value unit`
bounds as window-frame placeholders so aggregate/window query parsing can
progress to the current unsupported-window diagnostics.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
aggregate_placeholder_argument ::= scalar_literal.
aggregate_placeholder_argument ::= aggregate_arithmetic_expression.
aggregate_placeholder_argument ::= cast_convert_expression.
aggregate_placeholder_argument ::= nested_aggregate_expression.

aggregate_arithmetic_expression ::= aggregate_arithmetic_expression PLUS aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_expression MINUS aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_expression STAR aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_expression SLASH aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_factor PLUS aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_factor MINUS aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_factor STAR aggregate_arithmetic_factor.
aggregate_arithmetic_expression ::= aggregate_arithmetic_factor SLASH aggregate_arithmetic_factor.
aggregate_arithmetic_factor ::= qualified_identifier.
aggregate_arithmetic_factor ::= scalar_literal.
nested_aggregate_expression ::= MIN LPAREN qualified_identifier RPAREN.
nested_aggregate_expression ::= MAX LPAREN qualified_identifier RPAREN.
nested_aggregate_expression ::= SUM LPAREN qualified_identifier RPAREN.
nested_aggregate_expression ::= AVG LPAREN qualified_identifier RPAREN.

expression ::= MIN LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= MAX LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= SUM LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= AVG LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= BIT_AND LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= BIT_OR LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= BIT_XOR LPAREN aggregate_placeholder_argument RPAREN aggregate_window_opt.
expression ::= COUNT LPAREN DISTINCT aggregate_placeholder_argument COMMA function_argument_list
               RPAREN aggregate_window_opt.
```

```lemon
expression ::= GROUP_CONCAT LPAREN expression COMMA function_argument_list
               group_concat_order_opt group_concat_separator_opt RPAREN aggregate_window_opt.
expression ::= GROUP_CONCAT LPAREN DISTINCT expression COMMA function_argument_list
               group_concat_order_opt group_concat_separator_opt RPAREN aggregate_window_opt.

group_key ::= LPAREN group_key RPAREN.
group_key ::= STRING.

window_frame_bound ::= INTERVAL expression date_interval_unit PRECEDING.
window_frame_bound ::= INTERVAL expression date_interval_unit FOLLOWING.
```

## Runtime Behavior

No SQLite fork hook is needed. This is parser, AST, and diagnostic-routing work.
Runtime must continue to:

- execute only already-supported descriptor-column aggregates and baseline
  window-function subsets;
- report unsupported aggregate-window diagnostics for aggregate nodes with
  `OVER` clauses;
- report existing unsupported grouped-aggregate diagnostics for non-descriptor
  group keys or unsupported aggregate arguments;
- report generic unsupported-function diagnostics for parser-only
  multi-expression `GROUP_CONCAT(DISTINCT ...)` placeholders.

## Tests

MySQL 8.4.9 expectations cover representative accepted syntax and result rows.
MyLite parser tests cover AST acceptance for broader aggregate/window forms.
Runtime tests cover explicit unsupported diagnostics and verify existing
supported aggregate forms are not regressed.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility for common aggregate/window syntax.
It does not mark full aggregate expression execution, multi-expression
`GROUP_CONCAT(DISTINCT)` execution, general expression grouping, `WITH ROLLUP`
execution, or aggregate-window execution as supported.
