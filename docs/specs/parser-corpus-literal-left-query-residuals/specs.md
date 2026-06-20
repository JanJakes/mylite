# Parser Corpus Literal-Left Query Residuals

This slice reduces valid MySQL 8.4.9 parser-corpus residuals where the normal
MyLite grammar still rejects complete query expression forms:

- user-variable assignment expressions in `GROUP BY` keys;
- quoted function names followed by a whitespace-separated argument list in
  expression context;
- qualified-column bare truth predicates that appear in otherwise unsupported
  query envelopes.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/assignment-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html

Runtime probes are captured in:

- `packages/libmylite/tests/mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`

## MySQL 8.4.9 Observations

Observed accepted forms include:

```sql
SELECT * FROM t_dates WHERE '100000000000000000000002' = value;
SELECT * FROM t_dates WHERE '2010-02-01 09:31:02.0' <= a;
SELECT * FROM t_dates WHERE '2010-02-01 09:31:02.0' >= a;
SELECT * FROM v_dates WHERE '2005.02.02' = f1;
SELECT 1 FROM t_dates GROUP BY @b := @a, @b;
SELECT `foo` ();
SELECT DISTINCT t2.col_int_key
FROM t1 LEFT JOIN t2 ON t1.col_varchar_10 = t2.col_varchar_10_key
WHERE t2.pk
ORDER BY t2.col_int_key;
```

The quoted-function probe parses and then resolves as a missing stored or
loadable function when no such function exists. That confirms parser admission
without requiring MyLite to implement stored-function resolution in this slice.

## Scope

In scope:

- post-failure parser classification for complete `GROUP BY @var := expr`
  keys;
- post-failure parser classification for complete quoted function calls in
  expression context, including whitespace between the quoted name and `(`;
- post-failure parser classification for qualified-column bare truth predicates
  such as `WHERE t2.pk`;
- deterministic unsupported-utility diagnostics at runtime.

Out of scope:

- executable general expression planning for table-backed predicates, grouping,
  ordering, or projection;
- user-variable assignment side effects in grouped query execution;
- stored-function, loadable-function, or function-creation support;
- SQL-mode-sensitive double-quoted identifier admission outside the existing
  `ANSI_QUOTES` surfaces;
- malformed corpus fragments and lexer-error leftovers.

## MyLite Grammar Direction

The future broad grammar shape is:

```lemon
predicate ::= expression comparison_operator expression.
predicate ::= expression BETWEEN expression AND expression.
predicate ::= expression IN LPAREN expression_list RPAREN.

group_key ::= expression.
expression ::= user_variable ASSIGN expression.

function_call ::= quoted_identifier LPAREN argument_list_opt RPAREN.

predicate ::= expression.
```

This slice does not install those broad grammar rules directly. It extends the
existing post-failure placeholder classifier so recognized complete residual
surfaces return `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`. The separate
[literal-left string comparison baseline](../baseline-literal-left-string-comparison-predicates/specs.md)
now executes the simple comparison forms listed above. The separate
[literal-left string BETWEEN / IN baseline](../baseline-literal-left-string-between-in-predicates/specs.md)
now executes the descriptor-backed `BETWEEN` and `IN` forms that were previously
tracked here.

## Runtime Behavior

Runtime behavior is intentionally unchanged for the remaining residual grouping,
function-call, and qualified bare-truth forms. Accepted residuals fail with the
standard unsupported-utility diagnostic and do not reach SQLite execution. This
prevents MyLite from accidentally applying SQLite comparison, collation,
temporal, user-variable, or function-resolution semantics to MySQL-specific
expression forms.

Malformed tails such as missing `BETWEEN` bounds, incomplete `IN` lists,
unfinished user-variable assignments, or unmatched function parentheses remain
syntax errors.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for the representative accepted forms above;
- parser placeholder acceptance for each remaining residual family;
- malformed-tail regression tests;
- runtime unsupported diagnostics for accepted placeholders;
- parser-corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This slice moves the remaining targeted residuals from syntax errors to explicit
placeholder diagnostics. Literal-left string comparison and descriptor-backed
literal-left `BETWEEN` / `IN` predicates are now handled by executable baselines
linked above. This residual slice does not
mark broad table-backed expression execution, user-variable assignment inside
queries, or stored/loadable function resolution as supported.
