# Parser Corpus Function Expression Placeholders

This slice reduces high-volume MySQL server-test parser-corpus failures where a
valid MySQL expression contains a function call shape that MyLite does not yet
execute in that query or DML context. The compatibility goal is to distinguish
valid-but-unsupported MySQL syntax from malformed SQL, returning MyLite's
existing deterministic unsupported-utility diagnostic after the normal parser
fails.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

## MySQL 8.4.9 Observations

Representative accepted MySQL forms from the corpus surface:

```sql
SELECT HEX(WEIGHT_STRING('a' AS CHAR(1)));
SELECT COUNT(DISTINCT a) FROM t GROUP BY b HAVING COUNT(DISTINCT a) > 1;
SELECT GROUP_CONCAT(name ORDER BY name SEPARATOR ',') FROM t GROUP BY grp;
SELECT latin1_f FROM t ORDER BY latin1_f, HEX(latin1_f);
SELECT * FROM t WHERE word = CAST(0xDF AS CHAR);
INSERT INTO t VALUES (DATE_FORMAT('2004-02-02','%M'));
UPDATE t SET a = DATE_ADD(NULL, INTERVAL 1 DAY);
DELETE FROM t WHERE fld3 = 'd%' ORDER BY RAND();
```

Observed behavior:

- MySQL's expression grammar admits function calls as scalar expressions.
- Aggregate functions admit modifiers such as `DISTINCT`; `GROUP_CONCAT()`
  additionally admits `ORDER BY` and `SEPARATOR` inside the argument list.
- `CAST()` and `CONVERT()` may appear as scalar predicate operands and query
  clause expressions.
- Zero-argument and user-defined function-call syntax is parse-valid even when
  later execution may fail because a function is unknown or not usable.
- Malformed argument lists, incomplete statements, and dangling operators remain
  syntax errors.

## Scope

MyLite already implements many individual scalar, aggregate, and window
functions in explicitly supported contexts. This slice does not broaden those
execution semantics. Instead, after the normal parser fails, the placeholder
classifier recognizes query and DML statements that contain a well-formed
function-call expression surface and classifies the whole statement as an
unsupported utility placeholder.

Recognized statement starts:

- `SELECT`, including parenthesized query-expression starts;
- `WITH` query starts;
- `TABLE` and `VALUES` query starts;
- `INSERT`, `REPLACE`, `UPDATE`, and `DELETE`.

Recognized function-call surface:

- identifier-like or keyword function name immediately followed by `(`;
- balanced argument parentheses;
- empty argument lists are allowed for parse-valid zero-argument and
  user-defined-function syntax;
- nonempty argument lists must not have missing arguments around top-level
  commas;
- the token after the closing parenthesis must be a plausible expression,
  clause, or statement continuation;
- the whole statement must not end in an obviously incomplete clause keyword,
  operator, comma, or open parenthesis.
- `ROW(...)` constructors and built-in window-only function names are left to
  their existing grammar-specific paths rather than being treated as generic
  function placeholders.
- direct text SQL containing parameter markers remains a syntax error until the
  prepared-statement path owns those expression placeholders.

The classifier intentionally runs only after the normal parser fails. Supported
function statements therefore keep their normal AST and runtime path. Newly
recognized statements return MyLite's existing unsupported-utility runtime
diagnostic instead of a raw syntax error.

Out of scope:

- broad Lemon expression grammar replacement;
- execution for newly classified function expressions;
- MySQL function result types, metadata, warnings, or side effects for these
  contexts;
- full aggregate modifier semantics;
- full table functions such as `JSON_TABLE()`;
- accepting malformed argument lists or incomplete statements.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned grammar surface. This slice
does not implement them directly in Lemon because the current grammar uses
narrow context-specific expression subsets and prior broad rewrites caused
conflicts. Instead, it recognizes these shapes in the post-failure placeholder
classifier.

```lemon
expression ::= function_call.
expression ::= aggregate_function_call.
expression ::= CAST LPAREN expression AS type_name RPAREN.
expression ::= CONVERT LPAREN expression COMMA type_name RPAREN.
expression ::= CONVERT LPAREN expression USING charset_name RPAREN.

function_call ::= function_name LPAREN function_argument_list_opt RPAREN.
function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.

aggregate_function_call ::= aggregate_name LPAREN aggregate_modifier_opt expression RPAREN.
aggregate_modifier_opt ::= .
aggregate_modifier_opt ::= DISTINCT.
aggregate_modifier_opt ::= ALL.

group_concat_call ::= GROUP_CONCAT LPAREN group_concat_arguments RPAREN.
group_concat_arguments ::= DISTINCT_opt expression_list group_concat_order_opt
                           group_concat_separator_opt.
```

## Runtime Behavior

No SQLite fork hook is needed. Runtime behavior uses the existing unsupported
utility statement path:

- parsing succeeds with `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`;
- execution returns `1064 / 42000` with the existing unsupported utility
  diagnostic;
- no tables, metadata, variables, warnings, or result rows are modified.

Supported function statements that already parse normally are unchanged.
Malformed statements remain syntax errors.

## Tests

Focused tests cover:

- parser placeholder classification for function calls in projection,
  aggregate arguments, `ORDER BY`, `GROUP BY`,
  `HAVING`, `WHERE`, `INSERT ... VALUES`, `UPDATE ... SET`, and `DELETE ...
  ORDER BY`;
- preservation of existing normal parsing for already admitted function forms,
  including `GROUP_CONCAT()` modifiers;
- parser preservation for existing supported scalar functions;
- parser syntax errors for missing function arguments, dangling operators, and
  incomplete clauses;
- preservation of direct text SQL parameter-marker syntax errors;
- runtime unsupported-utility diagnostics for newly classified placeholders;
- MySQL 8.4.9 expectation script coverage for representative accepted syntax;
- parser corpus benchmark movement.

## Compatibility Status

This slice improves parser compatibility for valid MySQL function expression
surfaces by classifying unsupported contexts explicitly. It does not mark broad
function expression execution, aggregate modifiers, table functions, or general
expression metadata as supported.
