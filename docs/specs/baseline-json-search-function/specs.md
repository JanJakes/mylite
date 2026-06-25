# Baseline JSON_SEARCH function

## Scope

This slice implements a MySQL 8.4.9-compatible baseline for
`JSON_SEARCH(json_doc, one_or_all, search_str[, escape_char[, path] ...])`.

The supported execution envelope matches the current MyLite JSON scalar
baseline:

- tableless `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar projection;
- row-scalar `IS NULL` / `IS NOT NULL` predicates and simple comparison through
  the existing row-scalar predicate machinery where supported;
- compatible non-key single-table `UPDATE` assignments.

The function searches JSON string values only. Numeric, Boolean, object, array,
and JSON `null` values are traversed when they contain children, but only
string leaves can match.

## MySQL 8.4.9 behavior used for this baseline

Official reference: MySQL 8.4 Reference Manual,
`Functions That Search JSON Values`, `JSON_SEARCH()`.

Runtime probes were run against the local `mylite-mysql-849` container and
confirmed:

- `one_or_all = 'one'` returns a single JSON path string for the first matched
  string in MySQL's normalized traversal order.
- `one_or_all = 'all'` returns a single path string for one match, a JSON array
  of path strings for multiple distinct matches, or SQL `NULL` for no match.
- SQL `NULL` in `json_doc`, `one_or_all`, `search_str`, or any path argument
  makes the function return SQL `NULL`, except that invalid non-`NULL`
  arguments before the null are still validated according to normal function
  evaluation.
- `search_str` uses `LIKE`-style `%` and `_` wildcards.
- Missing or SQL `NULL` `escape_char` uses backslash. Empty escape disables
  escaping. A one-character escape literal is admitted. Longer escape strings
  raise `1210 / HY000`.
- Invalid `one_or_all` raises `3154 / 42000`.
- Invalid JSON document text raises `3141 / 22032`.
- Binary JSON document strings raise `3144 / 22032`.
- Non-string, non-JSON document values raise `3146 / 22032`.
- Invalid JSON path syntax raises `3143 / 42000`.

## Syntax

MyLite Lemon-syntax intent:

```lemon
expression ::= JSON_SEARCH LPAREN function_argument_list RPAREN.
expression ::= JSON_SEARCH LPAREN RPAREN.
```

Nonempty calls are represented as a list-argument function node so the runtime
can process the optional path tail. Zero arguments produce an argument-count
diagnostic node in the parser; one- and two-argument calls are accepted into the
same function node and produce the same native function parameter-count
diagnostic during scalar or row-scalar planning.

## Semantics

Argument handling:

- `json_doc` must be a JSON value or nonbinary string containing JSON text.
- `one_or_all` is case-insensitive for `one` and `all`; other values are errors.
- `search_str` is treated as a SQL string pattern.
- `escape_char` is optional. Missing or SQL `NULL` means backslash. Empty string
  disables escaping. A one-byte string sets the escape byte. Wider values are
  outside this baseline.
- Path arguments are optional. Without paths, search starts at `$`. With paths,
  each path limits the search to the selected subdocument and output paths keep
  the root-relative prefix.

Matching:

- `%` matches zero or more bytes.
- `_` matches exactly one byte.
- The escape character removes wildcard meaning from the following byte.
- If the escape character itself is `%` or `_`, it is recognized as an escape
  byte before wildcard interpretation. A trailing escape byte is matched as a
  literal byte.
- Matching is bytewise and case-sensitive for the supported utf8mb4/bin-style
  baseline.

Path output:

- Array legs render as `[index]`.
- Object members that are simple JSON path identifiers render as `.name`.
- Other object members render as `."json-escaped-member"`.
- `all` deduplicates exact output paths.

## Errors and warnings

The baseline emits MySQL-shaped diagnostics for:

- wrong argument count: `1582 / 42000`;
- invalid `one_or_all`: `3154 / 42000`;
- invalid escape width: `1210 / HY000`;
- invalid JSON text: `3141 / 22032`;
- invalid JSON path: `3143 / 42000`;
- binary JSON document text: `3144 / 22032`;
- invalid JSON document data type: `3146 / 22032`.

No warnings are expected for successful baseline calls.

## MyLite implementation plan

- Add `JSON_SEARCH` lexer/parser/AST support.
- Add `mylite_json_search()` in a dedicated JSON helper source file.
- Add a private SQLite UDF `_mylite_json_search()` for row-scalar execution.
- Reuse existing JSON scalar argument decoders for document, path, string, and
  SQL `NULL` behavior.
- Reuse the existing JSON path parser for simple path scopes.

## Known gaps

- JSON path wildcards, recursive descent, ranges, and filters are not executed.
  They return MyLite's existing unsupported JSON path diagnostic in row-scalar
  contexts and are documented as outside the baseline.
- Arbitrary expression arguments outside the current row-scalar envelope remain
  unsupported.
- Matching is bytewise for the current supported string/collation subset; full
  MySQL character-set and collation interaction is future work.
- Multi-byte escape characters and prepared-statement compile-time constant
  checks are outside this baseline.
- `JSON_TABLE()` and JSON Schema validation functions remain separate missing
  JSON surfaces.
