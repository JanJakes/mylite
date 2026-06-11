# Parser Corpus Order Expression Residuals

This slice reduces remaining MySQL server-test parser-corpus failures around
order-key expressions and unary logical negation. It admits MySQL-valid syntax
without expanding MyLite's executable ordering planner beyond the currently
documented descriptor-column subsets.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html
- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### Unary `!`

MySQL treats `!` as a unary logical-not operator with higher precedence than
keyword `NOT` in the default SQL mode. MyLite already has AST and runtime
support for logical-not expressions, but the lexer/parser bridge did not map
the `!` operator into the expression grammar.

This slice maps `!` to a dedicated high-precedence parser token and reuses the
existing logical-not AST operator. Keyword `NOT` keeps its existing lower
precedence and grammar behavior.

### SELECT `ORDER BY` Window Functions

MySQL permits window functions in the select list and top-level `ORDER BY`
clause. MyLite already parses and partially executes the supported
projection-only window-function subset. This slice factors the existing
window-function grammar so the same parser surface can be used as a
top-level `ORDER BY` key.

Runtime execution remains limited. Window functions in `ORDER BY` are parsed
as MySQL-valid syntax, but unsupported by the current descriptor order planner
unless a later slice adds execution support.

### DML `ORDER BY` Expressions

MySQL single-table `DELETE` and `UPDATE` statements accept `ORDER BY`
expressions. MyLite previously admitted only one identifier key in the shared
DML `ORDER BY` grammar.

This slice changes the shared single-table DML `ORDER BY` key from a qualified
identifier to the existing expression nonterminal. Existing descriptor-column
keys still produce the same AST shape, while expression keys reach runtime and
fail with the current explicit unsupported ordering diagnostic unless they fit
the documented descriptor-column subset.

Joined `DELETE` and joined `UPDATE` remain non-executable with `ORDER BY` /
`LIMIT` as documented.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape.

```lemon
expression ::= LOGICAL_NOT expression.
expression ::= NOT expression.
```

```lemon
window_function_expression ::= ROW_NUMBER LPAREN RPAREN over_clause.
window_function_expression ::= RANK LPAREN RPAREN over_clause.
window_function_expression ::= DENSE_RANK LPAREN RPAREN over_clause.
window_function_expression ::= PERCENT_RANK LPAREN RPAREN over_clause.
window_function_expression ::= CUME_DIST LPAREN RPAREN over_clause.
window_function_expression ::= NTILE LPAREN expression RPAREN over_clause.
window_function_expression ::= LAG LPAREN expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LAG LPAREN expression COMMA expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LAG LPAREN expression COMMA expression COMMA expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LEAD LPAREN expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LEAD LPAREN expression COMMA expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LEAD LPAREN expression COMMA expression COMMA expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= FIRST_VALUE LPAREN expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= LAST_VALUE LPAREN expression RPAREN window_null_treatment_opt over_clause.
window_function_expression ::= NTH_VALUE LPAREN expression COMMA expression RPAREN window_null_treatment_opt over_clause.

expression ::= window_function_expression.
select_order_key ::= window_function_expression.
```

```lemon
order_clause_opt ::= ORDER BY expression order_direction_opt.
```

## Runtime Behavior

No SQLite fork hook is needed.

- `!` expressions reuse the current scalar logical-not evaluation where the
  surrounding statement is already executable.
- Window functions in top-level `ORDER BY` parse into normal SELECT ASTs but
  return the current unsupported ordering diagnostic at execution.
- Single-table DML expression order keys parse into normal DML ASTs but return
  the current unsupported ordering diagnostic unless they are descriptor
  columns in the documented supported families.

## Tests

MySQL 8.4.9 expectations cover:

- scalar `!` behavior and an observed nested `DO ... RLIKE ... !@var` corpus
  shape;
- `SELECT ... ORDER BY RANK() OVER (...)`;
- single-table `DELETE` and `UPDATE` with parenthesized system-variable
  expression order keys.

MyLite tests cover parser AST admission, executable `!` scalar behavior,
unsupported diagnostics for newly parsed order-expression surfaces, and
malformed syntax preservation. The parser corpus benchmark over
`mysql-server-tests-queries.csv` must be rerun before commit.

## Compatibility Status

This slice improves parser compatibility for MySQL-valid expression order
keys and unary `!`. It does not implement general expression ordering,
window-function ordering execution, SQL-mode-specific `HIGH_NOT_PRECEDENCE`,
or wider DML ordering semantics.
