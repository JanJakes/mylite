# Baseline JSON_TABLE literal table source

## Scope

This slice implements a MySQL 8.4.9-shaped `JSON_TABLE()` baseline for literal
JSON documents used as a `FROM` table source.

Supported:

- `JSON_TABLE(<string literal>, <row path literal> COLUMNS (...)) [AS] alias`;
- row paths `$` and `$[*]`;
- `name FOR ORDINALITY`;
- `name type PATH '<json path>'` with optional `NULL ON EMPTY` and
  `NULL ON ERROR` clauses;
- `name type EXISTS PATH '<json path>'`;
- integer, string-family, and `JSON` output descriptors;
- empty rowsets;
- result rows flowing through normal SQLite filtering, ordering, and projection;
- protocol result metadata for the synthetic columns.

Deferred:

- column-backed or lateral `JSON_TABLE()` documents;
- nested path/column clauses;
- `DEFAULT ... ON EMPTY`, `DEFAULT ... ON ERROR`, and `ERROR ON ...`;
- row paths beyond `$` and `$[*]`;
- full MySQL JSON-to-SQL coercion;
- DML targets and metadata surfaces beyond direct result metadata.

## MySQL 8.4.9 behavior used for this baseline

Official reference: MySQL 8.4 Reference Manual,
`JSON Table Functions`, `JSON_TABLE()`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
confirmed:

- `JSON_TABLE()` is a table function and requires an alias; missing aliases
  raise `3667 / 42000`.
- `FOR ORDINALITY` starts at 1 and increments per emitted row.
- `$` emits one row for the document root.
- `$[*]` emits one row for each top-level array element and no rows for an
  empty array.
- missing PATH values use SQL `NULL` when the effective empty behavior is
  `NULL ON EMPTY`;
- integer conversion failures use SQL `NULL` when `NULL ON ERROR` applies;
- `EXISTS PATH` columns return 1 for present paths and 0 for missing paths;
- `JSON` output columns return normalized JSON text or SQL `NULL`.

## Syntax

MyLite Lemon-syntax intent:

```lemon
table_source ::= json_table_source.

json_table_source ::=
    JSON_TABLE LPAREN expression COMMA string_text_literal COLUMNS LPAREN
    json_table_column_list RPAREN RPAREN derived_table_alias_opt.

json_table_column_list ::= json_table_column.
json_table_column_list ::= json_table_column_list COMMA json_table_column.

json_table_column ::= identifier FOR ORDINALITY.
json_table_column ::= identifier column_type PATH string_text_literal json_table_null_behavior_opt.
json_table_column ::= identifier column_type EXISTS PATH string_text_literal json_table_null_behavior_opt.

json_table_null_behavior_opt ::= .
json_table_null_behavior_opt ::= json_table_null_behavior.
json_table_null_behavior_opt ::= json_table_null_behavior json_table_null_behavior.
json_table_null_behavior ::= NULL ON EMPTY.
json_table_null_behavior ::= NULL ON ERROR.
```

The parser accepts an expression in the document position so later slices can
support column and function sources without replacing the AST shape. This
baseline runtime rejects non-string-literal documents with a predictable
unsupported diagnostic.

## Semantics

Planning evaluates the literal JSON document and literal row path once, then
materializes a small synthetic source descriptor. The generated SQLite SQL is a
derived rowset made from bound parameters, so SQLite still handles the outer
projection, predicates, joins that are otherwise admitted, ordering, and limit.

Column behavior:

- Ordinality columns are nullable `BIGINT` descriptors in protocol metadata and
  currently contain non-`NULL` 1-based integers.
- Integer typed PATH columns accept JSON integer values and booleans. Missing,
  JSON `null`, non-integer numeric, object, array, or failed conversion values
  become SQL `NULL`.
- String-family PATH columns return JSON strings as SQL text. Other JSON values
  are emitted as normalized JSON text.
- JSON PATH columns return normalized JSON text.
- EXISTS columns return integer `1` or `0` while preserving the declared output
  descriptor.

## Diagnostics

The baseline emits MySQL-shaped or MyLite-supported diagnostics for:

- missing alias: `3667 / 42000`;
- invalid JSON document text: `3141 / 22032`;
- invalid JSON path syntax: `3143 / 42000`;
- non-literal document expressions: `1064 / 42000`;
- unsupported row path shape: `1064 / 42000`;
- unsupported output descriptor type: `1064 / 42000`.

## Storage, SQLite, and performance

No SQLite fork hook is needed for this slice. MyLite owns parsing, JSON DOM
evaluation, column descriptor planning, and parameter binding; SQLite executes
the resulting synthetic rowset as normal SQL.

The current literal baseline is intended for small compatibility rowsets. A
future column-backed/lateral implementation should stream or otherwise avoid
large eager materialization when application workloads expose that need.

## Known gaps

The implemented baseline is not a complete `JSON_TABLE()` engine. The most
important future work is lateral column input, nested path support, full
`ON EMPTY` / `ON ERROR` behavior, broader row-path support, and MySQL's full
coercion rules for typed output columns.
