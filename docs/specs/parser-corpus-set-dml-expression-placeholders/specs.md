# Parser Corpus SET And DML Expression Placeholders

This slice reduces remaining MySQL server-test parser-corpus failures where
MySQL accepts general scalar expressions in variable assignments and DML value
positions, but MyLite's runtime still supports only documented scalar subsets.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/insert.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

MySQL uses `expr` in `SET variable = expr`, `INSERT ... VALUES` row values,
and `INSERT ... ON DUPLICATE KEY UPDATE` / `UPDATE` assignment values. MyLite
already executes narrow expression subsets in these positions, including
selected literal, variable, function, temporal, subquery, and same-column
arithmetic forms.

This slice admits additional well-formed failed statements as unsupported
placeholders when they contain recognized scalar operator surfaces in:

- `SET` assignment values;
- `INSERT` / `REPLACE` `VALUES` or `VALUE` row constructors;
- `INSERT` / `REPLACE` duplicate-key update assignment values;
- single-table, joined, or otherwise unsupported `UPDATE` assignment values.

The recognized operator surface includes MySQL arithmetic, bitwise, logical,
and unary operators already used by MyLite's expression-clause fallback,
including multiplication. The fallback is used only after normal parsing fails,
so existing executable `SET`, `INSERT`, `REPLACE`, and `UPDATE` subsets keep
their current AST and runtime behavior.

This slice intentionally does not implement broad assignment expression
execution, table-column expression evaluation in `INSERT` values, division or
modulo DML arithmetic, general system-variable expression mutation, stored
program local-variable semantics, or SQLite pass-through.

## MyLite Grammar Snippets

These snippets describe the intended future MyLite-owned Lemon grammar shape.
The implemented behavior is post-failure placeholder classification.

```lemon
set_assignment ::= set_variable ASSIGN expression.

insert_value ::= expression.
insert_values_source ::= VALUES insert_row_list.
insert_values_source ::= VALUE insert_row_list.

duplicate_update_assignment ::= column_name ASSIGN expression.
update_assignment ::= qualified_identifier ASSIGN expression.
```

## Runtime Behavior

Recognized statements become `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT` and
return the deterministic unsupported-utility diagnostic at execution time.
This preserves correctness while the runtime cannot yet reproduce MySQL's
expression typing, conversion, warning, row-order, and assignment side-effect
semantics.

Malformed expression tails remain syntax errors. Examples include incomplete
operator tails in `SET @x = 1 +`, `INSERT INTO t VALUES (1+)`, and
`UPDATE t SET a = a * WHERE id = 1`.

## Tests

MySQL 8.4.9 expectations cover representative valid syntax for arithmetic
`SET`, `INSERT ... VALUES`, duplicate-key update, and `UPDATE` assignment
expressions. MyLite parser tests cover placeholder AST admission and malformed
tail rejection. Runtime tests cover unsupported diagnostics for newly admitted
surfaces and preserve one existing executable same-column update arithmetic
parse path.

The parser corpus benchmark over `build/perf-data/mysql-server-tests-queries.csv`
must be rerun before commit to measure accepted query movement.

Final corpus measurement for this slice:

```text
parse.csv.mysql_server_tests: queries=69595 ok=68905 errors=690
parse_status: lexer_error=21 syntax_error=668 stack_overflow=1
```

The previous committed slice accepted 68,850 of the same 69,595 queries, so
this slice admits 55 additional corpus queries without changing lexer or
stack-overflow counts.

## Compatibility Status

This slice improves parser compatibility for MySQL-valid assignment and DML
value expressions by converting recognized syntax failures into explicit
unsupported placeholders. It does not broaden executable expression planning or
storage conversion.
