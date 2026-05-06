# Index Hint Parser Placeholders

## Scope

This feature adds MySQL table-reference index hint syntax as a parser
placeholder. MyLite recognizes the hint clauses and ignores them during
binding and execution:

- `USE INDEX (...)`
- `USE KEY (...)`
- `FORCE INDEX (...)`
- `FORCE KEY (...)`
- `IGNORE INDEX (...)`
- `IGNORE KEY (...)`
- optional `FOR JOIN`, `FOR ORDER BY`, and `FOR GROUP BY` scopes

The syntax is accepted after a base table name and optional table alias in
supported `SELECT`, single-table `UPDATE`, joined `UPDATE`, single-table
`DELETE`, and multi-table `DELETE` table references.

## Sources

- MySQL 8.4 Reference Manual, Index Hints:
  https://dev.mysql.com/doc/refman/8.4/en/index-hints.html
- MySQL 8.4 Reference Manual, `JOIN` Clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- Runtime probes against local `mysql:8.4.9` container
  `mylite-mysql-849`.

The specification is independently authored from official documentation and
observed runtime behavior.

## Syntax

MyLite Lemon grammar shape:

```lemon
table_factor ::= table_name opt_table_alias opt_index_hint_list.
single_update_target ::= table_name opt_table_alias opt_index_hint_list.
single_delete_target ::= delete_table_name opt_table_alias opt_index_hint_list.

opt_index_hint_list ::= .
opt_index_hint_list ::= index_hint_list.

index_hint_list ::= index_hint.
index_hint_list ::= index_hint_list index_hint.

index_hint ::= USE index_hint_index_or_key opt_index_hint_scope
    LPAREN opt_index_hint_name_list RPAREN.
index_hint ::= IGNORE index_hint_index_or_key opt_index_hint_scope
    LPAREN opt_index_hint_name_list RPAREN.
index_hint ::= FORCE index_hint_index_or_key opt_index_hint_scope
    LPAREN opt_index_hint_name_list RPAREN.

index_hint_index_or_key ::= INDEX.
index_hint_index_or_key ::= KEY.

opt_index_hint_scope ::= .
opt_index_hint_scope ::= FOR JOIN.
opt_index_hint_scope ::= FOR ORDER BY.
opt_index_hint_scope ::= FOR GROUP BY.

opt_index_hint_name_list ::= .
opt_index_hint_name_list ::= index_hint_name_list.

index_hint_name_list ::= index_hint_name.
index_hint_name_list ::= index_hint_name_list COMMA index_hint_name.

index_hint_name ::= identifier.
index_hint_name ::= PRIMARY.
```

Hints do not create AST nodes. The table-reference AST is the same as it would
be without hints.

## Runtime Semantics

Index hints are advisory optimizer controls in MySQL. MyLite currently has no
MySQL-compatible optimizer/index selection layer, so recognized hints are
ignored. They do not validate named indexes, affect diagnostics, affect result
metadata, or change statement effects.

This is a compatibility placeholder for applications that emit index hints in
otherwise supported SQL.

## Deferred

- Optimizer behavior for `USE`, `FORCE`, and `IGNORE`.
- Validation of referenced index names.
- Interaction with future physical secondary-index selection.
- Replacement of legacy index hints with optimizer hints.

## Tests

Coverage includes:

- parser acceptance for one-table `SELECT`
- parser acceptance for hinted joined table factors
- parser acceptance for single-table `UPDATE` and `DELETE`
- runtime execution showing supported `SELECT`, joined `SELECT`, `UPDATE`, and
  `DELETE` statements ignore hints without changing result shape or side
  effects
