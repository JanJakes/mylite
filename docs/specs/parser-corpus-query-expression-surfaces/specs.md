# Parser Corpus Query Expression Surfaces

This slice reduces high-volume MySQL server-test parser failures around query
expression structure: parenthesized query expressions, `TABLE` and `VALUES`
query blocks in set-operation positions, `NATURAL` / `USING` join syntax, and
window-function null-treatment syntax. The goal is to admit MySQL 8.4.9-shaped
syntax where MyLite can safely preserve semantics or return a deterministic
unsupported diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/values.html
- https://dev.mysql.com/doc/refman/8.4/en/join.html
- https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html

Runtime probes were run against the local MySQL 8.4.9 container
`mylite-mysql-849` with `mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw`.

## Scope

### Parenthesized Query Expressions

MySQL treats `SELECT`, `TABLE`, and `VALUES` as query blocks that can be used in
query expressions, optionally parenthesized and combined by set operators.
Parenthesized query expressions may also carry outer `ORDER BY`, `LIMIT`, and
`INTO` clauses. This slice admits:

- top-level `(SELECT ...)`, `(TABLE ...)`, and `(VALUES ...)` forms;
- top-level nested parenthesized query expressions;
- parenthesized query blocks as set-operation operands;
- `TABLE` and `VALUES` operands in set-operation chains;
- parenthesized query expressions as derived-table sources.

Runtime support remains conservative. A parenthesized query expression that
wraps a single existing supported child statement without an outer query-level
clause executes by delegating to that child. Parenthesized set-operation forms,
`TABLE` / `VALUES` set-operation operands, branch-local `ORDER BY` / `LIMIT`,
derived `TABLE` / `VALUES` sources, and mixed set-operator precedence are
parser-admitted but must return deterministic unsupported diagnostics unless an
existing executor already supports the exact shape.

Unparenthesized `TABLE`-started and `VALUES`-started set-operation chains are
parser-admitted without a trailing query-level `ORDER BY` / `LIMIT` in this
slice. MySQL 8.4.9 accepts the broader ordered/limited forms, but MyLite leaves
those outer clauses for a later query-expression grammar refactor to avoid
introducing Lemon conflicts in the existing `SELECT` order/limit grammar.

### `VALUES` Query Blocks

Standalone `VALUES ROW(...)` execution already exists in MyLite. This slice
admits `VALUES` where MySQL treats it as a query block, including
`VALUES ROW(1), ROW(2) UNION SELECT 2` and
`SELECT * FROM (VALUES ROW(...)) AS alias(col, ...)`. The latter requires
derived-table aliases with optional column lists.

Derived `VALUES` execution is not implemented in this slice. It is accepted at
parse time and rejected at runtime with an explicit unsupported diagnostic.
This avoids silently materializing rows through an incomplete descriptor model.

### `NATURAL` And `USING` Joins

MySQL `USING (col[, ...])` joins coalesce named join columns in wildcard output,
and `NATURAL` joins infer the `USING` column list from common column names.
Runtime probes confirmed that these forms are not equivalent to an ordinary
`ON` join in result-column shape or matching behavior.

This slice admits:

- `JOIN ... USING (col[, ...])`;
- `INNER JOIN`, `CROSS JOIN`, and `STRAIGHT_JOIN` with `USING`;
- `LEFT [OUTER] JOIN` and `RIGHT [OUTER] JOIN` with `USING`;
- `NATURAL JOIN`, `NATURAL INNER JOIN`, `NATURAL LEFT [OUTER] JOIN`, and
  `NATURAL RIGHT [OUTER] JOIN`.

Execution must reject `NATURAL` and `USING` joins with explicit unsupported
diagnostics. It must not lower them to ordinary joins because that would expose
wrong result columns and could change duplicate/`NULL` matching behavior.

### Window Null Treatment

MySQL value and navigation window functions accept optional `RESPECT NULLS` or
`IGNORE NULLS` between the function call and `OVER`. Observed MySQL 8.4.9
behavior:

- `FIRST_VALUE`, `LAST_VALUE`, `NTH_VALUE`, `LAG`, and `LEAD` accept
  `RESPECT NULLS`, which is the default behavior.
- The same functions parse `IGNORE NULLS` but return
  `1235 / 42000` because MySQL 8.4.9 does not support the behavior.
- Ranking functions such as `ROW_NUMBER()` do not admit a null-treatment clause.
- `NTH_VALUE(expr, n FROM FIRST|LAST)` remains a syntax error in MySQL 8.4.9.

This slice parses null treatment for the five supported value/navigation
functions. `RESPECT NULLS` executes like the existing default for currently
supported window envelopes. `IGNORE NULLS` returns a deterministic unsupported
diagnostic before execution.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
statement ::= parenthesized_query_expression.

query_expression_body ::= select_statement.
query_expression_body ::= table_statement.
query_expression_body ::= values_statement.
query_expression_body ::= parenthesized_query_expression.

parenthesized_query_expression ::=
    LPAREN query_expression_body RPAREN query_expression_order_limit_opt.
parenthesized_query_expression ::=
    LPAREN compound_select_statement RPAREN query_expression_order_limit_opt.

compound_select_statement ::= select_statement union_term_list.
query_compound_statement ::= table_statement union_term_list.
query_compound_statement ::= values_statement union_term_list.
query_compound_statement ::= parenthesized_query_expression union_term_list.
union_term ::= UNION union_modifier_opt query_expression_body.
union_term ::= INTERSECT union_modifier_opt query_expression_body.
union_term ::= EXCEPT union_modifier_opt query_expression_body.

derived_table_source ::= LPAREN query_expression_body RPAREN derived_table_alias.
derived_table_alias ::= table_alias_opt.
derived_table_alias ::= table_alias LPAREN identifier_list RPAREN.
```

```lemon
join_condition_opt ::= .
join_condition_opt ::= ON predicate.
join_condition_opt ::= USING LPAREN identifier_list RPAREN.

joined_table_source ::=
    table_source natural_join_operator table_source.
joined_table_source ::=
    joined_table_source natural_join_operator table_source.

natural_join_operator ::= NATURAL JOIN.
natural_join_operator ::= NATURAL INNER JOIN.
natural_join_operator ::= NATURAL LEFT JOIN.
natural_join_operator ::= NATURAL LEFT OUTER JOIN.
natural_join_operator ::= NATURAL RIGHT JOIN.
natural_join_operator ::= NATURAL RIGHT OUTER JOIN.
```

```lemon
window_null_treatment_opt ::= .
window_null_treatment_opt ::= RESPECT NULLS.
window_null_treatment_opt ::= IGNORE NULLS.

expression ::= LAG LPAREN expression RPAREN window_null_treatment_opt over_clause.
expression ::= LEAD LPAREN expression RPAREN window_null_treatment_opt over_clause.
expression ::= FIRST_VALUE LPAREN expression RPAREN window_null_treatment_opt over_clause.
expression ::= LAST_VALUE LPAREN expression RPAREN window_null_treatment_opt over_clause.
expression ::= NTH_VALUE LPAREN expression COMMA expression RPAREN
               window_null_treatment_opt over_clause.
```

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser/AST work plus runtime
classification:

- simple parenthesized wrappers delegate to the wrapped statement;
- parenthesized query expressions with outer query clauses return an explicit
  unsupported diagnostic;
- `NATURAL` and `USING` joins return explicit unsupported diagnostics before
  select/update/delete planning;
- `IGNORE NULLS` returns a MySQL-shaped unsupported diagnostic;
- newly accepted unsupported set-operation operands remain rejected at runtime
  instead of being executed through a mismatched `SELECT`-only path.

## Tests

MySQL 8.4.9 expectations are verified with a focused shell script covering:

- parenthesized `SELECT`, `TABLE`, `VALUES`, and set-operation forms;
- `VALUES` as a set-operation operand and as a derived table;
- `USING` and `NATURAL` join output shape;
- `RESPECT NULLS`, `IGNORE NULLS`, and invalid ranking/null-treatment syntax.

MyLite parser tests cover AST acceptance for the same syntax classes. Runtime
tests cover delegated simple parenthesized query expressions and explicit
diagnostics for unsupported `NATURAL`, `USING`, derived `VALUES`, query-level
clauses on parenthesized wrappers, and `IGNORE NULLS`.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice moves selected query-expression syntax from syntax errors to parser
acceptance or explicit runtime diagnostics. It does not mark general
parenthesized query-expression execution, derived `VALUES`, `NATURAL` /
`USING` join semantics, mixed set-operation precedence, branch-local query
clauses, or `IGNORE NULLS` execution as supported.
