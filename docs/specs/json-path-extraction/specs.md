# JSON Path Extraction

## Scope

This slice implements the first path-aware JSON runtime surface for MyLite:

- JSON path parsing and evaluation for root, object-member, quoted-member,
  array-index, `last`, array-range, member wildcard, array wildcard, and
  recursive-descent path legs.
- `JSON_EXTRACT(json_doc, path[, path] ...)`.
- `JSON_CONTAINS_PATH(json_doc, one_or_all, path[, path] ...)`.
- `JSON_KEYS(json_doc[, path])`.
- `JSON_LENGTH(json_doc[, path])`.
- `col->'path'` and `col->>'path'` JSON column operators.

`JSON_VALUE()`, JSON mutators, JSON search functions beyond
`JSON_CONTAINS_PATH()`, JSON aggregate functions, binary JSON column storage,
JSON comparison ordering, multi-valued indexes, and partial-update storage
metadata remain separate slices.

## Sources

The behavior is based on the MySQL 8.4 JSON function and JSON path
documentation plus observed MySQL 8.4.9 runtime behavior.

- <https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json.html>

## Syntax

Function calls use ordinary scalar function syntax:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
```

The JSON column operators are parsed only for identifiers on the left-hand
side, matching MySQL's column-oriented operator syntax:

```lemon
primary_expression ::= qualified_identifier JSON_EXTRACT STRING.
primary_expression ::= qualified_identifier JSON_UNQUOTE_EXTRACT STRING.
```

The right-hand operand must be a string literal path. A parenthesized string
literal or arbitrary expression to the right of `->` is not part of this slice.

## Path Semantics

MyLite path parsing accepts:

- `$` for the document root.
- `.name` for unquoted object members.
- `."name"` for quoted object members, including JSON string escapes.
- `.*` for all object members.
- `[N]` for zero-based array indexes.
- `[last]` and `[last-N]` for indexes relative to the end of an array.
- `[N to M]` ranges.
- `[*]` for all array elements.
- `**` recursive descent followed by another leg, such as `$**.id`.

Objects are normalized the same way as the scalar JSON foundation: duplicate
keys keep the last value and serialized member order is by key byte length,
then byte value. Arrays preserve element order.

Wildcard, range, and recursive paths can produce multiple matches. If
`JSON_EXTRACT()` is given multiple paths, or a single path can match multiple
values, MyLite autowraps matched values in a JSON array. If no path matches,
the result is SQL `NULL`. A matched JSON `null` is returned as JSON text
`null`, not SQL `NULL`.

`JSON_KEYS()` rejects paths containing `*`, `**`, or array ranges with MySQL
error 3149. It returns SQL `NULL` when the selected value is missing or is not
an object.

`JSON_LENGTH()` returns:

- the number of members for an object;
- the number of elements for an array;
- `1` for scalar JSON values;
- SQL `NULL` for a missing path;
- the autowrapped match count for multi-match paths.

`JSON_CONTAINS_PATH()` accepts case-insensitive `one` and `all` mode strings.
It returns `1` or `0`, and returns SQL `NULL` if any argument is SQL `NULL`.

## Diagnostics

Invalid JSON document text raises error 3141 with the JSON parser position.
Non-string JSON document arguments raise error 3146. Invalid path expressions
raise error 3143. `JSON_KEYS()` wildcard, recursive, or range paths raise
error 3149. Invalid `JSON_CONTAINS_PATH()` `one_or_all` mode raises error
3154.

MyLite stores these as statement diagnostics using the existing expression
warning/error path.

## Metadata

`JSON_EXTRACT()` and `JSON_KEYS()` return MySQL JSON field metadata, binary
flag, nullable results, and a byte-scaled JSON document display length, such as
`4294967292` under `utf8mb4`. `JSON_CONTAINS_PATH()` and `JSON_LENGTH()` return
nullable signed `LONGLONG` metadata. `->` returns JSON metadata, while `->>`
returns long binary text metadata with the MySQL-observed connection-sensitive
display length.

## Runtime Coverage

The runtime tests cover:

- scalar extraction, missing paths, JSON `null`, multiple paths, wildcards,
  quoted members, ranges, and recursive descent;
- `JSON_CONTAINS_PATH()` one/all behavior;
- `JSON_KEYS()` object, nested object, non-object, and wildcard rejection;
- `JSON_LENGTH()` object, nested object, range, and missing paths;
- `->` and `->>` in projection, `ORDER BY`, and predicates;
- result metadata and MySQL error codes for invalid JSON, path, mode, and
  path-wildcard cases.
