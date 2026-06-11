# Parser Corpus Expression Residual Surfaces

This slice admits remaining MySQL 8.4.9 scalar-expression corpus shapes that
are valid SQL but still fall out of MyLite's narrower statement grammars. The
implementation goal is parser compatibility and deterministic diagnostics, not
new expression execution semantics.

## Sources

- MySQL 8.4 Reference Manual, expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, operator precedence:
  <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
- MySQL 8.4 Reference Manual, comparison operators:
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_expression_residual_surfaces_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented in this slice:

- `DO` statements that contain valid but unsupported expression function or
  operator forms are classified as unsupported utility placeholders after the
  normal parser fails.
- Targeted query statements with verified expression residuals are admitted as
  unsupported utility placeholders when the existing grammar cannot parse them
  safely. This includes decimal-left predicates, parenthesized predicate-result
  comparisons, symbolic-`!` arithmetic over `FROM DUAL`, and shift operators
  inside interval arithmetic.
- Function-call placeholders may be followed by `ASC` or `DESC`, covering
  function expressions in `ORDER BY` items after normal parsing fails.
- Four-argument keyword-named `INSERT(...)` function calls are admitted in
  `GROUP BY` expression contexts after normal parsing fails.
- Function calls in `GROUP BY ... WITH ROLLUP`, parenthesized `LIKE` operands,
  all-identifier `BETWEEN` bounds, mixed identifier `IN` lists, and
  `INTERVAL(expr) unit` residuals are admitted as unsupported utility
  placeholders.
- Charset introducer string literals such as `_latin1'a'` in broader expression
  contexts are admitted as unsupported utility placeholders.
- Bare `CURRENT_DATE` and `CURRENT_TIME` in unsupported predicate positions are
  admitted as unsupported utility placeholders.

Out of scope:

- Executing newly admitted broad expression forms.
- Replacing MyLite's current context-specific expression grammar with a general
  MySQL expression grammar.
- Treating malformed functions, missing operands, incomplete tails, truncated
  corpus rows, or legacy removed statements as valid.
- New SQLite fork hooks, storage changes, catalog changes, or dependencies.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies that MySQL accepts representative forms:

- decimal left operands in predicates, such as `0.9 > t0.c0`;
- predicate-result comparisons, such as `(a > b) <> c`;
- `DO COUNT(DISTINCT ROUND(...))`;
- unary `!` and arithmetic in `DO` and `SELECT`;
- shift operators inside interval arithmetic;
- `_latin1` charset introducer literals in predicates;
- bare `CURRENT_DATE` and `CURRENT_TIME` in predicate operands;
- function expressions in `ORDER BY ... DESC`;
- keyword-named functions, function group keys with `WITH ROLLUP`,
  parenthesized `LIKE` patterns, identifier `BETWEEN` / `IN` operands, and
  parenthesized interval values.

These statements may return empty results or ordinary scalar rows in MySQL. In
MyLite, the newly admitted forms execute through the existing unsupported
utility placeholder diagnostic until expression semantics are implemented.

## MyLite Grammar Snippets

These snippets describe the intended future MyLite-owned grammar surface. This
slice implements post-failure placeholder classification instead of installing a
broad Lemon grammar rewrite.

```lemon
do_statement ::= DO expression_list.

expression ::= expression comparison_operator expression.
expression ::= expression shift_operator expression.
expression ::= expression bitwise_operator expression.
expression ::= NOT_OPERATOR expression.
expression ::= charset_introducer string_literal.
expression ::= CURRENT_DATE.
expression ::= CURRENT_TIME.
expression ::= expression LIKE LPAREN expression RPAREN.
expression ::= expression BETWEEN identifier AND identifier.
expression ::= expression IN LPAREN identifier_list RPAREN.
expression ::= INTERVAL LPAREN expression RPAREN date_interval_unit.

order_key ::= expression ASC.
order_key ::= expression DESC.
group_key ::= function_call WITH ROLLUP.
```

## Runtime Semantics

No SQLite fork hook is needed. This slice is parser fallback and diagnostic
routing:

- existing normally parsed expression statements keep their current AST and
  runtime paths;
- newly admitted residual forms return `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`;
- runtime execution returns the existing `1064 / 42000` unsupported utility
  diagnostic with no result rows, no warnings, no catalog mutation, and no
  variable side effects;
- malformed argument lists such as missing function arguments and incomplete
  operator tails remain syntax errors.

## Tests

- `mysql_parser_corpus_expression_residual_surfaces_expectations.sh` records
  representative MySQL 8.4.9 behavior.
- `parser_corpus_expression_residual_surfaces_test.c` covers placeholder
  classification and syntax-error preservation.
- `runtime_parser_corpus_expression_residual_surfaces_test.c` covers the
  unsupported diagnostic path for newly admitted forms.

The parser corpus benchmark over
`build/perf-data/mysql-server-tests-queries.csv` must be rerun before commit.

Latest run after this slice:

```text
parse.csv.mysql_server_tests: queries=69595 ok=69404 errors=191
parse_status: lexer_error=21 syntax_error=169 stack_overflow=1
```

## Compatibility Status

This slice improves parser compatibility for broad expression surfaces by
converting valid MySQL syntax from parser failure into explicit unsupported
placeholder behavior. It does not mark broad expression execution, full
character-set literal semantics, general temporal predicate conversion, or
general expression ordering/grouping as supported.
