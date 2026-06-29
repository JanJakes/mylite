# Parser Corpus JSON And Statistical Aggregate Window Surfaces

This slice extends the parser-corpus aggregate/window placeholder work to cover
MySQL 8.4.9 aggregate-window syntax whose function names are not dedicated
MyLite parser tokens yet.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

MySQL accepts JSON and statistical aggregate functions with an `OVER` clause,
including:

- `JSON_ARRAYAGG(expr) OVER window_spec_or_name`;
- `JSON_OBJECTAGG(key_expr, value_expr) OVER window_spec_or_name`;
- `STDDEV(expr)`, `STDDEV_POP(expr)`, `STDDEV_SAMP(expr)`,
  `VAR_POP(expr)`, `VAR_SAMP(expr)`, and `VARIANCE(expr)` with an `OVER`
  clause.

MyLite did not implement executable JSON or statistical aggregate-window
semantics when this parser slice landed. Statistical aggregate windows are now
covered by `docs/specs/baseline-statistical-aggregate-window-functions/specs.md`.
JSON aggregate windows remain parser placeholders so applications and corpus
probes reach a deterministic unsupported diagnostic instead of a syntax error.

The parser must continue to reject arbitrary scalar or unknown function names
with `OVER`; `foo(...) OVER ()` is not part of this placeholder surface.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
generic_aggregate_window_name ::= IDENTIFIER
    /* semantic name check:
       JSON_ARRAYAGG, JSON_OBJECTAGG,
       STDDEV, STDDEV_POP, STDDEV_SAMP,
       VAR_POP, VAR_SAMP, VARIANCE */

expression ::= generic_aggregate_window_name LPAREN RPAREN aggregate_window_opt.
expression ::= generic_aggregate_window_name LPAREN function_argument_list RPAREN
               aggregate_window_opt.
```

When `aggregate_window_opt` is absent, the existing `GENERIC_FUNCTION` AST
behavior is preserved. When it is present, the parser accepts only the listed
aggregate names and appends the window spec/reference as the last child.

## Runtime Behavior

No SQLite fork hook is needed for the parser placeholder behavior. Parsed JSON
aggregate-window placeholders must trigger the same deterministic unsupported
aggregate-window diagnostic as the existing unsupported aggregate-window paths.
Statistical aggregate-window execution uses public SQLite window callbacks in
the later statistical baseline.

Non-window `JSON_ARRAYAGG(...)`, `JSON_OBJECTAGG(...)`, `STDDEV(...)`,
`VARIANCE(...)`, and similar generic calls keep the existing unsupported
generic-function behavior until a later aggregate implementation slice lands.

## Tests

MySQL 8.4.9 expectations cover representative accepted JSON and statistical
aggregate-window syntax. MyLite parser tests cover AST admission and ensure
arbitrary generic functions with `OVER` still fail to parse. Runtime tests
verify that admitted JSON placeholders return the existing unsupported
aggregate-window diagnostic. Statistical runtime coverage lives with the
statistical aggregate-window baseline.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility for JSON and statistical aggregate
window syntax. JSON aggregate-window execution remains unsupported here;
statistical aggregate-window execution is documented by the later baseline.
