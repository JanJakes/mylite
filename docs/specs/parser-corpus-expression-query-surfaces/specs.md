# Parser Corpus Expression Query Surfaces

This slice reduces high-volume MySQL server-test parser failures in expression
and query clauses where MyLite already has compatible AST placeholders or
nearby runtime diagnostics. The goal is to admit common MySQL 8.4.9 syntax and
then either execute through existing behavior or reject with a deterministic
compatibility diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/regexp.html
- https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html

Runtime probes are verified against MySQL 8.4.9 before the slice is marked
complete.

## Scope

### Unary Binary Conversion

MySQL accepts unary `BINARY expr` as a binary-string conversion. MyLite already
has AST and execution support for `CAST(expr AS BINARY)` and related binary
conversion forms. This slice parses unary `BINARY expr` into the same binary
conversion AST node so existing scalar planning owns execution.

### REGEXP / RLIKE Expressions

MySQL permits `REGEXP`, `RLIKE`, `NOT REGEXP`, and `NOT RLIKE` between general
expressions, not only descriptor columns and string literals. MyLite already
has operator kinds and baseline ASCII regexp execution for selected predicate
forms. This slice parses expression-level regexp predicates and leaves runtime
support limited to the documented regexp subset.

### Keyword Function Placeholders

The corpus contains keyword function calls that cannot be parsed by the generic
`IDENTIFIER(...)` rule because the function names are MySQL keywords. This
slice admits targeted keyword-function placeholders:

- `ROW(...)` row constructors in scalar expression positions;
- `VALUES(column)` in nested expressions, such as duplicate-key update
  expressions;
- `GROUPING(...)` used with `GROUP BY ... WITH ROLLUP`;
- spatial constructor names such as `POINT(...)` and `GEOMETRYCOLLECTION(...)`.

These functions are parsed as generic functions unless MyLite already has a
specific supported AST node. Unsupported execution must fail explicitly instead
of becoming a syntax error or silently evaluating with SQLite semantics.

### CHAR(... USING charset)

MySQL `CHAR()` accepts an optional `USING charset` clause. MyLite's current
`CHAR()` executor supports integer, boolean, and `NULL` arguments, but not
character-set conversion. This slice admits the `USING` form as a parser
placeholder and rejects it at runtime until charset conversion semantics are
implemented.

### Expression GROUP BY And ORDER BY Keys

MySQL accepts expressions in `GROUP BY` and `ORDER BY`, including ordinals,
postfix `COLLATE`, and unary `BINARY`. MyLite's grouped executor remains
descriptor-backed and only supports documented grouped-expression subsets.
This slice expands parser acceptance for common expression keys while preserving
existing runtime diagnostics for unsupported grouped execution.

### DISTINCT Aggregate Syntax

MySQL accepts `DISTINCT` in several aggregate forms, including expression
arguments and `GROUP_CONCAT(DISTINCT expr ...)`. MyLite already supports
`COUNT(DISTINCT descriptor_column)` and non-distinct grouped aggregates. This
slice admits common DISTINCT aggregate syntax:

- `COUNT(DISTINCT expr[, expr...])`;
- `SUM(DISTINCT expr)` and `AVG(DISTINCT expr)`;
- `GROUP_CONCAT(DISTINCT expr ORDER BY ... SEPARATOR ...)`.

Supported existing forms keep their behavior. Newly admitted DISTINCT aggregate
forms that are not MyLite-supported must route to generic placeholders or
existing unsupported diagnostics; they must not execute by ignoring `DISTINCT`.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
expression ::= BINARY expression.

expression ::= expression REGEXP expression.
expression ::= expression RLIKE expression.
expression ::= expression NOT REGEXP expression.
expression ::= expression NOT RLIKE expression.

keyword_function_token ::= ROW.
keyword_function_token ::= VALUES.
keyword_function_token ::= GROUPING.
keyword_function_token ::= POINT.
keyword_function_token ::= LINESTRING.
keyword_function_token ::= POLYGON.
keyword_function_token ::= MULTIPOINT.
keyword_function_token ::= MULTILINESTRING.
keyword_function_token ::= MULTIPOLYGON.
keyword_function_token ::= GEOMETRYCOLLECTION.
keyword_function_token ::= GEOMCOLLECTION.

expression ::= keyword_function_token LPAREN RPAREN.
expression ::= keyword_function_token LPAREN function_argument_list RPAREN.
expression ::= CHAR LPAREN function_argument_list USING option_name RPAREN.
```

```lemon
group_key ::= INTEGER.
group_key ::= qualified_identifier COLLATE option_name.
group_key ::= BINARY qualified_identifier.
group_key ::= WEEK LPAREN expression RPAREN.
group_key ::= YEAR LPAREN expression RPAREN.
group_key ::= MONTH LPAREN expression RPAREN.
group_key ::= DAYOFMONTH LPAREN expression RPAREN.

select_order_key ::= selected_grouped_aggregate_expression.
select_order_key ::= BINARY qualified_identifier.
select_order_key ::= BINARY STRING.
select_order_key ::= predicate_collate_expression.

expression ::= COUNT LPAREN DISTINCT qualified_identifier COMMA function_argument_list RPAREN.
expression ::= COUNT LPAREN DISTINCT count_distinct_placeholder_argument RPAREN.
expression ::= SUM LPAREN DISTINCT expression RPAREN.
expression ::= AVG LPAREN DISTINCT expression RPAREN.
expression ::= GROUP_CONCAT LPAREN DISTINCT expression group_concat_order_opt
               group_concat_separator_opt RPAREN.
```

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser and runtime-classification
work:

- unary `BINARY` uses the existing binary conversion path;
- supported regexp predicates use the existing baseline regexp runtime, with
  scalar binary/binary matching case-sensitive and mixed binary/nonbinary
  non-`NULL` operands reporting MySQL's `3995 / HY000` character-set mismatch;
- unsupported keyword functions, `CHAR(... USING ...)`, row constructors,
  `GROUPING()`, and unsupported DISTINCT aggregate forms fail with explicit
  diagnostics;
- expression group keys parse but execute only where the grouped executor
  already supports the expression shape.

## Tests

MySQL 8.4.9 expectations cover representative syntax and observed result rows
for supported MySQL forms. MyLite parser tests cover parse acceptance and AST
shape. Runtime tests cover successful reuse of existing unary-binary and regexp
paths plus explicit unsupported diagnostics for newly admitted placeholders.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice moves common expression syntax from parser failures to support or
documented parser-placeholder behavior. It does not mark full row-constructor
execution, `GROUPING()` execution, charset conversion in `CHAR(... USING ...)`,
general expression grouping, or broad DISTINCT aggregate semantics as
supported. Result-option-before-`DISTINCT` select modifier ordering remains a
separate parser-refactor item because the current SELECT modifier grammar is
fixed-order.
